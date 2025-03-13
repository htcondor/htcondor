#!/usr/bin/env python3

#
# This job script sets hpc_annex_start_time to the current time
# if an ad with the specified attribute and value
# appears in the specified collector (the corresponding annex).
#
# Assumes string values.
#

#
# To have this job monitor the specified collector, use the condor cron_*
# job attributes to poll it using this script.
#

import os
import sys
import time

import logging

import htcondor2 as htcondor

(exe, local_job_id, attribute_name, attribute_value, collector_name) = sys.argv

logger = logging.getLogger(__name__)
collector = htcondor.Collector(collector_name)
ads = collector.query(
    constraint=f'{attribute_name} == "{attribute_value}"',
)

if len(ads) == 0:
    print(f'Did not find machines with {attribute_name} == "{attribute_value}"')
    exit(0)


schedd = htcondor.Schedd()
schedd.edit(local_job_id, "hpc_annex_start_time", f"{int(time.time())}")
print(f"Set {local_job_id}'s hpc_annex_start_time to now.")

exit(0)
