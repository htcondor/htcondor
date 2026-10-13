from collections import defaultdict

import classad2
import htcondor2


# - Required of all snake daemons.  -------------------------------------------

def updateDaemonAd(daemonAd : classad2.ClassAd):
    daemonAd['Name'] = 'CBS'
    daemonAd['MyType'] = 'CBS'

    return daemonAd


# - At regular intervals, compute contention.  --------------------------------

def polling_handler():
    while True:
        compute_contention()

        signalled = yield 60

        if signalled:
            # We're done.
            return False


def compute_contention():

    collector = htcondor2.Collector()
    slots = collector.query(
        ad_type=htcondor2.AdType.Slot,
    )
    submitters = collector.query(
        ad_type=htcondor2.AdType.Submitter
    )

    scheduler = htcondor2.Schedd()
    autoclusters = scheduler.query(
        opts=htcondor2.QueryOpt.AutoCluster
    )


    htcondor2.enable_log()
    # htcondor2.log(htcondor2.LogLevel.Always, f"slots = {slots}")
    # htcondor2.log(htcondor2.LogLevel.Always, f"submitters = {submitters}")
    # htcondor2.log(htcondor2.LogLevel.Always, f"autoclusters = {autoclusters}")


    # For every slot, compute how many jobs would run if there were an
    # infinite number of such slots.
    #
    # Ignore p-slot splitting for now.
    #
    # Also count per auto-cluster how many different slots match.
    slot_count = defaultdict(lambda: 0)
    match_count = defaultdict(lambda: 0)
    for slot in slots:
        htcondor2.log(htcondor2.LogLevel.Always, slot['Name'])
        for autocluster in autoclusters:
            htcondor2.log(
                htcondor2.LogLevel.Always, autocluster['JobIDs']
            )
            slot_matches_autocluster = slot.matches(autocluster)
            autocluster_matches_slot = autocluster.matches(slot)
            if slot_matches_autocluster and autocluster_matches_slot:
                slot_count[slot['Name']] += autocluster['JobCount']
                # Is this right?
                match_count[autocluster['AutoClusterID']] += 1
            else:
                htcondor2.log(htcondor2.LogLevel.Always, f"{slot_matches_autocluster} / {autocluster_matches_slot}")
        htcondor2.log(htcondor2.LogLevel.Always, "")

    for name, count in slot_count.items():
        htcondor2.log(
            htcondor2.LogLevel.Always,
            f"{name}: {count}"
        )

    for ID, count in match_count.items():
        htcondor2.log(
            htcondor2.LogLevel.Always,
            f"AutoClusterID {ID}: {count}"
        )
