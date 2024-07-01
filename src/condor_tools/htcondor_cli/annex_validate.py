from typing import (
	Optional,
	List,
	Dict,
)

#
# We need a little in the way of formal structure for the system/queue table
# because Bridges 2 and Perlmutter don't behave anything like the others.
#
# Note that `queues` must be a dict for the help messages to work right,
# but the data structure can otherwise be empty.
#
class System:
    def __init__(self, *,
        pretty_name: str,
        host_name: str,
        default_queue: str,
        batch_system: str,
        executable: str,
        other_scripts: list,
        allocation_reqd: bool = False,
        queues: dict,
    ):
        assert isinstance(pretty_name, str)
        self.pretty_name = pretty_name

        assert isinstance(host_name, str)
        self.host_name = host_name

        assert isinstance(default_queue, str)
        self.default_queue = default_queue

        assert isinstance(batch_system, str)
        self.batch_system =  batch_system

        assert isinstance(executable, str)
        self.executable = executable

        assert isinstance(other_scripts, list)
        self.other_scripts = other_scripts

        assert isinstance(allocation_reqd, bool)
        self.allocation_required = allocation_reqd

        assert isinstance(queues, dict)
        self.queues = queues


    def __str__(self):
        rv = f"{self.__class__.__name__}("
        rv += f"pretty_name='{self.pretty_name}'"
        rv += f", host_name='{self.host_name}'"
        rv += f", default_queue='{self.default_queue}'"
        rv += f", batch_system='{self.batch_system}'"
        rv += f", executable='{self.executable}'"
        other_scripts_list = ", ".join(other_scripts)
        rv += f", other_scripts='{other_scripts_list}'"
        rv += f", allocation_required={self.allocation_required}"
        rv += ")"
        return rv


    def get_constraints(self, queue_name: str, gpus: Optional[int], gpu_type: Optional[str]) -> Optional[dict]:
        return self.queues.get(queue_name)


    # Return the queue name.  This only needs to return the queue name because
    # Perlmutter requires -q instead of -p (like all other SLURM systems)
    # and the least-awful way to add that was by prefixing `queue_name` to
    # indicate that it was a queue instead of a partition.
    #
    # What do we suck at?  Naming things.
    def validate_system_specific_constraints(self, queue_name, cpus, mem_mb):
        return queue_name


class Bridges2System(System):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)

    def validate_system_specific_constraints(self, queue_name, cpus, mem_mb):
        if queue_name == "RM-shared":
            if mem_mb is not None:
                error_string = f"The 'RM-shared' queue assigns memory proportional to the number of CPUs.  Do not specify --mem_mb for this queue."
                raise ValueError(error_string)
        elif queue_name == "EM":
            if cpus is None or cpus % 24 != 0:
                error_string = f"The 'EM-shared' queue only allows requests of 24, 47, 72, or 96 CPUs.  Use --cpus to specify."
                raise ValueError(error_string)
            if mem_mb is not None:
                error_string = f"The 'EM-shared' queue assigns memory proportional to the number of CPUs.  Do not specify --mem_mb for this queue."
                raise ValueError(error_string)
        elif queue_name == "GPU-shared":
            if cpus is not None or mem_mb is not None:
                error_string = f"The 'GPU-shared' queue assigns CPUs and memory proportional to the number of GPUs.  Do not specify --cpus or --mem_mb for this partition."
                raise ValueError(error_string)
        else:
            pass
        return queue_name

    def get_constraints(self, queue_name, gpus, gpu_type):
        queue = self.queues.get(queue_name)
        if queue is None:
            return None

        # Don't add valid GPU information to a queue description just
        # because the user asked for it.
        if not queue_name.startswith("GPU"):
            return queue

        # Bridges 2 has three different sets of GPU machines, each of
        # of which can be specified in either the whole-node ("GPU")
        # or shared ("GPU-shared") queue.
        #
        # If the user didn't asked for a specific GPU type, tell them
        # which type to ask for.

        if gpus == 16 and gpu_type == "v100-32":
            queue = { **queue, **{
                "cores_per_node":       48,
                "ram_per_node":         1536 * 1024,
                "gpus_per_node":        16,
                "max_nodes_per_job":    1,
                },
            }
        elif gpu_type == "v100-32":
            queue = { **queue, **{
                "cores_per_node":       40,
                "ram_per_node":         512 * 1024,
                "gpus_per_node":        8,
                "max_nodes_per_job":    4,
                },
            }
        elif gpu_type == "v100-16":
            queue = { **queue, **{
                "cores_per_node":       40,
                "ram_per_node":         192 * 1024,
                "gpus_per_node":        8,
                "max_nodes_per_job":    4,
                },
            }
        else:
            error_string = f"The system {self.pretty_name} has the following types of GPUs: v100-16, v100-32.  Use --gpu-type to select."
            raise ValueError(error_string)

        # The GPU-shared queue only allows 4 GPUs per node.
        if queue_name == "GPU-shared":
            queue["gpus_per_node"] = 4

        return queue


