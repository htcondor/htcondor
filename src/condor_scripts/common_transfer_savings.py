#!/usr/bin/python3

#
# Quantify the transfer savings from HTCondor's common-files feature.
#
# This is a merged variant of common_transfer_savings.py that can group the
# accounting two ways:
#
#   * by cluster (the default): the right unit when a single cluster has many
#     procs/jobs, because those jobs share their common files with each other.
#     The files are staged to an execution point (EP) once and every later job
#     of the cluster that lands on that EP reuses the staged copy instead of
#     transferring it again.  This reproduces the behaviour of the original
#     common_transfer_savings.py.
#
#   * by DAG (--dag): the right unit for a DAG made of single-proc clusters
#     (one process per node).  Per cluster there is then only one job, one
#     epoch and at most one transfer, so per-cluster accounting always reports
#     ~0% savings even when the feature is saving a great deal -- because for
#     single-proc DAG nodes the sharing happens *across* clusters, not within
#     one.
#
# HTCondor already shares common-file catalogs across the clusters of a DAG.
# Per determineCIFScopeAndType() in src/condor_utils/guidance.cpp, the "scope"
# that decides whether two jobs may share a staged catalog is the job's
# DAGManJobId if it has one, and otherwise its ClusterID.  A catalog's internal
# name (makeCIFName()) is built from that scope plus the catalog's base name
# and content hash, so two single-proc nodes in the same DAG that reference a
# catalog with the same name and contents produce the *same* internal catalog
# and reuse a single staging.  --dag groups by exactly that scope, which is
# what makes the savings for single-proc DAG nodes visible.
#
# Note that cross-node sharing only helps for *named* catalogs
# (CommonInputCatalogs, which includes common container images auto-named
# "container_<image>").  The old-style anonymous CommonInputFiles gets a base
# name of "clusterID_<ClusterID>", which is unique per node and so is NOT
# shared across a DAG; the arithmetic below reflects that automatically (such
# catalogs show one transfer per node and thus no cross-node savings).
#

import sys
import argparse
from collections import defaultdict

import htcondor2 as htcondor
import classad2 as classad


def human(nbytes):
    # Small readability helper; the raw byte counts are still printed too.
    value = float(nbytes)
    for unit in ('B', 'KiB', 'MiB', 'GiB', 'TiB', 'PiB'):
        if abs(value) < 1024.0 or unit == 'PiB':
            if unit == 'B':
                return f"{int(value)} {unit}"
            return f"{value:.1f} {unit}"
        value /= 1024.0


def common_transfer_bytes(entry):
    #
    # Extract the number of bytes moved by one COMMON (transfer) epoch ad.
    #
    # The size lives in the TransferCommonStats sub-ad as one "<Protocol>SizeBytes"
    # attribute per protocol used (CedarSizeBytes, HTTPSizeBytes, ...).  When the
    # sub-ad also lists the protocols in "Protocols" we sum those; but some
    # records -- e.g. plugin-driven (non-CEDAR) transfers -- carry a stats ad
    # with no "Protocols" attribute, so we fall back to summing every
    # "*SizeBytes" attribute present.  If there's no stats ad at all, the epoch
    # ad itself carries CedarSizeBytes.
    #
    stats_ad = entry.get('TransferCommonStats')
    if stats_ad is not None:
        # Sum every "*SizeBytes" attribute in the stats ad (CedarSizeBytes plus
        # one per plugin protocol).  We iterate the ad's actual keys rather than
        # the "Protocols" list because some records -- e.g. plugin-driven
        # transfers -- omit "Protocols".
        #
        # See https://github.com/PelicanPlatform/pelican/issues/3622 for a
        # possible reason for `protocols` list to be incomplete.  It's arguably
        # an HTCondor bug as well, since it knows which protocol(s) it invoked
        # the plug-in to handle.
        total = 0
        for key in stats_ad:
            if key.endswith("SizeBytes"):
                total += int(stats_ad.get(key, 0) or 0)
        if total > 0:
            return total

    # No stats ad (or an empty one): the epoch ad itself carries the size.
    return int(entry.get('CedarSizeBytes', 0) or 0)


