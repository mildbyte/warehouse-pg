//---------------------------------------------------------------------------
//	CExternalEngine.h
//
//	Registry of the external engine (see IExternalEngine) and the hooks the
//	search calls into it.
//---------------------------------------------------------------------------
#ifndef GPOPT_CExternalEngine_H
#define GPOPT_CExternalEngine_H

#include "gpos/base.h"

#include "gpopt/base/CExecEngineSpec.h"
#include "gpopt/operators/CExpression.h"

namespace gpopt
{
using namespace gpos;

class CColRefSet;
class CExpressionHandle;
class CXform;
class CXformResult;
class IExternalEngine;

// creates the engine for one optimization, or null when it is disabled
typedef IExternalEngine *(*PfnExternalEngineCreate)(CMemoryPool *mp);

class CExternalEngineUtils
{
public:
	// Register the engine whose regions are CustomScans with the methods
	// registered as custom_scan_name; false when the engine was built
	// against another Orca (see SzExternalEngineBuild).
	static BOOL Register(const CHAR *custom_scan_name,
						 PfnExternalEngineCreate create, const CHAR *build);

	// forget the registered engine; optimizations already running keep theirs
	static void Unregister();

	// the registered engine's CustomScan name, or null
	static const CHAR *SzCustomScanName();

	// the engine for a new optimization, or null
	static IExternalEngine *PengineCreate(CMemoryPool *mp);

	// the optimization's engine, or null
	static const IExternalEngine *Pengine();

	// The rest ask the optimization's engine and answer no without one.

	// add engine alternatives after pxform added results to pxfres from
	// index ulFirst on for the binding pexpr
	static void AddAlternatives(CMemoryPool *mp, const CXform *pxform,
								CExpression *pexpr, CXformResult *pxfres,
								ULONG ulFirst);

	static BOOL FColsSupported(CColRefSet *pcrs);

	// append the engine twins of the enforcers in pdrgpexpr
	static void AppendEngineEnforcers(CMemoryPool *mp,
									  CExpressionHandle &exprhdl,
									  CExpressionArray *pdrgpexpr,
									  EExecEngine eeRequired);
};

}  // namespace gpopt

#endif	// !GPOPT_CExternalEngine_H
