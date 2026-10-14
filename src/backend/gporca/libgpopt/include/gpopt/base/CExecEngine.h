//---------------------------------------------------------------------------
//	CExecEngine.h
//
//	Execution engine plan property: the PG executor or the external engine
//	(see IExternalEngine). Operators running in the external engine require
//	and derive EeeExternal; the CPhysicalEngineConvert enforcer switches
//	between engines and is where the conversion cost lives.
//---------------------------------------------------------------------------
#ifndef GPOPT_CExecEngine_H
#define GPOPT_CExecEngine_H

#include "gpos/base.h"

namespace gpopt
{
using namespace gpos;

enum EExecEngine
{
	EeePG = 0,
	EeeExternal,

	// Requirement-only variants: the engine must be produced natively, not by
	// a conversion. An EngineConvert asks its child for these, otherwise
	// ToPG/ToEngine would request each other's contexts and deadlock the search
	// (Orca breaks the same cycle for Motion/Sort with ANY/empty requests).
	EeePGNative,
	EeeExternalNative,

	EeeSentinel
};

inline EExecEngine
EeBase(EExecEngine ee)
{
	return (EeeExternal == ee || EeeExternalNative == ee) ? EeeExternal : EeePG;
}

inline BOOL
FEeNative(EExecEngine ee)
{
	return EeePGNative == ee || EeeExternalNative == ee;
}

inline const CHAR *
SzExecEngine(EExecEngine ee)
{
	switch (ee)
	{
		case EeeExternal:
			return "External";
		case EeePGNative:
			return "PG (native)";
		case EeeExternalNative:
			return "External (native)";
		default:
			return "PG";
	}
}

class CColRefSet;
class CExpression;
class COrderSpec;
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

	static BOOL FOrderSupported(COrderSpec *pos);

	static CExpression *PexprEngineMotion(CMemoryPool *mp,
										  CExpressionHandle &exprhdl,
										  CExpression *pexprMotion);
};

}  // namespace gpopt

#endif	// !GPOPT_CExecEngine_H
