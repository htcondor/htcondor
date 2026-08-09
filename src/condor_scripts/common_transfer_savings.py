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
        # transfers -- omit "Protocols"; iterating keys also avoids depending on
        # attribute-name casing.
        total = 0
        for key in stats_ad:
            if key.endswith("SizeBytes"):
                total += int(stats_ad.get(key, 0) or 0)
        if total > 0:
            return total

    # No stats ad (or an empty one): the epoch ad itself carries the size.
    return int(entry.get('CedarSizeBytes', 0) or 0)


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
    constraint = None
    if requested_id is not None:
        if by_dag:
            constraint = (
                f'(ClusterID == {requested_id}) || '
                f'(DAGManJobId == {requested_id}) || '
                f'(EpochAdType =!= "EPOCH")'
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

    for entry in results:
        clusterID = entry['ClusterID']
        epoch_ad_type = entry['EpochAdType']

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
            if 'ProcID' in entry:
                ProcsByCluster[clusterID].add(entry['ProcID'])

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

    if len(CommonTransfersByClusterID) == 0:
        what = "DAG/scope" if by_dag else "cluster ID"
        if requested_id is None:
            print("Found no common file transfers.")
        else:
            print(f"Found no common file transfer for {what} {requested_id}.")
        sys.exit(0)

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
        label = f"DAG {group}" if is_dag else f"Cluster {group}"

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
