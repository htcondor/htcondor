import time
import re
import getpass

from datetime import datetime
from copy import deepcopy

import htcondor
import classad

from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb

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
                        f"\$\({map_from}\)",
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
        "nobatch": {
            "args": ("--nobatch",),
            "help": "Display individual job clusters",
            "action": "store_true",
        },
        "owner": {
            "args": ("--owner",),
            "help": "Show sets from the named user instead of your own sets",
            "default": getpass.getuser(),
        },
    }

    def __init__(self, logger, job_set_name, **options):

        schedd = htcondor.Schedd()

        job_set_ads = schedd.query(
            constraint = f"(JobSetName == {classad.quote(job_set_name)}) && (Owner == {classad.quote(options['owner'])})",
            projection = ["ClusterId", "ProcId", "JobStatus", "QDate", "JobBatchName", "JobSetName"]
        )

        if len(job_set_ads) == 0:
            logger.error(f"""No active job sets found matching "{job_set_name}" for user {options['owner']}.""")
            return

        template = {
            "BATCH_NAME": "",
            "DONE": 0,
            "RUN": 0,
            "IDLE": 0,
            "REMOVED": 0,
            "HOLD": 0,
            "SUSPENDED": 0,
            "TOTAL": 0,
            "JobIds": [],
            "QDates": [],
        }

        job_set = deepcopy(template)
        job_set["QDates"] = []

        clusters = {}
        for ad in job_set_ads:
            job_set["BATCH_NAME"] = f"Set: {ad['JobSetName']}"
            job_set["JobIds"].append(f"{ad['ClusterId']}.{ad['ProcId']}")
            job_set["QDates"].append(ad.get("QDate", int(time.time())))
            job_set["DONE"] += ad["JobStatus"] == 4
            job_set["RUN"] += ad["JobStatus"] == 2 or ad["JobStatus"] == 6
            job_set["IDLE"] += ad["JobStatus"] == 1
            job_set["REMOVED"] += ad["JobStatus"] == 3
            job_set["HOLD"] += ad["JobStatus"] == 5
            job_set["SUSPENDED"] += ad["JobStatus"] == 7
            job_set["TOTAL"] += 1

            if options["nobatch"]:
                cluster = ad["ClusterId"]
                if cluster not in clusters:
                    clusters[cluster] = deepcopy(template)
                    del clusters[cluster]["JobIds"]
                    clusters[cluster]["ProcIds"] = []
                    clusters[cluster]["BATCH_NAME"] = ad.get("JobBatchName", f"ID: {cluster}")
                clusters[cluster]["ProcIds"].append(ad['ProcId'])
                clusters[cluster]["QDates"].append(ad.get("QDate", int(time.time())))
                clusters[cluster]["DONE"] += ad["JobStatus"] == 4
                clusters[cluster]["RUN"] += ad["JobStatus"] == 2 or ad["JobStatus"] == 6
                clusters[cluster]["IDLE"] += ad["JobStatus"] == 1
                clusters[cluster]["REMOVED"] += ad["JobStatus"] == 3
                clusters[cluster]["HOLD"] += ad["JobStatus"] == 5
                clusters[cluster]["SUSPENDED"] += ad["JobStatus"] == 7
                clusters[cluster]["TOTAL"] += 1

        if len(job_set["JobIds"]) > 1:
            job_set["JobIds"].sort()
            job_set["JOB_IDS"] = f"{job_set['JobIds'][0]}-{job_set['JobIds'][-1]}"
            job_set["QDates"].sort()
        else:
            job_set["JOB_IDS"] = job_set["JobIds"][0]
        job_set["SUBMITTED"] = datetime.fromtimestamp(job_set["QDates"][0]).strftime("%m/%d %H:%M")

        if options["nobatch"]:
            for cluster in clusters:
                if len(clusters[cluster]["ProcIds"]) > 1:
                    clusters[cluster]["ProcIds"].sort()
                    clusters[cluster]["JOB_IDS"] = f"{cluster}.{clusters[cluster]['ProcIds'][0]}-{clusters[cluster]['ProcIds'][-1]}"
                    clusters[cluster]["QDates"].sort()
                else:
                    clusters[cluster]["JOB_IDS"] = f"{cluster}.{clusters[cluster]['ProcIds'][0]}"
                clusters[cluster]["SUBMITTED"] = datetime.fromtimestamp(clusters[cluster]["QDates"][0]).strftime("%m/%d %H:%M")


        print_job_set = deepcopy(job_set)
        cols = ["BATCH_NAME", "SUBMITTED", "DONE", "RUN", "IDLE", "REMOVED", "HOLD", "SUSPENDED", "TOTAL", "JOB_IDS"]
        skip_cols = {"REMOVED", "HOLD", "SUSPENDED"}
        left_justify_cols = {"BATCH_NAME", "JOB_IDS"}
        col_len = {}
        col_jus = {}
        del_cols = []
        for i, col in enumerate(cols):
            if (col in skip_cols) and (print_job_set[col] == 0):
                del_cols.append(i)
                continue
            elif isinstance(print_job_set[col], int):
                if print_job_set[col] == 0:
                    print_job_set[col] = "-"
                else:
                    print_job_set[col] = str(print_job_set[col])
            col_len[col] = max(len(col), len(print_job_set[col]))
            if col in left_justify_cols:
                col_jus[col] = "<"
            else:
                col_jus[col] = ">"

        del_cols.reverse()
        deleted_cols = [cols.pop(i) for i in del_cols]

        status_fmt = "  ".join([f"{{{col}:{col_jus[col]}{col_len[col]}.{col_len[col]}}}" for col in cols])

        header = status_fmt.format(**{col:col for col in cols})
        print()
        print(f"-- Schedd: {htcondor.param['FULL_HOSTNAME']} : {schedd.location.address.split('?')[0]}?... @ {datetime.now().strftime('%m/%d/%y %H:%M:%S')}")
        print(header)
        if options["nobatch"]:
            for cluster in sorted(list(clusters.keys())):
                for col in cols:
                    if isinstance(clusters[cluster][col], int):
                        if clusters[cluster][col] == 0:
                            clusters[cluster][col] = "-"
                        else:
                            clusters[cluster][col] = str(clusters[cluster][col])
                print(status_fmt.format(**clusters[cluster]))
            print("-"*len(header))
        print(status_fmt.format(**print_job_set))
        print(f"{job_set['TOTAL']} jobs; {job_set['DONE']} completed; {job_set['REMOVED']} removed; {job_set['IDLE']} idle; {job_set['RUN']} running; {job_set['HOLD']} held; {job_set['SUSPENDED']} suspended")
        print()


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

       constraint = "(InJobSet == True)"
       if not options["allusers"]:
           constraint += f" && (Owner == {classad.quote(getpass.getuser())})"
       job_set_ads = schedd.query(
            constraint = constraint,
            projection = ["Owner", "JobSetName"]
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
