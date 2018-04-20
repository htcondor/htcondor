#!/usr/bin/env python3

import datetime
import functools
import itertools
import json
import enum
import os
import random
import argparse
import collections
import shutil
from copy import copy, deepcopy
from pathlib import Path
from typing import Dict, Union, Iterable, List, Tuple, Callable, Any

Number = Union[float, int]

THIS_DIR = Path(__file__).absolute().parent

with (THIS_DIR / 'fake_names' / 'users.txt').open(mode = 'r') as f:
    NAMES = f.readlines()
NAMES = [name.strip() + '@chtc.wisc.edu' for name in NAMES]

with (THIS_DIR / 'fake_names' / 'machines.txt').open(mode = 'r') as f:
    MACHINES = f.readlines()
MACHINES = [f'{machine.strip()}.wisc.edu' for machine in MACHINES]

TEXT_WIDTH = shutil.get_terminal_size((80, 20)).columns


class StrEnum(str, enum.Enum):
    def __repr__(self):
        return self.value


class RecordType(StrEnum):
    SUBMITTER = 'submitter'
    SLOT = 'slot'


class TimeSpan(StrEnum):
    DAILY = 'daily'
    WEEKLY = 'weekly'
    MONTHLY = 'monthly'

    @property
    def density(self):
        return TIMESPAN_TO_INTERVAL[self]

    @property
    def format(self):
        return TIMESPAN_TO_FORMAT[self]

    @property
    def interval(self):
        return TIMESPAN_TO_INTERVAL[self]

    @property
    def is_breakpoint(self):
        return TIMESPAN_TO_DETERMINE_BREAKPOINT[self]

    @property
    def intervals_to_keep(self):
        return TIMESPAN_TO_INTERVALS_TO_KEEP[self]

    def make_previous_interval_start(self, date):
        return TIMESPAN_TO_MAKE_PREVIOUS_INTERVAL_START[self](date)


TIMESPAN_TO_INTERVAL = {
    TimeSpan.DAILY: datetime.timedelta(minutes = 5),
    TimeSpan.WEEKLY: datetime.timedelta(hours = 1),
    TimeSpan.MONTHLY: datetime.timedelta(hours = 2),
}

TIMESPAN_TO_FORMAT = {
    TimeSpan.DAILY: '%Y-%m-%d',
    TimeSpan.WEEKLY: '%G-W%V',
    TimeSpan.MONTHLY: '%Y-%m',
}

# given any date, determine the start of the PREVIOUS interval
TIMESPAN_TO_MAKE_PREVIOUS_INTERVAL_START = {
    TimeSpan.DAILY: lambda date: (date - datetime.timedelta(days = 1)).replace(hour = 0, minute = 0, second = 0, microsecond = 0),
    TimeSpan.WEEKLY: lambda date: (date - datetime.timedelta(days = date.weekday())).replace(hour = 0, minute = 0, second = 0, microsecond = 0),
    TimeSpan.MONTHLY: lambda date: (date.replace(day = 1) - datetime.timedelta(days = 1)).replace(day = 1, hour = 0, minute = 0, second = 0, microsecond = 0),
}

TIMESPAN_TO_INTERVALS_TO_KEEP = {
    TimeSpan.DAILY: 2,
    TimeSpan.WEEKLY: 2,
    TimeSpan.MONTHLY: 12,
}

# a function that, given the current and previous dates, determines if it's time to roll the file over
TIMESPAN_TO_DETERMINE_BREAKPOINT = {
    TimeSpan.DAILY: lambda current, previous: current.day != previous.day,
    TimeSpan.WEEKLY: lambda current, previous: current.isocalendar()[1] != previous.isocalendar()[1],
    TimeSpan.MONTHLY: lambda current, previous: current.month != previous.month,
}


class Record:
    DATE_KEY = 'Date'


class JobStatus(StrEnum):
    UNEXPANDED = 'Unexpanded'
    IDLE = 'Idle'
    RUNNING = 'Running'
    REMOVED = 'Removed'
    COMPLETED = 'Completed'
    HELD = 'Held'
    SUBMISSION_ERROR = 'Submission_err'


class SubmitterRecord(Record):
    NAME_KEY = 'Name'
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


class SlotType(StrEnum):
    DYNAMIC = 'Dynamic'
    PARTITIONABLE = 'Partitionable'


