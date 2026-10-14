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
	EacAggregateSplit,	// aggregate oid whose partial state is a plain value
	EacMotionKey,		// type oid the engine can hash like cdbhash
	EacWindowFunc		// window-only function oid (aggregates ask EacAggregate)
};

// mirrors OrcaEngineOp
enum EArrowOp
{
	EaopToArrow = 0,
	EaopToPG,
	EaopHashAgg,
	EaopHashJoin,
	EaopComputeScalar,
	EaopFilter,
	EaopSort,  // sort enforcers and limits
	EaopMotion,	 // motions that ship Arrow batches
	EaopWindow,
	EaopScan  // scans the engine reads into batches itself
};

using FnArrowCapability = BOOL (*)(EArrowCapability kind, ULONG oid);
using FnArrowCost = DOUBLE (*)(EArrowOp op, DOUBLE pg_cost, DOUBLE rows,
							   DOUBLE width);

// Set by the host (gpopt) before optimization. A null callback disables
// Arrow alternatives entirely.
struct SArrowEngineConfig
{
	FnArrowCapability m_capability{nullptr};
	FnArrowCost m_cost{nullptr};
	// bitmask of 1 << EArrowOp; 0 disables the engine
	ULONG m_operators{0};
	// joins/aggs whose inputs are all smaller than this get no Arrow twin
	DOUBLE m_min_rows{0};
	// per-segment bytes of a hash join build the engine holds in memory;
	// larger builds run as sort-merge joins. 0 means unlimited.
	DOUBLE m_hash_build_limit{0};
};

extern SArrowEngineConfig g_arrow_engine;

class COrderSpec;
class CExpressionHandle;
class CLogicalSequenceProject;

class CArrowEngineUtils
{
public:
	static BOOL FEnabled(EArrowOp op);

	// whether some relational child is big enough to be worth an engine switch
	static BOOL FLargeEnough(CExpression *pexpr);

	// the full tree of a scalar bound by a pattern leaf, or null if the
	// group has no exact representative
	static CExpression *PexprScalarTree(CExpression *pexprScalar);

	// would the hash join xforms implement this logical join as well
	static BOOL FHashJoinable(CMemoryPool *mp, CExpression *pexprJoin,
							  CExpression *pexprScalarTree);

	// whether every scalar node under the expression can run in Arrow
	static BOOL FScalarSupported(CExpression *pexprScalar);

	// whether every column's type can cross the PG/Arrow boundary
	static BOOL FColsSupported(CColRefSet *pcrs);

	// whether a sort on the given keys can run in Arrow
	static BOOL FOrderSupported(COrderSpec *pos);

	// whether the engine can evaluate these window specs over input sorted
	// by the first one
	static BOOL FWindowSupported(CLogicalSequenceProject *pop);

	// Arrow twin of a motion enforcer placed over the expression exprhdl
	// describes, or null when the motion can't ship Arrow batches
	static CExpression *PexprArrowMotion(CMemoryPool *mp,
										 CExpressionHandle &exprhdl,
										 CExpression *pexprMotion);
};

}  // namespace gpopt

#endif	// !GPOPT_CExecEngine_H
