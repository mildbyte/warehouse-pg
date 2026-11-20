/*-------------------------------------------------------------------------
 *
 * memquota.h
 *	  Routines related to memory quota for queries.
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/memquota.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef MEMQUOTA_H_
#define MEMQUOTA_H_

#include "nodes/plannodes.h"
#include "cdb/cdbplan.h"

typedef enum ResManagerMemoryPolicy
{
	RESMANAGER_MEMORY_POLICY_NONE,
	RESMANAGER_MEMORY_POLICY_AUTO,
	RESMANAGER_MEMORY_POLICY_EAGER_FREE
} ResManagerMemoryPolicy;

extern ResManagerMemoryPolicy gp_resmanager_memory_policy_default;
extern bool						gp_log_resmanager_memory_default;
extern int						gp_resmanager_memory_policy_auto_fixed_mem_default;
extern bool						gp_resmanager_print_operator_memory_limits_default;

extern ResManagerMemoryPolicy	*gp_resmanager_memory_policy;
extern bool						*gp_log_resmanager_memory;
extern int						*gp_resmanager_memory_policy_auto_fixed_mem;
extern bool						*gp_resmanager_print_operator_memory_limits;

#define GP_RESMANAGER_MEMORY_LOG_LEVEL NOTICE

#define IsResManagerMemoryPolicyNone() (*gp_resmanager_memory_policy == RESMANAGER_MEMORY_POLICY_NONE)
#define IsResManagerMemoryPolicyAuto() (*gp_resmanager_memory_policy == RESMANAGER_MEMORY_POLICY_AUTO)
#define IsResManagerMemoryPolicyEagerFree() (*gp_resmanager_memory_policy == RESMANAGER_MEMORY_POLICY_EAGER_FREE)

#define LogResManagerMemory() (*gp_log_resmanager_memory == true)
#define ResManagerPrintOperatorMemoryLimits() (*gp_resmanager_print_operator_memory_limits == true)

extern void PolicyAutoAssignOperatorMemoryKB(PlannedStmt *stmt, uint64 memoryAvailable);
extern void PolicyEagerFreeAssignOperatorMemoryKB(PlannedStmt *stmt, uint64 memoryAvailable);
extern void AssignOperatorMemoryKB(PlannedStmt *stmt, uint64 memoryAvailable);

/*
 * Hook for extensions: the number of memory-intensive operators a plan node
 * counts as, or -1 to leave it to the built-in rules (one for a Sort, Hash,
 * hashed Agg and the like, none for the rest).
 *
 * The policies split the statement's memory among the memory-intensive
 * operators, and set operatorMemKB of a node counting as n of them to n
 * operators' shares. An extension's CustomScan that runs several sorts,
 * hash tables or the like of its own can count as that many, to get memory
 * for them in its operatorMemKB, which its executor then keeps to.
 *
 * The nodes under it are counted as usual. Whether a node is a blocking
 * operator, which EAGER_FREE groups the operators by, is still up to the
 * built-in rules. A CustomScan isn't one, so its operators are taken to use
 * their memory together with the rest of its operator group.
 */
typedef int (*plan_mem_intensive_ops_hook_type) (Plan *plan, PlannedStmt *stmt);
extern PGDLLIMPORT plan_mem_intensive_ops_hook_type plan_mem_intensive_ops_hook;

/**
 * Inverse for explain analyze.
 */
extern uint64 PolicyAutoStatementMemForNoSpill(PlannedStmt *stmt, uint64 minOperatorMemKB);

/**
 * Is result node memory intensive?
 */
extern bool IsResultMemoryIntensive(Result *res);

/*
 * Calculate the amount of memory reserved for the query
 */
extern int64 ResourceManagerGetQueryMemoryLimit(PlannedStmt* stmt);

#endif /* MEMQUOTA_H_ */