class PerlmutterSystem(System):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)

    def validate_system_specific_constraints(self, queue_name, cpus, mem_mb):
        return f"q,{queue_name}"

    def get_constraints(self, queue_name, gpus, gpu_type):
        queue = self.queues.get(queue_name)
        if queue is None:
            return None

        if gpus is None or gpus <= 0:
            if queue_name == "regular":
                return { **queue,
                    "max_nodes_per_job":    3072,
                    "cores_per_node":       128,
                    "ram_per_node":         512 * 1023,
                }
            elif queue_name == "debug":
                return { **queue,
                    "cores_per_node":       128,
                    "ram_per_node":         512 * 1024,
                }
            elif queue_name == "shared":
                return queue
            else:
                return None
        else:
            if queue_name == "regular":
                return { **queue,
                    "max_nodes_per_job":    1536,
                    "cores_per_node":       64,
                    "ram_per_node":         256 * 1024,

                    "gpus_per_node":        4,
                }
            elif queue_name == "debug":
                return { **queue,
                    "cores_per_node":       64,
                    "ram_per_node":         256 * 1024,

                    "gpus_per_node":        4,
                }
            elif queue_name == "shared":
                error_string = f"'{queue_name}' is not a GPU queue on the system named '{self.pretty_name}'."
                raise ValueError(error_string)
            else:
                return None


#
# For queues with the whole-node allocation policy, cores and RAM -per-node
# are only informative at the moment; we could compute the necessary number
# of nodes for the user (as long as we echo it back to them), but let's not
# do that for now and just tell the user to size their requests with the
# units appropriate to the queue.
#
# * The key is currently both the value of the command-line option.
# * We omit queues (partitions) which this tool doesn't support.
# * ram_per_node is in MB.
#