class SlotState(StrEnum):
    UNCLAIMED = 'Unclaimed'
    CLAIMED = 'Claimed'


class SlotActivity(StrEnum):
    IDLE = 'Idle'
    BUSY = 'Busy'


class SlotRecord(Record):
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


def gcd_timedeltas(a: datetime.timedelta, b: datetime.timedelta):
    while b != datetime.timedelta():
        (a, b) = (b, a % b)
    return a


def make_timespan_data(record_data, make_record: Callable, now: datetime.datetime):
    # find the earliest date that we'll need
    earliest_dates = []
    for timespan in TimeSpan:
        earliest_date = now.replace(hour = 0, minute = 0, second = 0, microsecond = 0)
        for _ in range(timespan.intervals_to_keep):
            earliest_date = timespan.make_previous_interval_start(earliest_date)

        earliest_dates.append(earliest_date)

    # generate dates going forward in time to current date
    date_to_records = {}
    start_date = min(earliest_dates)
    date = start_date
    dt = functools.reduce(gcd_timedeltas, TIMESPAN_TO_INTERVAL.values())  # the timestep we need between the dense records is the GCD of the intervals for all of the timespans
    print('Generating dates...'.ljust(TEXT_WIDTH))
    while date < now:
        print(f'\rGenerated {date}'.ljust(TEXT_WIDTH), end = '')
        date_to_records[date] = None
        date += dt
    print('\rGenerated dates'.ljust(TEXT_WIDTH))

    # assign records to timespans
    span_to_interval_number = {timespan: 0 for timespan in TimeSpan}
    records_by_span_and_interval_number = collections.defaultdict(list)
    previous_date = None
    print('Generating records...'.ljust(TEXT_WIDTH))
    for current_date in reversed(tuple(date_to_records)):
        for timespan in TimeSpan:
            # you adjust the interval number first because we're going backwards in time, not forwards!
            if previous_date is not None and timespan.is_breakpoint(current_date, previous_date):
                span_to_interval_number[timespan] += 1

            if (current_date - start_date) % timespan.interval == datetime.timedelta() and span_to_interval_number[timespan] < timespan.intervals_to_keep:
                if date_to_records[current_date] is None:
                    print(f'\rGenerating records for {current_date}'.ljust(TEXT_WIDTH), end = '')
                    date_to_records[current_date] = [make_record(current_date, data) for data in record_data]
                records_by_span_and_interval_number[timespan, span_to_interval_number[timespan]].extend(date_to_records[current_date])

        previous_date = current_date
    print('\rGenerated records'.ljust(TEXT_WIDTH))

    return records_by_span_and_interval_number


def make_submitter_record(date: datetime.datetime, name_and_machine: Tuple[str, str]):
    return SubmitterRecord(
        date = date,
        name = name_and_machine[0],
        machine = name_and_machine[1],
        jobs = {
            JobStatus.IDLE: random.randint(80, 120),
            JobStatus.RUNNING: random.randint(30, 50),
            JobStatus.HELD: random.randint(0, 10),
        },
    )


def make_slot_record(date: datetime.datetime, *args):
    return SlotRecord(
        date = date,
        slot_type = random.choice(tuple(SlotType)),
        architecture = 'X86_64',
        operating_system = 'LINUX',
        state = random.choice(tuple(SlotState)),
        activity = random.choice(tuple(SlotActivity)),
        cpus = random.randint(0, 8),
    )


def ensure_parents_exist(path: Path):
    path.parent.mkdir(parents = True, exist_ok = True)


def flatten_list_of_lists(list_of_lists: List[List[Any]]):
    """Flatten one level of nesting."""
    return itertools.chain.from_iterable(list_of_lists)


def write_records_to_json(
    records: Iterable[Record],
    output_path: Path,
    compressed: bool = False,
    indent: int = 2,
    flatten: bool = False,
):
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
        json.dump(j, f, indent = indent if not compressed else None, separators = (', ', ': ') if not compressed else (',', ':'))


