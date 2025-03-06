import os
import json
import stat
import sys
import tempfile
import errno
import argparse
import traceback
import htcondor2 as htcondor

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives.asymmetric import ec


def camelize(snake):
    """
    Convert a SNAKE_CASE string (e.g. DAEMON_NAME) to CamelCase (e.g. DaemonName).
    Useful for getting an appropriate log file name for a daemon.

    :param snake: snake_cased string
    :return: CamelCased string"""
    return "".join([x.capitalize() for x in snake.split("_")])


def parse_args(daemon_name="CREDMON_OAUTH"):
    """
    Parse commandline arguments

    :return: argparse.Namespace object containing arugment values
    """
    parser = argparse.ArgumentParser()
    parser.add_argument("--name", "-n", dest="daemon_name", default=daemon_name, help="Credmon daemon name (default: %(default)s)")
    parser.add_argument("--cred-dir", "-c", help="Path to credential directory")
    parser.add_argument("--log-file", "-l", help="Path to log file")
    parser.add_argument("--debug", "-d", action="store_true", help="Set log level to DEBUG")
    parser.add_argument("--verbose", "-v", action="store_true", help="Print log to stderr. Equivalent to the -debug flag in other HTCondor tools.")
    args = parser.parse_args()

    # Daemon name should be all uppercase to match other daemons
    args.daemon_name = args.daemon_name.upper()

    return parser.parse_args()


class HTCondorLogger:
    """
    Class that mimics the Python logging module but logs using htcondor.log
    """

    def __init__(self, subdaemon=None):
        self.subdaemon = ""
        if subdaemon is not None:
            self.subdaemon = "{0}: ".format(subdaemon)

    def debug(self, msg, *mvars):
        htcondor.log(htcondor.LogLevel.Always | htcondor.LogLevel.Verbose, self.subdaemon + msg % mvars)

    def info(self, msg, *mvars):
        htcondor.log(htcondor.LogLevel.Always, self.subdaemon + msg % mvars)

    def warning(self, msg, *mvars):
        htcondor.log(htcondor.LogLevel.Always, "WARNING: {0}".format(self.subdaemon + msg % mvars))

    def error(self, msg, *mvars):
        htcondor.log(htcondor.LogLevel.Error, self.subdaemon + msg % mvars)

    def exception(self, msg, *mvars):
        htcondor.log(htcondor.LogLevel.Error, "{0}\n{1}".format(self.subdaemon + msg % mvars, traceback.format_exc()))


def setup_logging(daemon_name, subdaemon=None, log_file=None, debug=False, verbose=False, **kwargs):
    """
    Set up logging

    :param daemon_name: Credmon daemon name
    :param log_file: Path to custom log file
    :param debug: Bool for setting debug log level
    :param verbose: Bool for logging to stderr

    :return: HTCondorLogger object
    """

    # Setting debug level first so all messages are emitted
    if debug:
        daemon_debug = "{0}_DEBUG".format(daemon_name)
        htcondor.param[daemon_debug] = "$({0}) D_FULLDEBUG".format(daemon_debug)

    if verbose:
        htcondor.enable_debug()

    if log_file is None:
        log_file = htcondor.param.get("{0}_LOG".format(daemon_name),
            os.path.join(htcondor.param["LOG"], "{0}Log".format(camelize(daemon_name))))

    # Explicitly set the log path so HTCondor doesn't write to ToolLog
    htcondor.param["{0}_LOG".format(daemon_name)] = log_file

    # Check writability to prevent htcondor.enable_log() from killing the script
    # before we can write a useful error message to stderr.
    try:
        with open(log_file, "ab"):
            pass
    except Exception as err:
        # Exit and notify the condor_master if not running with -verbose
        if not verbose:
            htcondor.enable_debug()
            htcondor.log(htcondor.LogLevel.Error, "Could not open log file: {0}".format(str(err)))
            sys.exit(44)  # 44 is the magic number to tell condor_master about logging errors
        htcondor.log(htcondor.LogLevel.Always, "WARNING: Could not open log file: {0}".format(str(err)))
    else:
        htcondor.enable_log()

    return HTCondorLogger(subdaemon=subdaemon)


