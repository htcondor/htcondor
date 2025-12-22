/***************************************************************
 *
 * Startup limit support for schedd
 *
 ***************************************************************/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_daemon_core.h"
#include "condor_io.h"
#include "qmgmt.h"
#include "qmgmt_startup_limits.h"
#include "condor_classad.h"
#include "compat_classad.h"
#include "compat_classad_util.h"
#include "condor_attributes.h"
#include "classad_helpers.h"
#include <chrono>
#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <memory>
#include <random>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Startup rate limits (per-schedd, in-memory)
// ---------------------------------------------------------------------------

struct RateLimit {
    double capacity{0};      // max tokens replenished per window
    double burst{0};         // extra depth allowed below zero tokens
    double max_burst_cost{0}; // cap on any single cost draw
    std::chrono::duration<double> window{std::chrono::seconds{1}}; // refill window (M seconds)
    double tokens{0};        // current tokens (can be negative down to -burst)
    std::chrono::steady_clock::time_point last_refill{};

    void init(double n, std::chrono::duration<double> win, std::chrono::steady_clock::time_point now, double burst_depth, double burst_cap) {
        capacity = std::max(0.0, n);
        burst = std::max(0.0, burst_depth);
        max_burst_cost = std::max(0.0, burst_cap);
        window = std::max(win, std::chrono::duration<double>(1.0));
        tokens = capacity;
        last_refill = now;
    }

    void refill(std::chrono::steady_clock::time_point now) {
        if (now <= last_refill) { return; }
        if (capacity <= 0) { tokens = 0; last_refill = now; return; }
        double rate = capacity / window.count();
        double elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(now - last_refill).count();
        tokens = std::min(capacity, tokens + rate * elapsed);
        tokens = std::max(-burst, tokens); // do not accumulate deeper debt than burst
        last_refill = now;
    }

    bool allow(std::chrono::steady_clock::time_point now, double cost) {
        refill(now);
        double need = std::max(0.0, cost);
        if (max_burst_cost > 0) {
            need = std::min(need, max_burst_cost);
        }
        double available = tokens + burst;
        if (available + 1e-9 < need) { return false; }
        tokens -= need;
        tokens = std::max(-burst, tokens);
        return true;
    }

    int capacityIn(std::chrono::duration<double> horizon, std::chrono::steady_clock::time_point now) {
        refill(now);
        double rate = (window.count() > 0) ? (capacity / window.count()) : 0.0;
        double future = tokens + rate * std::max(0.0, horizon.count());
        future = std::min(capacity, future);
        future = std::max(-burst, future);
        double available = std::min(capacity + burst, future + burst);
        return (int)floor(available + 1e-6);
    }
};

struct StartupLimit {
    std::string uuid;
    std::string tag;
    std::string display_name;      // human-friendly name; falls back to uuid
    std::string expr_source;       // rewritten expression string
    std::unique_ptr<classad::ExprTree> expr;
    std::string cost_expr_source;
    std::unique_ptr<classad::ExprTree> cost_expr;
    bool cost_expr_warned{false};
    classad::References job_refs;
    classad::References target_refs;
    bool job_only{false};          // true if expression references only job attrs
    RateLimit rate;
    double burst{0};               // tokens allowed below zero
    double max_burst_cost{0};
    time_t expires_at{0};
    long long jobs_skipped{0};
    long long matches_ignored{0};
    long long block_seconds{0};
    long long matches_ignored_reported{0};
    long long block_seconds_reported{0};
    time_t last_report_time{0};
    time_t last_ignored{0};
    std::unordered_map<std::string, time_t> ignored_sources; // key=user|pool
};

static std::unordered_map<std::string, StartupLimit> StartupLimits;
static int StartupLimitCleanupTid = -1;
static int StartupLimitMaxExpiration = 0; // seconds
static int StartupLimitBanWindow = 60;     // seconds after an ignored match
static int StartupLimitLookahead = 60;     // seconds for throughput throttling

static std::string make_source_key(const char *user, const char *pool) {
    std::string key = user ? user : "";
    key += "|";
    if (pool) { key += pool; }
    return key;
}

