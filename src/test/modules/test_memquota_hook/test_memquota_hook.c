/*-------------------------------------------------------------------------
 *
 * test_memquota_hook.c
 *		Test plan_mem_intensive_ops_hook (cdb/memquota.h).
 *
 * test_memquota_hook.sort_ops: the number of memory-intensive operators a
 * Sort counts as, or -1 for the built-in rules.
 *
 * test_memquota_hook.report: report each plan node's operatorMemKB as the
 * query starts on the coordinator.
 *
 * IDENTIFICATION
 *		src/test/modules/test_memquota_hook/test_memquota_hook.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "cdb/cdbvars.h"
#include "cdb/memquota.h"
#include "executor/executor.h"
#include "fmgr.h"
#include "utils/guc.h"

PG_MODULE_MAGIC;

void		_PG_init(void);

static int	sort_ops = -1;
static bool report = false;

static plan_mem_intensive_ops_hook_type prev_ops_hook = NULL;
static ExecutorStart_hook_type prev_executor_start = NULL;

static int
test_ops(Plan *plan, PlannedStmt *stmt)
{
	if (IsA(plan, Sort) && sort_ops >= 0)
		return sort_ops;
	if (prev_ops_hook)
		return prev_ops_hook(plan, stmt);
	return -1;
}

static const char *
node_name(Plan *plan)
{
	switch (nodeTag(plan))
	{
		case T_Motion:
			return "Motion";
		case T_Sort:
			return "Sort";
		case T_SeqScan:
			return "Seq Scan";
		case T_HashJoin:
			return "Hash Join";
		case T_Hash:
			return "Hash";
		case T_Agg:
			return "Agg";
		case T_Result:
			return "Result";
		case T_Limit:
			return "Limit";
		default:
			return "other";
	}
}

static void
report_plan(Plan *plan, int depth)
{
	if (plan == NULL)
		return;
	elog(NOTICE, "%*s%s: %lu kB", depth * 2, "", node_name(plan),
		 (unsigned long) plan->operatorMemKB);
	report_plan(plan->lefttree, depth + 1);
	report_plan(plan->righttree, depth + 1);
}

static void
test_executor_start(QueryDesc *queryDesc, int eflags)
{
	if (prev_executor_start)
		prev_executor_start(queryDesc, eflags);
	else
		standard_ExecutorStart(queryDesc, eflags);

	if (report && Gp_role == GP_ROLE_DISPATCH)
		report_plan(queryDesc->plannedstmt->planTree, 0);
}

void
_PG_init(void)
{
	DefineCustomIntVariable("test_memquota_hook.sort_ops",
							"Memory-intensive operators a Sort counts as.",
							"-1 leaves it to the built-in rules.",
							&sort_ops,
							-1, -1, 100,
							PGC_USERSET, 0,
							NULL, NULL, NULL);
	DefineCustomBoolVariable("test_memquota_hook.report",
							 "Report each plan node's operatorMemKB.",
							 NULL,
							 &report,
							 false,
							 PGC_USERSET, 0,
							 NULL, NULL, NULL);

	prev_ops_hook = plan_mem_intensive_ops_hook;
	plan_mem_intensive_ops_hook = test_ops;
	prev_executor_start = ExecutorStart_hook;
	ExecutorStart_hook = test_executor_start;
}
