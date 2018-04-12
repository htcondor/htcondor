#!usr/bin/env python3
import datetime
import itertools
import json
import enum
import random
import argparse
from pathlib import Path
from typing import Dict, Union, Iterable, List


class StrEnum(str, enum.Enum):
    def __repr__(self):
        return self.value


class JobStatus(StrEnum):
    UNEXPANDED = 'Unexpanded'
    IDLE = 'Idle'
    RUNNING = 'Running'
    REMOVED = 'Removed'
    COMPLETED = 'Completed'
    HELD = 'Held'
    SUBMISSION_ERROR = 'Submission_err'


class SubmitterRecord:
    NAME_KEY = 'Name'
    DATE_KEY = 'Date'
    MACHINE_KEY = 'Machine'
    JOBS_KEY = 'Jobs'
    JOB_STATUS_KEY = 'JobStatus'
    COUNT_KEY = 'Count'

    def __init__(
        self,
        *,
        date: datetime.datetime = None,
        name: str,
        machine: str,
        jobs: Dict[JobStatus, int],
    ):
        self.date = date
        self.name = name
        self.machine = machine
        self.jobs = jobs

    def to_json(self):
        j_base = {
            self.NAME_KEY: self.name,
            self.MACHINE_KEY: self.machine,
        }
        if self.date is not None:
            j_base[self.DATE_KEY] = datetime.datetime.strftime(self.date, '%Y-%m-%dT%H:%M:%S')

        j = [{self.JOB_STATUS_KEY: job_status, self.COUNT_KEY: num_jobs, **j_base}
             for job_status, num_jobs in self.jobs.items()]

        return j

    def to_json_compact(self):
        j = {
            self.NAME_KEY: self.name,
            self.MACHINE_KEY: self.machine,
            self.JOBS_KEY: {status: count for status, count in self.jobs.items()},
        }

        if self.date is not None:
            j[self.DATE_KEY] = datetime.datetime.strftime(self.date, '%Y-%m-%dT%H:%M:%S')

        return j


class SlotType(StrEnum):
    DYNAMIC = 'Dynamic'
    PARTITIONABLE = 'Partitionable'


class SlotState(StrEnum):
    UNCLAIMED = 'Unclaimed'
    CLAIMED = 'Claimed'


class SlotActivity(StrEnum):
    IDLE = 'Idle'
    BUSY = 'Busy'


class MachineRecord:
    DATE_KEY = 'Date'
    SLOT_TYPE_KEY = 'SlotType'
    ARCH_KEY = 'Arch'
    OPSYS_KEY = 'OpSys'
    STATE_KEY = 'State'
    ACTIVITY_KEY = 'Activity'
    CPUS_KEY = 'Cpus'

    def __init__(
        self,
        *,
        date: datetime.datetime = None,
        slot_type: SlotType,
        architecture: str,
        operating_system: str,
        state: SlotState,
        activity: SlotActivity,
        cpus: int,
    ):
        self.date = date
        self.slot_type = slot_type
        self.arch = architecture
        self.op_sys = operating_system
        self.state = state
        self.activity = activity
        self.cpus = cpus

    def to_json(self):
        j = {
            self.SLOT_TYPE_KEY: self.slot_type,
            self.ARCH_KEY: self.arch,
            self.OPSYS_KEY: self.op_sys,
            self.STATE_KEY: self.state,
            self.ACTIVITY_KEY: self.activity,
            self.CPUS_KEY: self.cpus,
        }

        if self.date is not None:
            j[self.DATE_KEY] = datetime.datetime.strftime(self.date, '%Y-%m-%dT%H:%M:%S')

        return j


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description = 'Generate mock data for HTCondor View',
    )
    parser.add_argument(
        'which',
        action = 'store',
        type = str,
        help = 'which type of data to generate: s|submitters or m|machines'
    )
    parser.add_argument(
        'output',
        action = 'store',
        type = str,
        help = 'the path to the output file',
    )
    parser.add_argument(
        '-n', '--number',
        action = 'store',
        type = int,
        default = 100,
        help = 'the number of records to create',
    )
    parser.add_argument(
        '-d', '--dated',
        action = 'store_true',
        help = 'if passed, add timestamps to the records'
    )
    parser.add_argument(
        '-s', '--submitters',
        action = 'store_true',
        help = 'produce submitter data instead of machine data',
    )
    parser.add_argument(
        '--compact_submitters',
        action = 'store_true',
        help = 'use alternate, more compact, JSON format for submitter data'
    )
    parser.add_argument(
        '-c', '--compressed',
        action = 'store_true',
        help = 'if passed, compress the output JSON',
    )
    parser.add_argument(
        '-i', '--indent',
        action = 'store',
        type = int,
        default = 2,
        help = 'the indentation of the output JSON',
    )
    parser.add_argument(
        '-m', '--machines',
        action = 'store',
        type = int,
        default = 10,
        help = 'the number of unique machine names',
    )

    return parser.parse_args()


