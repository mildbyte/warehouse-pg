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
//	segment and a serialized batch. The sending region gets the DXL of the
//	motion, with the region below it, in place of its own, and computes the
//	segment as the motion would; a region shipping to a CTE producer or a
//	Sequence keeps its own DXL.
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
class COrderSpec;
class CXform;
class CXformResult;

const CHAR *const EngineMotionSegidCol = "engine_segid";
const CHAR *const EngineMotionPayloadCol = "engine_payload";

class IExternalEngine : public CRefCount
{
public:
	// bump on any change to this interface
	static const ULONG AbiVersion = 4;

	~IExternalEngine() override = default;

	// add engine alternatives after pxform added results to pxfres from
	// index ulFirst on for the binding pexpr
	virtual void AddAlternatives(CMemoryPool *mp, const CXform *pxform,
								 CExpression *pexpr, CXformResult *pxfres,
								 ULONG ulFirst) const = 0;

	// whether every column's type can cross the engine boundary
	virtual BOOL FColsSupported(CColRefSet *pcrs) const = 0;

	// whether a sort on the given keys can run in the engine
	virtual BOOL FOrderSupported(COrderSpec *pos) const = 0;

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
