//---------------------------------------------------------------------------
//	CPhysicalEngineConvert.h
//
//	Enforcer for the execution engine property: converts its child's output
//	to the target engine (PG tuples -> engine batches or back). Every other
//	property passes through unchanged.
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalEngineConvert_H
#define GPOPT_CPhysicalEngineConvert_H

#include "gpos/base.h"

#include "gpopt/operators/CPhysical.h"

namespace gpopt
{
class CPhysicalEngineConvert : public CPhysical
{
private:
	EExecEngine m_target;

public:
	CPhysicalEngineConvert(const CPhysicalEngineConvert &) = delete;

	CPhysicalEngineConvert(CMemoryPool *mp, EExecEngine target);

	EOperatorId
	Eopid() const override
	{
		return EopPhysicalEngineConvert;
	}

	const CHAR *
	SzId() const override
	{
		return EeeExternal == m_target ? "CPhysicalToEngine" : "CPhysicalToPG";
	}

	EExecEngine
	EeTarget() const
	{
		return m_target;
	}

	BOOL Matches(COperator *pop) const override;

	ULONG HashValue() const override;

	BOOL
	FInputOrderSensitive() const override
	{
		return true;
	}

	CColRefSet *PcrsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
							 CColRefSet *pcrsRequired, ULONG child_index,
							 CDrvdPropArray *pdrgpdpCtxt,
							 ULONG ulOptReq) override;

	CCTEReq *PcteRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
						  CCTEReq *pcter, ULONG child_index,
						  CDrvdPropArray *pdrgpdpCtxt,
						  ULONG ulOptReq) const override;

	COrderSpec *PosRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
							COrderSpec *posRequired, ULONG child_index,
							CDrvdPropArray *pdrgpdpCtxt,
							ULONG ulOptReq) const override;

	CDistributionSpec *PdsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
								   CDistributionSpec *pdsRequired,
								   ULONG child_index,
								   CDrvdPropArray *pdrgpdpCtxt,
								   ULONG ulOptReq) const override;

	CRewindabilitySpec *PrsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
									CRewindabilitySpec *prsRequired,
									ULONG child_index,
									CDrvdPropArray *pdrgpdpCtxt,
									ULONG ulOptReq) const override;

	BOOL FProvidesReqdCols(CExpressionHandle &exprhdl, CColRefSet *pcrsRequired,
						   ULONG ulOptReq) const override;

	CEnfdDistribution::EDistributionMatching
	Edm(CReqdPropPlan *prppInput, ULONG, CDrvdPropArray *, ULONG) override
	{
		return prppInput->Ped()->Edm();
	}

	CEnfdOrder::EOrderMatching
	Eom(CReqdPropPlan *prppInput, ULONG, CDrvdPropArray *, ULONG) override
	{
		return prppInput->Peo()->Eom();
	}

	COrderSpec *PosDerive(CMemoryPool *mp,
						  CExpressionHandle &exprhdl) const override;

	CDistributionSpec *PdsDerive(CMemoryPool *mp,
								 CExpressionHandle &exprhdl) const override;

	CRewindabilitySpec *PrsDerive(CMemoryPool *mp,
								  CExpressionHandle &exprhdl) const override;

	CExecEngineSpec *
	PesRequired(CMemoryPool *mp, CExpressionHandle &, ULONG) const override
	{
		return GPOS_NEW(mp)
			CExecEngineSpec(EeeExternal == m_target ? EeePG : EeeExternal);
	}

	// The child must produce the other engine natively, which stops ToEngine
	// and ToPG requesting each other forever and rules out a ToEngine
	// straight over a ToPG, which only converts twice. A Motion between them
	// stays possible: it is a PG operator.
	CEnfdExecEngine::EExecEngineMatching
	Eeem(CExpressionHandle &, ULONG) const override
	{
		return CEnfdExecEngine::EeemNative;
	}

	CEnfdProp::EPropEnforcingType EpetOrder(
		CExpressionHandle &exprhdl, const CEnfdOrder *peo) const override;

	CEnfdProp::EPropEnforcingType EpetDistribution(
		CExpressionHandle &exprhdl,
		const CEnfdDistribution *ped) const override;

	CEnfdProp::EPropEnforcingType EpetRewindability(
		CExpressionHandle &exprhdl,
		const CEnfdRewindability *per) const override;

	CEnfdProp::EPropEnforcingType EpetExecEngine(
		CExpressionHandle &exprhdl, const CEnfdExecEngine *pee) const override;

	BOOL
	FPassThruStats() const override
	{
		return true;
	}

	static CPhysicalEngineConvert *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopPhysicalEngineConvert == pop->Eopid());

		return dynamic_cast<CPhysicalEngineConvert *>(pop);
	}

	IOstream &OsPrint(IOstream &os) const override;
};

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalEngineConvert_H
