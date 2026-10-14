//---------------------------------------------------------------------------
//	CExecEngineSpec.h
//
//	Execution engine plan property: whether a plan's output is PG tuples or
//	batches of the external engine (see IExternalEngine). Operators running
//	in the external engine require and derive EeeExternal; the
//	CPhysicalEngineConvert enforcer switches between engines and is where
//	the conversion cost lives.
//---------------------------------------------------------------------------
#ifndef GPOPT_CExecEngineSpec_H
#define GPOPT_CExecEngineSpec_H

#include "gpos/base.h"

#include "gpopt/base/CPropSpec.h"

namespace gpopt
{
using namespace gpos;

enum EExecEngine
{
	EeePG = 0,
	EeeExternal,

	EeeSentinel
};

class CExecEngineSpec : public CPropSpec
{
private:
	EExecEngine m_ee;

public:
	CExecEngineSpec(const CExecEngineSpec &) = delete;

	explicit CExecEngineSpec(EExecEngine ee);

	EExecEngine
	Eee() const
	{
		return m_ee;
	}

	BOOL
	Matches(const CExecEngineSpec *pes) const
	{
		return m_ee == pes->Eee();
	}

	BOOL
	FSatisfies(const CExecEngineSpec *pes) const
	{
		return Matches(pes);
	}

	// adds the conversion to this engine
	void AppendEnforcers(CMemoryPool *mp, CExpressionHandle &exprhdl,
						 CReqdPropPlan *prpp, CExpressionArray *pdrgpexpr,
						 CExpression *pexpr) override;

	ULONG
	HashValue() const override
	{
		return (ULONG) m_ee;
	}

	CColRefSet *PcrsUsed(CMemoryPool *mp) const override;

	EPropSpecType
	Epst() const override
	{
		return EpstExecEngine;
	}

	IOstream &OsPrint(IOstream &os) const override;
};

}  // namespace gpopt

#endif	// !GPOPT_CExecEngineSpec_H
