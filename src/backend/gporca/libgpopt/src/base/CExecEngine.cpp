//---------------------------------------------------------------------------
//	CExecEngine.cpp
//---------------------------------------------------------------------------

#include "gpopt/base/CExecEngine.h"

#include "gpopt/base/COrderSpec.h"
#include "gpopt/base/CWindowFrame.h"
#include "gpopt/operators/CLogicalSequenceProject.h"
#include "gpopt/operators/CScalarWindowFunc.h"

#include "gpopt/base/CDistributionSpecHashed.h"
#include "gpopt/base/CDistributionSpecReplicated.h"
#include "gpopt/base/CDrvdPropPlan.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPhysicalJoin.h"
#include "gpopt/base/CCastUtils.h"
#include "gpopt/operators/CPhysicalMotionBroadcast.h"
#include "gpopt/operators/CPhysicalMotionGather.h"
#include "gpopt/operators/CPhysicalMotionHashDistribute.h"
#include "gpopt/operators/CPhysicalMotionRandom.h"

#include "gpopt/base/CColRef.h"
#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/CScalarAggFunc.h"
#include "gpopt/operators/CScalarArrayCmp.h"
#include "gpopt/operators/CScalarCoalesce.h"
#include "gpopt/operators/CScalarIf.h"
#include "gpopt/operators/CScalarIsDistinctFrom.h"
#include "gpopt/operators/CScalarCast.h"
#include "gpopt/operators/CScalarCmp.h"
#include "gpopt/operators/CScalarConst.h"
#include "gpopt/operators/CScalarFunc.h"
#include "gpopt/operators/CScalarIdent.h"
#include "gpopt/operators/CScalarOp.h"
#include "gpopt/operators/CScalarSwitch.h"
#include "gpopt/search/CGroup.h"
#include "gpopt/search/CGroupExpression.h"
#include "naucrates/md/CMDIdGPDB.h"
#include "naucrates/md/IMDType.h"
#include "naucrates/statistics/IStatistics.h"

#include <cstdio>
#include <cstdlib>

using namespace gpopt;
using namespace gpmd;

// prototype: ORCA_ARROW_GAPS=1 logs every refusal to the server log
static BOOL
FGapTrace()
{
	static const BOOL f = nullptr != std::getenv("ORCA_ARROW_GAPS");
	return f;
}

SArrowEngineConfig gpopt::g_arrow_engine;

static BOOL
FAsk(EArrowCapability kind, IMDId *mdid)
{
	if (nullptr == mdid || !mdid->IsValid())
	{
		return false;
	}
	BOOL ok = g_arrow_engine.m_capability(kind,
										  CMDIdGPDB::CastMdid(mdid)->Oid());
	if (!ok && FGapTrace())
	{
		fprintf(stderr, "ARROW_GAP ask %d %u\n", (int) kind,
				CMDIdGPDB::CastMdid(mdid)->Oid());
	}
	return ok;
}

BOOL
CArrowEngineUtils::FEnabled(EArrowOp op)
{
	return nullptr != g_arrow_engine.m_capability &&
		   0 != (g_arrow_engine.m_operators & (1u << op));
}

BOOL
CArrowEngineUtils::FLargeEnough(CExpression *pexpr)
{
	BOOL known = false;
	for (ULONG ul = 0; ul < pexpr->Arity(); ul++)
	{
		CExpression *pexprChild = (*pexpr)[ul];
		if (pexprChild->Pop()->FScalar())
		{
			continue;
		}
		CGroupExpression *pgexpr = pexprChild->Pgexpr();
		const IStatistics *stats = nullptr != pgexpr ? pgexpr->Pgroup()->Pstats()
											   : pexprChild->Pstats();
		if (nullptr == stats)
		{
			return true;
		}
		known = true;
		if (stats->Rows() >= g_arrow_engine.m_min_rows)
		{
			return true;
		}
	}
	return !known;
}

