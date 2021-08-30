import os
import tempfile
import time

from pathlib import Path

TMP_DIR = Path(os.environ.get("_CONDOR_TMPDIR", Path(tempfile.gettempdir()))
    / Path("htcondor_cli") / Path(str(time.time())))