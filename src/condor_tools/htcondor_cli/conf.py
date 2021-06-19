import os
import tempfile
import time

from pathlib import Path

TMP_DIR = tempfile.gettempdir() / Path("htcondor_cli") / Path(str(time.time()))