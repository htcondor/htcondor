#!/usr/bin/env pytest

import os

from ornithology import (
    action,
    Condor,
)

import htcondor2


@action
def the_condor(test_dir):
    local_dir = test_dir / "condor"

    with Condor(
        local_dir=local_dir,
        config={
            'CONTAINER_IMAGES_COMMON_BY_DEFAULT': 'FALSE',
            'CONTAINER_REGEX_COMMON_BY_DEFAULT': '',
        }
    ) as the_condor:
        yield the_condor


@action
def the_job_description():
    return """
        universe = vanilla
        shell = cat common-file input-file-$(ProcID) > output-file-$(ProcID); sleep 10
        MY.CommonInputFiles = "common-file"
        transfer_input_files = input-file-$(ProcID)
        transfer_output_files = output-file-$(ProcID)
        should_transfer_files = YES
        requirements = HasCommonFilesTransfer
        request_cpus = 1
        request_memory = 1024
        log = $(ClusterID).log
    """


TEST_CASES = {
    "sif-blank-no": {
        "submit":   "container_image = busybox.sif",
        "regex":    "",
        "expected": False,
    },
    "sif-match-yes": {
        "submit":   "container_image = busybox.sif",
        "regex":    '''^.*\\.sif$''',
        "expected": True,
    },
    "http-match-yes": {
        "submit":    "container_image = http://example.tld/busybox.sif",
        "regex":    '''^http://.*$''',
        "expected": True,
    },
    "osdf-match-yes": {
        "submit":   "container_image = osdf://example/namespace/busybox.sif",
        "regex":    '''^osdf://.*$''',
        "expected": True,
    },
    "sif-other-no": {
        "submit":   "container_image = busybox.sif",
        "regex":    '''^osdf://.*$''',
        "expected": False,
    },
}


@action
def the_cluster_ads(the_condor, the_job_description):
    clusterAds = {}
    schedd = the_condor.get_local_schedd()
    original = htcondor2.param.get('CONTAINER_REGEX_COMMON_BY_DEFAULT', "")

    for name, value in TEST_CASES.items():
        jd = the_job_description + f"{value['submit']}\n"
        htcondor2.param['CONTAINER_REGEX_COMMON_BY_DEFAULT'] = value['regex']
        submit = htcondor2.Submit(jd)
        sr = schedd.submit(submit, count=1)
        clusterAds[name] = sr.clusterad()
    htcondor2.param['CONTAINER_REGEX_COMMON_BY_DEFAULT'] = original

    return clusterAds


class TestCIC_Regex():

    def test_container_commonality_is_as_expected(self, the_cluster_ads):
        for name, ad in the_cluster_ads.items():
            common = ad.get('_x_catalog_condor_container_image') is not None
            print(f"{name} -> {common}")
            assert TEST_CASES[name]['expected'] == common
