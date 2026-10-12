//---------------------------------------------------------------------------
//	CExecEngine.cpp
//---------------------------------------------------------------------------

#include "gpopt/base/CExecEngine.h"

#include "gpopt/base/CColRef.h"
#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/CScalarAggFunc.h"
#include "gpopt/operators/CScalarCast.h"
#include "gpopt/operators/CScalarCmp.h"
#include "gpopt/operators/CScalarConst.h"
#include "gpopt/operators/CScalarFunc.h"
#include "gpopt/operators/CScalarIdent.h"
#include "gpopt/operators/CScalarOp.h"
#include "gpopt/search/CGroup.h"
#include "gpopt/search/CGroupExpression.h"
#include "naucrates/md/CMDIdGPDB.h"
#include "naucrates/md/IMDType.h"

using namespace gpopt;
using namespace gpmd;

SArrowEngineConfig gpopt::g_arrow_engine;

static BOOL
FAsk(EArrowCapability kind, IMDId *mdid)
{
	if (nullptr == mdid || !mdid->IsValid())
	{
		return false;
	}
	return g_arrow_engine.m_capability(kind,
									   CMDIdGPDB::CastMdid(mdid)->Oid());
}

BOOL
CArrowEngineUtils::FEnabled(EArrowOperatorMask op)
{
	return g_arrow_engine.m_enabled && nullptr != g_arrow_engine.m_capability &&
		   0 != (g_arrow_engine.m_operators & op);
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
CArrowEngineUtils::FScalarSupported(CExpression *pexpr)
{
	GPOS_CHECK_STACK_SIZE;

	// Xforms binding the scalar child to a pattern leaf see only its top
	// operator; check the group's full scalar expression instead.
	CGroupExpression *pgexpr = pexpr->Pgexpr();
	if (nullptr != pgexpr && pgexpr->Arity() != pexpr->Arity())
	{
		CGroup *pgroup = pgexpr->Pgroup();
		if (!pgroup->FScalarRepIsExact() || nullptr == pgroup->PexprScalarRep())
		{
			return false;
		}
		return FScalarSupported(pgroup->PexprScalarRep());
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

		case COperator::EopScalarAggFunc:
		{
			CScalarAggFunc *popAgg = CScalarAggFunc::PopConvert(pop);
			if (popAgg->IsDistinct() ||
				EaggfunckindNormal != popAgg->AggKind() ||
				EaggfuncstageIntermediate == popAgg->Eaggfuncstage())
			{
				return false;
			}
			// direct args, ORDER BY and DISTINCT lists must be empty
			for (ULONG ul = EaggfuncIndexDirectArgs; ul < pexpr->Arity(); ul++)
			{
				if (0 != (*pexpr)[ul]->Arity())
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
