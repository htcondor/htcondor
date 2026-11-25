"""Lightweight worker targets for testing adstash run_workers. No heavy imports."""

import signal
import time


def worker_fast(*a):
    pass


def worker_slow(*a):
    time.sleep(60)


def worker_hangs(*a):
    signal.signal(signal.SIGTERM, signal.SIG_IGN)
    time.sleep(9999)


def worker_errors(*a):
    raise RuntimeError("Failed parsing ad")
