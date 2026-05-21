# Plan: Add Prometheus Export Support via new condor_metricd binary

## Implementation Discipline

- **Surgical edits only.** Make the smallest possible change that achieves each goal. No reformatting, no refactoring, no style cleanup, no moving code unless the move is the feature itself.
- **No unsolicited changes.** Preserve existing indentation, brace style, comment style, and whitespace exactly. The git diff must be easy for a human reviewer to scan.
- **Don't reorder existing code** unless the reorder is the feature.
- **Don't rename existing identifiers** (variables, classes, members) unless the rename is the feature. `GangliaD` keeps its class name; the existing `Metric` members keep theirs.
- **Existing condor_gangliad behavior must be 100% preserved.** Any admin currently using `condor_gangliad` with their existing `GANGLIAD_*` config should see zero change. We achieve this by installing TWO binaries from the same source and dispatching by `argv[0]`.

## Git Commits

Four commits on the current branch (`V25_X-HTCONDOR-3374-prometheus`). Every commit message must contain `HTCONDOR-3374`. **No `git push`**, **no `git commit --amend`**, **never skip pre-commit hooks**.

| # | Commit scope | Primary files |
|---|---|---|
| 1 | Install `condor_metricd` as a `clone_install` of `condor_gangliad`; add `argv[0]`-based mode dispatch; in metricd mode use new `METRICD_*` engine knobs and `GANGLIA_*` (renamed) backend-specific knobs. **NO Prometheus yet, NO architecture changes.** Gangliad mode 100% unchanged. | `src/gangliad/*.cpp,*.h`, `CMakeLists.txt`, file rename |
| 2 | Add Prometheus backend & multi-backend architecture: `MetricD`, `PrometheusD`, `PrometheusMetric`; new keywords `ExportMetric`/`PrometheusLabels`/`Counter`; new fields `Metric::export_systems`/`prometheus_labels`/`timestamp`; default labels; timestamp output. Only active in metricd mode. | `src/gangliad/` new + edited source |
| 3 | Documentation updates | `docs/admin-manual/configuration/gangliad.rst` (+ related) |
| 4 | Ornithology regression test | `src/condor_tests/test_prometheus_metrics.py` |

After each commit: `git status` shows clean, `git log -1 --oneline` shows `HTCONDOR-3374`.

---

## Context

`condor_gangliad` currently collects HTCondor metrics from the collector and publishes them to Ganglia. Goals:
1. Add a Prometheus text-format file export backend
2. Introduce `condor_metricd` as a new binary alongside `condor_gangliad` (both installed via `clone_install`, both built from the same source); `argv[0]` selects mode
3. In `condor_metricd` mode: single collector query dispatched to multiple backends (Ganglia + Prometheus, easily extensible)
4. In `condor_gangliad` mode: legacy behavior 100% unchanged

---

## Architecture (final state after all commits)

### Two binaries from one source — `clone_install` pattern (modeled on `condor_rm`)

`src/gangliad/CMakeLists.txt` builds `condor_metricd` and uses `clone_install()` to also install it as `condor_gangliad`. The daemon `main()` checks `argv[0]` to set a global `g_legacy_gangliad_mode` flag and the subsystem name. The dprintf subsystem then automatically resolves `GANGLIAD_LOG` (gangliad mode) or `METRICD_LOG` (metricd mode) without any custom fallback.

```
condor_metricd binary  ──── installed twice ────►  /usr/sbin/condor_metricd
src/gangliad/*.cpp                                 /usr/sbin/condor_gangliad   (clone_install)
                                                    │
                            main(argv) inspects argv[0]:
                              ├── "condor_gangliad" → legacy mode (g_legacy_gangliad_mode=true,  subsys=GANGLIAD)
                              └── "condor_metricd"  → modern mode (g_legacy_gangliad_mode=false, subsys=METRICD)
```

### Class layout (final, after commit 2)

```
StatsD          (engine: timer, collector query, metric eval machinery)
  ├── MetricD    (modern-mode top-level; owns GangliaD + PrometheusD as members)
  ├── GangliaD   (always exists; in legacy mode is top-level, in modern mode is a backend)
  └── PrometheusD (modern-mode-only backend)

Metric
  ├── GangliaMetric   (existing; adds gangliaMetricType()/gangliaSlope())
  └── PrometheusMetric (new; adds prometheusType())
```

### Mode-specific behavior (final state)

