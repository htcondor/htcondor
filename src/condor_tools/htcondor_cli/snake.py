"""
HTCondor CLI for running Snakemake workflow

- need `-help` flag to guide users about the commands
"""
from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb

class Submit(Verb):
    """
    Milestone One: htcondor snake submit
    when running htcondor snake submit:
    - snakemake_long.py is invoked for the long running job for the workflow
    --> this launches a local universe job
    - Snakemake jobs are submitted: 2 ways
    --> from a profile configuration
    --> from regular snakemake commands


    #additional behavior
    - after running condor_q -nobatch or watch_q: local universe job should appear sth like
    `snakemake_$(clusterid)` as a job management id
    """
    pass


#class Status(Verb):
#    pass


#class Halt(Verb):
#    pass


class Snake(Noun):
    """
    Run operations on Snakemake workflows via HTCondor
    """

    class submit(Submit):
        pass

    @classmethod
    def verbs(cls):
        return [cls.submit] 