CExpression *
CArrowEngineUtils::PexprScalarTree(CExpression *pexprScalar)
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
CArrowEngineUtils::FHashJoinable(CMemoryPool *mp, CExpression *pexprJoin,
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

BOOL
CArrowEngineUtils::FColsSupported(CColRefSet *pcrs)
{
	CColRefSetIter it(*pcrs);
	while (it.Advance())
	{
		if (!FAsk(EacType, it.Pcr()->RetrieveType()->MDId()))
		{
			return false;
		}
	}
	return true;
}

BOOL
CArrowEngineUtils::FOrderSupported(COrderSpec *pos)
{
	if (!FEnabled(EaopSort) || pos->IsEmpty())
	{
		return false;
	}
	for (ULONG ul = 0; ul < pos->UlSortColumns(); ul++)
	{
		if (!FAsk(EacType, pos->Pcr(ul)->RetrieveType()->MDId()) ||
			!FAsk(EacOperator, pos->GetMdIdSortOp(ul)))
		{
			return false;
		}
	}
	return true;
}

// DataFusion evaluates every window of one node over the same sorted input
// and finds peers with each window's own keys, so the first spec (which
// Orca sorts by) must start with every other spec. RANGE and GROUPS offsets
// need PG's type-specific in_range functions, and frame exclusion has no
// DataFusion counterpart.
BOOL
CArrowEngineUtils::FWindowSupported(CLogicalSequenceProject *pop)
{
	COrderSpecArray *pdrgpos = pop->Pdrgpos();
	CWindowFrameArray *pdrgpwf = pop->Pdrgpwf();
	if (!FEnabled(EaopWindow))
	{
		return false;
	}
	for (ULONG ul = 0; ul < pdrgpos->Size(); ul++)
	{
		COrderSpec *pos = (*pdrgpos)[ul];
		if (!pos->IsEmpty() && !FOrderSupported(pos))
		{
			return false;
		}
		if (0 < ul && !(*pdrgpos)[0]->FSatisfies(pos))
		{
			return false;
		}
	}
	for (ULONG ul = 0; ul < pdrgpwf->Size(); ul++)
	{
		CWindowFrame *pwf = (*pdrgpwf)[ul];
		if (CWindowFrame::IsEmpty(pwf))
		{
			continue;
		}
		if (CWindowFrame::EfesNone != pwf->Efes() &&
			CWindowFrame::EfesNulls != pwf->Efes())
		{
			return false;
		}
		CWindowFrame::EFrameBoundary bounds[] = {pwf->EfbLeading(),
												 pwf->EfbTrailing()};
		CExpression *values[] = {pwf->PexprLeading(), pwf->PexprTrailing()};
		for (ULONG edge = 0; edge < 2; edge++)
		{
			switch (bounds[edge])
			{
				case CWindowFrame::EfbUnboundedPreceding:
				case CWindowFrame::EfbUnboundedFollowing:
				case CWindowFrame::EfbCurrentRow:
					break;
				case CWindowFrame::EfbBoundedPreceding:
				case CWindowFrame::EfbBoundedFollowing:
					if (CWindowFrame::EfsRows != pwf->Efs() ||
						nullptr == values[edge] ||
						COperator::EopScalarConst !=
							values[edge]->Pop()->Eopid())
					{
						return false;
					}
					break;
				default:
					return false;
			}
		}
	}
	return true;
}

BOOL
CArrowEngineUtils::FScalarSupported(CExpression *pexpr)
{
	GPOS_CHECK_STACK_SIZE;

	// Xforms binding the scalar child to a pattern leaf see only its top
	// operator; check the group's full scalar expression instead.
	CExpression *pexprTree = PexprScalarTree(pexpr);
	if (pexprTree != pexpr)
	{
		return nullptr != pexprTree && FScalarSupported(pexprTree);
	}

	COperator *pop = pexpr->Pop();
	switch (pop->Eopid())
	{
		case COperator::EopScalarProjectList:
		case COperator::EopScalarProjectElement:
		case COperator::EopScalarBoolOp:
		case COperator::EopScalarNullTest:
		case COperator::EopScalarValuesList:
			break;

		case COperator::EopScalarIdent:
			return FAsk(EacType, CScalarIdent::PopConvert(pop)
									 ->Pcr()
									 ->RetrieveType()
									 ->MDId());

		case COperator::EopScalarConst:
			return FAsk(EacType, CScalarConst::PopConvert(pop)->MdidType());

		case COperator::EopScalarIf:
			if (!FAsk(EacType, CScalarIf::PopConvert(pop)->MdidType()))
			{
				return false;
			}
			break;

		case COperator::EopScalarCoalesce:
			if (!FAsk(EacType, CScalarCoalesce::PopConvert(pop)->MdidType()))
			{
				return false;
			}
			break;

		case COperator::EopScalarIsDistinctFrom:
			if (!FAsk(EacOperator,
					  CScalarIsDistinctFrom::PopConvert(pop)->MdIdOp()))
			{
				return false;
			}
			break;

		// x op ANY/ALL (literal list); arrays themselves never cross
		case COperator::EopScalarArrayCmp:
		{
			if (!FAsk(EacOperator, CScalarArrayCmp::PopConvert(pop)->MdIdOp()))
			{
				return false;
			}
			CExpression *pexprArray = (*pexpr)[1];
			if (COperator::EopScalarArray != pexprArray->Pop()->Eopid())
			{
				if (FGapTrace())
				{
					fprintf(stderr, "ARROW_GAP op CScalarArrayCmp(non-literal)\n");
				}
				return false;
			}
			for (ULONG ul = 0; ul < pexprArray->Arity(); ul++)
			{
				CExpression *pexprElem = (*pexprArray)[ul];
				if (COperator::EopScalarConst != pexprElem->Pop()->Eopid() ||
					!FScalarSupported(pexprElem))
				{
					return false;
				}
			}
			return FScalarSupported((*pexpr)[0]);
		}

		case COperator::EopScalarCmp:
			if (!FAsk(EacOperator, CScalarCmp::PopConvert(pop)->MdIdOp()))
			{
				return false;
			}
			break;

		case COperator::EopScalarOp:
			if (!FAsk(EacOperator, CScalarOp::PopConvert(pop)->MdIdOp()) ||
				!FAsk(EacType, CScalarOp::PopConvert(pop)->MdidType()))
			{
				return false;
			}
			break;

		case COperator::EopScalarBooleanTest:
		case COperator::EopScalarSwitchCase:
			break;

		case COperator::EopScalarSwitch:
			if (!FAsk(EacType, CScalarSwitch::PopConvert(pop)->MdidType()))
			{
				return false;
			}
			break;

		case COperator::EopScalarFunc:
			if (!FAsk(EacFunction, CScalarFunc::PopConvert(pop)->FuncMdId()) ||
				!FAsk(EacType, CScalarFunc::PopConvert(pop)->MdidType()))
			{
				return false;
			}
			break;

		case COperator::EopScalarCast:
		{
			CScalarCast *popCast = CScalarCast::PopConvert(pop);
			if (!FAsk(EacType, popCast->MdidType()))
			{
				return false;
			}
			IMDId *func = popCast->FuncMdId();
			if (nullptr != func && func->IsValid() &&
				!FAsk(EacFunction, func))
			{
				return false;
			}
			break;
		}

		case COperator::EopScalarWindowFunc:
		{
			CScalarWindowFunc *popWin = CScalarWindowFunc::PopConvert(pop);
			if (popWin->IsDistinct() ||
				CScalarWindowFunc::EwsImmediate != popWin->Ews() ||
				!FAsk(popWin->FAgg() ? EacAggregate : EacWindowFunc,
					  popWin->FuncMdId()) ||
				!FAsk(EacType, popWin->MdidType()))
			{
				return false;
			}
			break;
		}

		case COperator::EopScalarAggFunc:
		{
			CScalarAggFunc *popAgg = CScalarAggFunc::PopConvert(pop);
			// a DISTINCT aggregate has no partial state
			if ((popAgg->IsDistinct() && popAgg->FSplit()) ||
				EaggfunckindNormal != popAgg->AggKind() ||
				EaggfuncstageIntermediate == popAgg->Eaggfuncstage())
			{
				return false;
			}
			// direct args and ORDER BY must be empty; the DISTINCT list is the
			// argument list itself
			for (ULONG ul = EaggfuncIndexDirectArgs; ul < pexpr->Arity(); ul++)
			{
				if (EaggfuncIndexDistinct != ul && 0 != (*pexpr)[ul]->Arity())
				{
					return false;
				}
			}
			EArrowCapability kind =
				popAgg->FSplit() ? EacAggregateSplit : EacAggregate;
			if (!FAsk(kind, popAgg->MDId()) ||
				!FAsk(EacType, popAgg->MdidType()))
			{
				return false;
			}
			return FScalarSupported((*pexpr)[EaggfuncIndexArgs]);
		}

		default:
			if (FGapTrace())
			{
				fprintf(stderr, "ARROW_GAP op %s\n", pop->SzId());
			}
			return false;
	}

	for (ULONG ul = 0; ul < pexpr->Arity(); ul++)
	{
		if (!FScalarSupported((*pexpr)[ul]))
		{
			return false;
		}
	}
	return true;
}

// An Arrow motion ships the child's batches as (segment, IPC payload) rows
// over the regular interconnect, so it is a plain Motion whose engine is
// Arrow on both ends. The duplicate-hazard redistributes GPDB turns into hash
// filters stay PG; hash keys must be columns the engine can hash exactly like
// cdbhash, so the result is co-located with tables. A merging gather ships
// unordered and the receiving island sorts.
CExpression *
CArrowEngineUtils::PexprArrowMotion(CMemoryPool *mp, CExpressionHandle &exprhdl,
									CExpression *pexprMotion)
{
	if (!FEnabled(EaopMotion))
	{
		return nullptr;
	}

	COperator *pop = pexprMotion->Pop();
	CPhysicalMotion *popArrow = nullptr;
	switch (pop->Eopid())
	{
		case COperator::EopPhysicalMotionHashDistribute:
		{
			CDistributionSpecHashed *pdsHashed = CDistributionSpecHashed::PdsConvert(
				CPhysicalMotionHashDistribute::PopConvert(pop)->Pds());
			if (CDistributionSpec::EdtHashed != pdsHashed->Edt() ||
				CUtils::FDuplicateHazardDistributionSpec(
					CDrvdPropPlan::Pdpplan(exprhdl.Pdp())->Pds()))
			{
				return nullptr;
			}
			CExpressionArray *pdrgpexpr = pdsHashed->Pdrgpexpr();
			for (ULONG ul = 0; ul < pdrgpexpr->Size(); ul++)
			{
				COperator *popKey = (*pdrgpexpr)[ul]->Pop();
				if (COperator::EopScalarIdent != popKey->Eopid() ||
					!FAsk(EacMotionKey, CScalarIdent::PopConvert(popKey)
											->Pcr()
											->RetrieveType()
											->MDId()))
				{
					return nullptr;
				}
			}
			pdsHashed->AddRef();
			popArrow = GPOS_NEW(mp) CPhysicalMotionHashDistribute(mp, pdsHashed);
			break;
		}

		case COperator::EopPhysicalMotionGather:
		{
			CPhysicalMotionGather *popGather =
				CPhysicalMotionGather::PopConvert(pop);
			if (popGather->Pos()->IsEmpty())
			{
				popArrow =
					GPOS_NEW(mp) CPhysicalMotionGather(mp, popGather->Est());
			}
			else
			{
				popGather->Pos()->AddRef();
				popArrow = GPOS_NEW(mp) CPhysicalMotionGather(
					mp, popGather->Est(), popGather->Pos());
			}
			break;
		}

		case COperator::EopPhysicalMotionRandom:
		{
			CDistributionSpecRandom *pds = CDistributionSpecRandom::PdsConvert(
				CPhysicalMotionRandom::PopConvert(pop)->Pds());
			if (pds->IsDuplicateSensitive() ||
				CUtils::FDuplicateHazardDistributionSpec(
					CDrvdPropPlan::Pdpplan(exprhdl.Pdp())->Pds()))
			{
				return nullptr;
			}
			pds->AddRef();
			popArrow = GPOS_NEW(mp) CPhysicalMotionRandom(mp, pds);
			break;
		}

		case COperator::EopPhysicalMotionBroadcast:
		{
			CDistributionSpecReplicated *pds =
				CDistributionSpecReplicated::PdsConvert(
					CPhysicalMotionBroadcast::PopConvert(pop)->Pds());
			popArrow = GPOS_NEW(mp)
				CPhysicalMotionBroadcast(mp, pds->FIgnoreBroadcastThreshold());
			break;
		}

		default:
			return nullptr;
	}

	popArrow->SetExecEngine(EeeArrow);
	CExpression *pexprChild = (*pexprMotion)[0];
	pexprChild->AddRef();
	return GPOS_NEW(mp) CExpression(mp, popArrow, pexprChild);
}