| Aspect | Legacy mode (`condor_gangliad`) | Modern mode (`condor_metricd`) |
|---|---|---|
| Subsystem name | `GANGLIAD` | `METRICD` |
| Log file | `GANGLIAD_LOG` | `METRICD_LOG` |
| Engine knobs | `GANGLIAD_INTERVAL`, `GANGLIAD_VERBOSITY`, etc. (unchanged) | `METRICD_INTERVAL`, `METRICD_VERBOSITY`, etc. (no fallback to GANGLIAD_*) |
| Ganglia defaults | `GANGLIAD_DEFAULT_CLUSTER/MACHINE/IP` (unchanged) | `GANGLIA_DEFAULT_CLUSTER/MACHINE/IP` (no fallback) |
| Ganglia reset knobs | `GANGLIAD_WANT_RESET_METRICS`, `GANGLIAD_RESET_METRICS_FILE` (unchanged) | `GANGLIA_WANT_RESET_METRICS` (default true), `GANGLIA_RESET_METRICS_FILE` |
| Top-level class | `GangliaD` directly (unchanged code path) | `MetricD` |
| Exports | Ganglia only (unchanged) | Ganglia + Prometheus (per `ExportMetric` setting) |
| Prometheus knobs | n/a | `PROMETHEUS_METRICS_FILE`, `PROMETHEUS_METRICS_INCLUDE_TIMESTAMP`, `PROMETHEUS_DEFAULT_LABELS`, `PROMETHEUS_WANT_RESET_METRICS`, `PROMETHEUS_RESET_METRICS_FILE` |

**Crucially:** there is **no synonym/fallback knob logic anywhere.** Each mode reads its own knob names. This keeps the diff small and the behavior unambiguous.

### Modern-mode publication flow (single cycle)