static std::string new_startup_uuid() {
    unsigned long long now = (unsigned long long)time(nullptr);
    unsigned long long rnd = ((unsigned long long)getpid() << 32) ^ (unsigned long long)get_random_uint_insecure();
    char buf[64];
    snprintf(buf, sizeof(buf), "%llx-%llx", now, rnd);
    return std::string(buf);
}

static void ensure_startup_limit_timer();
static void cancel_startup_limit_timer_if_idle();
static void startup_limit_cleanup(int /*tid*/);
static long long compute_blocked_seconds(const StartupLimit &lim, time_t window_start, time_t window_end);

static void startup_limit_log(const StartupLimit &lim, const char *msg) {
    if (!IsDebugLevel(D_FULLDEBUG)) { return; }
    dprintf(D_FULLDEBUG, "StartupLimit[%s:%s] %s\n", lim.tag.c_str(), lim.uuid.c_str(), msg ? msg : "");
}

static void startup_limit_refresh_config()
{
    StartupLimitMaxExpiration = param_integer("STARTUP_LIMIT_MAX_EXPIRATION", 5*60, 1);
    StartupLimitBanWindow     = param_integer("STARTUP_LIMIT_BAN_WINDOW", 60, 1);
    StartupLimitLookahead     = param_integer("STARTUP_LIMIT_LOOKAHEAD", 60, 1);
}

static void ensure_startup_limit_timer()
{
    if (StartupLimitCleanupTid >= 0 || StartupLimits.empty()) { return; }
    StartupLimitCleanupTid = daemonCore->Register_Timer(60, 60, startup_limit_cleanup, "startup_limit_cleanup");
}

static void cancel_startup_limit_timer_if_idle()
{
    if (!StartupLimits.empty() || StartupLimitCleanupTid < 0) { return; }
    daemonCore->Cancel_Timer(StartupLimitCleanupTid);
    StartupLimitCleanupTid = -1;
}

static long long compute_blocked_seconds(const StartupLimit &lim, time_t window_start, time_t window_end)
{
    if (window_end <= window_start) { return 0; }
    long long total = 0;
    for (const auto &kv : lim.ignored_sources) {
        time_t start = kv.second;
        time_t end = start + StartupLimitBanWindow;
        time_t a = std::max(start, window_start);
        time_t b = std::min(end, window_end);
        if (b > a) { total += (long long)(b - a); }
    }
    return total;
}

static void startup_limit_cleanup(int /*tid*/)
{
    startup_limit_refresh_config();

    time_t now_wall = time(nullptr);
    for (auto it = StartupLimits.begin(); it != StartupLimits.end(); ) {
        StartupLimit &lim = it->second;

        time_t window_start = lim.last_report_time ? lim.last_report_time : (lim.last_ignored ? lim.last_ignored : now_wall);
        long long blocked_delta = compute_blocked_seconds(lim, window_start, now_wall);
        if (blocked_delta > 0) { lim.block_seconds += blocked_delta; }
        long long ignored_delta = lim.matches_ignored - lim.matches_ignored_reported;
        long long block_report_delta = lim.block_seconds - lim.block_seconds_reported;

        if (ignored_delta > 0 || block_report_delta > 0) {
            const std::string &name = lim.display_name.empty() ? lim.uuid : lim.display_name;
            dprintf(D_ALWAYS,
                    "StartupLimit summary name=%s uuid=%s ignored_delta=%lld block_seconds_delta=%lld window_start=%lld window_end=%lld\n",
                    name.c_str(), lim.uuid.c_str(),
                    (long long)ignored_delta, (long long)block_report_delta,
                    (long long)window_start, (long long)now_wall);
        }

        lim.matches_ignored_reported = lim.matches_ignored;
        lim.block_seconds_reported = lim.block_seconds;
        lim.last_report_time = now_wall;

        if (lim.expires_at && lim.expires_at <= now_wall) {
            startup_limit_log(lim, "expired; removing");
            it = StartupLimits.erase(it);
        } else {
            ++it;
        }
    }
    cancel_startup_limit_timer_if_idle();
}

