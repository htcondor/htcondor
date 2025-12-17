#!/usr/bin/env pytest

import logging

import htcondor2 as htcondor

from ornithology import *

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

"""
This test creates a new X.509 user credential, submits a job that uses that
credential, and verfies that HTCondor can delegate the credential.
"""

@action
def path_to_the_x509_cred(default_condor, test_dir):
    # If openssl is told to write both private key and public cert to the
    # same file, it'll put the key first.
    # The standard proxy file layout puts the certificate first and our code
    # to read a proxy file will fail if that ordering isn't maintained.
    # Thus, we need to manually assemble the file here to have the cert
    # appear first.
    key_path = test_dir / "test.key"
    cred_path = test_dir / "test.pem"
    rv = default_condor.run_command(
        ["openssl", "req", "-nodes", "-x509", "-newkey", "rsa:4096", "-keyout", key_path.as_posix(), "-out", cred_path.as_posix(), "-days", "365", "-subj", "/C=US/ST=Wisconsin/L=Madison/O=HTCondor/CN=Test Credential"],
        timeout=8,
    )
    assert rv.returncode == 0
    key_data = key_path.read_text()
    with open(cred_path, 'a') as cred_file:
        cred_file.write(key_data)
    cred_path.chmod(0o600)
    return cred_path

@action
def path_to_the_job_script(test_dir):
    path = test_dir / "job.sh"
    script = """
        #!/bin/bash
        if [ -z "$X509_USER_PROXY" ] ; then
            echo "X509_USER_PROXY not defined"
            exit 1
        fi
        if [ ! -r "$X509_USER_PROXY" ] ; then
            echo "X509_USER_PROXY isn't a readable file"
            exit 2
        fi
        proxy_subj=`openssl x509 -in $X509_USER_PROXY -noout -subject -nameopt rfc2253`
        if [ $? != 0 ] ; then
            echo "openssl failed"
            exit 3
        fi
        if ! echo "$proxy_subj" | grep -q 'subject= *CN=[0-9]*,CN=Test' ; then
            echo "Proxy subject doesn't look valid:"
            echo $proxy_subj
            exit 4
        fi
        exit 0
        """
    write_file(path, format_script(script))
    return path

class TestX509Delegation:
    def test_delegate_job_x509_credential(self, default_condor, path_to_the_x509_cred, path_to_the_job_script):
        job = default_condor.submit(
            {
                "universe": "vanilla",
                "executable": path_to_the_job_script.as_posix(),
                "output": "job.out",
                "error": "job.err",
                "log": "job.log",
                "should_transfer_files": "YES",
                "when_to_transfer_output": "ON_EXIT",
                "x509userproxy": path_to_the_x509_cred.as_posix(),
                "leave_in_queue": "true",
            }
        )
        assert job.wait(condition=ClusterState.all_complete, timeout=60)

        assert job.state[0] == JobStatus.COMPLETED

        job_ad = job.query()[0]
        exit_code = job_ad['ExitCode']
        assert exit_code == 0
