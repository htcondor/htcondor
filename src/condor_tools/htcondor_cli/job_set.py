import time
import re
import getpass

from datetime import datetime
from collections import defaultdict

import htcondor2 as htcondor
import classad2 as classad

from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb
from htcondor_cli.utils import readable_time, readable_size, s

JSM_HTC_JOBSET_SUBMIT = 5

class Submit(Verb):
    """
    Submits a job set when given a job set file
    """

    options = {
        "job_set_file": {
            "args": ("job_set_file",),
            "help": "Job set submit file",
        },
    }

    def __init__(self, logger, job_set_file, **options):

        self.name = None
        self.itemdata = None
        self.jobs = []

        schedd = htcondor.Schedd()
        self.parse_job_set_file(job_set_file)

        if self.name is None:
            raise ValueError(f"""Required name not found in {job_set_file}""")
        if self.itemdata is None:
            raise ValueError(f"""Required iterator not found in {job_set_file}""")
        if len(self.jobs) == 0:
            logger.warning(f"Warning: No jobs found in {job_set_file}")

        # Schedd *should* advertise if job sets are enabled, but doesn't
        # fix this when it does
        if not htcondor.param.get("USE_JOBSETS", False):
            raise ValueError(f"""Job sets not enabled on this schedd by an administrator, because USE_JOBSETS is false in the configuration.  Skipping submit.""")

        # Submit the jobs
        submit_results = []
        for i, job in enumerate(self.jobs):
            try:
                submit_result = schedd.submit(job, itemdata=iter(self.itemdata))
                logger.debug(f"Submitted job {i} to job cluster {submit_result.cluster()}.")
                submit_results.append(submit_result)
            except RuntimeError as e:
                logger.error(f"Error while submitting job {i} from {job_set_file}:\n{str(e)}")

                # On error, rollback any submitted jobs from this job set
                for submitted_job in submit_results:
                    logger.debug(f"Attempting to remove job cluster {submitted_job.cluster()}.")
                    try:
                        schedd.act(
                            htcondor.JobAction.RemoveX,
                            job_spec=f"ClusterId == {submitted_job.cluster()}",
                            reason=f"Aborted job set {self.name} submission."
                        )
                    except Exception:
                        pass

                # Then exit early
                return

        logger.info(f"Submitted job set {self.name} containing {len(self.jobs)} job clusters.")


    def parse_job_set_file(self, job_set_file):
        commands = {"name", "iterator", "job"}
        iterator_types = {"table"}
        lineno = 0
        with open(job_set_file, "rt") as f:
            while f:

                line = f.readline()
                if line == "":
                    break
                lineno += 1

                line = line.strip()
                if line == "" or line.startswith("#"):
                    continue

                try:
                    command = line.split()[0].split("=")[0].casefold()
                except IndexError:
                    raise IndexError(f"""Malformed command in {job_set_file} at line {lineno}.""")

                if command not in commands:
                    raise ValueError(f"""Unrecognized command "{command}" in {job_set_file} at line {lineno}.""")

                if command == "name":
                    if self.name is not None:
                        raise ValueError(f"""Job set name can only be set once, second name found in {job_set_file} at line {lineno}.""")
                    try:
                        value = line.split("#")[0].split("=")[1].strip()
                    except IndexError:
                        raise IndexError(f"""Malformed {command} command in {job_set_file} at line {lineno}.""")
                    if value.strip() == "":
                        raise ValueError(f"""Blank job set name found in {job_set_file} at line {lineno}.""")

                    self.name = value

                elif command == "iterator":
                    if self.itemdata is not None:
                        raise ValueError(f"""Job set iterator can only be set once, second iterator found in {job_set_file} at line {lineno}.""")

                    try:
                        value = line.split("#")[0].split("=")[1].strip()
                    except IndexError:
                        raise IndexError(f"""Malformed {command} command in {job_set_file} at line {lineno}.""")

                    if len(value.split()) < 3:
                        raise ValueError(f"""Unparseable iterator "{value}" in {job_set_file} at line {lineno}.""")

                    iterator_type = value.split()[0]
                    if iterator_type not in iterator_types:
                        raise ValueError(f"""Unknown iterator type "{iterator_type}" in {job_set_file} at line {lineno}.""")

                    if iterator_type == "table":

                        # Get the column names
                        iterator_names = value.replace(",", " ").split()[1:-1]
                        iterator_names = [x.strip() for x in iterator_names]

                        # Read the iterator values into a itemdata list of dicts
                        iterator_source = value.split()[-1]
                        if iterator_source == "{":
                            inline = "{"
                            inlineno = 0
                            inline_data = ""
                            while inline != "":
                                inline = f.readline()
                                inlineno += 1

                                if inline.strip() == "":
                                    continue

                                if inline.split("#")[0].strip() == "}":
                                    break

                                # Assume that a newly opened bracket without
                                # a closing bracket means that there was an error.
                                try:
                                    if inline.split("#")[0].split()[-1].strip() == "{":
                                        raise ValueError(f"""Unclosed bracket in {job_set_file} starting at line {lineno}.""")
                                except IndexError:
                                    pass  # Let the parser handle this situation

                                inline_data += inline
                            else:
                                raise ValueError(f"""Unclosed bracket in {job_set_file} starting at line {lineno}.""")
                            self.itemdata = self.parse_columnar_itemdata(iterator_names, inline_data, lineno=lineno, fname=job_set_file)
                            lineno += inlineno
                        else:
                            try:
                                with open(iterator_source, "rt") as f_iter:
                                    self.itemdata = self.parse_columnar_itemdata(iterator_names, f_iter.read(), fname=iterator_source)
                            except IOError as e:
                                raise IOError(f"Error opening table file {iterator_source} in {job_set_file} at line {lineno}:\n{str(e)}")

                elif command == "job":
                    try:
                        value = " ".join(line.split("#")[0].strip().split()[1:])
                    except IndexError:
                        raise IndexError(f"""Malformed {command} command in {job_set_file} at line {lineno}.""")

                    # Get the variable name mappings
                    mappings = []
                    if len(value.split()) > 1:
                        mapping_strs = ",".join(value.split()[:-1])
                        if "=" in mapping_strs:
                            for mapping_str in mapping_strs.split(","):
                                mapping = tuple(x.strip() for x in mapping_str.split("="))
                                if len(mapping) != 2:
                                    raise ValueError(f"""Unsupported mapping "{mapping_str}" in {job_set_file} at line {lineno}.""")
                                mappings.append(mapping)
                        else:
                            raise ValueError(f"""Unsupported mapping "{' '.join(value.split()[:-1])}" in {job_set_file} at line {lineno}.""")
                    mappings = dict(mappings)

                    # Read the job submit description into a Submit object
                    job_source = value.split()[-1]
                    if job_source == "{":
                        inline = "{"
                        inlineno = 0
                        inline_data = ""
                        while inline != "":
                            inline = f.readline()
                            inlineno += 1

                            if inline.strip() == "":
                                continue

                            if inline.split("#")[0].strip() == "}":
                                break

                            # Assume that a newly opened bracket without
                            # a closing bracket means that there was an error.
                            try:
                                 if inline.split("#")[0].split()[-1].strip() == "{":
                                     raise ValueError(f"""Unclosed bracket in {job_set_file} starting at line {lineno}.""")
                            except IndexError:
                                pass  # Let the parser handle this situation

                            inline_data += inline.lstrip()
                        else:
                            raise ValueError(f"""Unclosed bracket in {job_set_file} starting at line {lineno}.""")
                        lineno += inlineno
                        submit_obj = htcondor.Submit(inline_data)
			#Set s_method to HTC_JOBSET_SUBMIT
                        submit_obj.setSubmitMethod(JSM_HTC_JOBSET_SUBMIT,True)
                    else:
                        try:
                            with open(job_source, "rt") as f_sub:
                                submit_obj = htcondor.Submit(f_sub.read())
				#Set s_method to HTC_JOBSET_SUBMIT
                                submit_obj.setSubmitMethod(JSM_HTC_JOBSET_SUBMIT,True)
                        except IOError as e:
                            raise IOError(f"Error opening submit description file {job_source} in {job_set_file} at line {lineno}:\n{str(e)}")

                    # Remap variables in the Submit object
                    submit_obj = self.remap_submit_variables(mappings, submit_obj)

                    # Store each job
                    self.jobs.append(submit_obj)

        # Add job set name to each job's Submit object
        for i_job, job in enumerate(self.jobs):
            job["MY.JobSetName"] = classad.quote(self.name)
            job["MY.InJobSet"] = True


    def parse_columnar_itemdata(self, column_names, column_data, lineno=0, fname=None):
        itemdata = []
        for i, line in enumerate(column_data.split("\n")):
            line = line.split("#")[0].strip()
            if line == "":
                continue

            column_values = [x.strip() for x in line.split(",")]
            if len(column_values) != len(column_names):
                raise ValueError(f"""Mismatch between number of iterator names ({len(column_names)}) and values ({len(column_values)}) in {fname} at line {lineno+i+1}.""")

            itemdata.append(dict(zip(column_names, column_values)))
        return itemdata


    def remap_submit_variables(self, mappings, submit_obj):
        if len(mappings) == 0:
            return submit_obj
        for submit_key in submit_obj.keys():
            if "$(" in submit_obj[submit_key]:
                for map_from, map_to in mappings.items():
                    submit_obj[submit_key] = re.sub(
                        fr"\$\({map_from}\)",
                        f"$({map_to})",
                        submit_obj[submit_key],
                        flags = re.IGNORECASE
                    )
        return submit_obj


