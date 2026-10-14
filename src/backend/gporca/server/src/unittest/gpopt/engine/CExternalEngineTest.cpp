//---------------------------------------------------------------------------
//	CExternalEngineTest.cpp
//---------------------------------------------------------------------------

#include "unittest/gpopt/engine/CExternalEngineTest.h"

#include "gpos/error/CAutoTrace.h"

#include "gpdbcost/CCostModelGPDB.h"
#include "gpopt/base/CDistributionSpecHashed.h"
#include "gpopt/base/CDrvdPropPlan.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/base/IExternalEngine.h"
#include "gpopt/mdcache/CMDCache.h"
#include "gpopt/minidump/CMinidumperUtils.h"
#include "gpopt/operators/CPhysicalAgg.h"
#include "gpopt/operators/CPhysicalMotionBroadcast.h"
#include "gpopt/operators/CPhysicalMotionGather.h"
#include "gpopt/operators/CPhysicalMotionHashDistribute.h"
#include "gpopt/optimizer/COptimizerConfig.h"
#include "gpopt/xforms/CXform.h"
#include "gpopt/xforms/CXformContext.h"
#include "gpopt/xforms/CXformResult.h"
#include "naucrates/md/CMDProviderMemory.h"

#include "unittest/base.h"
#include "unittest/gpopt/CTestUtils.h"

using namespace gpopt;
using namespace gpdbcost;

namespace
{
// An engine that runs scans, filters, projections, hash aggregates, hash
// joins and CTEs, ships batches over every motion but a merging gather, and costs
// its operators at half of what PG would.
class CTestEngine : public IExternalEngine
{
private:
	static BOOL
	FTwin(COperator *pop)
	{
		switch (pop->Eopid())
		{
			case COperator::EopPhysicalTableScan:
			case COperator::EopPhysicalFilter:
			case COperator::EopPhysicalComputeScalar:
			case COperator::EopPhysicalInnerHashJoin:
			case COperator::EopPhysicalLeftOuterHashJoin:
			case COperator::EopPhysicalSequence:
			case COperator::EopPhysicalCTEProducer:
				return true;
			case COperator::EopPhysicalHashAgg:
			case COperator::EopPhysicalScalarAgg:
				return COperator::EgbaggtypeIntermediate !=
					   CPhysicalAgg::PopConvert(pop)->Egbaggtype();
			default:
				return false;
		}
	}

public:
	void
	AddAlternatives(CMemoryPool *mp, const CXform *pxform, CExpression *pexpr,
					CXformResult *pxfres, ULONG ulFirst) const override
	{
		if (CUtils::HasOuterRefs(pexpr))
		{
			return;
		}
		for (ULONG ul = 0; ul < pexpr->Arity(); ul++)
		{
			if ((*pexpr)[ul]->Pop()->FScalar() &&
				(*pexpr)[ul]->DeriveHasSubquery())
			{
				return;
			}
		}
		CExpressionArray *pdrgpexpr = pxfres->Pdrgpexpr();
		BOOL fWanted = false;
		for (ULONG ul = ulFirst; ul < pdrgpexpr->Size(); ul++)
		{
			fWanted = fWanted || FTwin((*pdrgpexpr)[ul]->Pop());
		}
		if (!fWanted)
		{
			return;
		}
		CXformContext *pxfctxt = GPOS_NEW(mp) CXformContext(mp);
		CXformResult *pxfresTwins = GPOS_NEW(mp) CXformResult(mp);
		pxform->Transform(pxfctxt, pxfresTwins, pexpr);
		CExpressionArray *pdrgpexprTwins = pxfresTwins->Pdrgpexpr();
		for (ULONG ul = 0; ul < pdrgpexprTwins->Size(); ul++)
		{
			CExpression *pexprTwin = (*pdrgpexprTwins)[ul];
			if (FTwin(pexprTwin->Pop()))
			{
				CPhysical::PopConvert(pexprTwin->Pop())
					->SetExecEngine(EeeExternal);
				pexprTwin->AddRef();
				pxfres->Add(pexprTwin);
			}
		}
		pxfresTwins->Release();
		pxfctxt->Release();
	}

