/*-------------------------------------------------------------------------
 *
 * orca.h
 *	  prototypes for the ORCA query planner
 *
 *
 * Portions Copyright (c) 2010-Present, VMware, Inc. or its affiliates
 * Portions Copyright (c) 2005-2009, Greenplum inc
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *			src/include/optimizer/orca.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef ORCA_H
#define ORCA_H

#include "pg_config.h"

#ifdef USE_ORCA

extern PlannedStmt * optimize_query(Query *parse, int cursorOptions, ParamListInfo boundParams);
extern Node *transformGroupedWindows(Node *node, void *context);

// plan_hint_hook generates HintState by parsing a Query.
typedef void *(*plan_hint_hook_type) (Query *parse);
extern PGDLLIMPORT plan_hint_hook_type plan_hint_hook;

/*
 * External execution engine. An extension that can run part of a plan
 * itself (e.g. a vectorized engine) points orca_external_engine at one of
 * these; Orca then also costs plans that run supported operators in that
 * engine, with conversions at the engine boundaries. Each engine region is
 * emitted as a CustomScan with the methods registered as custom_scan_name:
 * custom_private holds a String with the region's DXL plan and custom_plans
 * the PG subplans feeding it, in DXL pre-order.
 */
typedef enum OrcaEngineAsk
{
	ORCA_ENGINE_ASK_TYPE,		/* values of this type can cross the boundary */
	ORCA_ENGINE_ASK_OPERATOR,
	ORCA_ENGINE_ASK_FUNCTION,
	ORCA_ENGINE_ASK_AGG,		/* single-stage aggregate */
	ORCA_ENGINE_ASK_AGG_SPLIT,	/* aggregate whose partial state is a plain value */
	ORCA_ENGINE_ASK_MOTION_KEY,	/* type the engine hashes exactly like cdbhash */
	ORCA_ENGINE_ASK_WINDOW_FUNC	/* window-only function */
} OrcaEngineAsk;

typedef enum OrcaEngineOp
{
	ORCA_ENGINE_OP_TO_ENGINE,	/* conversion from PG tuples */
	ORCA_ENGINE_OP_TO_PG,		/* conversion back to PG tuples */
	ORCA_ENGINE_OP_HASH_AGG,
	ORCA_ENGINE_OP_HASH_JOIN,
	ORCA_ENGINE_OP_COMPUTE_SCALAR,
	ORCA_ENGINE_OP_FILTER,
	ORCA_ENGINE_OP_SORT,
	ORCA_ENGINE_OP_MOTION,		/* motion shipping engine batches */
	ORCA_ENGINE_OP_WINDOW,
	ORCA_ENGINE_OP_SCAN			/* table scans producing engine batches */
} OrcaEngineOp;

/*
 * An engine motion ships an island's output as rows of these two columns:
 * the target segment (for redistribution) and a serialized batch. The
 * sending island's custom_private gets a second String with its route.
 */
#define ORCA_ENGINE_MOTION_SEGID_COL "engine_segid"
#define ORCA_ENGINE_MOTION_PAYLOAD_COL "engine_payload"

typedef struct OrcaEngineConfig
{
	uint32		ops;			/* bitmask of 1 << OrcaEngineOp; 0 disables */
	double		min_rows;		/* skip joins/aggs whose inputs are all smaller */
} OrcaEngineConfig;

typedef enum OrcaEngineJoinType
{
	ORCA_ENGINE_JOIN_NONE,		/* not a hash join */
	ORCA_ENGINE_JOIN_INNER,
	ORCA_ENGINE_JOIN_LEFT,
	ORCA_ENGINE_JOIN_RIGHT,
	ORCA_ENGINE_JOIN_FULL,
	ORCA_ENGINE_JOIN_SEMI,
	ORCA_ENGINE_JOIN_ANTI,
	ORCA_ENGINE_JOIN_ANTI_NOT_IN	/* null-aware anti join */
} OrcaEngineJoinType;

/* Orca cost model parameters, for engines that cost their own algorithms */
typedef struct OrcaEngineCostUnits
{
	double		sort_tup_width;
	double		join_feeding_tup_column;
	double		join_feeding_tup_width;
	double		join_output_tup;
} OrcaEngineCostUnits;

typedef struct OrcaEngineCostInput
{
	OrcaEngineOp op;
	OrcaEngineJoinType join_type;
	/*
	 * Orca's cost of the PG operator excluding its children, over all
	 * rebinds (0 for conversions). For hash joins it assumes the build side
	 * fits in memory; pg_cost_swapped is the same with the inputs' roles
	 * exchanged.
	 */
	Cost		pg_cost;
	Cost		pg_cost_swapped;
	double		rebinds;
	double		rows;			/* output, per segment */
	double		width;
	double		child_rows[2];	/* per segment; hash joins: probe, build */
	double		child_width[2];
	int			join_cols;		/* columns the join condition uses */
	/* conversions: the columns crossing the boundary */
	int			ncols;
	const Oid  *col_types;
	const bool *col_byval;
	const double *col_widths;	/* average bytes */
	const OrcaEngineCostUnits *units;
} OrcaEngineCostInput;

typedef struct OrcaExternalEngine
{
	const char *custom_scan_name;
	/* Called once per optimization. */
	void		(*configure) (OrcaEngineConfig *config);
	bool		(*supports) (OrcaEngineAsk ask, Oid oid);
	/* Cost, in Orca units, of running in->op in the engine. */
	Cost		(*cost) (const OrcaEngineCostInput *in);
} OrcaExternalEngine;

extern PGDLLIMPORT const OrcaExternalEngine *orca_external_engine;

#endif

#endif /* ORCA_H */