class Remove(Verb):
    """
    Removes a given job set
    """

    options = {
        "job_set_name": {
            "args": ("job_set_name",),
            "help": "Job set name",
        },
        "owner": {
            "args": ("--owner",),
            "help": "Remove sets from the named user instead of your own sets (requires superuser)",
            "default": getpass.getuser(),
        },
    }

    def __init__(self, logger, job_set_name, **options):

        schedd = htcondor.Schedd()

        try:
            remove_ad = schedd.act(
                htcondor.JobAction.Remove,
                job_spec=f"(JobSetName == {classad.quote(job_set_name)}) && (Owner == {classad.quote(options['owner'])})",
                reason=f"via htcondor jobset remove (by user {getpass.getuser()})"
            )
        except Exception as e:
            logger.error(f"Error while trying to remove job set {job_set_name}:\n{str(e)}")

        logger.info(f"Removed {remove_ad['TotalSuccess']} jobs matching job set {job_set_name} for user {options['owner']}.")
        if remove_ad.get("TotalError", 0) > 0:
            logger.warning(f"Warning: Could not remove {remove_ad['TotalError']} matching jobs.")


class Status(Verb):
    """
    Displays given active job set status
    """

    options = {
        "job_set_name": {
            "args": ("job_set_name",),
            "help": "Job set name",
        },
        "owner": {
            "args": ("--owner",),
            "help": "Show sets from the named user instead of your own sets",
            "default": getpass.getuser(),
        },
        "skip_history": {
            "args": ("--skip-history",),
            "action": "store_true",
            "default": False,
            "help": "Skip checking history for completed or removed job clusters",
        },
    }

    def __init__(self, logger, job_set_name, **options):

        status_mapping = {
            "NumIdle":      "{jobs} job{s} idle",
            "NumRunning":   "{jobs} job{s} running",
            "NumHeld":      "{jobs} job{s} held",
            "NumCompleted": "{jobs} job{s} completed",
            "NumSuspended": "{jobs} job{s} suspended",
            "NumRemoved":   "{jobs} job{s} removed",
            "NumSchedulerIdle":      "{jobs} DAGMan scheduler job{s} idle",
            "NumSchedulerRunning":   "{jobs} DAGMan scheduler job{s} running",
            "NumSchedulerHeld":      "{jobs} DAGMan scheduler job{s} held",
            "NumSchedulerCompleted": "{jobs} DAGMan scheduler job{s} completed",
            "NumSchedulerRemoved":   "{jobs} DAGMan scheduler job{s} removed",
        }
        status_order = ["NumIdle", "NumRunning", "NumHeld", "NumCompleted", "NumSuspended", "NumRemoved",
            "NumSchedulerIdle", "NumSchedulerRunning", "NumSchedulerHeld", "NumSchedulerCompleted", "NumSchedulerRemoved"]
        status_required = {"NumIdle", "NumRunning", "NumCompleted"}

        schedd = htcondor.Schedd()

        # Get the job set info
        job_set_ads = schedd.query(
            constraint = f"""(MyType == "JobSet") && (JobSetName == {classad.quote(job_set_name)}) && (Owner == {classad.quote(options["owner"])})""",
            opts = htcondor.QueryOpts.IncludeJobsetAds,
        )
        if len(job_set_ads) == 0:
            logger.error(f"""No active job sets found matching "{job_set_name}" for user {options["owner"]}.""")
            # TODO: check history?
            return
        if len(job_set_ads) > 1:  # This should never happen.
            logger.warning(f"""Found multiple job sets matching "{job_set_name}" for user {options["owner"]}, displaying JobSetId {job_set_ads[0]["JobSetId"]}.""")
        job_set_ad = job_set_ads[0]
        job_set_id = job_set_ad["JobSetId"]
        job_set_name = job_set_ad["JobSetName"]

        # Build the list of job statuses
        statuses = []
        for status in status_order:
            if status in status_required or job_set_ad.get(status, 0) > 0:
                n = job_set_ad.get(status, 0)
                statuses.append(status_mapping[status].format(jobs=n, s=s(n)))

        # Get the cluster ids from all of the jobs in the job set
        # (completed/removed jobs may be in the history)
        job_ads_in_queue = schedd.query(constraint = f"JobSetId == {job_set_id}", projection = ["ClusterId"])
        job_ads_in_history = []
        if (len(job_ads_in_queue) < (job_set_ad["NumJobs"] + job_set_ad["NumSchedulerJobs"])) and not options["skip_history"]:
            try:
                logger.info(f"{job_set_name} may contain completed and/or removed jobs, checking history...")
                logger.info("(This may take a while, Ctrl-C to skip.)")
                job_ads_in_history = schedd.history(constraint = f"JobSetId == {job_set_id}", projection = ["ClusterId"])
            except KeyboardInterrupt:
                logger.warning(f"History check skipped.")
        job_ads = list(job_ads_in_queue) + list(job_ads_in_history)

        # Total up the number of jobs in each cluster
        clusters = defaultdict(int)
        for job_ad in job_ads:
            clusters[job_ad["ClusterId"]] += 1
        clusters = [f"\tJob cluster {cid} with {n} total job{s(n)}" for cid, n in clusters.items()]
        clusters.sort()

        # Print info
        logger.info(f"{job_set_name} currently has {', '.join(statuses[:-1])}, and {statuses[-1]}.")
        logger.info(f"{job_set_name} contains:")
        logger.info("\n".join(clusters))