def atomic_output(file_contents, output_fname, mode=stat.S_IRUSR):
    """
    Given file bytes and a destination filename, write to the
    destination in an atomic and durable manner.
    """
    dir_name, file_name = os.path.split(output_fname)

    tmp_fd, tmp_file_name = tempfile.mkstemp(dir = dir_name, prefix=file_name)
    try:
        with os.fdopen(tmp_fd, 'wb') as fp:
            fp.write(file_contents)

        # atomically move new tokens in place
        atomic_rename(tmp_file_name, output_fname, mode=mode)

    finally:
        try:
            os.unlink(tmp_file_name)
        except OSError:
            pass


def create_credentials(args):
    """
    Auto-create the credentials for the condor_credmon; if there are already
    credentials present, leave them alone.
    """

    logger = setup_logging(**vars(args))

    private_keyfile = htcondor.param.get("LOCAL_CREDMON_PRIVATE_KEY", "/etc/condor/scitokens-private.pem")
    public_keyfile = htcondor.param.get("LOCAL_CREDMON_PUBLIC_KEY", "/etc/condor/scitokens.pem")

    try:
        fd = os.open(private_keyfile, os.O_CREAT | os.O_EXCL, 0o700)
    except Exception:
        logger.info("Using existing credential at %s for local signer", private_keyfile)
        return

    # We are the exclusive owner of the private keyfile; we must either
    # succeed or fail and delete the keyfile.
    try:
        private_key = ec.generate_private_key(
            ec.SECP256R1(),
            backend=default_backend()
        )
        public_key = private_key.public_key()

        private_pem = private_key.private_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PrivateFormat.TraditionalOpenSSL,
            encryption_algorithm=serialization.NoEncryption()
        )
        atomic_output(private_pem, private_keyfile, mode=0o600)

        public_pem = public_key.public_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PublicFormat.SubjectPublicKeyInfo
        )
        atomic_output(public_pem, public_keyfile, mode=0o644)
        logger.info("Successfully creeated a new credential for local credmon at %s", private_keyfile)
    except:
        logger.exception("Failed to create a default private/public keypair; local credmon functionality may not work.")
        os.unlink(private_keyfile)
        raise
    finally:
        os.close(fd)


def get_cred_dir(cred_dir = None):
    '''
    Detects the path for the credential directory from the condor_config,
    makes sure the credential directory exists with the correct permissions,
    and returns the path to the credential directory. Instead of detecting the
    path from the condor_config, a custom path for the credential directory can
    be passed as an optional argument.

    :param cred_dir: Path to custom credential directory
    :return: Path to the credential directory
    '''

    # Get the location of the credential directory
    if (cred_dir is None) and (htcondor is not None) and (
            ('SEC_CREDENTIAL_DIRECTORY' in htcondor.param) or
            ('SEC_CREDENTIAL_DIRECTORY_OAUTH' in htcondor.param)
            ):
        cred_dir = htcondor.param.get('SEC_CREDENTIAL_DIRECTORY_OAUTH', htcondor.param.get('SEC_CREDENTIAL_DIRECTORY'))
    elif cred_dir is not None:
        pass
    else:
        raise RuntimeError('The credential directory must be specified in condor_config as SEC_CREDENTIAL_DIRECTORY_OAUTH or passed as an argument')

    # Create the credential directory if it doesn't exist
    if not os.path.exists(cred_dir):
        os.makedirs(cred_dir,
                        (stat.S_ISGID | stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR | stat.S_IRGRP | stat.S_IWGRP | stat.S_IXGRP))

    # Make sure the permissions on the credential directory are correct
    try:
        if (os.stat(cred_dir).st_mode & (stat.S_IROTH | stat.S_IWOTH | stat.S_IXOTH)):
            raise RuntimeError('The credential directory is readable and/or writable by others.')
    except OSError:
        raise RuntimeError('The credmon cannot verify the permissions of the credential directory.')
    if not os.access(cred_dir, (os.R_OK | os.W_OK | os.X_OK)):
        raise RuntimeError('The credmon does not have access to the credential directory.')
        
    return cred_dir


