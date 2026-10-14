//---------------------------------------------------------------------------
//	CExecEngineSpec.cpp
//---------------------------------------------------------------------------

#include "gpopt/base/CExecEngineSpec.h"

#include "gpopt/base/CColRefSet.h"
#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/CPhysicalEngineConvert.h"

using namespace gpopt;

CExecEngineSpec::CExecEngineSpec(EExecEngine ee) : m_ee(ee)
{
	GPOS_ASSERT(EeeSentinel > ee);
}

void
CExecEngineSpec::AppendEnforcers(CMemoryPool *mp, CExpressionHandle &,
								 CReqdPropPlan *, CExpressionArray *pdrgpexpr,
								 CExpression *pexpr)
{
	pexpr->AddRef();
	pdrgpexpr->Append(GPOS_NEW(mp) CExpression(
		mp, GPOS_NEW(mp) CPhysicalEngineConvert(mp, m_ee), pexpr));
}

CColRefSet *
CExecEngineSpec::PcrsUsed(CMemoryPool *mp) const
{
	return GPOS_NEW(mp) CColRefSet(mp);
}

IOstream &
CExecEngineSpec::OsPrint(IOstream &os) const
{
	return os << (EeeExternal == m_ee ? "External" : "PG");
}
