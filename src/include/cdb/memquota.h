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
 * How the memory policies count a plan node.
 *
 * memIntenseOps: the memory-intensive operators the node counts as. The
 * policies split the statement's memory among the memory-intensive
 * operators, and set operatorMemKB of a node counting as n of them to n
 * operators' shares.
 *
 * blocking: the plan above doesn't go on until the node's subtree has
 * returned all its rows, as it doesn't past a Hash or a Sort. EAGER_FREE
 * groups the operators by the blocking ones, as the operators of a
 * subtree done before the rest of the plan runs needn't share the memory
 * with it; a blocking node (without external parameters) starts a group of
 * its own.
 */
typedef struct PlanMemoryInfo
{
	uint64		memIntenseOps;
	bool		blocking;
} PlanMemoryInfo;

/*
 * Hook for extensions: adjust how the memory policies count a plan node,
 * which `info` holds by the built-in rules (one memory-intensive operator for
 * a Sort, Hash, hashed Agg and the like, none for the rest; a Hash, Sort or
 * blocking Agg is blocking). `parent` is the node above it, or NULL for the
 * top node.
 *
 * An extension's CustomScan that runs several sorts, hash tables or the
 * like of its own can count as that many operators, to get memory for them
 * in its operatorMemKB, which its executor then keeps to. An input the
 * CustomScan reads whole before it returns a row, as a hash table's build
 * side, is blocking, as the Hash over it would be.
 */
typedef void (*plan_memory_hook_type) (Plan *plan, Plan *parent, PlannedStmt *stmt,
									   PlanMemoryInfo *info);
extern PGDLLIMPORT plan_memory_hook_type plan_memory_hook;

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