1. `StatsD::publishMetrics()` runs on `MetricD`'s timer (inherited from `StatsD`):
   - Heartbeat check → `MetricD::sendHeartbeats()` (delegates to `m_ganglia`)
   - `MetricD::initializeHostList()` (delegates to `m_ganglia`)
   - **One** call to inherited `getDaemonAds()` → `daemon_ads` (uses `MetricD`'s own `m_target_types`)
   - `this->publishMetricsFromAds(daemon_ads)` — virtual; `MetricD` overrides to dispatch:
     - `m_ganglia.publishMetricsFromAds(daemon_ads)`
     - `m_prometheus.publishMetricsFromAds(daemon_ads)`
   - `MetricD::sendHeartbeats()` (delegates to `m_ganglia`)
   - `cleanupOldPreviousValues()` (inherited)
   - `this->postPublishMetrics()` — virtual; `MetricD` overrides to call `m_prometheus.postPublishMetrics()` → writes Prometheus file

2. `StatsD::publishMetricsFromAds(daemon_ads)` (new virtual; default impl used by each backend):
   - `clearAggregateMetrics()`
   - `mapDaemonIPs(daemon_ads)`
   - `determineExecuteNodes(daemon_ads)` if applicable
   - For each `daemon_ad`: `publishDaemonMetrics(daemon_ad)` iterates own `m_metrics`, calls `this->newMetric()` (→ `GangliaMetric*` or `PrometheusMetric*`), then `metric->evaluateDaemonAd(…, this)` which polymorphically dispatches to `this->publishMetric()` or `this->addToAggregateValue()`
   - `publishAggregateMetrics()`

**Correctness:** Because `GangliaD::publishDaemonMetrics()` (inherited) always calls `this->newMetric()` (→ `GangliaMetric*`), `GangliaD::publishMetric()`'s existing `static_cast<GangliaMetric const *>` remains valid — no refactoring needed.

**Note on MetricD's inherited `m_metrics`:** `MetricD` parses metric definitions to populate `m_target_types` for the collector query, but its `m_metrics` list is never iterated for publication (`publishMetricsFromAds` is overridden). 3-way parse at startup is harmless.

**Timer suppression for backends:** Add a `bool register_timer = true` parameter to `StatsD::initAndReconfig()`. `MetricD` passes `true`. Backends pass `false`. (Defaults preserve existing callers — only `MetricD`/backends pass it explicitly.)

### Legacy-mode flow (commit 1+2 onward, behaviorally identical to today)

1. `main()` sees `argv[0]` ≈ `"condor_gangliad"` → sets `g_legacy_gangliad_mode = true`, subsystem `GANGLIAD`
2. `main_init()`: `gangliad = new GangliaD(); gangliad->initAndReconfig();`
3. `GangliaD::initAndReconfig()` reads `GANGLIAD_*` knobs (unchanged), calls `StatsD::initAndReconfig("GANGLIAD")` (unchanged), runs as today.

---

## New Metric-Definition Keywords (modern-mode-only, but parsed harmlessly in legacy mode)

| Keyword | Type | Description |
|---|---|---|
| `ExportMetric` | string expression | Comma-separated backend names: `"ganglia"`, `"prometheus"`, `"ganglia, prometheus"`. Empty/omitted = all enabled backends. Pool-wide default via `METRICD_DEFAULT_EXPORT_METRIC`. In legacy mode the field is parsed but never read (GangliaD's `publishMetric()` check passes when `export_systems` is empty). |
| `PrometheusLabels` | ClassAd string expression | Evaluates to a label set, e.g. `strcat("machine=\"",Machine,"\"")`. Merged with `PROMETHEUS_DEFAULT_LABELS`; per-metric overrides same-key defaults. Ignored in legacy mode. |
| `Counter` | boolean | Synonym for `Derivative`. Applies in both modes (harmless additive). |

---

## Timestamps in Prometheus Output

- Add `int64_t timestamp{0}` field to `Metric` base class.
- In `Metric::evaluateDaemonAd()`, populate via `daemon_ad.EvaluateAttrInt(ATTR_LAST_HEARD_FROM, timestamp)`.
- Config knob `PROMETHEUS_METRICS_INCLUDE_TIMESTAMP` (default false). When true, `PrometheusD::writeMetricsFile()` appends ` <timestamp * 1000>` to each sample line when `timestamp != 0`.

---

## Modern-Mode Config Knobs

### Engine-level (read in `StatsD::initAndReconfig("METRICD", …)`)

| Knob | Default |
|---|---|
| `METRICD_INTERVAL` | 60 |
| `METRICD_VERBOSITY` | 0 |
| `METRICD_REQUIREMENTS` | *(empty)* |
| `METRICD_PER_EXECUTE_NODE_METRICS` | true |
| `METRICD_WANT_PROJECTION` | false |
| `METRICD_METRICS_CONFIG_DIR` | `/etc/condor/ganglia.d/` |
| `METRICD_DEFAULT_EXPORT_METRIC` | *(empty = all)* |
| `METRICD_LOG` | `$(LOG)/MetricdLog` |

### Ganglia-specific (read in `GangliaD::initAndReconfig()`)

Unchanged: `GANGLIA_CONFIG`, `GANGLIA_GMETRIC`, `GANGLIA_LIB`, `GANGLIA_LIB_PATH`, `GANGLIA_LIB64_PATH`, `GANGLIA_GSTAT_COMMAND`, `GANGLIA_SEND_DATA_FOR_ALL_HOSTS`, `GANGLIAD_MIN_METRIC_LIFETIME`.

Renamed (modern-mode names; legacy-mode keeps old names):

| Modern mode | Legacy mode (unchanged) |
|---|---|
| `GANGLIA_DEFAULT_CLUSTER` | `GANGLIAD_DEFAULT_CLUSTER` |
| `GANGLIA_DEFAULT_MACHINE` | `GANGLIAD_DEFAULT_MACHINE` |
| `GANGLIA_DEFAULT_IP` | `GANGLIAD_DEFAULT_IP` |
| `GANGLIA_WANT_RESET_METRICS` (default **true**) | `GANGLIAD_WANT_RESET_METRICS` |
| `GANGLIA_RESET_METRICS_FILE` | `GANGLIAD_RESET_METRICS_FILE` |

In code (`gangliad.cpp`), each rename is a one-line picker: `const char *knob = g_legacy_gangliad_mode ? "GANGLIAD_DEFAULT_CLUSTER" : "GANGLIA_DEFAULT_CLUSTER";`

### Prometheus-specific (read in `PrometheusD::initAndReconfig()`)

| Knob | Default | Description |
|---|---|---|
| `PROMETHEUS_METRICS_FILE` | *(empty — disabled)* | Output file path; backend disabled if empty. |
| `PROMETHEUS_METRICS_INCLUDE_TIMESTAMP` | `false` | Append ms timestamp to each sample line. |
| `PROMETHEUS_DEFAULT_LABELS` | *(empty)* | Default label set, e.g. `pool="mypoolname"`. |
| `PROMETHEUS_WANT_RESET_METRICS` | **false** | Per-backend reset-metrics flag. |
| `PROMETHEUS_RESET_METRICS_FILE` | spool-based | Used only if above is true. |

---

## Prometheus Label Merging

`PrometheusD` stores `m_default_labels` parsed from `PROMETHEUS_DEFAULT_LABELS` as `std::map<std::string,std::string>`. For each sample, `buildEffectiveLabels(metric)`:
1. Starts from `m_default_labels`
2. Parses `metric.prometheus_labels` and overlays (per-metric overrides same-key defaults)
3. Re-serializes sorted as `key="value",…` wrapped in `{…}`

Private helpers `parseLabels(string) → map` and `serializeLabels(map) → string` in `prometheusd.cpp`.

---

## Prometheus Metric Naming & Types

- **Name validation:** Must match `[a-zA-Z_:][a-zA-Z0-9_:]*`. If invalid: `dprintf(D_ERROR, …)` and skip.
- **Unit suffix** (best-effort, D_FULLDEBUG): if `Units` is `bytes`/`seconds`/`milliseconds`/`microseconds`/`percent`/`%`, append `_bytes`/`_seconds`/`_milliseconds`/`_microseconds`/`_ratio` when not already present.
- **Counter suffix:** if `derivative && aggregate==NO_AGGREGATE` and name doesn't end with `_total`, append `_total` (after unit suffix).
- **Prometheus type:** `"counter"` if `derivative && aggregate==NO_AGGREGATE`; else `"gauge"`.
- **HELP text:** `metric.desc` + (if `units` non-empty) `" (" + metric.units + ")"`.

---

## Commit 1 — Implementation Steps

**Goal:** `condor_metricd` is installed alongside `condor_gangliad` (same binary, clone_install). `argv[0]` dispatch picks subsystem name and mode flag. In metricd mode, the engine knobs are read as `METRICD_*` and the Ganglia defaults/reset-metrics knobs are read as `GANGLIA_*`. In gangliad mode, **everything is identical to today**. NO Prometheus code yet, NO MetricD class yet — `GangliaD` is still the top-level class in both modes.

### Steps

- [x] **1.1** `git mv src/gangliad/gangliad_main.cpp src/gangliad/metricd_main.cpp`
- [x] **1.2** In `src/gangliad/metricd_main.cpp`:
  - Add a header-visible global declaration: `extern bool g_legacy_gangliad_mode;` is best placed in `gangliad.h` (or a small new shared header) — but keep the actual definition in `metricd_main.cpp`. Choice for minimal diff: put `extern bool g_legacy_gangliad_mode;` in `statsd.h` near the top, define it in `metricd_main.cpp`.
  - In `main()`, before `set_mySubSystem`, parse `argv[0]`'s basename (use `condor_basename()` from `condor_utils` if available, or `strrchr(argv[0], DIR_DELIM_CHAR)`); if the basename contains `"gangliad"`, set `g_legacy_gangliad_mode = true` and call `set_mySubSystem("GANGLIAD", true, SUBSYSTEM_TYPE_DAEMON);` otherwise `g_legacy_gangliad_mode = false` and `set_mySubSystem("METRICD", true, SUBSYSTEM_TYPE_DAEMON);`.
  - Leave the rest (`dc_main_init = main_init;` etc.) unchanged.
- [x] **1.3** In `src/gangliad/statsd.h`: add `extern bool g_legacy_gangliad_mode;` near the top (after the include guard, before class declarations).
- [x] **1.4** In `src/gangliad/CMakeLists.txt`:
  - Update the source list filename: `gangliad_main.cpp` → `metricd_main.cpp`.
  - Rename the binary: `condor_exe( condor_metricd "${GANGLIAD}" ${C_LIBEXEC} "${CONDOR_LIBS}" OFF )`.
  - Add: `clone_install( condor_metricd "${C_LIBEXEC}" condor_gangliad "${C_LIBEXEC}" )` immediately after the `condor_exe(...)` line.
  - The `install( FILES ganglia_default_metrics ... )` line stays as-is.
- [x] **1.5** In `src/gangliad/gangliad.cpp`, function `GangliaD::initAndReconfig()`:
  - Change the `StatsD::initAndReconfig("GANGLIAD")` call to: `StatsD::initAndReconfig(g_legacy_gangliad_mode ? "GANGLIAD" : "METRICD");`. This is the ONLY change to this line.
- [x] **1.6** In `src/gangliad/statsd.cpp`, function `StatsD::initAndReconfig()`:
  - **Delete** the three blocks that handle `GANGLIAD_DEFAULT_CLUSTER`, `GANGLIAD_DEFAULT_MACHINE`, `GANGLIAD_DEFAULT_IP` (the param/parser/Insert sequences). These move to `GangliaD::initAndReconfig()`.
  - **Delete** the block that handles `<service>_WANT_RESET_METRICS` / `<service>_RESET_METRICS_FILE` (sets `m_reset_metrics_filename`). This moves to `GangliaD::initAndReconfig()`.
- [x] **1.7** In `src/gangliad/gangliad.cpp`, function `GangliaD::initAndReconfig()` (after the `StatsD::initAndReconfig(...)` call):
  - **Add** the three default-expression blocks (cluster/machine/ip), each using a knob-name picker:
    ```cpp
    const char *cluster_knob = g_legacy_gangliad_mode ? "GANGLIAD_DEFAULT_CLUSTER" : "GANGLIA_DEFAULT_CLUSTER";
    std::string default_cluster_expr;
    param(default_cluster_expr, cluster_knob);
    if (!default_cluster_expr.empty()) { /* parse and Insert into m_default_metric_ad as before */ }
    ```
  - Same pattern for `MACHINE` and `IP`.
  - **Add** the reset-metrics block, also with a knob-name picker:
    ```cpp
    m_reset_metrics_filename.clear();
    const char *want_reset_knob = g_legacy_gangliad_mode ? "GANGLIAD_WANT_RESET_METRICS" : "GANGLIA_WANT_RESET_METRICS";
    const char *reset_file_knob = g_legacy_gangliad_mode ? "GANGLIAD_RESET_METRICS_FILE" : "GANGLIA_RESET_METRICS_FILE";
    bool want_reset_default = !g_legacy_gangliad_mode; // GANGLIA defaults to true; preserve GANGLIAD default behavior
    if (param_boolean(want_reset_knob, want_reset_default)) {
        /* existing logic that sets m_reset_metrics_filename, suffix handling, etc. */
    }
    ```
  - **NOTE:** verify the legacy default for `GANGLIAD_WANT_RESET_METRICS` was `false` in the original code; if so, `want_reset_default` should be `false` for legacy mode and `true` for modern mode (i.e. `!g_legacy_gangliad_mode` flips correctly only if legacy default was false). Cross-check against the deleted code in step 1.6 to preserve legacy behavior exactly.
- [x] **1.8** Build: from build dir, `ninja condor_metricd`. Confirm zero new warnings.
- [x] **1.9** Verify install paths: build the install target and confirm both `condor_metricd` and `condor_gangliad` appear in the install tree (e.g., `find install_dir -name 'condor_*gangliad*' -o -name 'condor_*metricd*'`).
- [ ] **1.10** Legacy smoke test: invoke binary as `condor_gangliad` with existing `GANGLIAD_*` config; verify it logs to `GANGLIAD_LOG`, uses `GANGLIAD_INTERVAL`, reads `GANGLIAD_DEFAULT_CLUSTER` etc. — identical to before.
- [ ] **1.11** Modern smoke test: invoke binary as `condor_metricd` with `METRICD_INTERVAL` and `GANGLIA_DEFAULT_CLUSTER` configured (no `GANGLIAD_*` defaults); verify the daemon comes up, logs to `METRICD_LOG`, uses the new knob values.
- [ ] **1.12** `git add` only the touched files. Commit using HEREDOC:
  ```
  git commit -m "$(cat <<'EOF'
  HTCONDOR-3374 Install condor_metricd as clone of condor_gangliad

  ... summary of behavior ...

  Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
  EOF
  )"
  ```
- [ ] **1.13** `git status` clean; `git log -1 --oneline` shows commit with `HTCONDOR-3374`.

---

## Commit 2 — Implementation Steps (Prometheus backend + multi-backend architecture)

**Goal:** In modern (metricd) mode, instantiate `MetricD` (multi-backend orchestrator) instead of `GangliaD` directly. Add `PrometheusD`/`PrometheusMetric`. Add new metric keywords/fields. Legacy mode (`condor_gangliad`) unaffected.

### Steps

- [x] **2.1** In `src/gangliad/statsd.h`, add to `class Metric` (adjacent to `derivative`):
  - `std::vector<std::string> export_systems;`
  - `std::string prometheus_labels;`
  - `int64_t timestamp{0};`
- [x] **2.2** In `src/gangliad/statsd.h`, add to `class StatsD`:
  - Public virtual: `virtual void publishMetricsFromAds(std::vector<ClassAd> &daemon_ads);`
  - Protected virtual (no-op default): `virtual void postPublishMetrics() {}`
  - Change signature: `virtual void initAndReconfig(char const *service_name, bool register_timer = true);` (default preserves existing callers)
  - Public accessor: `const std::vector<std::string> &getTargetTypes() const { return m_target_types; }`
- [x] **2.3** In `src/gangliad/statsd.cpp`, add `#define ATTR_EXPORT_METRIC "ExportMetric"`, `#define ATTR_PROMETHEUS_LABELS "PrometheusLabels"`, `#define ATTR_COUNTER "Counter"` adjacent to existing `ATTR_*` defines.
- [x] **2.4** In `src/gangliad/statsd.cpp`, `Metric::Metric()`: append `prometheus_labels("")`, `timestamp(0)` to initializer list (adjacent to existing initializers).
- [x] **2.5** In `src/gangliad/statsd.cpp`, `Metric::evaluateDaemonAd()`:
  - After `metric_ad.EvaluateAttrBool(ATTR_DERIVATIVE,derivative);`: `if (!derivative) metric_ad.EvaluateAttrBool(ATTR_COUNTER, derivative);`
  - After the `evaluateOptionalString(ATTR_UNITS, …)` call:
    ```cpp
    std::string export_str;
    if (!evaluateOptionalString(ATTR_EXPORT_METRIC, export_str, metric_ad, daemon_ad, regex_groups)) return false;
    export_systems = split(export_str);
    if (!evaluateOptionalString(ATTR_PROMETHEUS_LABELS, prometheus_labels, metric_ad, daemon_ad, regex_groups)) return false;
    ```
  - Before the publish/aggregate dispatch at the end: `daemon_ad.EvaluateAttrInt(ATTR_LAST_HEARD_FROM, timestamp);` (ensure `condor_attributes.h` is included — likely already)
- [x] **2.6** In `src/gangliad/statsd.cpp`, `StatsD::initAndReconfig()`:
  - Wrap the existing timer registration block in `if (register_timer)`.
  - Add a block (only when `!g_legacy_gangliad_mode`) parsing `METRICD_DEFAULT_EXPORT_METRIC`: read string; if non-empty, build a classad string-literal expression and `m_default_metric_ad.Insert(ATTR_EXPORT_METRIC, expr)`.
- [x] **2.7** In `src/gangliad/statsd.cpp`, `StatsD::publishMetrics()`:
  - **Extract** the block from `clearAggregateMetrics()` through `publishAggregateMetrics()` (inclusive) into a new function `StatsD::publishMetricsFromAds(std::vector<ClassAd> &daemon_ads)`.
  - In `publishMetrics()`, replace the extracted block with `publishMetricsFromAds(daemon_ads);`.
  - Add `postPublishMetrics();` immediately before the timing-skip check at the end.
- [x] **2.8** In `src/gangliad/gangliad.cpp`, `GangliaD::initAndReconfig()`:
  - In modern mode only, change the `StatsD::initAndReconfig(...)` call to pass `register_timer=false` when this `GangliaD` is being used as a backend (i.e., owned by a `MetricD`). Simplest mechanism: have `GangliaD` learn it's a backend via a constructor flag. Add to `GangliaD`: `GangliaD(bool as_backend = false);` and store `m_as_backend`. In `initAndReconfig`: pass `register_timer = !m_as_backend`. (Legacy mode constructs `GangliaD()` directly → `m_as_backend=false` → register_timer true → unchanged behavior.)
- [x] **2.9** In `src/gangliad/gangliad.h`: add the `m_as_backend` field and the constructor parameter (minimal diff).
- [x] **2.10** In `src/gangliad/gangliad.cpp`, `GangliaD::publishMetric()`: at the top, add:
  ```cpp
  if (!metric.export_systems.empty() && !contains_anycase(metric.export_systems, "ganglia")) {
      return;
  }
  ```
- [x] **2.11** Create `src/gangliad/prometheusd.h`:
  ```cpp
  class PrometheusMetric : public Metric {
  public:
      std::string prometheusType() const;
  };
  class PrometheusD : public StatsD {
  public:
      PrometheusD();
      virtual void initAndReconfig(const char *unused = nullptr);
      virtual Metric *newMetric(Metric const *copy_me = NULL);
      virtual void publishMetric(Metric const &metric);
      virtual void postPublishMetrics();
      virtual void initializeHostList() {}
      virtual void sendHeartbeats() {}
  private:
      std::string m_output_file;
      bool m_include_timestamp{false};
      std::map<std::string,std::string> m_default_labels;
      struct PendingMetric { std::string name, labels, value, help, prom_type; int64_t timestamp{0}; };
      std::vector<PendingMetric> m_pending;
      std::string buildPrometheusName(const Metric &m) const;
      std::string buildPrometheusHelp(const Metric &m) const;
      std::string buildEffectiveLabels(const Metric &m) const;
      void writeMetricsFile();
      static std::map<std::string,std::string> parseLabels(const std::string &s);
      static std::string serializeLabels(const std::map<std::string,std::string> &m);
  };
  ```
- [x] **2.12** Create `src/gangliad/prometheusd.cpp` implementing:
  - `PrometheusMetric::prometheusType()` → `"counter"` if `derivative && aggregate==NO_AGGREGATE`, else `"gauge"`.
  - `PrometheusD::PrometheusD()` (default).
  - `PrometheusD::initAndReconfig()`: calls `StatsD::initAndReconfig("METRICD", false)`; reads `PROMETHEUS_METRICS_FILE` → `m_output_file`; `PROMETHEUS_METRICS_INCLUDE_TIMESTAMP` → `m_include_timestamp`; `PROMETHEUS_DEFAULT_LABELS` → `m_default_labels = parseLabels(...)`; `PROMETHEUS_WANT_RESET_METRICS` (default false) / `PROMETHEUS_RESET_METRICS_FILE` → `m_reset_metrics_filename`.
  - `PrometheusD::newMetric()`: returns `new PrometheusMetric(*static_cast<const PrometheusMetric*>(copy_me))` if `copy_me`, else `new PrometheusMetric()`.
  - `PrometheusD::publishMetric()`: short-circuit if `m_output_file.empty()`; check `metric.export_systems` (skip if non-empty and lacks "prometheus"); build name via `buildPrometheusName(metric)`; validate against regex (use `[a-zA-Z_:][a-zA-Z0-9_:]*` rule); on invalid → `dprintf(D_ERROR, ...)` and return; build labels via `buildEffectiveLabels(metric)`; get value string via `metric.getValueString(value)`; create `PendingMetric` and append to `m_pending`.
  - `PrometheusD::postPublishMetrics()`: `writeMetricsFile(); m_pending.clear();`
  - `PrometheusD::writeMetricsFile()`: if `m_output_file` empty → return; open `<m_output_file>.tmp` via `safe_fopen_wrapper()`; group `m_pending` by name (preserve first-occurrence ordering); for each group write `# HELP name help\n# TYPE name type\n` then each sample as `<name><labels> <value>[ <timestamp_ms>]\n`; `fclose`; `rename(tmp, m_output_file.c_str())`. Log errors via `dprintf(D_ERROR, ...)`.
  - `buildPrometheusName(m)`: start from `m.name`; apply unit suffix table; apply counter suffix.
  - `buildPrometheusHelp(m)`: `m.desc` + (if `!m.units.empty()`) `" (" + m.units + ")"`.
  - `buildEffectiveLabels(m)`: copy `m_default_labels`; `parseLabels(m.prometheus_labels)` overlay; `serializeLabels(merged)`.
  - `parseLabels`: simple parser for `key="value",key2="value2"` (handles backslash-escaped quotes within values).
  - `serializeLabels`: sort by key; output `key="value",…` wrapped in `{…}` (empty map → empty string, not `{}`).
- [x] **2.13** Create `src/gangliad/metricd.h`:
  ```cpp
  #ifndef __METRICD_H__
  #define __METRICD_H__
  #include "statsd.h"
  #include "gangliad.h"
  #include "prometheusd.h"
  class MetricD : public StatsD {
  public:
      MetricD();
      virtual void initAndReconfig(const char *unused = nullptr);
      virtual Metric *newMetric(Metric const *copy_me = NULL);
      virtual void publishMetric(Metric const &) {}
      virtual void publishMetricsFromAds(std::vector<ClassAd> &daemon_ads);
      virtual void postPublishMetrics();
      virtual void initializeHostList();
      virtual void sendHeartbeats();
  private:
      GangliaD    m_ganglia;
      PrometheusD m_prometheus;
  };
  #endif
  ```
- [x] **2.14** Create `src/gangliad/metricd.cpp`:
  - `MetricD::MetricD() : m_ganglia(true) {}` — passes `as_backend=true` so GangliaD doesn't register its own timer.
  - `MetricD::initAndReconfig()`: `StatsD::initAndReconfig("METRICD", true);` (registers MetricD timer); `m_ganglia.initAndReconfig(); m_prometheus.initAndReconfig();`. After both, merge backends' target types into own:
    ```cpp
    std::set<std::string,caselt> all;
    for (auto &t : m_ganglia.getTargetTypes()) all.insert(t);
    for (auto &t : m_prometheus.getTargetTypes()) all.insert(t);
    m_target_types.assign(all.begin(), all.end());
    ```
  - `MetricD::newMetric()`: returns `new Metric(*copy_me)` or `new Metric()` (unused but must compile).
  - `MetricD::publishMetricsFromAds(daemon_ads)`: `m_ganglia.publishMetricsFromAds(daemon_ads); m_prometheus.publishMetricsFromAds(daemon_ads);`
  - `MetricD::postPublishMetrics()`: `m_prometheus.postPublishMetrics();`
  - `MetricD::initializeHostList()`: `m_ganglia.initializeHostList();`
  - `MetricD::sendHeartbeats()`: `m_ganglia.sendHeartbeats();`
- [x] **2.15** In `src/gangliad/metricd_main.cpp`:
  - Add `#include "metricd.h"`.
  - Add a second global `MetricD *metricd = nullptr;` next to existing `GangliaD *gangliad = nullptr;`.
  - In `main_init()`: branch on `g_legacy_gangliad_mode`. If true: existing `gangliad = new GangliaD(); gangliad->initAndReconfig();`. If false: `metricd = new MetricD(); metricd->initAndReconfig();`.
  - In `main_config()`: same branching.
  - In `Stop()`: delete whichever pointer is non-null.
- [x] **2.16** In `src/gangliad/CMakeLists.txt`: add `metricd.cpp`, `prometheusd.cpp` to the source list.
- [x] **2.17** Build: `ninja condor_metricd`. Resolve any errors. Zero new warnings.
- [ ] **2.18** Smoke test (legacy unchanged): invoke as `condor_gangliad`; verify no `*.prom` file is created even if `PROMETHEUS_METRICS_FILE` is set; verify legacy ganglia logging unchanged.
- [ ] **2.19** Smoke test (modern): invoke as `condor_metricd` with `PROMETHEUS_METRICS_FILE = /tmp/test.prom`, `GANGLIA_LIB = NOOP`, `PROMETHEUS_DEFAULT_LABELS = pool="testpool"`; after one `METRICD_INTERVAL`, verify file exists with valid Prometheus format, includes `pool="testpool"`, has `# HELP` and `# TYPE`.
- [ ] **2.20** Smoke test (ExportMetric routing): add `ExportMetric = "prometheus"` to a metric; verify only in `.prom` file, not in noop ganglia log lines (search the MetricdLog).
- [ ] **2.21** `git add` only the touched + new files. Commit with HEREDOC, `HTCONDOR-3374` in message, Co-Authored-By trailer.
- [ ] **2.22** `git status` clean; `git log -1` shows commit with `HTCONDOR-3374`.

---

## Commit 3 — Implementation Steps (Documentation)

- [x] **3.1** In `docs/admin-manual/configuration/gangliad.rst`:
  - Add a top-level note: "`condor_gangliad` is preserved as a legacy alias of `condor_metricd`; admins not adopting Prometheus need no config changes."
  - Document each `METRICD_*` engine knob as the modern name; explicitly state the `GANGLIAD_*` form is used when invoking the binary as `condor_gangliad`.
  - Document `GANGLIA_DEFAULT_CLUSTER`/`MACHINE`/`IP` for metricd mode; `GANGLIAD_DEFAULT_*` for legacy mode.
  - Document `GANGLIA_WANT_RESET_METRICS` (default true in modern mode) / `GANGLIA_RESET_METRICS_FILE`; legacy mode uses `GANGLIAD_WANT_RESET_METRICS`.
  - Add a "Prometheus Export" section: `PROMETHEUS_METRICS_FILE`, `PROMETHEUS_METRICS_INCLUDE_TIMESTAMP`, `PROMETHEUS_DEFAULT_LABELS`, `PROMETHEUS_WANT_RESET_METRICS`, `PROMETHEUS_RESET_METRICS_FILE`.
  - Document new metric-definition keywords: `ExportMetric`, `PrometheusLabels`, `Counter`.
- [x] **3.2** In `docs/admin-manual/cm-configuration.rst` Ganglia section: add a brief note that `condor_metricd` supports Prometheus export; cross-link to `gangliad.rst`.
- [x] **3.3** Add a version-history entry (find the right path under `docs/version-history/`) noting the addition of `condor_metricd` + Prometheus export + HTCONDOR-3374.
- [x] **3.4** Optionally build docs (`cd docs && make html`) to confirm no RST errors.
- [ ] **3.5** `git add docs/`. Commit with HEREDOC, `HTCONDOR-3374` in message.
- [ ] **3.6** `git status` clean.

---

## Commit 4 — Implementation Steps (Ornithology test)

- [ ] **4.1** Create `src/condor_tests/test_prometheus_metrics.py` following the pattern of `test_activation_metrics.py` / `test_bogus_collector.py`.
- [ ] **4.2** `@standup` fixture: start a Condor with `condor_metricd` in `DAEMON_LIST`, plus:
  ```python
  config = {
    "DAEMON_LIST":                       "$(DAEMON_LIST) METRICD",
    "METRICD":                           "$(LIBEXEC)/condor_metricd",
    "GANGLIA_LIB":                       "NOOP",
    "PROMETHEUS_METRICS_FILE":           str(test_dir / "metrics.prom"),
    "PROMETHEUS_METRICS_INCLUDE_TIMESTAMP": "true",
    "PROMETHEUS_DEFAULT_LABELS":         'pool="testpool"',
    "METRICD_INTERVAL":                  "10",
    "METRICD_METRICS_CONFIG_DIR":        str(test_dir / "metrics.d"),
  }
  ```
- [ ] **4.3** Use `write_file` (from `ornithology`) to create metric definition files in the config dir containing:
  - `ganglia_only_test_jobs` with `ExportMetric = "ganglia"`
  - `prometheus_only_test_jobs` with `ExportMetric = "prometheus"`, `PrometheusLabels = strcat("machine=\"",Machine,"\"")`
  - `test_bytes_transferred` with `Counter = true`, `Units = "bytes"`
  - A metric with an invalid name like `"invalid name!"` (should be skipped)
  - `both_backend_test_metric` with no `ExportMetric`
- [ ] **4.4** `@action` fixture `prom_file_contents`: poll for up to 30s; returns file contents or `None`.
- [ ] **4.5** `class TestPrometheusMetrics` with one assertion per behavior:
  - `test_file_exists`
  - `test_has_help_and_type_lines`
  - `test_ganglia_only_metric_absent`
  - `test_prometheus_only_metric_present`
  - `test_per_metric_label_present`
  - `test_default_label_present`
  - `test_counter_gets_total_suffix`
  - `test_counter_type_annotation`
  - `test_gauge_type_annotation`
  - `test_invalid_name_metric_absent`
  - `test_timestamps_present`
  - `test_both_backend_metric_present`
- [ ] **4.6** Check if `src/condor_tests/CMakeLists.txt` enumerates tests; if test discovery is by `test_*.py` glob, no CMakeLists edit needed.
- [ ] **4.7** Run locally: `pytest src/condor_tests/test_prometheus_metrics.py -v --log-cli-level INFO`. All asserts pass.
- [ ] **4.8** `git add` and commit with `HTCONDOR-3374`. `git status` clean; `git log --oneline -4` shows all four commits.

---

## Prometheus Text Format Output Example

```
# HELP htcondor_jobs_running Number of running jobs (jobs)
# TYPE htcondor_jobs_running gauge
htcondor_jobs_running{pool="mypoolname",machine="submit.wisc.edu"} 42 1716163200000
# HELP htcondor_file_transfer_bytes_total Bytes transferred (bytes)
# TYPE htcondor_file_transfer_bytes_total counter
htcondor_file_transfer_bytes_total{pool="mypoolname",machine="submit.wisc.edu"} 1234567890 1716163200000
```

(Merged `PROMETHEUS_DEFAULT_LABELS = pool="mypoolname"` + per-metric `PrometheusLabels = strcat("machine=\"",Machine,"\"")`.)

---

## Risks / Open Questions

- **Subsystem registration:** Verify "METRICD" doesn't require registration in `condor_master`'s known-subsystem list beyond `set_mySubSystem()`. If `condor_master` has a hardcoded daemon table or DAEMON validation, address that surgically in commit 1.
- **`g_legacy_gangliad_mode` linkage:** Declared `extern` in `statsd.h`, defined in `metricd_main.cpp`. Verify no link-order issues across `gangliad.cpp`, `statsd.cpp`, `prometheusd.cpp`, `metricd.cpp`.
- **Verifying legacy default for `GANGLIAD_WANT_RESET_METRICS`:** Step 1.7 cross-checks with the deleted code; the legacy default must be preserved exactly.
- **`condor_basename()` helper:** Prefer it over manual `strrchr` if available in `condor_utils` — keeps the dispatch logic clean.
- **`clone_install` destination dir:** Confirm `${C_LIBEXEC}` is the correct destination for both (matches today's install for `condor_gangliad`). If `condor_gangliad` lives elsewhere today, use that path for both clones.