SYSTEM_TABLE = {
    "perlmutter": PerlmutterSystem( **{
        "pretty_name":      "Perlmutter",
        "host_name":        "perlmutter-p1.nersc.gov",
        "default_queue":    "regular",
        "batch_system":     "SLURM",
        "executable":       "spark.sh",
        "other_scripts":    ["spark.pilot", "spark.multi-pilot"],
        "allocation_reqd":  True,  # Only for GPUs, oddly.

        # Actually "QoS" limits.  See get_constraints().
        "queues": {
            "regular": {
                "max_duration":         12 * 60 * 60,
                "allocation_type":      "node",

                "max_jobs_in_queue":    5000,
            },
            "debug": {
                "max_nodes_per_job":    8,
                "max_duration":         30 * 60,
                "allocation_type":      "node",

                "max_jobs_in_queue":    5,
            },
            "shared": {
                "max_nodes_per_job":    1,
                "max_duration":         6 * 60 * 60,
                "allocation_type":      "cores_or_ram",
                "cores_per_node":       64,
                "ram_per_node":         256 * 1024,

                "max_jobs_in_queue":    5000,
            },
        },
    }
    ),

    "delta": System( **{
        "pretty_name":      "Delta",
        "host_name":        "login.delta.ncsa.illinois.edu",
        "default_queue":    "cpu",
        "batch_system":     "SLURM",
        "executable":       "spark.sh",
        "other_scripts":    ["spark.pilot", "spark.multi-pilot"],
        "allocation_reqd":  True,

        "queues": {
            "cpu": {
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "cores_or_ram",
                "cores_per_node":       128,
                "ram_per_node":         256 * 1024,
                "max_nodes_per_job":    4, # "TBD"
            },
            "cpu-interactive": {
                "max_duration":         30 * 60,
                "allocation_type":      "cores_or_ram",
                "cores_per_node":       128,
                "ram_per_node":         256 * 1024,
                "max_nodes_per_job":    4, # "TBD"
            },
            "gpuA100x4": {
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "cores_or_ram",
                "cores_per_node":       64,
                "ram_per_node":         256 * 1024,
                "gpus_per_node":        4,
                "max_nodes_per_job":    4, # "TBD"
            },
            "gpuA100x4-preempt": {
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "cores_or_ram",
                "cores_per_node":       64,
                "ram_per_node":         256 * 1024,
                "gpus_per_node":        4,
                "max_nodes_per_job":    4, # "TBD"
            },
            "gpuA100x8": {
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "cores_or_ram",
                "cores_per_node":       128,
                "ram_per_node":         2048 * 1024,
                "gpus_per_node":        8,
                "max_nodes_per_job":    4, # "TBD"
            },
            "gpuA40x4": {
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "cores_or_ram",
                "cores_per_node":       64,
                "ram_per_node":         256 * 1204,
                "gpus_per_node":        4,
                "max_nodes_per_job":    4, # "TBD"
            },
            "gpuA40x4-preempt": {
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "cores_or_ram",
                "cores_per_node":       64,
                "ram_per_node":         256 * 1204,
                "gpus_per_node":        4,
                "max_nodes_per_job":    4, # "TBD"
            },
# Disabled until HTCONDOR-2377 is released (and the well-known URL for
# binaries is updated to match).
#            "gpuMI100x8": {
#                "max_duration":         48 * 60 * 60,
#                "allocation_type":      "cores_or_ram",
#                "cores_per_node":       128,
#                "ram_per_node":         2048 * 1024,
#                "gpus_per_node":        8,
#                "max_nodes_per_job":    4, # "TBD"
#            },
        },
    }
    ),

    "spark": System( **{
        "pretty_name":      "Spark",
        "host_name":        "hpclogin3.chtc.wisc.edu",
        "default_queue":    "shared",
        "batch_system":     "SLURM",
        "executable":       "spark.sh",
        "other_scripts":    ["spark.pilot", "spark.multi-pilot"],
        "allocation_reqd":  False,

        "queues": {
            "shared": {
                "max_duration":         7* 24 * 60 * 60,
                "allocation_type":      "cores_or_ram",
                "cores_per_node":       64,             # or 128
                "ram_per_node":         512,
                "max_nodes_per_job":    2,              # "max 320 cores/job"
            },
            "int": {
                "max_duration":         4 * 60 * 60,
                "allocation_type":      "cores_or_ram",
                "cores_per_node":       64,             # or 128
                "ram_per_node":         512,            # "max 64 per job"
                "max_nodes_per_job":    1,              # "max 16 cores/job"
            },
            "pre": {
                "max_duration":         24 * 60 * 60,
                "allocation_type":      "cores_or_ram",
                "cores_per_node":       64,             # or 128
                "ram_per_node":         512,
                "max_nodes_per_job":    2,              # "max 320 cores/job"
            },
        },
    }
    ),

    "stampede2": System( **{
        "pretty_name":      "Stampede 2",
        "host_name":        "stampede2.tacc.utexas.edu",
        "default_queue":    "normal",
        "batch_system":     "SLURM",
        "executable":       "hpc.sh",
        "other_scripts":    ["hpc.pilot", "hpc.multi-pilot"],
        "allocation_reqd":  False,

        "queues": {
            "normal": {
                "max_nodes_per_job":    256,
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "node",
                "cores_per_node":       68,
                "ram_per_node":         96 * 1024,

                "max_jobs_in_queue":    50,
            },
            "development": {
                "max_nodes_per_job":    16,
                "max_duration":         2 * 60 * 60,
                "allocation_type":      "node",
                "cores_per_node":       68,
                "ram_per_node":         96 * 1024,

                "max_jobs_in_queue":    1,
            },
            "skx-normal": {
                "max_nodes_per_job":    128,
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "node",
                "cores_per_node":       48,
                "ram_per_node":         192 * 1024,

                "max_jobs_in_queue":    20,
            },
        },
    },
    ),

    "expanse": System( **{
        "pretty_name":      "Expanse",
        "host_name":        "login.expanse.sdsc.edu",
        "default_queue":    "compute",
        "batch_system":     "SLURM",
        "executable":       "spark.sh",
        "other_scripts":    ["spark.pilot", "spark.multi-pilot"],
        "allocation_reqd":  True,

        "queues": {
            "compute": {
                "max_nodes_per_job":    32,
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "node",
                "cores_per_node":       128,
                "ram_per_node":         256 * 1024,

                "max_jobs_in_queue":    64,
            },
            "gpu": {
                "max_nodes_per_job":    4,
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "node",
                "cores_per_node":       40,
                "ram_per_node":         384 * 1024,
                "gpus_per_node":        4,

                "max_jobs_in_queue":    8,

                "gpu_flag_type":        "job",
            },
            "shared": {
                "max_nodes_per_job":    1,
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "cores_or_ram",
                "cores_per_node":       128,
                "ram_per_node":         256 * 1024,

                "max_jobs_in_queue":    4096,
            },
            "gpu-shared": {
                "max_nodes_per_job":    1,
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "cores_or_ram",
                "cores_per_node":       40,
                "ram_per_node":         384 * 1024,
                "gpus_per_node":        4,

                "max_jobs_in_queue":    24,

                "gpu_flag_type":        "job",
            },
        },
    },
    ),

    "anvil": System( **{
        "pretty_name":      "Anvil",
        "host_name":        "anvil.rcac.purdue.edu",
        "default_queue":    "wholenode",
        "batch_system":     "SLURM",
        "executable":       "spark.sh",
        "other_scripts":    ["spark.pilot", "spark.multi-pilot"],
        "allocation_reqd":  False,

        "queues": {
            "wholenode": {
                "max_nodes_per_job":    16,
                "max_duration":         96 * 60 * 60,
                "allocation_type":      "node",
                "cores_per_node":       128,
                "ram_per_node":         256000,

                "max_jobs_in_queue":    128,
            },
            "wide": {
                "max_nodes_per_job":    56,
                "max_duration":         12 * 60 * 60,
                "allocation_type":      "node",
                "cores_per_node":       128,
                "ram_per_node":         256000,

                "max_jobs_in_queue":    10,
            },
            "shared": {
                "max_nodes_per_job":    1,
                "max_duration":         96 * 60 * 60,
                "allocation_type":      "cores_or_ram",
                "cores_per_node":       128,
                "ram_per_node":         256000,

                "max_jobs_in_queue":    None,
            },
            # Max of 12 GPUs per user and 32 per allocation.  Since
            # every node has 4 GPUs, and you get the whole node....
            "gpu": {
                "max_nodes_per_job":    3,
                "max_duration":         48 * 60 * 60,
                "max_jobs_in_queue":    None,

                "allocation_type":      "node",

                "cores_per_node":       128,
                "ram_per_node":         512000,

                "gpus_per_node":        4,
            },
            "gpu-debug": {
                "max_nodes_per_job":    1,
                "max_duration":         30 * 60,
                "max_jobs_in_queue":    1,

                "allocation_type":      "node",

                "cores_per_node":       128,
                "ram_per_node":         512000,

                "gpus_per_node":        2,
            },
        },
    },
    ),

    "bridges2": Bridges2System( **{
        "pretty_name":      "Bridges-2",
        "host_name":        "bridges2.psc.edu",
        "default_queue":    "RM",
        "executable":       "spark.sh",
        "other_scripts":    ["spark.pilot", "spark.multi-pilot"],
        "batch_system":     "SLURM",
        "allocation_reqd":  False,

        "queues": {
            "RM": {
                "max_nodes_per_job":    50,
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "node",
                "cores_per_node":       128,
                "ram_per_node":         253000,

                "max_jobs_in_queue":    50,
            },
            "RM-512": {
                "max_nodes_per_job":    2,
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "node",
                "cores_per_node":       128,
                "ram_per_node":         515000,

                "max_jobs_in_queue":    50,
            },
            "RM-shared": {
                "max_nodes_per_job":    1,
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "cores_or_ram",
                "cores_per_node":       64,
                "ram_per_node":         253000 // 2,

                "max_jobs_in_queue":    50,
            },
            "EM": {
                "max_nodes_per_job":    1,
                "max_duration":         120 * 60 * 60,
                "allocation_type":      "cores_or_ram",
                "cores_per_node":       96,
                # The SLURM config for the EM queue specifies "MaxMemPerCPU"
                "ram_per_node":         42955 * 96,

                "max_jobs_in_queue":    50,
            },

            # Deliberately incomplete.  See get_constraints().
            "GPU": {
                "max_nodes_per_job":    4,
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "node",
                "gpu_types":            [ 'v100-32', 'v100-16' ],
                "gpu_flag_type":        "job",
            },
            "GPU-shared": {
                "max_nodes_per_job":    1,
                "max_duration":         48 * 60 * 60,
                "allocation_type":      "gpu-shared",
                "gpu_types":            [ 'v100-32', 'v100-16' ],
                "gpu_flag_type":        "job",
            },
        },
    },
    ),

    "path-facility": System( **{
        "pretty_name":      "PATh Facility",
        "host_name":        "ap1.facility.path-cc.io",
        "default_queue":    "cpu",
        "batch_system":     "HTCondor",
        "executable":       "path-facility.sh",
        "other_scripts":    ["path-facility.pilot", "path-facility.multi-pilot"],
        "allocation_reqd":  False,

        "queues": {
            "cpu": {
                # This is actually max-jobs-per-request for this system,
                # but for now, it's easier to think about each request
                # being a single job like it is for SLURM.
                "max_nodes_per_job":    1, # FIXME
                "max_duration":         60 * 60 * 72, # FIXME
                "allocation_type":      "cores_or_ram",
                "cores_per_node":       64,
                "ram_per_node":         244 * 1024,

                "max_jobs_in_queue":    1000, # FIXME
            },
            # When we formally support GPUs, we'll need to add the GPU
            # queue here, and -- because HTCondor doesn't have multiple
            # queues -- some special code somewhere to insert request_gpus.
            # (Seems like we should allow the user to specify how many
            # GPUs per node for the PATh Facility, too, so check that
            # against the queue name (warn if cpu queue?) and also default
            # it to 1 if the queue name is 'gpu'?)
            #
            # Also, we may not want to call them "nodes" for the PATh
            # facility.
        },
    },
    ),
}