static bool parse_and_rewrite_expr(const classad::ExprTree *expr_tree, const std::string &expr_str, std::unique_ptr<classad::ExprTree> &out_expr, std::string &rewritten, bool &job_only, classad::References &job_refs, classad::References &target_refs)
{
    std::unique_ptr<classad::ExprTree> owned;
    classad::ExprTree *tree = nullptr;

    if (expr_tree) {
        owned.reset(expr_tree->Copy());
        tree = owned.get();
    } else {
        classad::ClassAdParser parser;
        parser.SetOldClassAd(true);
        if (!parser.ParseExpression(expr_str, tree)) {
            return false;
        }
        owned.reset(tree);
    }

    NOCASE_STRING_MAP mapping;
    mapping["JOB"] = "MY";
    mapping["MACHINE"] = "TARGET";
    RewriteAttrRefs(tree, mapping);

    job_refs.clear();
    target_refs.clear();
    // Collect MY and TARGET references (plus unscoped into both) in a single walk.
    GetAttrRefsOfScopesOrUnscoped(tree, job_refs, "MY", target_refs, "TARGET");
    job_only = target_refs.empty();

    rewritten.clear();
    ExprTreeToString(tree, rewritten);
    out_expr = std::move(owned);
    return true;
}

static bool eval_expr_bool(classad::ExprTree *expr, ClassAd *job, ClassAd *match_ad, bool &result)
{
    classad::Value val;
    if (!EvalExprTree(expr, job, match_ad, val, classad::Value::BOOLEAN_VALUE, "MY", "TARGET")) {
        return false; // evaluation error, propagate failure
    }

    if (val.IsBooleanValueEquiv(result)) { return true; }

    // Map undefined to false but propagate the error value.
    if (val.IsUndefinedValue()) {
        result = false;
        return true;
    }
    if (val.IsErrorValue()) {
        return false;
    }

    // Any other non-boolean is treated as a failure to evaluate.
    return false;
}

static bool eval_expr_number(classad::ExprTree *expr, ClassAd *job, ClassAd *match_ad, double &result)
{
    classad::Value val;
    if (!EvalExprTree(expr, job, match_ad, val, classad::Value::NUMBER_VALUES, "MY", "TARGET")) { return false; }
    return val.IsNumber(result);
}

static bool startup_limit_matches(const StartupLimit &lim, ClassAd *job, ClassAd *match_ad)
{
    if (!lim.expr) { return true; }
    bool result = false;
    if (!eval_expr_bool(lim.expr.get(), job, match_ad, result)) {

        dprintf(D_FULLDEBUG, "StartupLimit expr eval failed tag=%s uuid=%s expr=%s\n",
                lim.tag.c_str(), lim.uuid.c_str(), lim.expr_source.c_str());
        return false;
    }
    return result;
}

static StartupLimit *find_limit_by_uuid(const std::string &uuid)
{
    auto it = StartupLimits.find(uuid);
    if (it == StartupLimits.end()) { return nullptr; }
    return &it->second;
}

static std::string join_list(const std::set<std::string> &items)
{
    std::string out;
    for (auto it = items.begin(); it != items.end(); ++it) {
        if (!out.empty()) { out += ", "; }
        out += *it;
    }
    return out;
}

