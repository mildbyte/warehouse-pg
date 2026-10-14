//---------------------------------------------------------------------------
//	IExternalEngine.h
//
//	An execution engine besides the PG executor that can run parts of a
//	plan. An extension implements this interface in C++ against the Orca
//	headers and registers a factory with CExternalEngineUtils::Register; Orca
//	then creates one engine per optimization and also costs plans that run
//	the operators it supports in that engine, with conversions at the engine
//	boundaries.
//
//	Each engine region is emitted as a CustomScan with the methods registered
//	under the engine's CustomScan name: custom_private holds a String with
//	the region's DXL plan and custom_plans the PG subplans feeding it, in DXL
//	pre-order. An engine motion ships a region's output as rows of
//	(EngineMotionSegidCol int4, EngineMotionPayloadCol bytea): the target
//	segment and a serialized batch. The sending region's custom_private gets
//	a second String with its route: "gather", "broadcast",
//	"random <numsegs>" or "hash <numsegs> <attno>:<hashfunc oid>...", attnos
//	numbering the region's DXL project list from 1; the region computes the
//	segment as cdbhash would.
//---------------------------------------------------------------------------
#ifndef GPOPT_IExternalEngine_H
#define GPOPT_IExternalEngine_H

#include "gpos/base.h"
#include "gpos/common/CRefCount.h"

#include "gpopt/base/CExecEngine.h"
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

const CHAR *const EngineMotionSegidCol = "engine_segid";
const CHAR *const EngineMotionPayloadCol = "engine_payload";

class IExternalEngine : public CRefCount
{
public:
	// bump on any change to this interface
	static const ULONG AbiVersion = 3;

	~IExternalEngine() override = default;

	// whether the engine implements this operator class at all
	virtual BOOL FEnabled(EEngineOp op) const = 0;

	virtual BOOL FHas(EEngineFeature feature) const = 0;

	// joins/aggs whose inputs are all smaller than this get no engine twin
	virtual DOUBLE MinRows() const = 0;

	virtual BOOL FSupports(EEngineAsk ask, IMDId *mdid) const = 0;

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
UlExternalEngineLayout()
{
	return (ULONG)(sizeof(CExpression) ^ (sizeof(CPhysical) << 8) ^
				   (sizeof(CExpressionHandle) << 16) ^
				   (sizeof(IExternalEngine) << 24));
}

}  // namespace gpopt

#endif	// !GPOPT_IExternalEngine_H
