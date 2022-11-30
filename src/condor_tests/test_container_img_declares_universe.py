#!/usr/bin/env pytest

#   Test_container_img_declares_universe.py
#
#   Test that declaring docker_image or container_image
#   in a submit file declares and initializes the
#   respective universe and job attributes.
#   Written by: Cole Bollig
#   Written:    2022-11-11

from ornithology import *

#Input/output file names
io_names = [
    "both",
    "universe",
    "docker",
    "container",
]
#Global test number
subtest_num = -1
#-------------------------------------------------------------------------
def write_sub_file(test_dir,exe,name,info):
    sub_file = test_dir / name
    contents = format_script(
f"""
executable = {exe}
log = test.log
arguments = 1
{info}
queue
"""
    )
    write_file(sub_file, contents)
    return sub_file

#-------------------------------------------------------------------------
@action(params={ #Params: TestName : added lines to submit file
    "BothImages":"docker_image=htcondor/mini:9.12-el7\ncontainer_image=docker://htcondor/mini:9.12-el7", #Both docker & container image: Fail
    "InvalidUniverse":"universe=vanilla\ndocker_image=htcondor/mini:9.12-el7",                           #Image and other Universe: Fail
    "DockerImg":"docker_image=htcondor/mini:9.12-el7",                                                   #Docker image & no universe: Pass
    "ContainerImg":"container_image=docker://htcondor/mini:9.12-el7",                                    #Container image & no universe: Pass
})
def dry_run_job(default_condor,test_dir,path_to_sleep,request):
    global subtest_num
    #Increment Test number
    subtest_num += 1
    #Write submit file with correct lines
    submit = write_sub_file(test_dir,path_to_sleep,f"{io_names[subtest_num]}.sub",request.param)
    #Run command
    cp = default_condor.run_command(["condor_submit",f"{submit}","-dry-run",f"{io_names[subtest_num]}.ad"])
    return cp #Return completed process

#=========================================================================
class TestContainerImgDeclaresUniverse:
    def test_image_init(self,dry_run_job,test_dir):
        expected_output = [
            #Failed submission outputs
            "Only one can be declared in a submit file.",
            "universe for job does not allow use of",
            #Successful submission job ad attributes
            "WantDocker=true",
            "WantContainer=true",
        ]
        #If first test (Fail because of both docker and container image)
        if subtest_num < 2:
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