static void fill_limit_ad(const StartupLimit &lim, ClassAd &ad)
{
    ad.Assign(ATTR_STARTUP_LIMIT_UUID, lim.uuid);
    ad.Assign(ATTR_STARTUP_LIMIT_TAG, lim.tag);
    ad.Assign(ATTR_STARTUP_LIMIT_NAME, lim.display_name.empty() ? lim.uuid : lim.display_name);
    ad.Assign(ATTR_STARTUP_LIMIT_EXPR, lim.expr_source);
    ad.Assign(ATTR_STARTUP_LIMIT_RATE_COUNT, (int)lim.rate.capacity);
    ad.Assign(ATTR_STARTUP_LIMIT_RATE_WINDOW, (int)lim.rate.window.count());
    ad.Assign(ATTR_STARTUP_LIMIT_BURST, (int)lim.burst);
    if (lim.max_burst_cost > 0) { ad.Assign(ATTR_STARTUP_LIMIT_MAX_BURST_COST, (int)lim.max_burst_cost); }
    if (!lim.cost_expr_source.empty()) { ad.Assign(ATTR_STARTUP_LIMIT_COST_EXPR, lim.cost_expr_source); }
    ad.Assign(ATTR_STARTUP_LIMIT_EXPIRATION, (long long)lim.expires_at);
    ad.Assign(ATTR_STARTUP_LIMIT_JOBS_SKIPPED, lim.jobs_skipped);
    ad.Assign(ATTR_STARTUP_LIMIT_MATCHES_IGNORED, lim.matches_ignored);
    if (lim.last_ignored) { ad.Assign(ATTR_STARTUP_LIMIT_LAST_IGNORED, (long long)lim.last_ignored); }
    if (!lim.ignored_sources.empty()) {
        std::set<std::string> keys;
        for (const auto &kv : lim.ignored_sources) { keys.insert(kv.first); }
        ad.Assign(ATTR_STARTUP_LIMIT_IGNORED_USERS, join_list(keys));
    }
}

} // namespace

bool StartupLimitsEmpty()
{
    return StartupLimits.empty();
}

void StartupLimitsCollectJobRefs(classad::References &refs)
{
    refs.clear();
    if (StartupLimits.empty()) { return; }
    for (const auto &kv : StartupLimits) {
        const StartupLimit &lim = kv.second;
        if (!lim.job_only) { continue; }
        refs.insert(lim.job_refs.begin(), lim.job_refs.end());
    }
}

