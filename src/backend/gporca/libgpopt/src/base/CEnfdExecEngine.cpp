//---------------------------------------------------------------------------
//	CEnfdExecEngine.cpp
//---------------------------------------------------------------------------

#include "gpopt/base/CEnfdExecEngine.h"

#include "gpopt/operators/CPhysical.h"

using namespace gpopt;

CEnfdExecEngine::CEnfdExecEngine(CExecEngineSpec *pes, EExecEngineMatching eeem)
	: m_pes(pes), m_eeem(eeem)
{
	GPOS_ASSERT(nullptr != pes);
	GPOS_ASSERT(EeemSentinel > eeem);
}

CEnfdExecEngine::~CEnfdExecEngine()
{
	CRefCount::SafeRelease(m_pes);
}

ULONG
CEnfdExecEngine::HashValue() const
{
	return gpos::CombineHashes(m_eeem + 1, m_pes->HashValue());
}

CEnfdProp::EPropEnforcingType
CEnfdExecEngine::Epet(CExpressionHandle &exprhdl, CPhysical *popPhysical) const
{
	return popPhysical->EpetExecEngine(exprhdl, this);
}

IOstream &
CEnfdExecEngine::OsPrint(IOstream &os) const
{
	(void) m_pes->OsPrint(os);
	return os << (EeemNative == m_eeem ? " match: native" : " match: satisfy");
}
