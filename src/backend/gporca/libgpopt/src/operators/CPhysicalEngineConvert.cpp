//---------------------------------------------------------------------------
//	CPhysicalEngineConvert.cpp
//---------------------------------------------------------------------------

#include "gpopt/operators/CPhysicalEngineConvert.h"

#include "gpos/base.h"

#include "gpopt/base/CDrvdPropPlan.h"
#include "gpopt/operators/CExpressionHandle.h"

using namespace gpopt;

CPhysicalEngineConvert::CPhysicalEngineConvert(CMemoryPool *mp,
											   EExecEngine target)
	: CPhysical(mp), m_target(target)
{
	SetExecEngine(target);
}

BOOL
CPhysicalEngineConvert::Matches(COperator *pop) const
{
	return Eopid() == pop->Eopid() &&
		   CPhysicalEngineConvert::PopConvert(pop)->EeTarget() == m_target;
}

ULONG
CPhysicalEngineConvert::HashValue() const
{
	return gpos::CombineHashes(COperator::HashValue(), (ULONG) m_target);
}

CColRefSet *
CPhysicalEngineConvert::PcrsRequired(CMemoryPool *mp,
									 CExpressionHandle &exprhdl,
									 CColRefSet *pcrsRequired,
									 ULONG child_index, CDrvdPropArray *,
									 ULONG)
{
	return PcrsChildReqd(mp, exprhdl, pcrsRequired, child_index,
						 gpos::ulong_max);
}

CCTEReq *
CPhysicalEngineConvert::PcteRequired(CMemoryPool *, CExpressionHandle &,
									 CCTEReq *pcter, ULONG, CDrvdPropArray *,
									 ULONG) const
{
	return PcterPushThru(pcter);
}

COrderSpec *
CPhysicalEngineConvert::PosRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
									COrderSpec *posRequired, ULONG child_index,
									CDrvdPropArray *, ULONG) const
{
	return PosPassThru(mp, exprhdl, posRequired, child_index);
}

CDistributionSpec *
CPhysicalEngineConvert::PdsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
									CDistributionSpec *pdsRequired,
									ULONG child_index, CDrvdPropArray *,
									ULONG) const
{
	return PdsPassThru(mp, exprhdl, pdsRequired, child_index);
}

CRewindabilitySpec *
CPhysicalEngineConvert::PrsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
									CRewindabilitySpec *prsRequired,
									ULONG child_index, CDrvdPropArray *,
									ULONG) const
{
	return PrsPassThru(mp, exprhdl, prsRequired, child_index);
}

BOOL
CPhysicalEngineConvert::FProvidesReqdCols(CExpressionHandle &exprhdl,
										  CColRefSet *pcrsRequired,
										  ULONG) const
{
	return FUnaryProvidesReqdCols(exprhdl, pcrsRequired);
}

COrderSpec *
CPhysicalEngineConvert::PosDerive(CMemoryPool *,
								  CExpressionHandle &exprhdl) const
{
	return PosDerivePassThruOuter(exprhdl);
}

CDistributionSpec *
CPhysicalEngineConvert::PdsDerive(CMemoryPool *,
								  CExpressionHandle &exprhdl) const
{
	return PdsDerivePassThruOuter(exprhdl);
}

CRewindabilitySpec *
CPhysicalEngineConvert::PrsDerive(CMemoryPool *mp,
								  CExpressionHandle &exprhdl) const
{
	return PrsDerivePassThruOuter(mp, exprhdl);
}

// Every other property was passed through to the child, so stacking another
// enforcer on top of a conversion is never useful.
CEnfdProp::EPropEnforcingType
CPhysicalEngineConvert::EpetOrder(CExpressionHandle &exprhdl,
								  const CEnfdOrder *peo) const
{
	if (peo->FCompatible(CDrvdPropPlan::Pdpplan(exprhdl.Pdp())->Pos()))
	{
		return CEnfdProp::EpetUnnecessary;
	}
	return CEnfdProp::EpetProhibited;
}

CEnfdProp::EPropEnforcingType
CPhysicalEngineConvert::EpetDistribution(CExpressionHandle &exprhdl,
										 const CEnfdDistribution *ped) const
{
	if (ped->FCompatible(CDrvdPropPlan::Pdpplan(exprhdl.Pdp())->Pds()))
	{
		return CEnfdProp::EpetUnnecessary;
	}
	return CEnfdProp::EpetProhibited;
}

CEnfdProp::EPropEnforcingType
CPhysicalEngineConvert::EpetRewindability(CExpressionHandle &exprhdl,
										  const CEnfdRewindability *per) const
{
	if (per->FCompatible(CDrvdPropPlan::Pdpplan(exprhdl.Pdp())->Prs()))
	{
		return CEnfdProp::EpetUnnecessary;
	}
	return CEnfdProp::EpetProhibited;
}

CEnfdProp::EPropEnforcingType
CPhysicalEngineConvert::EpetEngine(CExpressionHandle &,
								   EExecEngine eeRequired) const
{
	return eeRequired == m_target ? CEnfdProp::EpetUnnecessary
								  : CEnfdProp::EpetProhibited;
}

IOstream &
CPhysicalEngineConvert::OsPrint(IOstream &os) const
{
	return os << SzId();
}
