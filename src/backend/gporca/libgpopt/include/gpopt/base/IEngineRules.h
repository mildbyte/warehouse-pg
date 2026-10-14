//---------------------------------------------------------------------------
//	IEngineRules.h
//
//	Placement rules of the external engine: which operators, scalars, orders
//	and motions can run in it. The engine's extension implements them in C++
//	against these headers and registers them with
//	CExternalEngineUtils::SetRules; without rules nothing runs in the engine.
//---------------------------------------------------------------------------
#ifndef GPOPT_IEngineRules_H
#define GPOPT_IEngineRules_H

#include "gpos/base.h"

#include "gpopt/cost/ICostModel.h"
#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPhysical.h"

namespace gpdbcost
{
class CCostModelGPDB;
}

namespace gpopt
{
using namespace gpos;

class CColRefSet;
class CLogicalSequenceProject;
class COrderSpec;
class CXformResult;

class IEngineRules
{
public:
	// bump on any change to this interface
	static const ULONG AbiVersion = 2;

	virtual ~IEngineRules() = default;

	// add pop over pexpr's children to the xform results, running in the
	// engine; takes pop
	virtual void AddAlternative(CMemoryPool *mp, CXformResult *pxfres,
								CPhysical *pop, CExpression *pexpr) const = 0;

	// whether some relational child is big enough to be worth an engine switch
	virtual BOOL FLargeEnough(CExpression *pexpr) const = 0;

	// whether every scalar node under the expression can run in the engine
	virtual BOOL FScalarSupported(CExpression *pexprScalar) const = 0;

	// whether every column's type can cross the engine boundary
	virtual BOOL FColsSupported(CColRefSet *pcrs) const = 0;

	// whether a sort on the given keys can run in the engine
	virtual BOOL FOrderSupported(COrderSpec *pos) const = 0;

	// whether the engine can evaluate these window specs
	virtual BOOL FWindowSupported(CLogicalSequenceProject *pop) const = 0;

	// engine twin of a motion enforcer placed over the expression exprhdl
	// describes, or null when the motion can't ship engine batches
	virtual CExpression *PexprEngineMotion(CMemoryPool *mp,
										   CExpressionHandle &exprhdl,
										   CExpression *pexprMotion) const = 0;

	// cost of the engine operator or conversion exprhdl describes, excluding
	// its children, which cost children
	virtual DOUBLE CostLocal(const gpdbcost::CCostModelGPDB *model,
							 CExpressionHandle &exprhdl,
							 const ICostModel::SCostingInfo *pci,
							 CCost children) const = 0;
};

// Compiled into both Orca and the extension, so a build against headers of
// another Orca revision or configuration is refused at registration.
inline ULONG
UlEngineRulesLayout()
{
	return (ULONG)(sizeof(CExpression) ^ (sizeof(CPhysical) << 8) ^
				   (sizeof(CExpressionHandle) << 16) ^
				   (sizeof(IEngineRules) << 24));
}

}  // namespace gpopt

#endif	// !GPOPT_IEngineRules_H