#
# ---------------------------------------------------------------------------
# Timing diagnostics
# ---------------------------------------------------------------------------
#
# The point of these is to answer the question the savings arithmetic above
# cannot: does common-file reuse actually make the workflow *finish sooner*,
# or does it only take load off the CDN / origin?  Those are different claims
# and the byte counts only support the second one.
#
# The metric that settles it is ActivationSetupDuration.  The shadow sets
# activation.StartTime when the startd accepts the claim (remoteresource.cpp:
# 218) and StartExecutionTime when the job body actually begins (:2148), then
# records the difference as ActivationSetupDuration (:2164).  Everything that
# stands between "we have a slot" and "the user's code is running" is inside
# that window -- common-file staging and mapping, ordinary input transfer, and
# starter setup.  It is therefore directly comparable between a run that uses
# common files and a control run that does not, which is exactly what we want.
#
# Two things worth knowing when reading the numbers:
#
#   * JobCurrentStart/FinishTransferInputDate cover only the job's *own* input
#     sandbox.  They come from the job's FileTransfer object
#     (remoteresource.cpp:2054-2057), and common files move over a separate
#     FileTransfer (the shadow's commonFTO), so common-file bytes are NOT in
#     that window.  Moving a container image into a catalog therefore shrinks
#     "input transfer" even when nothing got faster -- the cost just moved into
#     the rest of ActivationSetupDuration.  Compare setup, not input transfer.
#
#   * CompletionDate is set by the schedd, not the shadow, so it is absent or
#     stale in EPOCH ads (which are written by the shadow).  We use
#     EpochWriteDate -- inserted into every epoch record at write time
#     (job_ad_instance_recording.cpp:227) -- as the end-of-run timestamp.
#


def dur(seconds):
    # Format a duration.  Returns a fixed-ish width string so columns line up.
    if seconds is None:
        return "--"
    seconds = float(seconds)
    if seconds < 0:
        return "--"
    if seconds < 60:
        return f"{seconds:.1f}s"
    if seconds < 3600:
        return f"{seconds / 60:.1f}m"
    if seconds < 86400:
        return f"{seconds / 3600:.2f}h"
    return f"{seconds / 86400:.2f}d"


def attr_int(entry, *names):
    # First of `names` present in `entry` and integer-valued, else None.
    for name in names:
        if name not in entry:
            continue
        try:
            return int(entry[name])
        except (TypeError, ValueError):
            continue
    return None


def attr_timestamp(entry, *names):
    # As attr_int(), but rejects zero/negative values.  HTCondor leaves unset
    # date attributes at 0 rather than removing them, and a 0 here would turn
    # into a ~56-year interval.
    value = attr_int(entry, *names)
    if value is None or value <= 0:
        return None
    return value


def span(entry, start_names, end_names):
    # Duration between two timestamp attributes, or None if either is missing.
    # Negative spans are treated as missing: they mean the two attributes came
    # from different runs (e.g. an attribute the shadow never refreshed).
    start = attr_timestamp(entry, *start_names)
    end = attr_timestamp(entry, *end_names)
    if start is None or end is None:
        return None
    delta = end - start
    return delta if delta >= 0 else None