def normalize_output_path(path: Union[Path, str]) -> Path:
    return Path(path).with_suffix('.json')


def ensure_parents_exist(path: Path):
    path.parent.mkdir(parents = True, exist_ok = True)


def flatten_list_of_lists(list_of_lists: List[List]):
    """Flatten one level of nesting."""
    return itertools.chain.from_iterable(list_of_lists)


def write_json_output(
    records: Iterable[Union[SubmitterRecord, MachineRecord]],
    output_path: Path,
    compressed: bool = False,
    indent: int = 2,
    flatten: bool = False):
    """

    Parameters
    ----------
    records
    output_path
    compressed
        If ``False``, "pretty-print" the JSON by adding indentation.
    indent
        The indentation to use if ``compressed`` is ``False``.
    flatten
        If each individual record was turned into a list of JSON objects, make this ``True`` to flatten them into a single list.
    """
    ensure_parents_exist(output_path)
    j = [record.to_json() for record in records]
    if flatten:
        j = list(flatten_list_of_lists(j))
    with output_path.open(mode = 'w') as f:
        json.dump(j, f, indent = indent if not compressed else None)


def main():
    args = parse_args()

    with open('mockmetric_data/names.txt') as f:
        NAMES = f.readlines()
    NAMES = [name.strip() + '@chtc.wisc.edu' for name in NAMES]

    with open('mockmetric_data/machines.txt') as f:
        MACHINES = f.readlines()
    MACHINES = [f'{machine.strip()}.wisc.edu' for machine in MACHINES]
    MACHINES = random.sample(MACHINES, k = args.machines)

    if args.compact_submitters:
        SubmitterRecord.to_json = SubmitterRecord.to_json_compact

    generate_submitters = args.which.lower() in ('s', 'submitters')
    generate_machines = args.which.lower() in ('m', 'machines')
    if generate_submitters:
        records = [
            SubmitterRecord(
                date = datetime.datetime.utcnow() if args.dated else None,
                name = name,
                machine = random.choice(MACHINES),
                jobs = {
                    JobStatus.IDLE: random.randint(0, 100),
                    JobStatus.RUNNING: random.randint(0, 100),
                    JobStatus.HELD: random.randint(0, 10),
                },
            )
            for name in random.sample(NAMES, k = args.number)
        ]
    elif generate_machines:
        records = [
            MachineRecord(
                date = datetime.datetime.utcnow() if args.dated else None,
                slot_type = random.choice(tuple(SlotType)),
                architecture = 'X86_64',
                operating_system = 'LINUX',
                state = random.choice(tuple(SlotState)),
                activity = random.choice(tuple(SlotActivity)),
                cpus = random.randint(0, 8),
            )
            for _ in range(args.number)
        ]
    else:
        raise RuntimeError('unknown option for WHICH')

    output_path = normalize_output_path(args.output)
    write_json_output(
        records,
        output_path,
        indent = args.indent,
        compressed = args.compressed,
        flatten = generate_submitters and not args.compact_submitters,
    )


if __name__ == '__main__':
    main()
