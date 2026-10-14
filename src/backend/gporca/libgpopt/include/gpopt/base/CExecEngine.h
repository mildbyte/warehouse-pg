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

namespace gpmd
{
class IMDId;
}

namespace gpopt
{
using namespace gpos;
using namespace gpmd;

// operator classes the engine may implement
enum EEngineOp
{
	EeopToEngine = 0,  // conversion from PG tuples
	EeopToPG,		   // conversion back to PG tuples
	EeopHashAgg,
	EeopHashJoin,
	EeopComputeScalar,
	EeopFilter,
	EeopSort,
	EeopMotion,	 // motion shipping engine batches
	EeopWindow,
	EeopScan,

	EeopSentinel
};

// what a capability question is about; the asked-about object is an mdid
enum EEngineAsk
{
	EeaskType = 0,	// values of this type can cross the boundary
	EeaskOperator,
	EeaskFunction,
	EeaskAgg,		  // single-stage aggregate
	EeaskAggSplit,	  // aggregate whose partial state is a plain value
	EeaskMotionKey,	  // type the engine hashes exactly like cdbhash
	EeaskWindowFunc,  // window-only function
	EeaskRelation,	  // table the engine scans itself

	EeaskSentinel
};

// abilities beyond the baseline every engine is assumed to have
enum EEngineFeature
{
	EefCorrelatedJoin = 0,	// outer references (correlated NL joins, subplans)
	EefNotInNLJoin,			// NL join with NOT IN semantics
	EefMultiKeyNotIn,		// null-aware anti hash join on several keys
	EefExprLimit,			// LIMIT/OFFSET that are not constants
	EefWindowMixedOrder,  // one node's window specs not all prefixes of the first
	EefWindowRangeOffset,  // RANGE/GROUPS frame offsets
	EefWindowExprOffset,   // ROWS frame offsets that are not constants
	EefWindowExclusion,	   // frame exclusion
	EefArrayExpr,		   // x op ANY/ALL over a non-literal array

	EefSentinel
};

class CExpression;
class CColRefSet;

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

class COrderSpec;
class CExpressionHandle;
class CLogicalSequenceProject;
class CPhysical;
class CXformResult;
class IExternalEngine;

// creates the engine for one optimization, or null when it is disabled
typedef IExternalEngine *(*PfnExternalEngineCreate)(CMemoryPool *mp);

class CExternalEngineUtils
{
public:
	// Register the engine whose regions are CustomScans with the methods
	// registered as custom_scan_name; false when the engine was built
	// against an incompatible Orca (see IExternalEngine::AbiVersion).
	static BOOL Register(const CHAR *custom_scan_name,
						 PfnExternalEngineCreate create, ULONG abi_version,
						 ULONG layout);

	// the registered engine's CustomScan name, or null
	static const CHAR *SzCustomScanName();

	// the engine for a new optimization, or null
	static IExternalEngine *PengineCreate(CMemoryPool *mp);

	// the optimization's external engine, or null
	static const IExternalEngine *Pengine();

	static BOOL FEnabled(EEngineOp op);

	static BOOL FHas(EEngineFeature feature);

	static BOOL FSupports(EEngineAsk ask, IMDId *mdid);

	// add pop over pexpr's children to the xform results, running in the
	// engine; takes pop
	static void AddAlternative(CMemoryPool *mp, CXformResult *pxfres,
							   CPhysical *pop, CExpression *pexpr);

	// whether some relational child is big enough to be worth an engine switch
	static BOOL FLargeEnough(CExpression *pexpr);

	// the full tree of a scalar bound by a pattern leaf, or null if the
	// group has no exact representative
	static CExpression *PexprScalarTree(CExpression *pexprScalar);

	// would the hash join xforms implement this logical join as well
	static BOOL FHashJoinable(CMemoryPool *mp, CExpression *pexprJoin,
							  CExpression *pexprScalarTree);

	// whether every scalar node under the expression can run in the engine
	static BOOL FScalarSupported(CExpression *pexprScalar);

	// whether every column's type can cross the engine boundary
	static BOOL FColsSupported(CColRefSet *pcrs);

	// whether a sort on the given keys can run in the engine
	static BOOL FOrderSupported(COrderSpec *pos);

	// whether the engine can evaluate these window specs over input sorted
	// by the first one
	static BOOL FWindowSupported(CLogicalSequenceProject *pop);

	// engine twin of a motion enforcer placed over the expression exprhdl
	// describes, or null when the motion can't ship engine batches
	static CExpression *PexprEngineMotion(CMemoryPool *mp,
										  CExpressionHandle &exprhdl,
										  CExpression *pexprMotion);
};

}  // namespace gpopt

#endif	// !GPOPT_CExecEngine_H
