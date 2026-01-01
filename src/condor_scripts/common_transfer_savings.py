#!/usr/bin/python3

from collections import defaultdict

import htcondor2 as htcondor
import classad2 as classad


schedd = htcondor.Schedd()
results = schedd.jobEpochHistory(
    ad_type=['epoch', 'common']
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


# For each cluster ID, how many epochs had at least one common file catalog?
EpochsByClusterID = defaultdict(lambda: 0)
# For each cluster ID, how many epochs had a common file catalog mapped?
CommonFilesMappedByClusterID = defaultdict(lambda: 0)
# For each cluster ID, how many common transfers occurred?
CommonTransfersByClusterID = defaultdict(lambda: 0)
# For each cluster ID, how many catalogs were defined?
CommonCatalogsByClusterID = defaultdict(lambda: 0)

for entry in results:
    clusterID = entry['ClusterID']
    epoch_ad_type = entry['EpochAdType']

    if epoch_ad_type == "COMMON":
        CommonTransfersByClusterID[clusterID] += 1
    elif epoch_ad_type == "EPOCH":
        old_style = False;
        if 'CommonInputFiles' in entry:
            old_style = True
            CommonCatalogsByClusterID[clusterID] += 1

        new_style = False
        if '_x_common_input_catalogs' in entry:
            new_style = True
            string_list = entry['_x_common_input_catalogs']
            # This isn't quite right, but it's probably close enough.
            list = string_list.split(',')
            CommonCatalogsByClusterID[clusterID] += len(list)

        if old_style or new_style:
            EpochsByClusterID[clusterID] += 1


        if 'CommonFilesMappedTime' in entry:
            CommonFilesMappedByClusterID[clusterID] += 1


# We could record the number of succesful mappings, but for now let's just
# assume that if any mapping in an epoch succeeded, that they all did.
WholeTransfersByClusterID = defaultdict(lambda: 0)
for clusterID, transfers in CommonTransfersByClusterID.items():
    transfers_per_cluster = CommonCatalogsByClusterID[clusterID]/EpochsByClusterID[clusterID]
    whole_transfers = transfers / transfers_per_cluster
    if float(int(whole_transfers)) != whole_transfers:
        print(
            f"Ignoring partial transfers in cluster ID {clusterID}: "
            f"{transfers} transfers / "
            f"(({CommonCatalogsByClusterID[clusterID]} common catalogs /"
            f"{EpochsByClusterID[clusterID]} epochs) = "
            f"{transfers_per_cluster} transfers_per_cluster) = "
            f"{whole_transfers} whole_transfers"
        )
    WholeTransfersByClusterID[clusterID] = int(whole_transfers)


total_epochs = 0
for clusterID, count in EpochsByClusterID.items():
    total_epochs += count

total_common_mappings = 0
for clusterID, count in CommonFilesMappedByClusterID.items():
    total_common_mappings += count

total_whole_transfers = 0
for clusterID, count in WholeTransfersByClusterID.items():
    total_whole_transfers += count


pct_a = int(
    ((total_common_mappings - total_whole_transfers)/total_common_mappings) * 100
)
pct_b = int(
    ((total_epochs - total_whole_transfers)/total_epochs) * 100
)
print(
    f"Of {total_epochs} total epochs, "
    f"{total_common_mappings} successfuly mapped common files "
    f"after performing {total_whole_transfers} whole transfers, "
    f"skipping {pct_a}% of those transfers, "
    f"or {pct_b}% of all possible transfers."
)