int QmgmtHandleCreateStartupLimit(const ClassAd &request, ClassAd &reply)
{
    startup_limit_refresh_config();

    std::string tag, expr_str;
    std::string display_name;
    std::string cost_expr_str;
    int rate_count = 0;
    int rate_window = 0;
    int burst = 0;
    int max_burst_cost = 0;
    int expires = StartupLimitMaxExpiration;
    request.LookupString(ATTR_STARTUP_LIMIT_TAG, tag);
    request.LookupString(ATTR_STARTUP_LIMIT_NAME, display_name);
    const classad::ExprTree *expr_tree = request.LookupExpr(ATTR_STARTUP_LIMIT_EXPR);
    const classad::ExprTree *cost_expr_tree = request.LookupExpr(ATTR_STARTUP_LIMIT_COST_EXPR);
    classad::ClassAdUnParser unparser;
    if (expr_tree) {
        unparser.Unparse(expr_str, expr_tree);
    }
    if (cost_expr_tree) {
        unparser.Unparse(cost_expr_str, cost_expr_tree);
    }
    if (cost_expr_str.empty()) { cost_expr_str = "1"; }
    request.LookupInteger(ATTR_STARTUP_LIMIT_RATE_COUNT, rate_count);
    request.LookupInteger(ATTR_STARTUP_LIMIT_RATE_WINDOW, rate_window);
    request.LookupInteger(ATTR_STARTUP_LIMIT_BURST, burst);
    request.LookupInteger(ATTR_STARTUP_LIMIT_MAX_BURST_COST, max_burst_cost);
    request.LookupInteger(ATTR_STARTUP_LIMIT_EXPIRATION, expires);

    expires = std::min(expires, StartupLimitMaxExpiration);

    if (tag.empty() || (!expr_tree && expr_str.empty()) || rate_count <= 0 || rate_window <= 0) {
        reply.Assign(ATTR_STARTUP_LIMIT_STATUS, -1);
        reply.Assign(ATTR_STARTUP_LIMIT_ERROR, "Missing tag/expression/rate");
        return 0;
    }

    std::string uuid;
    request.LookupString(ATTR_STARTUP_LIMIT_UUID, uuid);
    auto existing = find_limit_by_uuid(uuid);

    time_t now_wall = time(nullptr);
    time_t new_expires_at = now_wall + expires;
    if (new_expires_at <= now_wall) {
        if (existing) {
            StartupLimits.erase(uuid);
            cancel_startup_limit_timer_if_idle();
        }
        reply.Assign(ATTR_STARTUP_LIMIT_STATUS, -1);
        reply.Assign(ATTR_STARTUP_LIMIT_ERROR, "Expiration is in the past; limit not installed");
        return 0;
    }

    if (uuid.empty()) { uuid = new_startup_uuid(); }

    std::unique_ptr<classad::ExprTree> expr;
    std::string rewritten;
    bool job_only = false;
    classad::References job_refs, target_refs;
    if (!parse_and_rewrite_expr(expr_tree, expr_str, expr, rewritten, job_only, job_refs, target_refs)) {
        reply.Assign(ATTR_STARTUP_LIMIT_STATUS, -1);
        reply.Assign(ATTR_STARTUP_LIMIT_ERROR, "Failed to parse expression");
        return 0;
    }

    std::unique_ptr<classad::ExprTree> cost_expr;
    std::string cost_rewritten;
    bool cost_job_only = true;
    classad::References cost_job_refs, cost_target_refs;
    if (!parse_and_rewrite_expr(cost_expr_tree, cost_expr_str, cost_expr, cost_rewritten, cost_job_only, cost_job_refs, cost_target_refs)) {
        reply.Assign(ATTR_STARTUP_LIMIT_STATUS, -1);
        reply.Assign(ATTR_STARTUP_LIMIT_ERROR, "Failed to parse cost expression");
        return 0;
    }

    auto now_steady = std::chrono::steady_clock::now();
    StartupLimit lim;
    if (existing) {
        lim.jobs_skipped = existing->jobs_skipped;
        lim.matches_ignored = existing->matches_ignored;
        lim.last_ignored = existing->last_ignored;
        lim.ignored_sources = existing->ignored_sources;
        lim.display_name = existing->display_name;
        lim.block_seconds = existing->block_seconds;
        lim.matches_ignored_reported = existing->matches_ignored_reported;
        lim.block_seconds_reported = existing->block_seconds_reported;
        lim.last_report_time = existing->last_report_time;
        lim.cost_expr_source = existing->cost_expr_source;
        lim.cost_expr_warned = existing->cost_expr_warned;
        if (existing->cost_expr) { lim.cost_expr.reset(existing->cost_expr->Copy()); }
    } else {
        lim.last_report_time = now_wall;
    }

    lim.uuid = uuid;
    lim.tag = tag;
    if (!display_name.empty()) {
        lim.display_name = display_name;
    } else if (!lim.display_name.empty()) {
        // preserve prior display name if present
    } else if (!tag.empty()) {
        lim.display_name = tag;
    } else {
        lim.display_name = uuid;
    }
    lim.expr = std::move(expr);
    lim.expr_source = rewritten;
    lim.cost_expr = std::move(cost_expr);
    lim.cost_expr_source = cost_rewritten;
    lim.job_refs = job_refs;
    lim.target_refs = target_refs;
    lim.job_refs.insert(cost_job_refs.begin(), cost_job_refs.end());
    lim.target_refs.insert(cost_target_refs.begin(), cost_target_refs.end());
    lim.job_only = lim.target_refs.empty();
    lim.burst = std::max(0, burst);
    lim.max_burst_cost = std::max(0, max_burst_cost);
    lim.rate.init(rate_count, std::chrono::duration<double>(rate_window), now_steady, lim.burst, lim.max_burst_cost);
    lim.expires_at = new_expires_at;

    StartupLimits[uuid] = std::move(lim);
    ensure_startup_limit_timer();

    dprintf(D_FULLDEBUG,
            "StartupLimit installed tag=%s uuid=%s rate=%d/%d burst=%d max_burst_cost=%d expires_in=%d expr=%s cost_expr=%s job_only=%d\n",
            tag.c_str(), uuid.c_str(), rate_count, rate_window, burst, max_burst_cost,
            expires, rewritten.c_str(), cost_rewritten.c_str(), (int)job_only);

    reply.Assign(ATTR_STARTUP_LIMIT_STATUS, 0);
    reply.Assign(ATTR_STARTUP_LIMIT_UUID, uuid);
    return 0;
}

