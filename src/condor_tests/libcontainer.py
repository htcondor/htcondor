import subprocess

def SingularityIsWorthy():
    result = subprocess.run(
        "singularity --version",
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT
    )
    output = result.stdout.decode('utf-8')

    if "apptainer" in output:
        return True

    if "3." in output:
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


def SingularityIsWorking():
    result = subprocess.run(
        "singularity exec -B/bin:/bin -B/lib:/lib -B/lib64:/lib64 -B/usr:/usr busybox.sif /bin/ls /",
        shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT
    )
    output = result.stdout.decode('utf-8')

    if result.returncode == 0:
        return True
    else:
        return False