	BOOL
	FColsSupported(CColRefSet *) const override
	{
		return true;
	}

	BOOL
	FOrderSupported(COrderSpec *) const override
	{
		return true;
	}

	CExpression *
	PexprEngineMotion(CMemoryPool *mp, CExpressionHandle &exprhdl,
					  CExpression *pexprMotion) const override
	{
		COperator *pop = pexprMotion->Pop();
		CPhysicalMotion *popEngine = nullptr;
		switch (pop->Eopid())
		{
			case COperator::EopPhysicalMotionHashDistribute:
			{
				CDistributionSpecHashed *pdsHashed =
					CDistributionSpecHashed::PdsConvert(
						CPhysicalMotionHashDistribute::PopConvert(pop)->Pds());
				if (CUtils::FDuplicateHazardDistributionSpec(
						CDrvdPropPlan::Pdpplan(exprhdl.Pdp())->Pds()))
				{
					return nullptr;
				}
				pdsHashed->AddRef();
				popEngine =
					GPOS_NEW(mp) CPhysicalMotionHashDistribute(mp, pdsHashed);
				break;
			}
			case COperator::EopPhysicalMotionGather:
			{
				CPhysicalMotionGather *popGather =
					CPhysicalMotionGather::PopConvert(pop);
				if (!popGather->Pos()->IsEmpty())
				{
					return nullptr;
				}
				popEngine =
					GPOS_NEW(mp) CPhysicalMotionGather(mp, popGather->Est());
				break;
			}
			case COperator::EopPhysicalMotionBroadcast:
			{
				CDistributionSpecReplicated *pds =
					CDistributionSpecReplicated::PdsConvert(
						CPhysicalMotionBroadcast::PopConvert(pop)->Pds());
				popEngine = GPOS_NEW(mp) CPhysicalMotionBroadcast(
					mp, pds->FIgnoreBroadcastThreshold());
				break;
			}
			default:
				return nullptr;
		}
		popEngine->SetExecEngine(EeeExternal);
		CExpression *pexprChild = (*pexprMotion)[0];
		pexprChild->AddRef();
		return GPOS_NEW(mp) CExpression(mp, popEngine, pexprChild);
	}