int QmgmtHandleQueryStartupLimits(const ClassAd &request, ReliSock *sock)
{
    startup_limit_refresh_config();

    std::string want_uuid, want_tag;
    request.LookupString(ATTR_STARTUP_LIMIT_UUID, want_uuid);
    request.LookupString(ATTR_STARTUP_LIMIT_TAG, want_tag);

    sock->encode();
    for (const auto &kv : StartupLimits) {
        const auto &lim = kv.second;
        if (!want_uuid.empty() && want_uuid != lim.uuid) { continue; }
        if (!want_tag.empty() && want_tag != lim.tag) { continue; }

        ClassAd ad;
        fill_limit_ad(lim, ad);
        putClassAd(sock, ad, PUT_CLASSAD_NO_PRIVATE);
        sock->end_of_message();
    }

    ClassAd last;
    last.Assign("Last", true);
    putClassAd(sock, last, PUT_CLASSAD_NO_PRIVATE);
    sock->end_of_message();
    return 0;
}

bool StartupLimitsAllowJob(JobQueueJob *job, ClassAd *match_ad, const char *user, const char *pool, std::unordered_set<std::string> *blocked_limits, bool record_stats)
{
    (void)user;
    (void)pool;
    if (StartupLimits.empty() || !job) {
        return true;
    }
    // Cleanup runs via periodic timer; avoid inline sweeps.
    time_t now_wall = time(nullptr);
    auto now = std::chrono::steady_clock::now();

    bool allowed = true;
    for (auto &kv : StartupLimits) {
        StartupLimit &lim = kv.second;
        if (lim.expires_at && lim.expires_at <= now_wall) { continue; }
        if (!startup_limit_matches(lim, job, match_ad)) {
            continue;
        }

        dprintf(D_FULLDEBUG | D_MATCH,
                "StartupLimit active tag=%s uuid=%s tokens=%.3f burst=%.3f cap=%.3f window=%.3f max_burst_cost=%.3f\n",
                lim.tag.c_str(), lim.uuid.c_str(), lim.rate.tokens, lim.rate.burst,
                lim.rate.capacity, lim.rate.window.count(), lim.rate.max_burst_cost);

        double cost = 1.0;
        if (lim.cost_expr) {
            if (!eval_expr_number(lim.cost_expr.get(), job, match_ad, cost)) {
                if (!lim.cost_expr_warned) {
                    dprintf(D_ALWAYS | D_MATCH,
                            "StartupLimit cost expression evaluation failed tag=%s uuid=%s expr=%s; defaulting cost to 1.0 (future failures suppressed)\n",
                            lim.tag.c_str(), lim.uuid.c_str(), lim.cost_expr_source.c_str());
                    lim.cost_expr_warned = true;
                }
                cost = 1.0;
            }
        }
        cost = std::max(0.0, cost);
        if (std::abs(cost) < std::numeric_limits<double>::epsilon()) { continue; }

        if (!lim.rate.allow(now, cost)) {
            if (IsDebugLevel(D_FULLDEBUG)) {
                double available = lim.rate.tokens + lim.rate.burst;
                dprintf(D_FULLDEBUG | D_MATCH,
                        "StartupLimit block tag=%s uuid=%s cost=%.3f available=%.3f cap=%.3f burst=%.3f max_burst_cost=%.3f\n",
                        lim.tag.c_str(), lim.uuid.c_str(), cost, available,
                        lim.rate.capacity, lim.rate.burst, lim.rate.max_burst_cost);
            }
            if (record_stats) { lim.jobs_skipped++; }
            allowed = false;
            if (blocked_limits) { blocked_limits->insert(lim.uuid); }
        }
    }
    return allowed;
}

void StartupLimitsRecordIgnoredMatches(const std::unordered_set<std::string> &blocked_limits, const char *user, const char *pool)
{
    if (blocked_limits.empty()) { return; }
    time_t now_wall = time(nullptr);
    std::string key = make_source_key(user, pool);
    for (const auto &uuid : blocked_limits) {
        auto lim = find_limit_by_uuid(uuid);
        if (!lim) { continue; }
        lim->matches_ignored++;
        lim->last_ignored = now_wall;
        lim->ignored_sources[key] = now_wall;
    }
}

