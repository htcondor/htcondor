#!/usr/bin/env pytest

#   Test_container_img_declares_universe.py
#
#   Test that declaring docker_image or container_image
#   in a submit file declares and initializes the
#   respective universe and job attributes.
#   Written by: Cole Bollig
#   Written:    2022-11-11

from ornithology import *
from htcondor2 import JobAction

#Input/output file names
io_names = [
    "both",
    "universe",
    "xfer_instance",
    "docker",
    "container",
    "xfer_docker",
    "xfer_oras",
    "xfer_osdf",
    "xfer_file",
    "vanilla_docker",
    "vanilla_container",
    "container_uni_docker",
    "container_uni_container",
]
#Global test number
subtest_num = -1
#-------------------------------------------------------------------------
def write_sub_file(test_dir,exe,name,info,time=1,queue=None):
    sub_file = test_dir / name
    queue_str = "queue" if queue is None else f"queue {queue}"
    contents = format_script(
f"""
executable = {exe}
log = test.log
arguments = {time}
should_transfer_files = yes
when_to_transfer_output = on_exit
{info}
{queue_str}
"""
    )
    write_file(sub_file, contents)
    return sub_file

#-------------------------------------------------------------------------
@standup
def condor(test_dir):
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "NUM_CPUS": "10",
            "NUM_SLOTS": "10",  # must be larger than the max number of jobs we hope to materialize
            "SCHEDD_MATERIALIZE_LOG": "$(LOG)/MaterializeLog",
            "SCHEDD_DEBUG": "D_MATERIALIZE:2 D_CAT $(SCHEDD_DEBUG)",
        },
    ) as condor:
        yield condor

#-------------------------------------------------------------------------
@action(params={ #Params: TestName : added lines to submit file
    "BothImages":"docker_image=htcondor/mini:9.12-el7\ncontainer_image=docker://htcondor/mini:9.12-el7", #Both docker & container image: Fail
    "InvalidUniverse":"universe=docker\ncontainer_image=docker://htcondor/mini:9.12-el7",                #Docker uni & container image: Fail
    "XferInstance":"container_image=instance://htcondor/mini:9.12-el7",                                      #instance image : Fail
    "DockerImg":"docker_image=htcondor/mini:9.12-el7",                                                   #Docker image & no universe: Pass
    "ContainerImg":"container_image=docker://htcondor/mini:9.12-el7",                                    #Container image & no universe: Pass
    "XferDocker":"container_image=docker://htcondor/mini:9.12-el7",                                    #docker image & verify image not in transfer_input_files
    "XferOras":"container_image=oras://htcondor/mini:9.12-el7",                                      #oras image & verify image not in transfer_input_files
    "OSDFInstance":"container_image=osdf://htcondor/mini:9.12-el7",                                      #instance image & verify image in transfer_input_files
    "FileInstance":"container_image=/htcondor/mini:9.12-el7",                                      #local filel image & verify image in transfer_input_files
    "Vanilla&Docker":"universe=vanilla\ndocker_image=htcondor/mini:9.12-el7",                            #Vanilla Universe with docker image: Pass
    "Vanilla&Container":"universe=vanilla\ncontainer_image=docker://htcondor/mini:9.12-el7",             #Vanilla Universe with contianer image: Pass
    "Container&Docker":"universe=container\ndocker_image=htcondor/mini:9.12-el7",                        #Container universe with docker image: Pass
    "Container&Container":"universe=container\ncontainer_image=docker://htcondor/mini:9.12-el7",         #Container universe with container image: Pass
})
def dry_run_job(condor,test_dir,path_to_sleep,request):
    global subtest_num
    subtest_num += 1
    #Write submit file with correct lines
    submit = write_sub_file(test_dir,path_to_sleep,f"{io_names[subtest_num]}.sub",request.param)
    #Run command
    cp = condor.run_command(["condor_submit",f"{submit}","-dry-run",f"{io_names[subtest_num]}.ad"])
    return cp #Return completed process

#-------------------------------------------------------------------------
NUM_PROCS=2
@action(params={
    "DockerImgLate": ("docker", f"max_idle={NUM_PROCS}\ndocker_image=htcondor/mini:9.12-el7"),
    "ContainerImgLate": ("container", f"max_idle={NUM_PROCS}\ncontainer_image=docker://htcondor/mini:9.12-el7"),
})
def run_late_materialization(condor, test_dir, path_to_sleep, request):
    filename = "late_" + request.param[0] + ".sub"
    submit_file = write_sub_file(test_dir, path_to_sleep, filename, request.param[1], 30, NUM_PROCS)
    submit_cmd = condor.run_command(["condor_submit", submit_file])
    clusterid, num_procs = parse_submit_result(submit_cmd)

    jobids = [JobID(clusterid, n) for n in range(num_procs)]

    condor.job_queue.wait_for_events(
        {jobid: [SetJobStatus(JobStatus.IDLE)] for jobid in jobids},
    )
    ads = condor.query(
        constraint=f"ClusterId=={clusterid}",
        projection=["ContainerImage","DockerImage","WantContainer","WantDockerImage"]
    )
    expect_docker_img = True if "docker_image" in request.param[1] else False
    return (ads, expect_docker_img)

#=========================================================================
class TestContainerImgDeclaresUniverse:
    def test_image_init(self,dry_run_job,test_dir):
        expected_output = [
            #Failed submission outputs
            "cannot declare both docker_image and container_image",
            "docker universe does not allow use of container_image.",
            "not a valid container image",
            #Successful submission job ad attributes
            "WantDockerImage=true",
            "WantContainer=true",
            "WantContainer=true",
            "WantContainer=true",
            "WantContainer=true",
            "WantContainer=true",
            "WantContainer=true",
            "WantContainer=true",
            "WantDockerImage=true",
            "WantContainer=true",
            "WantDockerImage=true",
            "WantContainer=true",
        ]
        #First tests expected to return non-zero
        if subtest_num < 3:
            #Make sure error message exists
            if dry_run_job.stderr == "":
                assert False
            else:
                #Verify error message
                assert expected_output[subtest_num] in dry_run_job.stderr
        else: #Else is pass test
            if dry_run_job.stderr != "": #Make sure no error occured
                assert False
            else:
                #Search output job ad file for set attribute
                foundAttr = False
                file_path = test_dir / f"{io_names[subtest_num]}.ad"
                f = open(file_path,"r")
                for line in f:
                    line = line.strip()
                    if line == expected_output[subtest_num]:
                        foundAttr = True
                        break
                assert foundAttr is True

    def test_late_materialization(self, run_late_materialization):
        ads, is_docker = run_late_materialization
        for ad in ads:
            assert "WantContainer" in ad
            if is_docker:
                assert "WantDockerImage" in ad
                assert "DockerImage" in ad
            else:
                assert "ContainerImage" in ad

