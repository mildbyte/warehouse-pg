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
#include "access/htup.h"
#include "storage/block.h"

#ifdef USE_ORCA

extern PlannedStmt * optimize_query(Query *parse, int cursorOptions, ParamListInfo boundParams);
extern Node *transformGroupedWindows(Node *node, void *context);

// plan_hint_hook generates HintState by parsing a Query.
typedef void *(*plan_hint_hook_type) (Query *parse);
extern PGDLLIMPORT plan_hint_hook_type plan_hint_hook;

/*
 * Statistics hooks for ORCA's metadata translation, the counterparts of the
 * PG planner's get_relation_info_hook and get_relation_stats_hook for tables
 * whose data and statistics live outside PG (such as tables an external
 * engine stores itself).
 *
 * orca_relation_size_hook: the relation's row and page counts. Returns false
 * to use pg_class.
 *
 * orca_att_stats_hook: a pg_statistic tuple for the column, palloc'd (the
 * caller frees it), or NULL to read pg_statistic.
 */
typedef bool (*orca_relation_size_hook_type) (Oid relid, double *tuples,
											  BlockNumber *pages);
extern PGDLLIMPORT orca_relation_size_hook_type orca_relation_size_hook;

typedef HeapTuple (*orca_att_stats_hook_type) (Oid relid, AttrNumber attnum);
extern PGDLLIMPORT orca_att_stats_hook_type orca_att_stats_hook;

#endif

#endif /* ORCA_H */