static void append_requirement(ClassAd &request_ad, const std::string &expr)
{
    std::string req_str;
    ExprTree *req = request_ad.Lookup(ATTR_REQUIREMENTS);
    if (req) {
        ExprTreeToString(SkipExprParens(req), req_str);
        req_str = "(" + req_str + ") && (" + expr + ")";
    } else {
        req_str = expr;
    }
    request_ad.InsertViaCache(ATTR_REQUIREMENTS, req_str);
}

void StartupLimitsAdjustRequest(JobQueueJob *job, ClassAd &request_ad, int *match_max, const char *user, const char *pool)
{
    if (StartupLimits.empty() || !job) { return; }
    time_t now_wall = time(nullptr);
    auto now = std::chrono::steady_clock::now();
    std::string key = make_source_key(user, pool);

    for (auto &kv : StartupLimits) {
        StartupLimit &lim = kv.second;
        if (lim.expires_at && lim.expires_at <= now_wall) { continue; }

        auto it = lim.ignored_sources.find(key);
        if (it == lim.ignored_sources.end()) { continue; }
        if ((now_wall - it->second) > StartupLimitBanWindow) { continue; }

        bool matches = true;
        if (lim.job_only && lim.expr) { eval_expr_bool(lim.expr.get(), job, nullptr, matches); }
        if (!matches) { continue; }

        if (lim.job_only) {
            if (match_max) {
                int cap = lim.rate.capacityIn(std::chrono::duration<double>(StartupLimitLookahead), now);
                *match_max = MIN(*match_max, cap);
            }
        } else {
            if (!lim.expr_source.empty()) {
                bool append = true;
                std::string expr_str = lim.expr_source;
                classad::Value flat_val;
                classad::ExprTree *flat_expr = nullptr;

                // Flatten against the resource request; if the expression is proven false, skip adding it.
                if (lim.expr && request_ad.Flatten(lim.expr.get(), flat_val, flat_expr)) {
                    if (flat_expr) {
                        ExprTreeToString(flat_expr, expr_str);
                        delete flat_expr;
                    } else {
                        bool bool_val = true;
                        if (flat_val.IsBooleanValue(bool_val) && !bool_val) {
                            append = false;
                        } else {
                            classad::ClassAdUnParser unparser;
                            unparser.Unparse(expr_str, flat_val);
                        }
                    }
                }

                if (append) {
                    append_requirement(request_ad, std::string("!(") + expr_str + ")");
                }
            }
        }
    }
}

int HandleCreateStartupLimitCommand(int, Stream *stream)
{
    ReliSock *sock = dynamic_cast<ReliSock *>(stream);
    if (!sock) { return FALSE; }

    ClassAd request, reply;
    sock->decode();
    if (!getClassAd(sock, request) || !sock->end_of_message()) {
        dprintf(D_ALWAYS, "CREATE_STARTUP_LIMIT failed to receive request\n");
        return FALSE;
    }

    QmgmtHandleCreateStartupLimit(request, reply);

    sock->encode();
    if (!putClassAd(sock, reply, PUT_CLASSAD_NO_PRIVATE) || !sock->end_of_message()) {
        dprintf(D_ALWAYS, "CREATE_STARTUP_LIMIT failed to send reply\n");
        return FALSE;
    }
    return TRUE;
}

int HandleQueryStartupLimitsCommand(int, Stream *stream)
{
    ReliSock *sock = dynamic_cast<ReliSock *>(stream);
    if (!sock) { return FALSE; }

    ClassAd request;
    sock->decode();
    if (!getClassAd(sock, request) || !sock->end_of_message()) {
        dprintf(D_ALWAYS, "QUERY_STARTUP_LIMITS failed to receive request\n");
        return FALSE;
    }

    QmgmtHandleQueryStartupLimits(request, sock);
    return TRUE;
}
