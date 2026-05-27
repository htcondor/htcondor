import os
import shutil
import subprocess
import tempfile
import time
from pathlib import Path


# The bind expression tests should pass to singularity (or set as
# SINGULARITY_BIND_EXPR in HTCondor config) when running jobs against
# the empty .sif built by make_empty_sif().  These mount the host's
# /bin, /usr, /lib, /lib64 into the otherwise-empty image so jobs can
# exec real binaries on any architecture.
EMPTY_SIF_BIND_EXPR = "/bin:/bin /usr:/usr /lib:/lib /lib64:/lib64"


def SingularityIsWorthy():
    result = subprocess.run(
        "/usr/bin/singularity --version",
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT
    )
    output = result.stdout.decode('utf-8')


    if "apptainer" in output:
        print("Singularity version ", output, "is worthy\n")
        return True

    if "3." in output:
        print("Singularity version ", output, "is NOT worthy\n")
        return True

    return False


# For the test to work, we need user namespaces to be working
# and enough of them.  This is a race, but better to try
# to test first.
def UserNamespacesFunctional():
    try:
        result = subprocess.run(["unshare", "-U", "/bin/sh", "-c", "exit 7"])
    except FileNotFoundError:
        return False
    if result.returncode == 7:
        print("unshare seems to work correctly, proceeding with test\n")
        return True
    else:
        print("unshare command failed, test cannot work, skipping test\n")
        return False


def make_empty_sif(target_path):
    """
    Build an empty .sif file at target_path by running `singularity build`
    on a tiny directory tree containing only /etc/passwd.  The resulting
    image has no /bin, /lib, /usr, etc; tests bind-mount the host's copies
    in (see EMPTY_SIF_BIND_EXPR), which lets the same image work on any
    architecture.

    Returns the target path as a Path on success, or None on failure.
    """
    target_path = Path(target_path)
    target_path.parent.mkdir(parents=True, exist_ok=True)

    image_root = target_path.parent / (target_path.name + ".image_root")
    if image_root.exists():
        shutil.rmtree(image_root)
    image_root.mkdir()
    (image_root / "etc").mkdir()
    shutil.copyfile("/etc/passwd", image_root / "etc" / "passwd")

    # some singularities need mksquashfs in path
    # which some linuxes have in /usr/sbin
    if "/usr/sbin" not in os.environ.get("PATH", ""):
        os.environ["PATH"] = os.environ.get("PATH", "") + ":/usr/sbin"

    # Newer apptainers use "proot" by default, which requires ptrace to
    # be working.  Systems which disable ptrace (e.g. for security reasons)
    # then break singularity build. Setting this env turns that off,
    # and reverts to the old way:
    os.environ["APPTAINER_IGNORE_PROOT"] = "1"

    for _ in range(5):
        # rarely we see this failing in batlab with "bad file descriptor"
        # so, just retry.
        try:
            target_path.unlink()
        except FileNotFoundError:
            pass

        r = os.system(f"singularity -s build {target_path} {image_root}")
        if r == 0:
            return target_path
        time.sleep(5)

    return None


_singularity_is_working_cache = None


def SingularityIsWorking():
    global _singularity_is_working_cache
    if _singularity_is_working_cache is not None:
        return _singularity_is_working_cache

    with tempfile.TemporaryDirectory() as td:
        sif = make_empty_sif(Path(td) / "empty.sif")
        if sif is None:
            _singularity_is_working_cache = False
            return False

        bind_args = " ".join(f"-B{m}" for m in EMPTY_SIF_BIND_EXPR.split())
        result = subprocess.run(
            f"singularity exec {bind_args} {sif} /bin/ls /",
            shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT
        )
        _singularity_is_working_cache = (result.returncode == 0)
        return _singularity_is_working_cache