def drop_pid(cred_dir):
    """
    Drop a PID file in the cred dir for condor to find.
    """
    curr_pid = os.getpid()
    pid_path = os.path.join(cred_dir, "pid")
    with open(pid_path, "w") as pid_fd:
        pid_fd.write("{0}".format(curr_pid))
    return

def credmon_incomplete(cred_dir):
    """
    Remove CREDMON_COMPLETE
    """
    # Arguably we should check for uptime, but it's just aklog that
    # occurs as a result, so no premature optimisation
    complete_name = os.path.join(cred_dir, 'CREDMON_COMPLETE')
    if os.path.isfile(complete_name):
        os.unlink(complete_name)

def credmon_complete(cred_dir):
    """
    Touch CREDMON_COMPLETE
    """
    complete_name = os.path.join(cred_dir, 'CREDMON_COMPLETE')
    with open(complete_name, 'a'):
        os.utime(complete_name, None)
    return

def atomic_rename(tmp_file, target_file, mode=stat.S_IRUSR):
    """
    If successful HTCondor will only be dealing with fully prepared and
    usable credential cache files.

    :param tmp_file: The temp file path containing
        the TGT acquired from the ngbauth service.
    :type tmp_file: string
    :param target_file: The target file.
    :return: Whether the chmod/rename was successful.
    :rtype: bool
    """
    
    os.chmod(tmp_file, mode)
    os.rename(tmp_file, target_file)

def atomic_output_json(output_object, output_fname):
    """
    Take a Python object and attempt to serialize it to JSON and write
    the resulting bytes to an output file atomically.

    This function does not return a value and throws an exception on failure.

    :param output_object: A Python object to be serialized into JSON.
    :param output_fname: A filename to write the object into.
    :type output_fname: string
    """

    dir_name, file_name = os.path.split(output_fname)

    tmp_fd, tmp_file_name = tempfile.mkstemp(dir = dir_name, prefix=file_name)
    try:
        with os.fdopen(tmp_fd, 'w') as fp:
            json.dump(output_object, fp)

        # atomically move new tokens in place
        atomic_rename(tmp_file_name, output_fname)

    finally:
        try:
            os.unlink(tmp_file_name)
        except OSError:
            pass

def generate_secret_key():
    """
    Return a secret key that is common across all sessions
    """

    cred_dir = htcondor.param.get("SEC_CREDENTIAL_DIRECTORY_OAUTH",
                    htcondor.param.get("SEC_CREDENTIAL_DIRECTORY",
                        "/var/lib/condor/oauth_credentials"))
    keyfile = os.path.join(cred_dir, "wsgi_session_key")

    # Create the secret key file, if possible, with read-only permissions, if it doesn't exist
    try:
        os.close(os.open(keyfile, os.O_CREAT | os.O_EXCL | os.O_RDWR, stat.S_IRUSR))
    except OSError as os_error:
        # An exception will be thrown if the file already exists, and that's fine and good.
        if os_error.errno != errno.EEXIST:
            sys.stderr.write("Unable to access WSGI session key at {0} ({1}); will use a non-persistent key.\n".format(keyfile, str(os_error)))
            return os.urandom(16)

    # Open the secret key file.
    try:
        with open(keyfile, 'rb') as f:
            current_key = f.read(24)
    except IOError as e:
        sys.stderr.write("Unable to access WSGI session key at {0} ({1}); will use a non-persistent key.\n".format(keyfile, str(e)))
        return os.urandom(16)

    # Make sure the key string isn't empty or truncated.
    if len(current_key) >= 16:
        sys.stderr.write("Using the persistent WSGI session key.\n")
        return current_key

    # We are responsible for generating the keyfile for this webapp to use.
    new_key = os.urandom(24)
    try:
        # Use atomic output so the file is only ever read-only
        atomic_output(new_key, keyfile, stat.S_IRUSR)
        sys.stderr.write("Successfully created a new persistent WSGI session key for scitokens-credmon application at {0}.\n".format(keyfile))
    except Exception as e:
        sys.stderr.write("Failed to atomically create a new persistent WSGI session key at {0} ({1}); will use a transient one.\n".format(keyfile, str(e)))
        return new_key
    return new_key
