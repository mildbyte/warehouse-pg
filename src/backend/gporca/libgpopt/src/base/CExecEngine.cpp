//---------------------------------------------------------------------------
//	CExecEngine.cpp
//---------------------------------------------------------------------------

#include "gpopt/base/CExecEngine.h"

#include "gpopt/base/CCastUtils.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/base/IExternalEngine.h"
#include "gpopt/operators/CPhysicalJoin.h"
#include "gpopt/optimizer/COptimizerConfig.h"
#include "gpopt/search/CGroup.h"
#include "gpopt/search/CGroupExpression.h"

using namespace gpopt;

const IExternalEngine *
CExternalEngineUtils::Pengine()
{
	COptCtxt *poctxt = COptCtxt::PoctxtFromTLS();
	return nullptr != poctxt ? poctxt->GetOptimizerConfig()->GetExternalEngine()
							 : nullptr;
}

BOOL
CExternalEngineUtils::FSupports(EEngineAsk ask, IMDId *mdid)
{
	const IExternalEngine *engine = Pengine();
	return nullptr != engine && nullptr != mdid && mdid->IsValid() &&
		   engine->FSupports(ask, mdid);
}

BOOL
CExternalEngineUtils::FEnabled(EEngineOp op)
{
	const IExternalEngine *engine = Pengine();
	return nullptr != engine && engine->FEnabled(op);
}

BOOL
CExternalEngineUtils::FHas(EEngineFeature feature)
{
	const IExternalEngine *engine = Pengine();
	return nullptr != engine && engine->FHas(feature);
}

CExpression *
CExternalEngineUtils::PexprScalarTree(CExpression *pexprScalar)
{
	CGroupExpression *pgexpr = pexprScalar->Pgexpr();
	if (nullptr == pgexpr || pgexpr->Arity() == pexprScalar->Arity())
	{
		return pexprScalar;
	}
	CGroup *pgroup = pgexpr->Pgroup();
	return pgroup->FScalarRepIsExact() ? pgroup->PexprScalarRep() : nullptr;
}

BOOL
CExternalEngineUtils::FHashJoinable(CMemoryPool *mp, CExpression *pexprJoin,
									CExpression *pexprScalarTree)
{
	// same preconditions as CXformUtils::ImplementHashJoin
	if (CUtils::HasOuterRefs(pexprJoin))
	{
		return false;
	}
	CExpression *pexprOuter = (*pexprJoin)[0];
	CExpression *pexprInner = (*pexprJoin)[1];
	CExpressionArray *pdrgpexpr =
		CCastUtils::PdrgpexprCastEquality(mp, pexprScalarTree);
	BOOL found = false;
	for (ULONG ul = 0; !found && ul < pdrgpexpr->Size(); ul++)
	{
		found = CPhysicalJoin::FHashJoinCompatible((*pdrgpexpr)[ul], pexprOuter,
												   pexprInner);
	}
	pdrgpexpr->Release();
	return found;
}

namespace
{
const CHAR *custom_scan_name = nullptr;
PfnExternalEngineCreate create = nullptr;
}  // namespace

BOOL
CExternalEngineUtils::Register(const CHAR *name, PfnExternalEngineCreate pfn,
							   ULONG abi_version, ULONG layout)
{
	if (IExternalEngine::AbiVersion != abi_version ||
		UlExternalEngineLayout() != layout)
	{
		return false;
	}
	custom_scan_name = name;
	create = pfn;
	return true;
}

const CHAR *
CExternalEngineUtils::SzCustomScanName()
{
	return custom_scan_name;
}

IExternalEngine *
CExternalEngineUtils::PengineCreate(CMemoryPool *mp)
{
	return nullptr != create ? create(mp) : nullptr;
}

void
CExternalEngineUtils::AddAlternative(CMemoryPool *mp, CXformResult *pxfres,
									 CPhysical *pop, CExpression *pexpr)
{
	const IExternalEngine *engine = Pengine();
	if (nullptr == engine)
	{
		pop->Release();
		return;
	}
	engine->AddAlternative(mp, pxfres, pop, pexpr);
}

BOOL
CExternalEngineUtils::FLargeEnough(CExpression *pexpr)
{
	const IExternalEngine *engine = Pengine();
	return nullptr != engine && engine->FLargeEnough(pexpr);
}

BOOL
CExternalEngineUtils::FScalarSupported(CExpression *pexprScalar)
{
	const IExternalEngine *engine = Pengine();
	return nullptr != engine && engine->FScalarSupported(pexprScalar);
}

BOOL
CExternalEngineUtils::FColsSupported(CColRefSet *pcrs)
{
	const IExternalEngine *engine = Pengine();
	return nullptr != engine && engine->FColsSupported(pcrs);
}

BOOL
CExternalEngineUtils::FOrderSupported(COrderSpec *pos)
{
	const IExternalEngine *engine = Pengine();
	return nullptr != engine && engine->FOrderSupported(pos);
}

BOOL
CExternalEngineUtils::FWindowSupported(CLogicalSequenceProject *pop)
{
	const IExternalEngine *engine = Pengine();
	return nullptr != engine && engine->FWindowSupported(pop);
}

CExpression *
CExternalEngineUtils::PexprEngineMotion(CMemoryPool *mp,
										CExpressionHandle &exprhdl,
										CExpression *pexprMotion)
{
	const IExternalEngine *engine = Pengine();
	return nullptr != engine
			   ? engine->PexprEngineMotion(mp, exprhdl, pexprMotion)
			   : nullptr;
}
