#!/usr/bin/python3
#
#  File:     lsf_status.py
#
#  Author:   George Papadimitriou
#  e-mail:   georgpap@isi.edu
#  Author:   Brian Bockelman
#  e-mail:   bbockelm@cse.unl.edu
#
#
# Copyright (c) University of Nebraska-Lincoln.  2012
# Copyright (c) University of Wisconsin-Madison.  2012
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""
Query LSF for the status of a given job

Internally, it creates a cache of the LSF bjobs response and will reuse this
for subsequent queries.
"""

from __future__ import print_function

import os
import re
import pwd
import sys
import time
import errno
import fcntl
import random
import struct
import subprocess
import signal
import tempfile
import traceback
import pickle
import csv
import binascii

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

cache_timeout = 60

launchtime = time.time()

def log(msg):
    """
    A very lightweight log - not meant to be used in production, but helps
    when debugging scale tests
    """
    print(time.strftime("%x %X"), os.getpid(), msg, file=sys.stderr)

def to_str(strlike, encoding="latin-1", errors="strict"):
    """Turns a bytes into a str or leaves it alone.
    The default encoding is latin-1 (which will not raise
    a UnicodeDecodeError); best to use when you want to treat the data
    as arbitrary bytes, but some function is expecting a str.
    """
    if isinstance(strlike, bytes):
        return strlike.decode(encoding, errors)
    return strlike

def createCacheDir():
    uid = os.geteuid()
    username = pwd.getpwuid(uid).pw_name
    cache_dir = os.path.join("/var/tmp", "bjobs_cache_%s" % username)

    try:
        os.mkdir(cache_dir, 0o755)
    except OSError as oe:
        if oe.errno != errno.EEXIST:
            raise
        s = os.stat(cache_dir)
        if s.st_uid != uid:
            raise Exception("Unable to check cache because it is owned by UID %d" % s.st_uid)

    return cache_dir

def initLog():
    """
    Determine whether to create a logfile based on the presence of a file
    in the user's bjobs cache directory.  If so, make the logfile there.
    """
    cache_dir = createCacheDir()
    if os.path.exists(os.path.join(cache_dir, "lsf_status.debug")):
        filename = os.path.join(cache_dir, "lsf_status.log")
    else:
        filename = "/dev/null"

    fd = open(filename, "a")
    # Do NOT close the file descriptor blahp originally hands us for stderr.
    # This causes blahp to lose all status updates.
    os.dup(2)
    os.dup2(fd.fileno(), 2)

# Something else from a prior life - see gratia-probe-common's GratiaWrapper.py
def ExclusiveLock(fd, timeout=120):
    """
    Grabs an exclusive lock on fd

    If the lock is owned by another process, and that process is older than the
    timeout, then the other process will be signaled.  If the timeout is
    negative, then the other process is never signaled.

    If we are unable to hold the lock, this call will not block on the lock;
    rather, it will throw an exception.

    By default, the timeout is 120 seconds.
    """

    # POSIX file locking is cruelly crude.  There's nothing to do besides
    # try / sleep to grab the lock, no equivalent of polling.
    # Why hello, thundering herd.

    # An alternate would be to block on the lock, and use signals to interupt.
    # This would mess up Gratia's flawed use of signals already, and not be
    # able to report on who has the lock.  I don't like indefinite waits!
    max_time = 30
    starttime = time.time()
    tries = 1
    while time.time() - starttime < max_time:
        try:
            fcntl.lockf(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            return
        except IOError as ie:
            if not ((ie.errno == errno.EACCES) or (ie.errno == errno.EAGAIN)):
                raise
            if check_lock(fd, timeout):
                time.sleep(.2) # Fast case; however, we have *no clue* how
                               # long it takes to clean/release the old lock.
                               # Nor do we know if we'd get it if we did
                               # fcntl.lockf w/ blocking immediately.  Blech.
                # Check again immediately, especially if this was the last
                # iteration in the for loop.
                try:
                    fcntl.lockf(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                    return
                except IOError as ie:
                    if not ((ie.errno == errno.EACCES) or (ie.errno == errno.EAGAIN)):
                        raise
        sleeptime = random.random()
        log("Unable to acquire lock, try %i; will sleep for %.2f " \
            "seconds and try for %.2f more seconds." % (tries, sleeptime, max_time - (time.time()-starttime)))
        tries += 1
        time.sleep(sleeptime)

    log("Fatal exception - Unable to acquire lock")
    raise Exception("Unable to acquire lock")

def check_lock(fd, timeout):
    """
    For internal use only.

    Given a fd that is locked, determine which process has the lock.
    Kill said process if it is older than "timeout" seconds.
    This will log the PID of the "other process".
    """

    pid = get_lock_pid(fd)
    if pid == os.getpid():
        return True

    if timeout < 0:
        log("Another process, %d, holds the cache lock." % pid)
        return False

    try:
        age = get_pid_age(pid)
    except:
        log("Another process, %d, holds the cache lock." % pid)
        log("Unable to get the other process's age; will not time it out.")
        return False

    log("Another process, %d (age %d seconds), holds the cache lock." % (pid, age))

    if age > timeout:
        os.kill(pid, signal.SIGKILL)
    else:
        return False

    return True

linux_struct_flock = "hhxxxxqqixxxx"
try:
    os.O_LARGEFILE
except AttributeError:
    start_len = "hhlli"

def get_lock_pid(fd):
    # For reference, here's the definition of struct flock on Linux
    # (/usr/include/bits/fcntl.h).
    #
    # struct flock
    # {
    #   short int l_type;   /* Type of lock: F_RDLCK, F_WRLCK, or F_UNLCK.  */
    #   short int l_whence; /* Where `l_start' is relative to (like `lseek').  */
    #   __off_t l_start;    /* Offset where the lock begins.  */
    #   __off_t l_len;      /* Size of the locked area; zero means until EOF.  */
    #   __pid_t l_pid;      /* Process holding the lock.  */
    # };
    #
    # Note that things are different on Darwin
    # Assuming off_t is unsigned long long, pid_t is int
    try:
        if sys.platform == "darwin":
            arg = struct.pack("QQihh", 0, 0, 0, fcntl.F_WRLCK, 0)
        else:
            arg = struct.pack(linux_struct_flock, fcntl.F_WRLCK, 0, 0, 0, 0)
        result = fcntl.fcntl(fd, fcntl.F_GETLK, arg)
    except IOError as ie:
        if ie.errno != errno.EINVAL:
            raise
        log("Unable to determine which PID has the lock due to a " \
            "python portability failure.  Contact the developers with your" \
            " platform information for support.")
        return False
    if sys.platform == "darwin":
        _, _, pid, _, _ = struct.unpack("QQihh", result)
    else:
        _, _, _, _, pid = struct.unpack(linux_struct_flock, result)
    return pid

def get_pid_age(pid):
    now = time.time()
    st = os.stat("/proc/%d" % pid)
    return now - st.st_ctime

def bjobs(jobid=""):
    """
    Call bjobs directly for a jobid.
    If none is specified, query all jobid's.

    Returns a python dictionary with the job info.
    """
    bjobs = get_bjobs_location()
    command = (bjobs, '-V')
    bjobs_process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    bjobs_version, _ = bjobs_process.communicate()
    bjobs_version = to_str(bjobs_version)
    log(bjobs_version)

    starttime = time.time()

    log("Starting bjobs.")
    if jobid != "":
        bjobs_process = subprocess.Popen(("%s -UF %s" % (bjobs, jobid)), stdout=subprocess.PIPE, stderr=subprocess.PIPE, close_fds=True, shell=True)
    else:
        bjobs_process = subprocess.Popen(("%s -UF -a" % bjobs), stdout=subprocess.PIPE, stderr=subprocess.PIPE, close_fds=True, shell=True)

    bjobs_process_stdout, bjobs_process_stderr = bjobs_process.communicate()
    bjobs_process_stdout = to_str(bjobs_process_stdout)
    bjobs_process_stderr = to_str(bjobs_process_stderr)

    if bjobs_process_stderr == "":
        result = parse_bjobs_fd(bjobs_process_stdout.splitlines())
    elif jobid != "":
        result = {jobid: {'BatchJobId': '"%s"' % jobid, 'JobStatus': '3', 'ExitCode': ' 0'}}
    else:
        result = {}

    exit_code = bjobs_process.returncode
    log("Finished bjobs (time=%f)." % (time.time()-starttime))

    if exit_code:
        raise Exception("bjobs failed with exit code %s" % str(exit_code))

    # If the job has completed...
    if jobid != "" and "JobStatus" in result[jobid] and (result[jobid]["JobStatus"] == '4' or result[jobid]["JobStatus"] == '3'):
        # Get the finished job stats and update the result
        finished_job_stats = get_finished_job_stats(jobid)
        result[jobid].update(finished_job_stats)

    return result

def which(program):
    """
    Determine if the program is in the path.

    arg program: name of the program to search
    returns: full path to executable, or None if executable is not found
    """
    def is_exe(fpath):
        return os.path.isfile(fpath) and os.access(fpath, os.X_OK)

    fpath, fname = os.path.split(program)
    if fpath:
        if is_exe(program):
            return program
    else:
        for path in os.environ["PATH"].split(os.pathsep):
            path = path.strip('"')
            exe_file = os.path.join(path, program)
            if is_exe(exe_file):
                return exe_file

    return None

#def convert_cpu_to_seconds(cpu_string):
#    import re
#    h,m,s = re.split(':',cpu_string)
#    return int(h) * 3600 + int(m) * 60 + int(s)

cpu_time_re = re.compile(r"CPU time used is ([0-9.]+) seconds")
max_mem_re = re.compile(r"MAX MEM: ([0-9.]+) (\w+);")
exit_status_re = re.compile(r"Exited [\w ]+ (-?[0-9]+). The CPU")
_cluster_type_cache = None
def get_finished_job_stats(jobid):
    """
    Get a completed job's statistics such as used RAM and cpu usage.
    """

    # List the attributes that we want
    return_dict = { "ImageSize": 0, "ExitCode": 0, "RemoteUserCpu": 0 }
    # First, determine if this is an lsf machine.
    uid = os.geteuid()
    username = pwd.getpwuid(uid).pw_name
    cache_dir = os.path.join("/var/tmp", "bjobs_cache_%s" % username)
    cluster_type_file = os.path.join(cache_dir, "cluster_type")
    global _cluster_type_cache
    if not _cluster_type_cache:
        # Look for the special file, cluster_type
        if os.path.exists(cluster_type_file):
            _cluster_type_cache = open(cluster_type_file).read()
        else:
            # No idea what type of cluster is running, not set, so give up
            log("cluster_type file is not present, not checking for completed job statistics")
            return return_dict

    # LSF completion
    if _cluster_type_cache == "lsf":
        log("Querying bjobs for completed job for jobid: %s" % (str(jobid)))
        bhist_process = subprocess.Popen(("bhist -UF %s" % str(jobid)), stdout=subprocess.PIPE, stderr=subprocess.PIPE, close_fds=True, shell=True)
        bhist_process_stdout, _ = bhist_process.communicate()
        bhist_process_stdout = to_str(bhist_process_stdout)

        for line in bhist_process_stdout.splitlines():
            line = line.strip()
            m = exit_status_re.search(line)
            if m:
                return_dict["ExitCode"] = int(m.group(1))
                continue

            m = cpu_time_re.search(line)
            if m:
                return_dict['RemoteUserCpu'] = m.group(1)
                continue

            m = max_mem_re.search(line)
            if m:
                mem_unit = m.group(2)
                factor = 1
                if mem_unit[0] == 'M':
                    factor = 1024
                elif mem_unit[0] == 'G':
                    factor = 1024**2
                elif mem_unit[0] == 'T':
                    factor = 1024**3
                elif mem_unit[0] == 'P':
                    factor = 1024**4
                elif mem_unit[0] == 'E':
                    factor = 1024**5
                return_dict["ImageSize"] = int(float(m.group(1))) * factor

    return return_dict

_bjobs_location_cache = None
def get_bjobs_location():
    """
    Locate the copy of bjobs the blahp configuration wants to use.
    """
    global _bjobs_location_cache
    if _bjobs_location_cache != None:
        return _bjobs_location_cache
    load_config_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'blah_load_config.sh')
    if os.path.exists(load_config_path) and os.access(load_config_path, os.R_OK):
        cmd = "/bin/bash -c 'source %s && echo ${lsf_binpath}bjobs'" % load_config_path
    else:
        cmd = 'which bjobs'
    child_stdout = os.popen(cmd)
    output = child_stdout.read()
    output = to_str(output)
    location = output.split("\n")[0].strip()
    if child_stdout.close():
        raise Exception("Unable to determine bjobs location: %s" % output)
    _bjobs_location_cache = location
    return location

job_id_re = re.compile(r"Job <([0-9]+)>")
exec_host_re = re.compile(r"Started [0-9]+ Task\(s\) on Host\(s\) ([\w< >]+),")
status_re = re.compile(r"Status <(\w+)>")
exit_status_re = re.compile(r"Exited [\w ]+ (-?[0-9]+). The CPU")
status_mapping = {"PEND": 1, "RUN": 2, "EXIT": 4, "DONE": 4, "PSUSP": 5, "USUSP": 5, "SSUSP": 5}

def parse_bjobs_fd(fd):
    """
    Parse the stdout fd of "bjobs -UF" into a python dictionary containing
    the information we need.
    """
    job_info = {}
    cur_job_id = None
    cur_job_info = {}
    for line in fd:
        line = line.strip()
        m = job_id_re.search(line)
        if m:
            if cur_job_id:
                job_info[cur_job_id] = cur_job_info
            cur_job_id = m.group(1)
            #print cur_job_id, line
            cur_job_info = {"BatchJobId": '"%s"' % cur_job_id.split(".")[0]}
        if cur_job_id == None:
            continue

        m = status_re.search(line)
        if m:
            status = status_mapping.get(m.group(1), 0)
            if status != 0:
                cur_job_info["JobStatus"] = str(status)
                cur_job_info["JobStatusInfo"] = m.group(1)
            continue

        m = exec_host_re.search(line)
        if m:
            worker_set = set(m.group(1).translate(None, "<>").split(" "))
            cur_job_info["WorkerNode"] = '"%s"' % ' '.join(worker_set)
            continue

        if cur_job_info["JobStatusInfo"] == "RUN":
            cur_job_info["ExitCode"] = ' 0'
        elif cur_job_info["JobStatusInfo"] == "EXIT":
            m = exit_status_re.search(line)
            if m:
                cur_job_info["ExitCode"] = ' %s' % m.group(1)
                continue

    if cur_job_id:
        job_info[cur_job_id] = cur_job_info

    return job_info

def job_dict_to_string(info):
    result = ["%s=%s;" % (i[0], i[1]) for i in info.items()]
    return "[" + " ".join(result) + " ]"

def fill_cache(cache_location):
    log("Starting query to fill cache.")
    results = bjobs()
    log("Finished query to fill cache.")
    (fd, filename) = tempfile.mkstemp(dir = "/var/tmp")
    # Open the file with a proper python file object
    f = os.fdopen(fd, "w")
    writer = csv.writer(f, delimiter='\t')
    try:
        try:
            for key, val in results.items():
                key = key.split(".")[0]
                str_val = binascii.b2a_hex(pickle.dumps(val))
                if str is not bytes:
                    str_val = str_val.decode()
                writer.writerow([key, str_val])
            os.fsync(fd)
        except:
            os.unlink(filename)
            raise
    finally:
        f.close()
    os.rename(filename, cache_location)

    # Create the cluster_type file
    uid = os.geteuid()
    username = pwd.getpwuid(uid).pw_name
    cache_dir = os.path.join("/var/tmp", "bjobs_cache_%s" % username)
    cluster_type_file = os.path.join(cache_dir, "cluster_type")
    (fd, filename) = tempfile.mkstemp(dir = "/var/tmp")
    global _cluster_type_cache
    if which("bhist"):
        os.write(fd, "lsf")
        _cluster_type_cache = "lsf"
    else:
        log("Unable to find cluster type")
    os.close(fd)
    os.rename(filename, cluster_type_file)

    global launchtime
    launchtime = time.time()

cache_line_re = re.compile(r"([0-9]+[\.\w\-]+):\s+(.+)")
def cache_to_status(jobid, fd):
    reader = csv.reader(fd, delimiter='\t')
    for row in reader:
        if row[0] == jobid:
            bytes_val = row[1]
            if str is not bytes:
                bytes_val = bytes_val.encode()
            return pickle.loads(binascii.a2b_hex(bytes_val))

def check_cache(jobid, recurse=True):
    uid = os.geteuid()
    username = pwd.getpwuid(uid).pw_name
    cache_dir = os.path.join("/var/tmp", "bjobs_cache_%s" % username)
    if recurse:
        try:
            s = os.stat(cache_dir)
        except OSError as oe:
            if oe.errno != 2:
                raise
            os.mkdir(cache_dir, 0o755)
            s = os.stat(cache_dir)
        if s.st_uid != uid:
            raise Exception("Unable to check cache because it is owned by UID %d" % s.st_uid)
    cache_location = os.path.join(cache_dir, "blahp_results_cache")
    try:
        fd = open(cache_location, "r+")
    except IOError as ie:
        if ie.errno != 2:
            raise
        # Create an empty file so we can hold the file lock
        fd = open(cache_location, "w+")
        ExclusiveLock(fd)
        # If someone grabbed the lock between when we opened and tried to
        # acquire, they may have filled the cache
        if os.stat(cache_location).st_size == 0:
            fill_cache(cache_location)
        fd.close()
        if recurse:
            return check_cache(jobid, recurse=False)
        else:
            return None
    ExclusiveLock(fd)
    s = os.fstat(fd.fileno())
    if s.st_uid != uid:
        raise Exception("Unable to check cache file because it is owned by UID %d" % s.st_uid)
    if (s.st_size == 0) or (launchtime - s.st_mtime > cache_timeout):
        # If someone filled the cache between when we opened the file and
        # grabbed the lock, we may not need to fill the cache.
        s2 = os.stat(cache_location)
        if (s2.st_size == 0) or (launchtime - s2.st_mtime > cache_timeout):
            fill_cache(cache_location)
        if recurse:
            return check_cache(jobid, recurse=False)
        else:
            return None
    return cache_to_status(jobid, fd)

def main():
    initLog()

    # Accept the optional -w argument, but ignore it
    if len(sys.argv) == 2:
        jobid_arg = sys.argv[1]
    elif len(sys.argv) == 3 and sys.argv[1] == "-w":
        jobid_arg = sys.argv[2]
    else:
        print("1Usage: lsf_status.sh lsf/<date>/<jobid>")
        return 1
    jobid = jobid_arg.split("/")[-1].split(".")[0]
    log("Checking cache for jobid %s" % jobid)
    cache_contents = None
    try:
        cache_contents = check_cache(jobid)
    except Exception as e:
        msg = "1ERROR: Internal exception, %s" % str(e)
        log(msg)
        #print msg
    if not cache_contents:
        log("Jobid %s not in cache; querying LSF" % jobid)
        results = bjobs(jobid)
        log("Finished querying LSF for jobid %s" % jobid)
        if not results or jobid not in results:
            log("1ERROR: Unable to find job %s" % jobid)
            print("1ERROR: Unable to find job %s" % jobid)
        else:
            log("0%s" % job_dict_to_string(results[jobid]))
            print("0%s" % job_dict_to_string(results[jobid]))
    else:
        log("Jobid %s in cache." % jobid)
        log("0%s" % job_dict_to_string(cache_contents))

        if cache_contents["JobStatus"] == '4' or cache_contents["JobStatus"] == '3':
            finished_job_stats = get_finished_job_stats(jobid)
            cache_contents.update(finished_job_stats)

        print("0%s" % job_dict_to_string(cache_contents))
    return 0

if __name__ == "__main__":
    try:
        sys.exit(main())
    except SystemExit:
        raise
    except Exception as e:
        exc_traceback = sys.exc_info()[2]
        tb = traceback.extract_tb(exc_traceback)
        log(traceback.format_exc())
        print("1ERROR: {0}: {1} (file {2}, line {3})".format(e.__class__.__name__, str(e).replace("\n", "\\n"),
                                                             tb[-1].filename, tb[-1].lineno))
        sys.exit(0)
