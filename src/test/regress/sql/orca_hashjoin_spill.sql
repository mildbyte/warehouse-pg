--
-- optimizer_hashjoin_spill_mem: the hash table size per segment past which
-- ORCA costs a hash join as spilling (0, the default, keeps 50MB). The PG
-- planner ignores it.
--
CREATE TABLE hjspill_f (a int, b int, pad text) DISTRIBUTED BY (a);
CREATE TABLE hjspill_a (a int, x text) DISTRIBUTED BY (a);
CREATE TABLE hjspill_b (b int, y text) DISTRIBUTED BY (b);
INSERT INTO hjspill_f SELECT i, i % 1000, repeat('x', 20) FROM generate_series(1, 100000) i;
INSERT INTO hjspill_a SELECT i, repeat('a', 50) FROM generate_series(1, 50000) i;
INSERT INTO hjspill_b SELECT i, repeat('b', 50) FROM generate_series(1, 1000) i;
ANALYZE hjspill_f;
ANALYZE hjspill_a;
ANALYZE hjspill_b;

SHOW optimizer_hashjoin_spill_mem;
-- hashing hjspill_a's 50000 rows doesn't spill
EXPLAIN (COSTS OFF)
SELECT count(*) FROM hjspill_f JOIN hjspill_a USING (a) JOIN hjspill_b USING (b);
-- past 64kB it would: ORCA joins hjspill_f with the small hjspill_b first
-- and hashes the result instead
SET optimizer_hashjoin_spill_mem = '64kB';
EXPLAIN (COSTS OFF)
SELECT count(*) FROM hjspill_f JOIN hjspill_a USING (a) JOIN hjspill_b USING (b);
SELECT count(*) FROM hjspill_f JOIN hjspill_a USING (a) JOIN hjspill_b USING (b);
RESET optimizer_hashjoin_spill_mem;
EXPLAIN (COSTS OFF)
SELECT count(*) FROM hjspill_f JOIN hjspill_a USING (a) JOIN hjspill_b USING (b);

DROP TABLE hjspill_f, hjspill_a, hjspill_b;
