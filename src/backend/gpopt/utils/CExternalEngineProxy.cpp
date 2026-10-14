//---------------------------------------------------------------------------
//	CExternalEngineProxy.cpp
//
//	Translates Orca's engine questions into orca_external_engine calls.
//---------------------------------------------------------------------------

#include "gpopt/utils/CExternalEngineProxy.h"

#include "gpopt/gpdbwrappers.h"
#include "naucrates/md/CMDIdGPDB.h"
#include "naucrates/md/IMDType.h"

using namespace gpdxl;
using namespace gpmd;

#define MATCH(orca, c) static_assert((int) (orca) == (int) (c), #orca " != " #c)

MATCH(EeopToEngine, ORCA_ENGINE_OP_TO_ENGINE);
MATCH(EeopToPG, ORCA_ENGINE_OP_TO_PG);
MATCH(EeopHashAgg, ORCA_ENGINE_OP_HASH_AGG);
MATCH(EeopHashJoin, ORCA_ENGINE_OP_HASH_JOIN);
MATCH(EeopComputeScalar, ORCA_ENGINE_OP_COMPUTE_SCALAR);
MATCH(EeopFilter, ORCA_ENGINE_OP_FILTER);
MATCH(EeopSort, ORCA_ENGINE_OP_SORT);
MATCH(EeopMotion, ORCA_ENGINE_OP_MOTION);
MATCH(EeopWindow, ORCA_ENGINE_OP_WINDOW);
MATCH(EeopScan, ORCA_ENGINE_OP_SCAN);
MATCH(EeaskType, ORCA_ENGINE_ASK_TYPE);
MATCH(EeaskOperator, ORCA_ENGINE_ASK_OPERATOR);
MATCH(EeaskFunction, ORCA_ENGINE_ASK_FUNCTION);
MATCH(EeaskAgg, ORCA_ENGINE_ASK_AGG);
MATCH(EeaskAggSplit, ORCA_ENGINE_ASK_AGG_SPLIT);
MATCH(EeaskMotionKey, ORCA_ENGINE_ASK_MOTION_KEY);
MATCH(EeaskWindowFunc, ORCA_ENGINE_ASK_WINDOW_FUNC);
MATCH(EejNone, ORCA_ENGINE_JOIN_NONE);
MATCH(EejInner, ORCA_ENGINE_JOIN_INNER);
MATCH(EejLeft, ORCA_ENGINE_JOIN_LEFT);
MATCH(EejRight, ORCA_ENGINE_JOIN_RIGHT);
MATCH(EejFull, ORCA_ENGINE_JOIN_FULL);
MATCH(EejSemi, ORCA_ENGINE_JOIN_SEMI);
MATCH(EejAnti, ORCA_ENGINE_JOIN_ANTI);
MATCH(EejAntiNotIn, ORCA_ENGINE_JOIN_ANTI_NOT_IN);

CExternalEngineProxy::CExternalEngineProxy(CMemoryPool *mp)
	: m_mp(mp), m_config{0, 0.0}
{
	gpdb::ExternalEngineConfigure(&m_config);
}

CExternalEngineProxy *
CExternalEngineProxy::PengineCreate(CMemoryPool *mp)
{
	if (nullptr == orca_external_engine)
	{
		return nullptr;
	}
	CExternalEngineProxy *engine = GPOS_NEW(mp) CExternalEngineProxy(mp);
	if (0 == engine->m_config.ops)
	{
		engine->Release();
		return nullptr;
	}
	return engine;
}

BOOL
CExternalEngineProxy::FEnabled(EEngineOp op) const
{
	return 0 != (m_config.ops & (1u << op));
}

DOUBLE
CExternalEngineProxy::MinRows() const
{
	return m_config.min_rows;
}

BOOL
CExternalEngineProxy::FSupports(EEngineAsk ask, IMDId *mdid) const
{
	return gpdb::ExternalEngineSupports((int) ask,
										CMDIdGPDB::CastMdid(mdid)->Oid());
}

DOUBLE
CExternalEngineProxy::Cost(const SEngineCostInput &input) const
{
	OrcaEngineCostUnits units = {
		input.m_sort_tup_width_unit, input.m_join_feeding_tup_column_unit,
		input.m_join_feeding_tup_width_unit, input.m_join_output_tup_unit};

	OrcaEngineCostInput in;
	in.op = (OrcaEngineOp) input.m_op;
	in.join_type = (OrcaEngineJoinType) input.m_join;
	in.pg_cost = input.m_pg_cost;
	in.pg_cost_swapped = input.m_pg_cost_swapped;
	in.rebinds = input.m_rebinds;
	in.rows = input.m_rows;
	in.width = input.m_width;
	for (ULONG ul = 0; ul < 2; ul++)
	{
		in.child_rows[ul] = input.m_child_rows[ul];
		in.child_width[ul] = input.m_child_width[ul];
	}
	in.join_cols = (int) input.m_join_cols;
	in.units = &units;

	const ULONG ncols = input.m_ncols;
	Oid *types = GPOS_NEW_ARRAY(m_mp, Oid, ncols + 1);
	bool *byval = GPOS_NEW_ARRAY(m_mp, bool, ncols + 1);
	for (ULONG ul = 0; ul < ncols; ul++)
	{
		const IMDType *type = input.m_col_types[ul];
		types[ul] = CMDIdGPDB::CastMdid(type->MDId())->Oid();
		byval[ul] = type->IsPassedByValue();
	}
	in.ncols = (int) ncols;
	in.col_types = types;
	in.col_byval = byval;
	in.col_widths = input.m_col_widths;

	DOUBLE cost = gpdb::ExternalEngineCost(&in);
	GPOS_DELETE_ARRAY(types);
	GPOS_DELETE_ARRAY(byval);
	return cost;
}