def validate_constraints( *,
    system,
    queue_name,
    nodes,
    lifetime_in_seconds,
    allocation,
    cpus,
    mem_mb,
    idletime_in_seconds,
    gpus,
    gpu_type,
):
    system = SYSTEM_TABLE[system]

    # (A) Is the queue a valid, known, and supported?
    queue = system.get_constraints(queue_name, gpus, gpu_type)
    if queue is None:
        pretty_name = system.pretty_name
        queue_list = "\n    ".join(system.queues)
        default_queue = system.default_queue

        error_string = f"'{queue_name}' is not a supported queue on the system named '{pretty_name}'."
        error_string = f"{error_string}  Supported queues are:\n    {queue_list}\nUse '{default_queue}' if you're not sure."
        raise ValueError(error_string)

    # (B) If the user did not specify CPUs or memory, then the queue's
    # allocation type must be 'node'.  (We supply a default number of nodes.)
    if cpus is None and mem_mb is None:
        if queue['allocation_type'] == 'cores_or_ram':
            error_string = f"The '{queue_name}' queue is not a whole-node queue.  You must specify either CPUs (--cpus) or memory (--mem_mb)."
            raise ValueError(error_string)

    # (C) If the queue's allocation type is 'node', then specifying CPUs or
    # memory is pointless.
    if queue['allocation_type'] == 'node':
        if cpus is not None or mem_mb is not None:
            error_string = f"The '{queue_name}' queue is a whole-node queue.  You can't specify CPUs (--cpus) or memory (--mem_mb)."
            raise ValueError(error_string)

        # Don't allow the user to request more than max nodes.
        mnpj = queue['max_nodes_per_job']
        if nodes > mnpj:
            error_string = f"The '{queue_name}' queue is limited to {mnpj} nodes per job.  Use --nodes to set."
            raise ValueError(error_string)

    # (D) If the queue's allocation type is 'cores_or_ram', you must specify
    # CPUs or memory.  (This might not actually be true, technically.)
    # The first check is presently redundant with check (A).
    #
    # When you specify CPUs or memory, you must specify less than the max.
    if queue['allocation_type'] == 'cores_or_ram':
        if cpus is None and mem_mb is None:
            error_string = f"The '{queue_name}' queue is a shared-node queue.  You must specify CPUs (--cpus) or memory (--mem_mb)."
            raise ValueError(error_string)

        rpn = queue['ram_per_node']
        if mem_mb is not None and mem_mb > rpn:
            error_string = f"The '{queue_name}' queue is limited to {rpn} MB per node.  Use --mem_mb to set."
            raise ValueError(error_string)

        if cpus is not None and cpus > queue['cores_per_node']:
            error_string = f"The '{queue_name}' queue is limited to {queue['cores_per_node']} cores per node."
            raise ValueError(error_string)

        # Don't allow the user to request more than max nodes.
        mnpj = queue['max_nodes_per_job']
        if nodes > mnpj:
            error_string = f"The '{queue_name}' queue is limited to {mnpj} nodes per job.  Use --nodes to set."
            raise ValueError(error_string)

    # (J) If the queue's allocation type is 'gpu-shared', then specifying
    #     CPUs or memory is pointless.
    if queue['allocation_type'] == 'gpu-shared':
        if cpus is not None or mem_mb is not None:
            error_string = f"The '{queue_name}' queue always assigns CPUs and RAM in proportion to the ratio of requested GPUs to total GPUs.  You can't specify CPUs (--cpus) or memory (--mem_mb)."
            raise ValueError(error_string)

        # Don't allow the user to request more than max nodes.
        mnpj = queue['max_nodes_per_job']
        if nodes > mnpj:
            error_string = f"The '{queue_name}' queue is limited to {mnpj} nodes per job.  Use --nodes to set."
            raise ValueError(error_string)

    # (E) You must not specify a lifetime longer than the queue's duration.
    if queue['max_duration'] < lifetime_in_seconds:
        error_string = f"The '{queue_name}' queue has a maximum duration of {queue['max_duration']} seconds, which is less than the requested lifetime ({lifetime_in_seconds} seconds).  Use --lifetime to set."
        raise ValueError(error_string)

    # (F) You must not specify an idletime longer than the queue's duration.
    #     There's an argument that doing so is the natural way to disable
    #     the idleness check entirely, so let's skip this check for now.
    # if queue['max_duration'] < idletime_in_seconds:
    #    error_string = f"The '{queue_name}' queue has a maximum duration of {queue['max_duration']} seconds, which is less than the requeqested lifetime ({idletime_in_seconds} seconds).  Use --idle-time to set."
    #    raise ValueError(error_string)

    # (G) You must not specify an idletime longer than the lifetime.
    if lifetime_in_seconds < idletime_in_seconds:
        error_string = f"You may not specify an idle time (--idle-time) longer than the lifetime (--lifetime)."
        raise ValueError(error_string)

    # (H) Some systems require an allocation be specified, even if it's
    #     your only one.
    if allocation is None and system.allocation_required:
        error_string = f"The system named '{system.pretty_name}' requires you to specify a project (--project), even if you only have one."
        raise ValueError(error_string)

    # (I) You must request an appropriate number of GPUs if the queue
    #     offer GPUs (unless it's a whole-node queue and you don't
    #     specify GPUs on the command-line at all, in which case we
    #     supply the default).
    gpus_per_node = queue.get('gpus_per_node')
    if gpus_per_node is not None:
        if gpus is None:
            if queue['allocation_type'] == 'node':
                gpus = gpus_per_node

        if gpus <= 0:
            error_string = f"The '{queue_name}' queue is a GPU queue.  You must specify a number of GPUs (--gpus)."
            raise ValueError(error_string)

        if gpus > gpus_per_node:
            error_string = f"The '{queue_name}' queue is limited to {gpus_per_node} GPUs per node.  Use --gpus to set."
            raise ValueError(error_string)

        # If the allocation type is node, you get (and are charged for) all
        # of the GPUs whether you asked for them or not.
        if queue['allocation_type'] == 'node':
            if gpus < gpus_per_node:
                error_string = f"The '{queue_name}' queue always assigns {gpus_per_node} GPUs per node.  Set --gpus to match."
                raise ValueError(error_string)

    # (L) You must not request GPUs from queues which don't offer them.
    if gpus_per_node is None:
        if gpus is None or gpus <= 0:
            pass
        else:
            error_string = f"The '{queue_name}' queue does not offer GPUs.  Do not set --gpus for this queue."
            raise ValueError(error_string)

    # (M) You must not specify a GPU type for a queue which doesn't offer them.
    #     You must specify a valid GPU type for queues which do.
    if gpu_type is not None:
        gpu_types = queue.get('gpu_types')
        if queue.get('gpu_types') is None:
            error_string = f"The '{queue_name}' queue only has one type of GPU.  Do not set --gpu-type for this queue."
            raise ValueError(error_string)
        elif not gpu_type in gpu_types:
            gpu_type_list = ", ".join(gpu_types)
            error_string = f"The system {system.pretty_name} has the following types of GPUs: {gpu_type_list}.  Use --gpu-type to select."
            raise ValueError(error_string)

    # (K) Run the system-specific constraint checker.
    queue_name = system.validate_system_specific_constraints(queue_name, cpus, mem_mb)

    if gpus_per_node is not None:
        if queue.get('gpu_flag_type') == 'job':
            if gpus is not None and gpus > 0:
                assert nodes is not None and nodes >= 1, f"Internal error during validation: node count ({nodes}) should have been >= 1 by now."
                gpus = gpus * nodes

    return gpus, queue_name