def write_output_by_timespan_and_interval(
    records_by_timespan_and_interval_number: Dict[Tuple[TimeSpan, int], Iterable[Record]],
    *,
    record_type: RecordType,
    output_dir: Union[str, Path],
    compressed: bool,
    indent: bool,
    flatten: bool,
):
    print('Writing JSON...'.ljust(TEXT_WIDTH))
    total_records = 0
    total_files = 0
    total_filesize = 0
    earliest_date_and_interval_stamps_by_timespan = collections.defaultdict(list)
    for (timespan, interval_number), records in records_by_timespan_and_interval_number.items():
        interval_stamp = records[-1].date.strftime(timespan.format)
        earliest_date_and_interval_stamps_by_timespan[timespan].append((records[-1].date, interval_stamp))

        path = Path(output_dir) / Path(f'{record_type}s.{timespan}.{interval_stamp}.json')
        print(f'\rWriting {path}'.ljust(TEXT_WIDTH), end = '')
        write_records_to_json(
            records,
            path,
            compressed = compressed,
            indent = indent,
            flatten = flatten,
        )

        total_files += 1
        total_records += len(records)
        total_filesize += path.stat().st_size

    print(f'\rWrote JSON: {total_files} files with {total_records} records, totalling {bytes_to_str(total_filesize)}'.ljust(TEXT_WIDTH))

    oldest = {}
    for timespan, earliest_date_and_interval_stamp in earliest_date_and_interval_stamps_by_timespan.items():
        oldest[timespan] = min(earliest_date_and_interval_stamp, key = lambda x: x[0])[1]

    oldest_file = Path(f'{record_type}.oldest.json')
    with oldest_file.open(mode = 'w') as f:
        json.dump(oldest, f, indent = 2)

    print(f'Wrote information on oldest available records to {oldest_file}')


def bytes_to_str(num_bytes: Number) -> str:
    """Return a number of bytes as a human-readable string."""
    for unit in ('bytes', 'KB', 'MB', 'GB'):
        if num_bytes < 1024:
            return f'{num_bytes:.1f} {unit}'
        num_bytes /= 1024
    return f'{num_bytes:.1f} TB'


def generate_submitters(args):
    names = random.sample(NAMES, k = args.number)
    machines = random.sample(MACHINES, k = args.machines)

    names_and_machines = tuple(zip(names, random.choices(machines, k = len(names))))
    records_by_timespan_and_interval_number = make_timespan_data(names_and_machines, make_submitter_record, now = args.now)

    write_output_by_timespan_and_interval(
        records_by_timespan_and_interval_number,
        record_type = args.record_type,
        output_dir = args.output,
        compressed = args.compressed,
        indent = args.indent,
        flatten = True,
    )


def generate_slots(args):
    data = tuple(range(args.number))
    records_by_timespan_and_interval_number = make_timespan_data(data, make_slot_record, now = args.now)

    write_output_by_timespan_and_interval(
        records_by_timespan_and_interval_number,
        record_type = args.record_type,
        output_dir = args.output,
        compressed = args.compressed,
        indent = args.indent,
        flatten = False,
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description = 'Generate mock metric data for HTCondor View',
    )
    parser.add_argument(
        'record_type',
        action = 'store',
        type = str,
        choices = tuple(repr(r) for r in RecordType),
        help = 'which type of record to generate',
    )
    parser.add_argument(
        '-o', '--output',
        action = 'store',
        type = str,
        default = os.getcwd(),
        help = 'the path to the output dir',
    )
    parser.add_argument(
        '-n', '--number',
        action = 'store',
        type = int,
        default = 100,
        help = 'the number of record sources per date',
    )
    parser.add_argument(
        '-m', '--machines',
        action = 'store',
        type = int,
        default = 10,
        help = 'the number of unique machine names',
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
        '--now',
        action = 'store',
        type = str,
        help = 'the date to think of as now (YYYY-MM-DD)',
    )

    args = parser.parse_args()

    args.record_type = RecordType(args.record_type)
    args.output = Path(args.output).absolute()
    try:
        args.now = datetime.datetime.strptime(args.now, '%Y-%m-%d')
    except TypeError:
        args.now = datetime.datetime.utcnow()

    return args


RECORD_TYPE_TO_GENERATOR = {
    RecordType.SUBMITTER: generate_submitters,
    RecordType.SLOT: generate_slots,
}


def main():
    args = parse_args()

    print(f'Mocking records for {args.number} {args.record_type}s ending at {args.now}, placing output in {args.output}')
    RECORD_TYPE_TO_GENERATOR[args.record_type](args)


if __name__ == '__main__':
    main()
