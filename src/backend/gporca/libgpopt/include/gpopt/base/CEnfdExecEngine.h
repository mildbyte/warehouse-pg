//---------------------------------------------------------------------------
//	CEnfdExecEngine.h
//
//	Enforceable execution engine property
//---------------------------------------------------------------------------
#ifndef GPOPT_CEnfdExecEngine_H
#define GPOPT_CEnfdExecEngine_H

#include "gpos/base.h"

#include "gpopt/base/CEnfdProp.h"
#include "gpopt/base/CExecEngineSpec.h"

namespace gpopt
{
using namespace gpos;

class CPhysical;

class CEnfdExecEngine : public CEnfdProp
{
public:
	enum EExecEngineMatching
	{
		// the engine may come from a conversion
		EeemSatisfy = 0,

		// The child must produce the engine itself: no conversion on top, and
		// (see CEngine::FCheckReqdProps) no conversion as the child either. A
		// conversion asks this of its child, otherwise ToEngine and ToPG would
		// request each other's contexts forever; Orca breaks the same cycle for
		// Motion and Sort with ANY and empty requests.
		EeemNative,

		EeemSentinel
	};

private:
	CExecEngineSpec *m_pes;

	EExecEngineMatching m_eeem;

public:
	CEnfdExecEngine(const CEnfdExecEngine &) = delete;

	CEnfdExecEngine(CExecEngineSpec *pes, EExecEngineMatching eeem);

	~CEnfdExecEngine() override;

	ULONG HashValue() const override;

	BOOL
	FCompatible(const CExecEngineSpec *pes) const
	{
		return pes->FSatisfies(m_pes);
	}

	CExecEngineSpec *
	PesRequired() const
	{
		return m_pes;
	}

	EExecEngineMatching
	Eeem() const
	{
		return m_eeem;
	}

	// engine enforcing type for the given operator
	EPropEnforcingType Epet(CExpressionHandle &exprhdl,
							CPhysical *popPhysical) const;

	CPropSpec *
	Pps() const override
	{
		return m_pes;
	}

	BOOL
	Matches(const CEnfdExecEngine *pee) const
	{
		return m_eeem == pee->Eeem() && m_pes->Matches(pee->PesRequired());
	}

	IOstream &OsPrint(IOstream &os) const override;
};

}  // namespace gpopt

#endif	// !GPOPT_CEnfdExecEngine_H