	DOUBLE
	CostLocal(const CCostModelGPDB *model, CExpressionHandle &exprhdl,
			  const ICostModel::SCostingInfo *pci,
			  CCost children) const override
	{
		if (COperator::EopPhysicalEngineConvert == exprhdl.Pop()->Eopid())
		{
			return 0.001 * pci->Rows();
		}
		return 0.5 *
			   std::max(model->CostPG(exprhdl, pci).Get() - children.Get(),
						0.0);
	}
};

const CHAR *rgszEngineMdps[] = {
	"../data/dxl/minidump/ExternalEngine-HashJoin.mdp",
	"../data/dxl/minidump/ExternalEngine-Agg.mdp",
	"../data/dxl/minidump/ExternalEngine-CTE.mdp",
	"../data/dxl/minidump/ExternalEngine-CorrelatedSubplan.mdp",
};

// the dump's configuration, with the test engine
COptimizerConfig *
PoconfWithEngine(CMemoryPool *mp, CDXLMinidump *pdxlmd)
{
	COptimizerConfig *dump = pdxlmd->GetOptimizerConfig();
	if (nullptr == dump)
	{
		dump = COptimizerConfig::PoconfDefault(mp);
	}
	else
	{
		dump->AddRef();
	}
	dump->GetEnumeratorCfg()->AddRef();
	dump->GetStatsConf()->AddRef();
	dump->GetCteConf()->AddRef();
	dump->GetCostModel()->AddRef();
	dump->GetHint()->AddRef();
	if (nullptr != dump->GetPlanHint())
	{
		dump->GetPlanHint()->AddRef();
	}
	dump->GetWindowOids()->AddRef();
	COptimizerConfig *optimizer_config = GPOS_NEW(mp) COptimizerConfig(
		dump->GetEnumeratorCfg(), dump->GetStatsConf(), dump->GetCteConf(),
		dump->GetCostModel(), dump->GetHint(), dump->GetPlanHint(),
		dump->GetWindowOids(), GPOS_NEW(mp) CTestEngine());
	dump->Release();
	return optimizer_config;
}

GPOS_RESULT
EresRunMinidump(CMemoryPool *mp, const CHAR *file_name)
{
	{
		CAutoTrace at(mp);
		at.Os() << "executing " << file_name;
	}

	CMDCache::Reset();
	CDXLMinidump *pdxlmd = CMinidumperUtils::PdxlmdLoad(mp, file_name);
	CMDProviderMemory *pmdp = GPOS_NEW(mp) CMDProviderMemory(mp, file_name);
	const CSystemIdArray *pdrgpsysid = pdxlmd->GetSysidPtrArray();
	CMDProviderArray *pdrgpmdp = GPOS_NEW(mp) CMDProviderArray(mp);
	pdrgpmdp->Append(pmdp);
	for (ULONG ul = 1; ul < pdrgpsysid->Size(); ul++)
	{
		pmdp->AddRef();
		pdrgpmdp->Append(pmdp);
	}

	GPOS_RESULT eres = GPOS_OK;
	COptimizerConfig *optimizer_config = PoconfWithEngine(mp, pdxlmd);
	{
		CMDAccessor mda(mp, CMDCache::Pcache(), pdrgpsysid, pdrgpmdp);
		CDXLNode *pdxlnPlan = CMinidumperUtils::PdxlnExecuteMinidump(
			mp, &mda, pdxlmd, file_name,
			CTestUtils::UlSegments(optimizer_config), 1 /*ulSessionId*/,
			1 /*ulCmdId*/, optimizer_config, nullptr /*pceeval*/);

		CAutoTrace at(mp);
		if (!CTestUtils::FPlanCompare(
				mp, at.Os(), pdxlnPlan,
				optimizer_config->GetEnumeratorCfg()->GetPlanId(),
				optimizer_config->GetEnumeratorCfg()->GetPlanSpaceSize(),
				pdxlmd->PdxlnPlan(), pdxlmd->GetPlanId(),
				pdxlmd->GetPlanSpaceSize(), true /*fMatchPlans*/,
				0 /*iCmpSpaceSize*/, file_name))
		{
			eres = GPOS_FAILED;
		}
		pdxlnPlan->Release();
	}

	optimizer_config->Release();
	pdrgpmdp->Release();
	GPOS_DELETE(pdxlmd);
	return eres;
}
IExternalEngine *
PengineTestCreate(CMemoryPool *mp)
{
	return GPOS_NEW(mp) CTestEngine();
}

}  // namespace

GPOS_RESULT
CExternalEngineTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CExternalEngineTest::EresUnittest_Register),
		GPOS_UNITTEST_FUNC(CExternalEngineTest::EresUnittest_RunMinidumps),
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}

// an engine built against another Orca is refused
GPOS_RESULT
CExternalEngineTest::EresUnittest_Register()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	GPOS_UNITTEST_ASSERT(!CExternalEngineUtils::Register(
		"test", PengineTestCreate, "4 another Orca"));
	GPOS_UNITTEST_ASSERT(nullptr == CExternalEngineUtils::PengineCreate(mp));

	GPOS_UNITTEST_ASSERT(CExternalEngineUtils::Register(
		"test", PengineTestCreate, SzExternalEngineBuild()));
	IExternalEngine *engine = CExternalEngineUtils::PengineCreate(mp);
	GPOS_UNITTEST_ASSERT(nullptr != engine);
	engine->Release();

	CExternalEngineUtils::Register(nullptr, nullptr, SzExternalEngineBuild());
	return GPOS_OK;
}

GPOS_RESULT
CExternalEngineTest::EresUnittest_RunMinidumps()
{
	CAutoTraceFlag atf(EopttraceEnumeratePlans, true);
	GPOS_RESULT eres = GPOS_OK;
	for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(rgszEngineMdps); ul++)
	{
		CAutoMemoryPool amp;
		if (GPOS_FAILED == EresRunMinidump(amp.Pmp(), rgszEngineMdps[ul]))
		{
			eres = GPOS_FAILED;
		}
	}
	return eres;
}
