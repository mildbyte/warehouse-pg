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
 * Asked by Orca whether a type/operator/function/aggregate oid can run in
 * the extension's Arrow engine. kind: 0 type, 1 operator, 2 function,
 * 3 aggregate, 4 aggregate whose partial state is a plain value.
 * Orca emits Arrow islands as CustomScans named ORCA_ARROW_ISLAND_NAME,
 * whose custom_private holds the island DXL and custom_plans its inputs.
 */
typedef bool (*orca_arrow_capability_hook_type) (int kind, Oid oid);
extern PGDLLIMPORT orca_arrow_capability_hook_type orca_arrow_capability_hook;
#define ORCA_ARROW_ISLAND_NAME "Orca Arrow Island"

#endif

#endif /* ORCA_H */
