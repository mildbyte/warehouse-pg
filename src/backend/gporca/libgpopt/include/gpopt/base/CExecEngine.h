//---------------------------------------------------------------------------
//	CExecEngine.h
//
//	Execution engine plan property (PG executor vs an external Arrow engine).
//	Arrow-flavoured physical operators require and derive EeeArrow; the
//	CPhysicalEngineConvert enforcer switches between engines and is where the
//	PG<->Arrow conversion cost lives.
//---------------------------------------------------------------------------
#ifndef GPOPT_CExecEngine_H
#define GPOPT_CExecEngine_H

#include "gpos/base.h"

namespace gpopt
{
using namespace gpos;

class CExpression;
class CColRefSet;

enum EExecEngine
{
	EeePG = 0,
	EeeArrow,

	// Requirement-only variants: the engine must be produced natively, not by
	// a conversion. An EngineConvert asks its child for these, otherwise
	// ToPG/ToArrow would request each other's contexts and deadlock the search
	// (Orca breaks the same cycle for Motion/Sort with ANY/empty requests).
	EeePGNative,
	EeeArrowNative,

	EeeSentinel
};

inline EExecEngine
EeBase(EExecEngine ee)
{
	return (EeeArrow == ee || EeeArrowNative == ee) ? EeeArrow : EeePG;
}

inline BOOL
FEeNative(EExecEngine ee)
{
	return EeePGNative == ee || EeeArrowNative == ee;
}

inline const CHAR *
SzExecEngine(EExecEngine ee)
{
	switch (ee)
	{
		case EeeArrow:
			return "Arrow";
		case EeePGNative:
			return "PG (native)";
		case EeeArrowNative:
			return "Arrow (native)";
		default:
			return "PG";
	}
}

// What the capability callback is asked about.
enum EArrowCapability
{
	EacType = 0,	   // type oid can be converted PG<->Arrow
	EacOperator,	   // operator oid
	EacFunction,	   // function oid
	EacAggregate,	   // single-stage aggregate oid
	EacAggregateSplit  // aggregate oid whose partial state is a plain value
};

using FnArrowCapability = BOOL (*)(EArrowCapability kind, ULONG oid);

// Set by the host (gpopt) before optimization. A null callback disables
// Arrow alternatives entirely.
struct SArrowEngineConfig
{
	FnArrowCapability m_capability{nullptr};
	BOOL m_enabled{false};
	// multiplier applied to an Arrow operator's local cost
	DOUBLE m_cost_factor{0.5};
	// per (row * byte) cost of a PG<->Arrow conversion
	DOUBLE m_conversion_cost{1.0e-6};
	// operator types that get an Arrow alternative (bitmask, see below)
	ULONG m_operators{~0u};
};

enum EArrowOperatorMask
{
	EaomHashAgg = 1 << 0,
	EaomHashJoin = 1 << 1,
	EaomComputeScalar = 1 << 2,
	EaomFilter = 1 << 3
};

extern SArrowEngineConfig g_arrow_engine;

class CArrowEngineUtils
{
public:
	static BOOL FEnabled(EArrowOperatorMask op);

	// whether every scalar node under the expression can run in Arrow
	static BOOL FScalarSupported(CExpression *pexprScalar);

	// whether every column's type can cross the PG/Arrow boundary
	static BOOL FColsSupported(CColRefSet *pcrs);
};

}  // namespace gpopt

#endif	// !GPOPT_CExecEngine_H
