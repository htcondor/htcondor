# Startup Limits Design Document

## Problem Statement
The SchedD currently lacks an effective mechanism between "run all jobs as quickly as
possible" and "put jobs on hold and do nothing".  The goal of the startup limits is to
provide a _mechanism_ that an external agent can utilize to slow down the startup rate
of a large class of jobs (such as all jobs from a given user or all jobs with a specific
sandbox) instead of requiring the agent to manipulate the startups job-by-job.  Given we
can have problems interacting with a specific class of EPs (such as rate limiting startups
at a given site), we'd like the rate limits to have both job and machine ad in the target.

This is an experimental design with a goal of integrating the AP with Pelican code.
Accordingly, we would like no Python or CLI client tools as we don't want others using the
mechanism as it might be redesigned or completely thrown away.

Since the external agent is going to be experimental as well, we want all limits to have
stringent leases, meaning the system goes back to "default HTCondor behavior" if the agent
crashes or the schedd restarted.

## Architecture
Create a set of limits inside the schedd with the following attributes:
- Expression specifying which job startups the limit is applied (evaluated
  in the context of the EP and job).
- A "cost" to startup a job.
- Rate limits (count of jobs per time interval)
- Name / UUID

The limits are evaluated when the AP is considering assigning the job to a match
it got from a matchmaker (or reusing a match).  This purposely is done before the
shadow is started.

If a rate limit prevents an otherwise-runnable job from starting on a fresh match from
the matchmaker, we say it "blocked" the match as the match will now be ignored by the
AP and cause more churn at the matchmaker.  We want a minimal feedback mechanism to prevent
this but are not trying to cover all cases:

- If a rate limit only references job attributes, when reduce the resource request list
  size (count of "jobs") to be no more than the count of jobs that would pass the rate limit
  in the next 60s.
- If a rate limit references both job and machine attributes, then append to the RRL's
  requirements that the matchmaker _not_ return slots matching the rate limit.

## Details

Startup limits are in-memory token-bucket throttles keyed by a user-provided tag/UUID. Each
limit installs via the `CREATE_STARTUP_LIMIT` RPC, is represented as a ClassAd, and is
queried via `QUERY_STARTUP_LIMITS`. A periodic timer handles expiration and reporting;
limits never survive schedd restarts.

Core pieces:
- **Limit definition**: Request ClassAd provides `StartupLimitTag`, optional `StartupLimitName`, `StartupLimitExpr` (match expression over job/TARGET), optional `StartupLimitCostExpr` (defaults to `1`), rate parameters `StartupLimitRateCount`/`StartupLimitRateWindow`, optional burst depth `StartupLimitBurst`, optional per-draw cap `StartupLimitMaxBurstCost`, and `StartupLimitExpiration` (bounded by config `STARTUP_LIMIT_MAX_EXPIRATION`).
- **Token bucket (RateLimit)**: Holds `capacity` (tokens per window), `window`, `tokens`, `burst` (allowed debt), and `max_burst_cost` (caps any single cost draw). Tokens refill linearly; tokens are clamped to `[ -burst, capacity ]`. A request with cost `c` is allowed if `tokens + burst >= min(c, max_burst_cost)`.
- **Matchmaking**: During matchmaking, the `StartupLimitsAllowJob` function checks all limits whose expr matches the job/match ad. Cost is evaluated (number-valued; defaults to 1, negatives are floored to 0). If the bucket cannot cover the cost, the job is skipped and the limit UUID may be recorded for later ban/adjustment.
- **Ignored-match accounting**: When a match is skipped, the function `StartupLimitsRecordIgnoredMatches` records the user/pool key and timestamps. A ban window (`STARTUP_LIMIT_BAN_WINDOW`) gates temporary back-off per source, and periodic cleanup aggregates block seconds and ignored counts for logging.
- **Requirement adjustment**: For subsequent matchmaking requests from a banned source, `StartupLimitsAdjustRequest` either tightens `match_max` (job-only limits, using `capacityIn` lookahead over `STARTUP_LIMIT_LOOKAHEAD`) or appends `!(StartupLimitExpr)` to the request `Requirements` for limits that reference TARGET. Before appending, the expression is flattened against the request ad; if flattening proves it `false`, the clause is skipped to avoid over-constraining.
- **Observability**: `QUERY_STARTUP_LIMITS` returns per-limit ads including UUID/tag/name, expr/cost_expr, rate/burst/max-burst-cost, expiration, skipped counts, ignored counts, last ignored time, and ignored user list. Periodic logs summarize ignored and blocked seconds deltas.

