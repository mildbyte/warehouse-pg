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

#include "gpopt/base/IExternalEngine.h"

namespace gpopt
{
using namespace gpos;

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

class CExternalEngineUtils
{
public:
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
