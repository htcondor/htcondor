# HTCondor

[HTCondor](https://htcondor.org/) is a
[Distributed High Throughput Computing](https://en.wikipedia.org/wiki/High-throughput_computing)
system developed at the
[Center for High Throughput Computing](http://chtc.cs.wisc.edu/)
at the University of Wisconsin - Madison.  With it, users can divide large
computing workloads into jobs and submit them to an HTCondor scheduler,
which will run them on worker nodes managed by HTCondor.

Prebuilt binaries for Linux, Windows and Mac can be
[downloaded.](https://htcondor.org/downloads/)

# htcondor snake submit CLI

`htcondor snake submit` is a CLI command that integrates HTCondor, Snakemake, and `snakemake-executor-plugin-htcondor`. 
It submits a **local-universe management job** that runs Snakemake internally, which in turn submits each rule as a separate HTCondor job. 
Because everything runs inside HTCondor, the workflow continues even if your SSH session disconnects.

## Full CLI syntax

```bash
htcondor snake submit [<Snakefile>] [--jobdir <name>] [-- <snakemake_arguments>]
```
or equivalently:
```bash
htcondor snake submit [--jobdir <name>] [<Snakefile>] [-- <snakemake_arguments>]
```

- `<Snakefile>` is path to the Snakefile. If omitted, defaults to `Snakefile` in the current submit directory.
- `--jobdir <name>` is optional. Directory where management job logs are written. Defaults to `logs/` in the current directory. Always supply `<name>` when using this flag.
- `--` separates `htcondor snake submit` arguments from arguments passed through to Snakemake. `<snakemake_arguments>` format is similar to how Snakemake's arguments are commonly written.

### Additional Examples

With a profile and Snakefile not in the current directory:
```bash
htcondor snake submit /path/to/Snakefile --jobdir mylogs -- --profile htcondor_profile
```
```bash
htcondor snake submit --jobdir mylogs /path/to/Snakefile -- --profile htcondor_profile
```

Without a profile:
```bash
htcondor snake submit -- --jobs 6 --cores 4
```

## Setting up on an access point (AP)

### Prerequisites

- **Conda or Mamba** for environment management (Mamba recommended).
  Refer to the [examples directory](https://github.com/htcondor/snakemake-executor-plugin-htcondor/tree/main/examples) for environment setup.
- **snakemake-executor-plugin-htcondor** installed in your environment.
  See [PyPI](https://pypi.org/project/snakemake-executor-plugin-htcondor) or the [GitHub repo](https://github.com/htcondor/snakemake-executor-plugin-htcondor).

### Step 1: Clone the repo

```bash
git clone https://github.com/guechhouth/htcondor-snake-submit.git
```

This creates an `htcondor-snake-submit/` directory wherever you run the command.

From there, run this command to get the absolute path to the parent directory of `htcondor-snake-submit/` and copy the path output:
```bash
pwd
```

### Step 2: Set PYTHONPATH

This tells Python to load the `htcondor_cli` package from this repo instead of the system install, which is what makes the `snake` command available.

***To make this permanent**, add this line to your `~/.bashrc` or `~/.bash_profile`.
Replace `$HOME/htcondor-snake-submit` with wherever you cloned:

```bash
export PYTHONPATH="$HOME/htcondor-snake-submit/src/condor_tools:$PYTHONPATH"
```

Then reload your shell config using `source ~/.bashrc`.

**For a one-time session only**, run this export directly. You will need to re-run this every time you open a new terminal or SSH session:

```bash
export PYTHONPATH="PathOutputFromStep1/htcondor-snake-submit/src/condor_tools:$PYTHONPATH"
```

### Step 3: Verify

```bash
htcondor snake submit --help
```

You should see the help text for `htcondor snake submit`:

```yaml
usage: htcondor snake submit [-h] [--jobdir JOBDIR] ...

positional arguments:
  snakemake_args   Snakefile followed by optional snakemake arguments. Usage: [--jobdir DIR] [Snakefile] [-- snakemake_args]. Snakefile and --jobdir must come before the -- separator.

optional arguments:
  -h, --help       show this help message and exit
  --jobdir JOBDIR  Optional directory for HTCondor management job log files. If omitted, a `logs` directory will be created at the current directory. Must be specified before the -- separator.
```

If you see `invalid choice: 'snake'...`, the PYTHONPATH is not set correctly. Make sure you specify correct path before `htcondor-snake-submit/src/condor_tools:$PYTHONPATH`

### Step 4: Using Wrapper Script 

Due to the current setup of the AP, we need to strip the `apptainer-prefix` from the arguments at the EP. 
You can follow this wrapper script:
```bash
#!/bin/bash

# Fail early if there's an issue
set -e

# When .cache files are created, they need to know where HOME is to write there.
# In this case, that should be the HTCondor scratch dir the job is executing in.
export HOME=$(pwd)
echo "HOME is set to: $(pwd)"


######## Only perform this step if you are packaging an environment using `tar` ########
######## Skip if you are using docker image provided in the yaml config file ###########

# Check if the tar file exists before trying to extract
if [ ! -f snakemake-env.tar.gz ]; then
    echo "ERROR: snakemake-env.tar.gz not found!"
    exit 1
fi

# Extract the portable conda environment that was transferred by HTCondor
echo "Extracting conda environment from snakemake-env.tar.gz..."
tar -xzf "snakemake-env.tar.gz"

# Activate the conda environment
echo "Activating conda environment"
export PATH=$(pwd)/bin:$PATH
echo "Conda environment activated successfully"

########################################################################################


# Strip --apptainer-prefix (absolute AP paths that don't apply on the execute node 
args=()
skip_next=false
for arg in "$@"; do
    if $skip_next; then
        skip_next=false
        continue
    fi
    case "$arg" in
        --apptainer-prefix)
            skip_next=true
            ;;
        --apptainer-prefix=*)
            ;;
        *)
            args+=("$arg")
            ;;
    esac
done

echo "Starting Snakemake job..."
echo "Stripped args count: ${#args[@]}"
snakemake "${args[@]}"
```

### Step 5: Activate your environment and run

Activate your Snakemake environment so that `snakemake` is on your PATH, then run your workflow:

```bash
conda/mamba activate <your-env>
htcondor snake submit /path/to/Snakefile -- --profile htcondor_profile
```

---

# Documentation

The [HTCondor manual](https://htcondor.org/manual/) is a
comprehensive reference for users and administrators of HTCondor.

# Community

The CHTC maintains active [email lists](https://htcondor.org/mail-lists/)
where the HTCondor community asks and answers questions about the installation,
use, or tuning of HTCondor.  If you have a question, encounter a surprise about
HTCondor, or a potential bug, these public email lists are the first place to go.

We welcome github pull requests for code fixes or documentation improvements, but if
you have ideas for a big feature change, please talk with us first.

[Throughput Computing](https://agenda.hep.wisc.edu/event/2297/)
is our annual community meeting in Madison, WI, and we often have an annual
meeting in Europe as well. We encourage everyone with an interest in HTCondor
to join us.

[Materials from past meetings](https://htcondor.org/past_condor_weeks.html)
include talks from science and industry, plus useful tutorials.

[HTCondor Wiki](http://condor-wiki.cs.wisc.edu/index.cgi/wiki) contains FAQs,
bug tickets, and more.

# LICENSE

The HTCondor source code is licensed under the Apache-2.0 Open Source License.
See the [NOTICE.txt](NOTICE.txt) file for full details.
