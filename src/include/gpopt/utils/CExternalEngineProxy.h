//---------------------------------------------------------------------------
//	CExternalEngineProxy.h
//
//	Orca's view (IExternalEngine) of the extension engine registered as
//	orca_external_engine.
//---------------------------------------------------------------------------
#ifndef GPOPT_CExternalEngineProxy_H
#define GPOPT_CExternalEngineProxy_H

extern "C" {
#include "postgres.h"

#include "nodes/params.h"
#include "nodes/plannodes.h"
#include "optimizer/orca.h"
}

#include "gpos/base.h"

#include "gpopt/base/IExternalEngine.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpopt;

class CExternalEngineProxy : public IExternalEngine
{
private:
	CMemoryPool *m_mp;

	OrcaEngineConfig m_config;

	explicit CExternalEngineProxy(CMemoryPool *mp);

public:
	~CExternalEngineProxy() override = default;

	// the registered engine, configured for this optimization, or null when
	// there is none or it enables no operators
	static CExternalEngineProxy *PengineCreate(CMemoryPool *mp);

	BOOL FEnabled(EEngineOp op) const override;

	DOUBLE MinRows() const override;

	BOOL FSupports(EEngineAsk ask, IMDId *mdid) const override;

	DOUBLE Cost(const SEngineCostInput &input) const override;
};

}  // namespace gpdxl

#endif	// !GPOPT_CExternalEngineProxy_H
