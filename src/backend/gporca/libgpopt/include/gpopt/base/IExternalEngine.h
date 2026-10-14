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

#include "gpopt/base/CExternalEngine.h"
#include "gpopt/cost/ICostModel.h"


namespace gpdbcost
{
class CCostModelGPDB;
}

namespace gpopt
{
using namespace gpos;

class CColRefSet;
class CExpression;
class CExpressionHandle;
class COrderSpec;
class CXform;
class CXformResult;

const CHAR *const EngineMotionSegidCol = "engine_segid";
const CHAR *const EngineMotionPayloadCol = "engine_payload";

class IExternalEngine : public CRefCount
{
public:
	~IExternalEngine() override = default;

	// add engine alternatives after pxform added results to pxfres from
	// index ulFirst on for the binding pexpr
	virtual void AddAlternatives(CMemoryPool *mp, const CXform *pxform,
								 CExpression *pexpr, CXformResult *pxfres,
								 ULONG ulFirst) const = 0;

	// whether every column's type can cross the engine boundary
	virtual BOOL FColsSupported(CColRefSet *pcrs) const = 0;

	// Engine twin of an enforcer (a Sort or Motion, say) placed over the
	// expression exprhdl describes under a request for output in eeRequired,
	// or null when the engine shouldn't run it. A twin under a PG request
	// gets a conversion above it.
	virtual CExpression *PexprEngineEnforcer(CMemoryPool *mp,
											 CExpressionHandle &exprhdl,
											 CExpression *pexprEnforcer,
											 EExecEngine eeRequired) const = 0;

	// cost of the engine operator or conversion exprhdl describes, excluding
	// its children, which cost children
	virtual DOUBLE CostLocal(const gpdbcost::CCostModelGPDB *model,
							 CExpressionHandle &exprhdl,
							 const ICostModel::SCostingInfo *pci,
							 CCost children) const = 0;
};

// bump on any change to IExternalEngine or CExternalEngineUtils
#define GPOPT_EXTERNAL_ENGINE_VERSION "6"

// gpos/base.h includes pg_config.h, except in standalone Orca builds
#ifdef PG_VERSION_STR
#define GPOPT_EXTERNAL_ENGINE_SERVER PG_VERSION_STR
#else
#define GPOPT_EXTERNAL_ENGINE_SERVER "standalone Orca"
#endif

#ifdef GPOS_DEBUG
#define GPOPT_EXTERNAL_ENGINE_ASSERTS " GPOS_DEBUG"
#else
#define GPOPT_EXTERNAL_ENGINE_ASSERTS ""
#endif

// The Orca build an engine is compiled against: interface version, server
// version and compiler, and whether Orca asserts, which changes class
// layouts. An engine builds against the exact headers of the server that
// loads it, the only stable ABI Orca has; Register refuses any other build.
inline const CHAR *
SzExternalEngineBuild()
{
	return GPOPT_EXTERNAL_ENGINE_VERSION
		" " GPOPT_EXTERNAL_ENGINE_SERVER GPOPT_EXTERNAL_ENGINE_ASSERTS;
}

}  // namespace gpopt

#endif	// !GPOPT_IExternalEngine_H