class List(Verb):
    """
    List all activate job sets
    """

    options = {
        "allusers": {
            "args": ("--allusers",),
            "help": "List active sets from all users",
            "action": "store_true",
        },
    }

    def __init__(self, logger, **options):

       schedd = htcondor.Schedd()

       constraint = """(MyType == "JobSet")"""
       if not options["allusers"]:
           constraint += f" && (Owner == {classad.quote(getpass.getuser())})"
       job_set_ads = schedd.query(
            constraint = constraint,
            projection = ["Owner", "JobSetName"],
            opts = htcondor.QueryOpts.IncludeJobsetAds,
        )

       if len(job_set_ads) == 0:
           logger.error(f"""No active job sets found.""")
           return

       owner_sets = set()
       for ad in job_set_ads:
           owner_sets.add((ad["Owner"], ad["JobSetName"]))

       owner_sets = list(owner_sets)
       owner_sets.sort()

       if options["allusers"]:
           owner_len = 0
           for (owner, dummy) in owner_sets:
               owner_len = max(owner_len, len(owner))
           fmt = f"{{owner:<{owner_len}}}  {{job_set_name}}"
           header = fmt.format(owner="OWNER", job_set_name="JOB_SET_NAME")
       else:
           fmt = "{job_set_name}"
           header = "JOB_SET_NAME"

       print(header)
       for (owner, job_set_name) in owner_sets:
           print(fmt.format(owner=owner, job_set_name=job_set_name))


class JobSet(Noun):
    """
    Run operations on HTCondor job sets
    """

    class submit(Submit):
        pass

    class list(List):
        pass

    class status(Status):
        pass

    class remove(Remove):
        pass

    @classmethod
    def verbs(cls):
        return [cls.submit, cls.list, cls.status, cls.remove]