## Implementation Notes
- **StartupLimit map**: `std::unordered_map<std::string, StartupLimit>` keyed by UUID. Each holds tag, name, expr/cost_expr trees and rewritten sources, job/target refs, job_only flag, `RateLimit` instance, burst/max_burst_cost, expiration, counters, ignored source map, and last report times.
- **RateLimit**: `init(n, window, now, burst, max_burst_cost)` sets capacity, burst, per-draw cap, tokens=capacity, and last_refill. `allow(now, cost)` refills, caps cost by `max_burst_cost` (>0), checks `tokens+burst`, debits tokens, clamps to `-burst`. `capacityIn(horizon, now)` projects available capacity with refill and clamps to burst bounds.
- **Creation**: `QmgmtHandleCreateStartupLimit` parses request ad, enforces required fields (tag, expr, count, window), caps expiration, parses expr and cost_expr (rewriting JOB->MY, MACHINE->TARGET), merges ref sets, sets job_only, and initializes rate/burst/max_burst_cost. Updates reuse stats when UUID exists; otherwise new UUID is minted. Reply returns status and UUID.
- **Query**: `QmgmtHandleQueryStartupLimits` filters by UUID/tag and streams limit ads via ReliSock, ending with a sentinel ClassAd.
- **Match-time allow**: `StartupLimitsAllowJob` iterates active limits, skips expired/non-matching, evaluates cost, and calls `rate.allow`. On deny, increments skipped and optionally returns UUID in `blocked_limits`.
- **Record ignored**: `StartupLimitsRecordIgnoredMatches` notes blocked limits per user/pool key, increments ignored counts, and timestamps for ban window tracking.
- **Adjust request**: `StartupLimitsAdjustRequest` consults ignored_sources within ban window. For job-only limits, it reduces `match_max` using projected capacity. For limits involving TARGET, it flattens the limit expr against the request ad; if the flattened result is not proven `false`, it appends `!(expr)` to `Requirements`. This avoids adding clauses that are already false after partial evaluation.
- **Cleanup/reporting**: A DaemonCore timer runs every 60s, refreshing config, accumulating blocked seconds per ignored source window, logging deltas, and expiring limits. Timer stops when map is empty.

### Attributes (ClassAd)
- Inputs: `StartupLimitTag`, `StartupLimitName` (opt), `StartupLimitExpr`, `StartupLimitCostExpr` (opt, default "1"), `StartupLimitRateCount`, `StartupLimitRateWindow`, `StartupLimitBurst` (opt), `StartupLimitMaxBurstCost` (opt), `StartupLimitExpiration`.
- Outputs (query): above plus `StartupLimitUUID`, `StartupLimitJobsSkipped`, `StartupLimitMatchesIgnored`, `StartupLimitLastIgnored` (opt), `StartupLimitIgnoredUsers` (set string), and expiration.

### Tests
- **Ornithology/pytest `test_startup_limits.py`**: covers creation, query, basic rate limiting, burst allowance, per-job cost expression, max-burst-cost capping, ignore tracking, ban window, and requirement adjustment behavior.

### Other
- Limits are in-memory; restart clears them.
- Expiration is mandatory; enforced max via `STARTUP_LIMIT_MAX_EXPIRATION` (defaults to 5
  minutes).
- Cost expressions that fail evaluation default to 1 with a log warning; negative/zero costs are treated as zero (no debit).
- `max_burst_cost` protects burst depth from a single expensive job; set to 0 to disable the cap.
