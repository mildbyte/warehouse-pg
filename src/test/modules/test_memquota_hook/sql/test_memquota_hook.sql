--
-- Test plan_mem_intensive_ops_hook.
--
-- The memory is assigned on the coordinator, so only it loads the module.
LOAD 'test_memquota_hook';

SET optimizer = off;
SET statement_mem = '10MB';
SET enable_hashjoin = on;
SET enable_mergejoin = off;
SET enable_nestloop = off;

CREATE TABLE mq_t (a int, b int) DISTRIBUTED BY (a);
CREATE TABLE mq_u (c int, d int) DISTRIBUTED BY (c);
INSERT INTO mq_t SELECT i, i % 7 FROM generate_series(1, 100) i;
INSERT INTO mq_u SELECT i, i % 5 FROM generate_series(1, 100) i;
ANALYZE mq_t;
ANALYZE mq_u;

SET test_memquota_hook.report = on;

-- the built-in rules: the Sort and the Hash are a memory-intensive operator
-- each, and get a share each of the memory the other nodes leave
SELECT count(*) FROM (SELECT * FROM mq_t JOIN mq_u ON a = c ORDER BY b) s;

-- the Sort as three: it gets three shares of four, the Hash one
SET test_memquota_hook.sort_ops = 3;
SELECT count(*) FROM (SELECT * FROM mq_t JOIN mq_u ON a = c ORDER BY b) s;

-- as none: the fixed memory of a node that isn't memory-intensive, and the
-- Hash gets the rest
SET test_memquota_hook.sort_ops = 0;
SELECT count(*) FROM (SELECT * FROM mq_t JOIN mq_u ON a = c ORDER BY b) s;

RESET test_memquota_hook.sort_ops;
RESET test_memquota_hook.report;
DROP TABLE mq_t;
DROP TABLE mq_u;
