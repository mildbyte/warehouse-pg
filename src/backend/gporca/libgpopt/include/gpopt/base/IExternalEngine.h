//---------------------------------------------------------------------------
//	IExternalEngine.h
//
//	An execution engine besides the PG executor that can run parts of a
//	plan. Operators the engine supports get twins that run in it, and
//	CPhysicalEngineConvert enforcers move rows between the engines. The host
//	implements this interface and hands it to the optimizer through
//	COptimizerConfig; without one, no engine alternatives are generated.
//---------------------------------------------------------------------------
#ifndef GPOPT_IExternalEngine_H
#define GPOPT_IExternalEngine_H

#include "gpos/base.h"
#include "gpos/common/CRefCount.h"

namespace gpmd
{
class IMDId;
class IMDType;
}  // namespace gpmd

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

enum EEngineJoin
{
	EejNone = 0,  // not a hash join
	EejInner,
	EejLeft,
	EejRight,
	EejFull,
	EejSemi,
	EejAnti,
	EejAntiNotIn,  // null-aware anti join

	EejSentinel
};

// Everything an engine needs to cost one operator; rows are per segment
struct SEngineCostInput
{
	EEngineOp m_op{EeopSentinel};
	EEngineJoin m_join{EejNone};

	// Orca's cost of the PG operator excluding its children, over all
	// rebinds (0 for conversions). For hash joins it assumes the build side
	// fits in memory; m_pg_cost_swapped exchanges the inputs' roles.
	DOUBLE m_pg_cost{0};
	DOUBLE m_pg_cost_swapped{0};

	DOUBLE m_rebinds{1};
	DOUBLE m_rows{0};
	DOUBLE m_width{0};

	// hash joins: probe, build
	DOUBLE m_child_rows[2]{0, 0};
	DOUBLE m_child_width[2]{0, 0};

	// columns the join condition uses
	ULONG m_join_cols{0};

	// conversions: the columns crossing the boundary
	ULONG m_ncols{0};
	const IMDType *const *m_col_types{nullptr};
	const DOUBLE *m_col_widths{nullptr};

	// cost model units, for engines that cost their own algorithms
	DOUBLE m_sort_tup_width_unit{0};
	DOUBLE m_join_feeding_tup_column_unit{0};
	DOUBLE m_join_feeding_tup_width_unit{0};
	DOUBLE m_join_output_tup_unit{0};
};

class IExternalEngine : public CRefCount
{
public:
	~IExternalEngine() override = default;

	// whether the engine implements this operator class at all
	virtual BOOL FEnabled(EEngineOp op) const = 0;

	virtual BOOL FHas(EEngineFeature feature) const = 0;

	// joins/aggs whose inputs are all smaller than this get no engine twin
	virtual DOUBLE MinRows() const = 0;

	virtual BOOL FSupports(EEngineAsk ask, IMDId *mdid) const = 0;

	// cost, in Orca units, of running input.m_op in the engine
	virtual DOUBLE Cost(const SEngineCostInput &input) const = 0;
};

}  // namespace gpopt

#endif	// !GPOPT_IExternalEngine_H