def summarize(values):
    # min / median / mean / p90 / max over a list of numbers.
    values = [v for v in values if v is not None]
    if not values:
        return None
    ordered = sorted(values)
    n = len(ordered)

    def percentile(p):
        # Nearest-rank.  Exact enough for the sample sizes involved, and it
        # never interpolates a value that no job actually exhibited.
        rank = int(-(-p * n // 100))
        return ordered[max(0, min(n - 1, rank - 1))]

    return {
        'n': n,
        'min': ordered[0],
        'median': percentile(50),
        'mean': sum(ordered) / n,
        'p90': percentile(90),
        'max': ordered[-1],
        'total': sum(ordered),
    }


def print_stat_row(label, stat):
    if stat is None:
        print(f"    {label:<24}       --")
        return
    print(
        f"    {label:<24} n={stat['n']:<6}"
        f"min {dur(stat['min']):>8}   med {dur(stat['median']):>8}   "
        f"mean {dur(stat['mean']):>8}   p90 {dur(stat['p90']):>8}   "
        f"max {dur(stat['max']):>8}"
    )


def collect_timing(entry):
    #
    # Pull the per-epoch (per run attempt) timings out of one EPOCH ad.
    # Every field may be None; the summaries skip missing values rather than
    # dropping the whole record, because which attributes are present varies
    # with how the run ended (completed, evicted, held).
    #
    return {
        # Claim accepted -> job body starts.  The headline metric.
        'setup': attr_int(entry, 'ActivationSetupDuration'),
        # The job's own sandbox only -- see the note above.
        'input_transfer': span(
            entry,
            ('JobCurrentStartTransferInputDate',),
            ('JobCurrentFinishTransferInputDate',),
        ),
        # Claim accepted -> common files mapped.  Present only for epochs that
        # actually mapped a catalog, and it is the number that separates the
        # job that paid for the staging from the ones that reused it.
        'common_wait': span(
            entry,
            ('JobCurrentStartDate',),
            ('CommonFilesMappedTime',),
        ),
        # Job body execution.
        'execute': attr_int(entry, 'ActivationExecutionDuration') or span(
            entry,
            ('JobCurrentStartExecutingDate',),
            ('EpochWriteDate',),
        ),
        # Whole activation, setup through teardown.
        'activation': attr_int(entry, 'ActivationDuration') or span(
            entry,
            ('JobCurrentStartDate',),
            ('EpochWriteDate',),
        ),
        # Idle in the queue before this run started.
        'queue_wait': span(entry, ('QDate',), ('JobCurrentStartDate',)),
        # Absolute stamps, for the makespan.
        'start': attr_timestamp(entry, 'JobCurrentStartDate'),
        'end': attr_timestamp(entry, 'EpochWriteDate'),
        'qdate': attr_timestamp(entry, 'QDate'),
        # Did this epoch map a catalog?  Lets us split the summaries.
        'mapped': 'CommonFilesMappedTime' in entry,
    }


def dagman_wall_clock(schedd, clusterID):
    #
    # True end-to-end time for a DAG, from the DAGMan job's own history record.
    #
    # DAGMan runs in the scheduler universe, so it has no shadow and therefore
    # writes no epoch ads -- the node jobs' epoch ads can only tell us when the
    # first node started and the last one finished.  That understates the real
    # cost of the workflow: it misses DAGMan startup and shutdown, and any
    # stretch where nodes were throttled, retrying, or waiting on a dependency
    # with nothing running.  For a throughput comparison the submit-to-finish
    # number is the honest one, so go get it.
    #
    # Returns None (quietly) if the DAGMan record has already aged out of the
    # history, or if history is not readable.
    #
    try:
        ads = schedd.history(
            constraint=f'ClusterID == {clusterID}',
            projection=[
                'ClusterID', 'ProcID', 'QDate', 'JobStartDate',
                'CompletionDate', 'RemoteWallClockTime', 'JobStatus',
                'JobUniverse', 'EnteredCurrentStatus',
            ],
            match=1,
        )
        ads = list(ads)
    except Exception:
        return None

    if not ads:
        return None
    ad = ads[0]

    return {
        'qdate': attr_timestamp(ad, 'QDate'),
        'start': attr_timestamp(ad, 'JobStartDate'),
        'end': attr_timestamp(ad, 'CompletionDate', 'EnteredCurrentStatus'),
        'wall': attr_int(ad, 'RemoteWallClockTime'),
    }


def report_timing(label, records, schedd, dagman_clusterID):
    #
    # Print the timing block for one group.  `records` is the list of per-epoch
    # dicts from collect_timing().
    #
    if not records:
        return

    print(f"  timing ({label})")

    #
    # Makespan, measured across the node jobs we can see.
    #
    starts = [r['start'] for r in records if r['start'] is not None]
    ends = [r['end'] for r in records if r['end'] is not None]
    qdates = [r['qdate'] for r in records if r['qdate'] is not None]

    if starts and ends:
        print(
            f"    first node start -> last node finish:   "
            f"{dur(max(ends) - min(starts))}"
        )
    if qdates and ends:
        print(
            f"    first submit     -> last node finish:   "
            f"{dur(max(ends) - min(qdates))}"
        )

    #
    # The DAGMan job's own wall clock, which is the number to quote when
    # comparing two runs end-to-end.
    #
    if dagman_clusterID is not None:
        dagman = dagman_wall_clock(schedd, dagman_clusterID)
        if dagman is not None:
            wall = None
            if dagman['start'] is not None and dagman['end'] is not None:
                wall = dagman['end'] - dagman['start']
            elif dagman['wall'] is not None:
                wall = dagman['wall']
            if wall is not None:
                print(
                    f"    DAGMan job wall clock:                  "
                    f"{dur(wall)}   (cluster {dagman_clusterID}, "
                    f"scheduler universe)"
                )
            if dagman['qdate'] is not None and dagman['end'] is not None:
                print(
                    f"    DAGMan submit -> finish:                "
                    f"{dur(dagman['end'] - dagman['qdate'])}"
                )
        else:
            print(
                f"    DAGMan job wall clock:                  "
                f"--   (no history record for cluster "
                f"{dagman_clusterID}; it may have aged out)"
            )

    #
    # Per-job distributions.  Sum of setup across all runs is the total
    # overhead the workflow paid; it is the quantity common files is supposed
    # to reduce.
    #
    print()
    setup = summarize([r['setup'] for r in records])
    print_stat_row("activation setup", setup)
    print_stat_row("  input transfer (own)",
                   summarize([r['input_transfer'] for r in records]))
    print_stat_row("  common-files wait",
                   summarize([r['common_wait'] for r in records]))
    print_stat_row("job execution", summarize([r['execute'] for r in records]))
    print_stat_row("whole activation",
                   summarize([r['activation'] for r in records]))
    print_stat_row("queue wait", summarize([r['queue_wait'] for r in records]))

    #
    # Split setup by whether the epoch mapped a catalog.  In a common-files run
    # the mapped population contains both the job that paid for the staging and
    # the ones that reused it, so a long tail here is the staging cost; in a
    # control run everything lands in "no common files" and the comparison is
    # against the other run's numbers.
    #
    mapped = [r['setup'] for r in records if r['mapped']]
    unmapped = [r['setup'] for r in records if not r['mapped']]
    if mapped and unmapped:
        print()
        print_stat_row("  setup, mapped", summarize(mapped))
        print_stat_row("  setup, not mapped", summarize(unmapped))

    if setup is not None:
        print()
        print(
            f"    total setup overhead across {setup['n']} run"
            f"{'s' if setup['n'] != 1 else ''}: {dur(setup['total'])}"
            f"   (this is what common files is meant to shrink)"
        )
    print()


def main(by_dag, requested_id):

    #
    # Build the query constraint.
    #
    # In cluster mode we can push the cluster filter down to the schedd exactly
    # as before.  In DAG mode we deliberately do NOT push a "DAGManJobId == X"
    # constraint, because COMMON (transfer) epoch ads carry only the ClusterID
    # of the node whose shadow performed the transfer -- they do not reliably
    # carry DAGManJobId.  A server-side DAGManJobId constraint would therefore
    # silently drop the very transfer records we need in order to count bytes.
    # Instead we narrow the server-side result to "EPOCH ads for the requested
    # scope, plus all COMMON ads" and resolve each ad's DAG scope in Python,
    # using the EPOCH ads (full job-ad copies, which do carry DAGManJobId) to
    # build a ClusterID -> scope map.  COMMON ads for other scopes are
    # discarded during folding below.
    #
    # Note that the COMMON clause must use "==" and not "=!= \"EPOCH\"".  The
    # ClassAd "=!=" (IS NOT) operator is total -- it never evaluates to
    # UNDEFINED -- so `EpochAdType =!= "EPOCH"` is TRUE for any ad that has no
    # EpochAdType attribute at all, and would drag in every such record in the
    # history.  Those records exist: EpochAdType was only written into the ad
    # *body* in 24.10.2 (before that it lived only on the banner line), and the
    # ad_type= filter matches on the banner, so it can't screen them out.  With
    # "==" a missing attribute evaluates to UNDEFINED and the ad is not
    # selected, which is what we want.  Nothing is lost by this: COMMON epoch
    # ads postdate 24.10.2, so every COMMON ad carries EpochAdType.
    constraint = None
    if requested_id is not None:
        if by_dag:
            constraint = (
                f'(ClusterID == {requested_id}) || '
                f'(DAGManJobId == {requested_id}) || '
                f'(EpochAdType == "COMMON")'
            )
        else:
            constraint = f'clusterID == {requested_id}'

    schedd = htcondor.Schedd()
    results = schedd.jobEpochHistory(
        ad_type=['epoch', 'common'],
        constraint=constraint,
    )

    #
    # Because the shadow can fall back to using normal input transfer for jobs
    # which specify common files, we have to record either (a) that we fell
    # back for a given epoch or (b) that we successfully mapped a catalog for
    # this epoch.  (Otherwise, that is, we can't be sure that if an epoch skipped
    # a transfer because of the common files functionality or not.)
    #
    # So we record (1), each epoch whose job ad defines at least one common
    # file catalog; and (2), each epoch which indicates a succesful mapping.
    #
    # For extra credit, we can correlate COMMON entries to epochs for sizes.
    #
    # We always accumulate per *cluster*, and separately learn each cluster's
    # DAG scope from its EPOCH ads.  After the scan we fold the per-cluster
    # counters into per-*group* counters (group == cluster, or group == DAG
    # scope for --dag) and run the savings arithmetic on those.  Folding after
    # the fact means we don't care whether a COMMON ad was seen before or after
    # the EPOCH ad that establishes its cluster's scope.
    #

    # For each cluster ID, how many epochs had at least one common file catalog?
    EpochsByClusterID = defaultdict(lambda: 0)
    # For each cluster ID, how many epochs had a common file catalog mapped?
    CommonFilesMappedByClusterID = defaultdict(lambda: 0)
    # For each cluster ID, how many common transfers occurred?
    CommonTransfersByClusterID = defaultdict(lambda: 0)
    # For each cluster ID, how many catalogs were defined?
    CommonCatalogsByClusterID = defaultdict(lambda: 0)
    # For each cluster ID, how many common bytes were transferred?
    CommonBytesByClusterID = defaultdict(lambda: 0)

    # ClusterID -> DAG scope (DAGManJobId if the job is a DAG node, else the
    # ClusterID itself), and the set of scopes that are actually DAGs.
    ClusterToScope = {}
    ScopeIsDAG = set()
    # For reporting: which procs each cluster ran.
    ProcsByCluster = defaultdict(set)

    # Per-epoch timing records (see collect_timing()).  Unlike every counter
    # above, these are gathered for *all* real job runs, whether or not the job
    # used common files at all -- a control run with data reuse disabled has no
    # COMMON ads and no common-file catalogs, and its timings are exactly what
    # we need in order to have something to compare against.
    TimingByClusterID = defaultdict(list)

    # Records we couldn't classify at all (see below); reported at the end so
    # that a partially-unreadable history doesn't look like a clean result.
    untyped_records = 0
    # EPOCH ads belonging to transfer shadows rather than to real job runs
    # (see below).  Counted so that the staging activity stays visible even
    # though it's excluded from the accounting.
    transfer_shadow_records = 0

    for entry in results:
        # An epoch history file accumulates across upgrades, so it can hold
        # records written before EpochAdType was added to the ad body (24.10.2)
        # or with a banner we couldn't parse a type out of.  We can't tell what
        # such a record is, so count it and move on rather than dying.
        epoch_ad_type = entry.get('EpochAdType')
        clusterID = entry.get('ClusterID')
        if epoch_ad_type is None or clusterID is None:
            untyped_records += 1
            continue

        if epoch_ad_type == "COMMON":
            CommonTransfersByClusterID[clusterID] += 1
            CommonBytesByClusterID[clusterID] += common_transfer_bytes(entry)
        elif epoch_ad_type == "EPOCH":
            # EPOCH ads are full job-ad copies, so this is where we can learn
            # the job's DAG scope.  DAGManJobId, when present, is the ClusterID
            # of the DAGMan job that owns this node.
            if 'DAGManJobId' in entry:
                scope = int(entry['DAGManJobId'])
                ScopeIsDAG.add(scope)
            else:
                scope = clusterID
            ClusterToScope[clusterID] = scope

            #
            # Skip the EPOCH ads written by "transfer shadows".
            #
            # When the schedd decides to stage a catalog it spawns a dedicated
            # shadow under the *prompting* job's cluster, with a mangled procID
            # (see promptingToTransferProcID() and FIRST_TRANSFER_PROC_ID in
            # src/condor_utils/transfer_proc.h; transfer procIDs are <= -1000).
            # That shadow is handed the prompting job's ad, so it carries
            # CommonInputCatalogs, and it writes an EPOCH ad when the schedd
            # eventually vacates it at KEEP_DATA_CLAIM_IDLE expiry
            # (BaseShadow::evictJob -> writeAdToEpoch).  It only ever stages,
            # never maps, so it has no CommonFilesMappedTime and would
            # otherwise be miscounted as an epoch that fell back to normal
            # input transfer -- inflating both the common-file epoch count and
            # the fell-back count, and depressing the final percentage.
            #
            # Prefer the explicit marker over the procID encoding.  The schedd
            # stamps IsTransferShadow = true into the ad it hands the transfer
            # shadow (schedd.cpp, alongside the ProcID rewrite and
            # TransferTheseCatalogs), and because an EPOCH record is an
            # unfiltered copy of the whole job ad it survives into the history.
            # The procID mangling is explicitly a stopgap -- see the comment on
            # FIRST_TRANSFER_PROC_ID, "at some point ... this should all
            # change" -- so treat it only as a fallback for records written
            # before the marker existed.  Note that any negative procID here is
            # a transfer shadow: epoch recording rejects procIDs strictly
            # between -1000 and 0 outright, per isInvalidProcID().
            #
            # We keep the DAG scope recorded above -- it came from the
            # prompting job's ad, so it is correct, and it usefully covers the
            # very clusters that have COMMON ads -- but count nothing else.
            #
            procID = entry.get('ProcID')
            is_transfer_shadow = bool(entry.get('IsTransferShadow', False))
            if not is_transfer_shadow and procID is not None:
                is_transfer_shadow = int(procID) < 0
            if is_transfer_shadow:
                transfer_shadow_records += 1
                continue

            if procID is not None:
                ProcsByCluster[clusterID].add(procID)

            # Timings for every real run, common files or not.
            TimingByClusterID[clusterID].append(collect_timing(entry))

            old_style = False
            if 'CommonInputFiles' in entry:
                old_style = True
                CommonCatalogsByClusterID[clusterID] += 1

            new_style = False
            if 'CommonInputCatalogs' in entry:
                new_style = True
                string_list = entry['CommonInputCatalogs']
                # This isn't quite right, but it's probably close enough.
                catalogs = string_list.split(',')
                CommonCatalogsByClusterID[clusterID] += len(catalogs)

            if old_style or new_style:
                EpochsByClusterID[clusterID] += 1

            if 'CommonFilesMappedTime' in entry:
                CommonFilesMappedByClusterID[clusterID] += 1

    if untyped_records:
        print(
            f"note: skipped {untyped_records} epoch record"
            f"{'s' if untyped_records != 1 else ''} with no EpochAdType or "
            f"ClusterID (written before HTCondor 24.10.2, which is when the ad "
            f"type was added to the ad body).  These cannot be classified and "
            f"are not counted below.\n"
        )

    if transfer_shadow_records:
        print(
            f"note: excluded {transfer_shadow_records} EPOCH record"
            f"{'s' if transfer_shadow_records != 1 else ''} written by "
            f"transfer shadows (IsTransferShadow / negative ProcID).  These "
            f"are catalog stagings, not job runs, and are not epochs that fell "
            f"back to normal input transfer.\n"
        )

    #
    # Fold the per-cluster counters into per-group counters.  In cluster mode
    # the group is the cluster itself (an identity fold); in DAG mode it's the
    # DAG scope.  A COMMON ad's cluster may not appear in ClusterToScope (e.g. a
    # COMMON ad for some scope we didn't otherwise ask about); such a cluster
    # maps to itself, and because it has no EPOCH ads in our result its group
    # will have zero epochs and be skipped below.
    #
    def group_of(cid):
        return ClusterToScope.get(cid, cid) if by_dag else cid

    def fold(by_cluster):
        by_group = defaultdict(lambda: 0)
        for cid, value in by_cluster.items():
            by_group[group_of(cid)] += value
        return by_group

    # Timing folds by concatenation rather than by addition.
    TimingByGroup = defaultdict(list)
    AllClustersByGroup = defaultdict(set)
    for cid, records in TimingByClusterID.items():
        TimingByGroup[group_of(cid)].extend(records)
        AllClustersByGroup[group_of(cid)].add(cid)

    #
    # Emit the timing report first, because it does not depend on the
    # common-files accounting in any way.  That ordering is deliberate: a
    # control run with data reuse disabled has no COMMON ads at all, and if we
    # bailed out on that before reporting timings there would be nothing to
    # compare a common-files run against.
    #
    for group, records in sorted(TimingByGroup.items()):
        if requested_id is not None and group != requested_id:
            continue
        is_dag = by_dag and group in ScopeIsDAG
        clusters = AllClustersByGroup[group]
        print(f"{'DAG' if is_dag else 'Cluster'} {group}")
        report_timing(
            f"{len(records)} run{'s' if len(records) != 1 else ''} over "
            f"{len(clusters)} cluster{'s' if len(clusters) != 1 else ''}",
            records,
            schedd,
            group if is_dag else None,
        )

    if len(CommonTransfersByClusterID) == 0:
        what = "DAG/scope" if by_dag else "cluster ID"
        if requested_id is None:
            print("Found no common file transfers.")
        else:
            print(f"Found no common file transfer for {what} {requested_id}.")
        if TimingByGroup:
            print(
                "  (Timings above are still valid -- this is what a run with "
                "data reuse disabled\n   looks like, and is the baseline for "
                "comparison.)"
            )
        return 0

    EpochsByGroup = fold(EpochsByClusterID)
    CommonFilesMappedByGroup = fold(CommonFilesMappedByClusterID)
    CommonTransfersByGroup = fold(CommonTransfersByClusterID)
    CommonCatalogsByGroup = fold(CommonCatalogsByClusterID)
    CommonBytesByGroup = fold(CommonBytesByClusterID)

    # Which clusters (and procs) make up each group, for reporting.
    ClustersByGroup = defaultdict(set)
    for cid in EpochsByClusterID:
        ClustersByGroup[group_of(cid)].add(cid)

    # We could record the number of succesful mappings, but for now let's just
    # assume that if any mapping in an epoch succeeded, that they all did.
    WholeTransfersByGroup = defaultdict(lambda: 0)
    BytesPerWholeTransferByGroup = defaultdict(lambda: 0)

    for group, transfers in sorted(CommonTransfersByGroup.items()):
        if requested_id is not None and group != requested_id:
            continue

        epochs = EpochsByGroup[group]
        if epochs == 0:
            # A COMMON ad whose group we never saw an EPOCH ad for; can't
            # attribute it.  (Shouldn't happen for a group we actually report.)
            continue

        is_dag = by_dag and group in ScopeIsDAG
        # The group header was already printed by the timing report above, so
        # name this block for what it is rather than repeating the header.
        label = (
            f"{'DAG' if is_dag else 'Cluster'} {group}: common-file accounting"
        )

        clusters = ClustersByGroup[group]
        # The node count is the number of distinct clusters that used common
        # files -- for a DAG whose nodes each point at a single-process ("queue")
        # submit file, that is exactly the number of DAG nodes.  We deliberately
        # do NOT sum ProcIDs here: distinct (cluster, proc) processes are only
        # computed for the sanity-check note below, and nothing in the savings
        # accounting depends on them.
        n_nodes = len(clusters)
        n_procs = sum(len(ProcsByCluster[c]) for c in clusters)
        multi_proc = sum(1 for c in clusters if len(ProcsByCluster[c]) > 1)

        transfers_per_epoch = CommonCatalogsByGroup[group] / epochs
        whole_transfers = transfers / transfers_per_epoch
        partial = float(int(whole_transfers)) != whole_transfers
        whole_transfers = int(whole_transfers)
        if whole_transfers == 0:
            continue
        WholeTransfersByGroup[group] = whole_transfers

        bytes_per_whole_transfer = int(
            CommonBytesByGroup[group] / whole_transfers
        )
        BytesPerWholeTransferByGroup[group] = bytes_per_whole_transfer

        # And for the money...  Note that this counts *epochs* (run instances)
        # and *whole transfers*, never the summed process figure above.
        mapped = CommonFilesMappedByGroup[group]
        fell_back = epochs - mapped
        reused = mapped - whole_transfers
        total_bytes = mapped * bytes_per_whole_transfer
        transferred = whole_transfers * bytes_per_whole_transfer
        skipped = total_bytes - transferred
        if total_bytes == 0:
            continue
        pct = int((skipped / total_bytes) * 100)

        print(label)
        if is_dag:
            print(f"  DAG nodes (clusters):     {n_nodes}")
        else:
            print(f"  cluster:                  {group}")
        print(
            f"  common-file epochs:       {epochs}"
            + (f"   ({mapped} mapped, {fell_back} fell back to normal transfer)"
               if fell_back else f"   (all {mapped} mapped)")
        )
        print(
            f"  whole transfers:          {whole_transfers} of {mapped} mapped "
            f"epoch{'s' if mapped != 1 else ''}"
            + (f"; {reused} reused an already-staged copy" if reused > 0 else "")
        )
        print(
            f"  common data required:     {human(total_bytes):>10}   "
            f"({total_bytes:,} B  =  {bytes_per_whole_transfer:,} B/transfer "
            f"× {mapped} mapped epochs)"
        )
        print(
            f"  common data transferred:  {human(transferred):>10}   "
            f"({transferred:,} B  in {whole_transfers} whole transfer"
            f"{'s' if whole_transfers != 1 else ''})"
        )
        print(
            f"  common data skipped:      {human(skipped):>10}   "
            f"({skipped:,} B)  —  {pct}% of required"
        )
        if partial:
            print(
                f"  note: catalogs/epoch was fractional "
                f"({CommonCatalogsByGroup[group]}/{epochs} = {transfers_per_epoch}); "
                f"whole-transfer count truncated to {whole_transfers}."
            )
        if n_procs != n_nodes:
            print(
                f"  note: the epoch history holds {n_procs} distinct "
                f"(cluster, proc) processes for these {n_nodes} node clusters "
                f"({multi_proc} cluster{'s' if multi_proc != 1 else ''} logged "
                f">1 proc).  The accounting above counts clusters and epochs, "
                f"not processes, so it is unaffected."
            )
        print()

    if requested_id is not None:
        return 0

    total_epochs = 0
    for group, count in EpochsByGroup.items():
        total_epochs += count

    total_common_mappings = 0
    for group, count in CommonFilesMappedByGroup.items():
        total_common_mappings += count

    total_whole_transfers = 0
    for group, count in WholeTransfersByGroup.items():
        total_whole_transfers += count

    if total_common_mappings == 0 or total_epochs == 0:
        return 0

    pct_a = int(
        ((total_common_mappings - total_whole_transfers) / total_common_mappings) * 100
    )
    pct_b = int(
        ((total_epochs - total_whole_transfers) / total_epochs) * 100
    )
    print("Totals across all groups:")
    print(f"  common-file epochs:   {total_epochs}")
    print(f"  mapped common files:  {total_common_mappings}")
    print(f"  whole transfers:      {total_whole_transfers}")
    print(
        f"  skipped {pct_a}% of the transfers those mapped epochs would have "
        f"needed, and {pct_b}% of all common-file epochs."
    )

    return 0


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="Quantify HTCondor common-files transfer savings."
    )
    parser.add_argument(
        "--dag",
        action="store_true",
        help="Group savings by DAG (DAGManJobId) instead of by cluster.  Use "
             "this for DAGs made of single-proc clusters, where sharing "
             "happens across nodes rather than within one cluster.",
    )
    parser.add_argument(
        "id",
        nargs="?",
        type=int,
        default=None,
        metavar="ID",
        help="Restrict to a single group.  Without --dag this is a ClusterID; "
             "with --dag it is the DAGManJobId (the ClusterID of the DAGMan "
             "job whose nodes you want to account for).",
    )
    args = parser.parse_args()
    sys.exit(main(args.dag, args.id))
