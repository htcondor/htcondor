# Copyright 2020 HTCondor Team, Computer Sciences Department,
# University of Wisconsin-Madison, WI.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import re
import os
import stat
import socket
import sys
import argparse
import shlex
from datetime import datetime


# In the HTCondor implementation, this quoting method is used
def quote(chirp_string):
    """
    Prepares a string to be used in a Chirp simple command

    :param chirp_string: the string to prepare
    :returns: escaped string

    """

    # '\\', ' ', '\n', '\t', '\r' must be escaped
    escape_chars = ["\\", " ", "\n", "\t", "\r"]
    escape_re = "(" + "|".join([re.escape(x) for x in escape_chars]) + ")"
    escape = re.compile(escape_re)

    # prepend escaped characters with \\
    replace = lambda matchobj: "\\" + matchobj.group(0)
    return escape.sub(replace, chirp_string)


# Helper recursive function to print output like condor_chirp
def _condor_chirp_print(data, indent=0):
    if data is None:
        return
    elif isinstance(data, list):
        for d in data:
            if isinstance(d, (list, dict)):
                _condor_chirp_print(d, indent + 1)
            else:
                print(indent * "\t" + str(d))
    elif isinstance(data, dict):
        for key, value in data.items():
            if isinstance(value, (list, dict)):
                print(indent * "\t" + str(key))
                _condor_chirp_print(value, indent + 1)
            else:
                if key in ["atime", "mtime", "ctime"]:
                    value = datetime.fromtimestamp(int(value)).ctime()
                print(indent * "\t" + "{0}: {1}".format(str(key), str(value)))
    else:
        print(indent * "\t" + str(data))


class HTChirp:
    """Chirp client for HTCondor

    A Chirp client compatible with the HTCondor Chirp implementation.

    If the host and port of a Chirp server are not specified, you are assumed
    to be running in a HTCondor job with ``WantIOProxy = true`` and therefore that
    ``$_CONDOR_SCRATCH_DIR/.chirp.config`` contains the host, port, and cookie for
    connecting to the embedded chirp proxy.
    """

    # static reference variables

    CHIRP_LINE_MAX = 1024
    CHIRP_AUTH_METHODS = ["cookie"]
    DEFAULT_MODE = (
        (stat.S_IRUSR | stat.S_IRGRP | stat.S_IROTH)
        | (stat.S_IWUSR | stat.S_IWGRP | stat.S_IWOTH)
        | (stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
    )

    # initialize

    def __init__(self, host=None, port=None, auth=["cookie"], cookie=None, timeout=10):
        """
        :param host: the hostname or ip of the Chirp server
        :param port: the port of the Chirp server
        :param auth: a list of authentication methods to try
        :param cookie: the cookie string, if trying cookie authentication
        :param timeout: socket timeout, in seconds
        """

        # initialize storage variables
        self.fds = {}  # open file descriptors

        chirp_config = ".chirp.config"
        try:
            chirp_config = os.path.join(os.environ["_CONDOR_SCRATCH_DIR"], chirp_config)
        except KeyError:
            pass

        if host and port:
            # don't read chirp_config if host and port are set
            pass
        elif ("cookie" in auth) and (not cookie) and os.path.isfile(chirp_config):
            # read chirp_config
            try:
                with open(chirp_config, "r") as f:
                    (host, port, cookie) = f.read().rstrip().split()
            except Exception:
                print("Error reading {0}".format(chirp_config))
                raise
        else:
            raise ValueError(
                ".chirp.config must be present or you must provide a host and port"
            )

        # store connection parameters
        self.host = host
        self.port = int(port)
        self.cookie = cookie
        self.timeout = timeout

        # connect and store authentication method
        self.authentication = None
        for auth_method in auth:
            try:
                self.connect(auth_method)
            except self.NotAuthenticated:
                self.disconnect()
            except NotImplementedError:
                self.disconnect()
                raise
            else:
                self.disconnect()
                self.authentication = auth_method
                break
        if self.authentication is None:
            raise self.NotAuthenticated(
                "Could not authenticate with methods {0}".format(auth)
            )

    # special methods

    def __enter__(self):
        """Establish a connection with the Chirp server"""
        self.connect()
        return self

    def __exit__(self, *args):
        """Close the connection with the Chirp server"""
        self.disconnect()

    def __del__(self):
        """Disconnect from the Chirp server when this object goes away"""
        self.disconnect()

    def __repr__(self):
        """Print a representation of this object"""
        return "{0}({1}, {2}) using {3} authentication".format(
            self.__class__.__name__, self.host, self.port, self.authentication
        )

    ## internal methods

    def _authenticate(self, method):
        """Test authentication method

        :param method: The authentication method to attempt

        """

        if method == "cookie":
            response = self._simple_command("cookie {0}\n".format(self.cookie))
            if not (str(response) == "0"):
                raise self.NotAuthenticated(
                    "Could not authenticate using {0}".format(method)
                )
        elif method in self.__class__.CHIRP_AUTH_METHODS:
            raise NotImplementedError(
                "Auth method '{0}' not implemented in this client".format(method)
            )
        else:
            raise ValueError("Unknown authentication method '{0}'".format(method))

    def _check_connection(self):
        if not self.is_connected():
            raise RuntimeError("The Chirp client is not connected to a Chirp server.")

    def _simple_command(self, cmd, get_response=True):
        """Send a command to the Chirp server

        :param cmd: The command to be sent
        :param get_response: Check for a response and return it
        :returns: The response from the Chirp server (if get_response is True)
        :raises InvalidRequest: If the command is invalid
        :raises RuntimeError: If the connection is broken

        """

        # check that client is connected
        self._check_connection()

        # check the command
        if cmd[-1] != "\n":
            raise self.InvalidRequest("The form of the request is invalid.")
        cmd = cmd.encode()

        # send the command
        bytes_sent = 0
        while bytes_sent < len(cmd):
            sent = self.socket.send(cmd[bytes_sent:])
            if sent == 0:
                raise RuntimeError("Connection to the Chirp server is broken.")
            bytes_sent = bytes_sent + sent

        if get_response:
            return self._simple_response()

    def _simple_response(self):
        """Get the response from the Chirp server after running a command

        :returns: The response from the Chirp server
        :raises EnvironmentError: if response is too large

        """

        # check that client is connected
        self._check_connection()

        # build up the response one byte at a time
        response = b""
        chunk = b""
        while chunk != b"\n":  # response terminated with \n
            chunk = self.socket.recv(1)
            response += chunk
            # make sure response doesn't get too large
            if len(response) > self.__class__.CHIRP_LINE_MAX:
                raise EnvironmentError("The server responded with too much data.")
        response = response.decode().rstrip()

        # check the response code if an int is returned
        try:
            int(response)
        except ValueError:
            pass
        else:
            self._check_response(int(response))

        return response

    def _check_response(self, response):
        """Check the response from the Chirp server for validity

        :raises ChirpError: Many different subclasses of ChirpError

        """

        chirp_errors = {
            -1: self.NotAuthenticated("The client has not authenticated its identity."),
            -2: self.NotAuthorized(
                "The client is not authorized to perform that action."
            ),
            -3: self.DoesntExist("There is no object by that name."),
            -4: self.AlreadyExists("There is already an object by that name."),
            -5: self.TooBig("That request is too big to execute."),
            -6: self.NoSpace("There is not enough space to store that."),
            -7: self.NoMemory("The server is out of memory."),
            -8: self.InvalidRequest("The form of the request is invalid."),
            -9: self.TooManyOpen("There are too many resources in use."),
            -10: self.Busy("That object is in use by someone else."),
            -11: self.TryAgain("A temporary condition prevented the request."),
            -12: self.BadFD("The file descriptor requested is invalid."),
            -13: self.IsDir("A file-only operation was attempted on a directory."),
            -14: self.NotDir("A directory operation was attempted on a file."),
            -15: self.NotEmpty(
                "A directory cannot be removed because it is not empty."
            ),
            -16: self.CrossDeviceLink("A hard link was attempted across devices."),
            -17: self.Offline("The requested resource is temporarily not available."),
            -127: self.UnknownError("An unknown error (-127) occured."),
        }

        if response in chirp_errors:
            raise chirp_errors[response]
        elif response < 0:
            raise self.UnknownError("An unknown error ({0}) occured.".format(response))

    def _get_fixed_data(self, length, output_file=None):
        """Get a fixed amount of data from the Chirp server

        :param length: The amount of data (in bytes) to receive
        :param output_file: Output file to store received data (optional)
        :returns: Received data, unless output_file is set, then returns number
            of bytes received.

        """

        # check that client is connected
        self._check_connection()

        length = int(length)

        if output_file:  # stream data to a file
            bytes_recv = 0
            chunk = b""
            with open(output_file, "wb") as fd:
                while bytes_recv < length:
                    chunk = self.socket.recv(self.__class__.CHIRP_LINE_MAX)
                    fd.write(chunk)
                    bytes_recv += len(chunk)
            return bytes_recv

        else:  # return data to method call
            data = b""
            chunk = b""
            while len(data) < length:
                chunk = self.socket.recv(self.__class__.CHIRP_LINE_MAX)
                data += chunk
            return data

    def _get_line_data(self):
        """Get one line of data from the Chirp server

        Most chirp commands return the length of data that will be returned, in
        which case the _get_fixed_data method should be used. This is for the
        few commands (stat, lstat) that do not return a fixed length.

        :returns: A line of data received from the Chirp server

        """

        # check that client is connected
        self._check_connection()

        data = b""
        while True:
            data += self.socket.recv(self.__class__.CHIRP_LINE_MAX)
            if data.decode()[-1] == "\n":
                break
        return data.decode()

    def _peek_buffer(self):
        """Peek in the socket buffer to see if data is waiting to be read

        :returns: True, if bytes in buffer, False if buffer is empty
        """

        self.socket.setblocking(0)
        try:
            buf = self.socket.recv(1, socket.MSG_PEEK)
        except socket.error:
            buf = b""
        self.socket.setblocking(1)
        return len(buf) > 0

    def _open(self, name, flags, mode=None):
        """Open a file on the Chirp server

        :param name: Path to file
        :param flags: File open modes (one or more of 'rwatcx')
        :param mode: Permission mode to set [default: 0777]
        :returns: File descriptor

        """

        # set the default permission
        if mode is None:
            mode = self.__class__.DEFAULT_MODE

        # check flags
        valid_flags = set("rwatcx")
        flags = set(flags)
        if not flags.issubset(valid_flags):
            raise ValueError("Flags must be one or more of 'rwatcx'")

        # get file descriptor
        fd = int(
            self._simple_command(
                "open {0} {1} {2}\n".format(quote(name), "".join(flags), int(mode))
            )
        )

        # store file info
        file_info = (quote(name), "".join(flags), int(mode))
        self.fds[fd] = file_info

        # get stat
        stat = self._get_line_data()

        return fd

    def _close(self, fd):
        """Close a file on the Chirp server

        :param fd: File descriptor

        """

        self._simple_command("close {0}\n".format(int(fd)))

    def _read(self, fd, length, offset=None, stride_length=None, stride_skip=None):
        """Read from a file on the Chirp server

        :param fd: File descriptor
        :param length: Number of bytes to read
        :param offset: Skip this many bytes when reading
        :param stride_length: Read this many bytes every stride_skip bytes
        :param stride_skip: Skip this many bytes between reads
        :returns: Data read from file

        """

        if offset is None and (stride_length, stride_skip) != (None, None):
            offset = 0  # assume offset is 0 if stride given but not offset

        if (offset, stride_length, stride_skip) == (None, None, None):
            # read
            rb = int(
                self._simple_command("read {0} {1}\n".format(int(fd), int(length)))
            )

        elif (offset != None) and (stride_length, stride_skip) == (None, None):
            # pread
            rb = int(
                self._simple_command(
                    "pread {0} {1} {2}\n".format(int(fd), int(length), int(offset))
                )
            )

        elif (stride_length, stride_skip) != (None, None):
            # sread
            rb = int(
                self._simple_command(
                    "sread {0} {1} {2} {3} {4}\n".format(
                        int(fd),
                        int(length),
                        int(offset),
                        int(stride_length),
                        int(stride_skip),
                    )
                )
            )

        else:
            raise self.InvalidRequest(
                "Both stride_length and stride_skip must be specified"
            )

        return self._get_fixed_data(rb)

    def _write(
        self, fd, data, length, offset=None, stride_length=None, stride_skip=None
    ):
        """Write to a file on the Chirp server

        :param fd: File descriptor
        :param data: Data to write
        :param length: Number of bytes to write
        :param offset: Skip this many bytes when writing
        :param stride_length: Write this many bytes every stride_skip bytes
        :param stride_skip: Skip this many bytes between writes
        :returns: Number of bytes written

        """

        # check that client is connected
        self._check_connection()

        if offset is None and (stride_length, stride_skip) != (None, None):
            offset = 0  # assume offset is 0 if stride given but not offset

        if (offset, stride_length, stride_skip) == (None, None, None):
            # write
            self._simple_command(
                "write {0} {1}\n".format(int(fd), int(length)), get_response=False
            )

        elif (offset != None) and (stride_length, stride_skip) == (None, None):
            # pwrite
            self._simple_command(
                "pwrite {0} {1} {2}\n".format(int(fd), int(length), int(offset)),
                get_response=False,
            )

        elif (stride_length, stride_skip) != (None, None):
            # swrite
            wb = self._simple_command(
                "swrite {0} {1} {2} {3} {4}\n".format(
                    int(fd),
                    int(length),
                    int(offset),
                    int(stride_length),
                    int(stride_skip),
                ),
                get_response=False,
            )

        else:
            raise self.InvalidRequest(
                "Both stride_length and stride_skip must be specified"
            )

        wfd = self.socket.makefile("wb")  # open socket as a file object
        wfd.write(data)  # write data
        wfd.close()  # close socket file object

        wb = int(self._simple_response())  # get bytes written
        return wb

    def _fsync(self, fd):
        """Flush unwritten data to disk

        :param fd: File descriptor

        """

        self._simple_command("fsync {0}\n".format(int(fd)))

    def _lseek(self, fd, offset, whence):
        """Move the position of a pointer in an open file

        :param fd: File descriptor
        :param offset: Number of bytes to move pointer
        :param whence: Where to base the offset from
        :returns: Position of pointer

        """

        pos = self._simple_command(
            "lseek {0} {1} {2}\n".format(int(fd), int(offset), int(whence))
        )
        return int(pos)

    ## public methods

    def is_connected(self):
        """Check if Chirp client is connected."""

        # check if the socket is open and exists
        try:
            self.socket.getsockname()
        except (NameError, AttributeError):
            return False
        except socket.error:
            return False

        return True

    def connect(self, auth_method=None):
        """Connect to and authenticate with the Chirp server

        :param auth_method: If set, try the specific authentication method

        """

        if not auth_method:
            auth_method = self.authentication

        # reconnect if already connected
        if self.is_connected():
            self.disconnect()

        # create the socket
        self.socket = socket.socket()
        self.socket.settimeout(self.timeout)

        # connect and authenticate
        self.socket.connect((self.host, self.port))
        self._authenticate(auth_method)

        # reset open file descriptors
        self.fds = {}

    def disconnect(self):
        """Close connection with the Chirp server"""

        try:
            self.socket.close()
        except socket.error:
            pass
        except (NameError, AttributeError):
            pass

        # reset open file descriptors
        self.fds = {}

    # HTCondor-specific methods

    def fetch(self, remote_file, local_file):
        """Copy a file from the submit machine to the execute machine.

        :param remote_file: Path to file to be sent from the submit machine
        :param local_file: Path to file to be written to on the execute machine
        :returns: Bytes written

        """

        return self.getfile(remote_file, local_file)

    def put(self, local_file, remote_file, flags="wct", mode=None):
        """Copy a file from the execute machine to the submit machine.

        Specifying flags other than 'wct' (i.e. 'create or truncate file') when
        putting large files is not recommended as the entire file must be read
        into memory.

        To put individual bytes into a file on the submit machine instead of
        an entire file, see the write() method.

        :param local_file: Path to file to be sent from the execute machine
        :param remote_file: Path to file to be written to on the submit machine
        :param flags: File open modes (one or more of 'rwatcx') [default: 'wct']
        :param mode: Permission mode to set [default: 0777]
        :returns: Size of written file

        """

        # Set default flags
        if flags is None:
            flags = "wct"

        flags = set(flags)

        if flags == set("wct"):
            # If default mode ('wct'), use putfile (efficient)
            return self.putfile(local_file, remote_file, mode)

        else:
            # If non-default mode, have to read entire file (inefficient)
            with open(local_file, "rb") as rfd:
                data = rfd.read()
            # And then use write
            wb = self.write(data, remote_file, flags, mode)
            # Better check how much data was written
            if wb < len(data):
                raise UserWarning(
                    "Only {0} bytes of {1} bytes in {2} were written".format(
                        wb, len(data), local_file
                    )
                )
            return wb

    def remove(self, remote_file):
        """Remove a file from the submit machine.

        :param remote_file: Path to file on the submit machine

        """

        self.unlink(remote_file)

    def get_job_attr(self, job_attribute):
        """Get the value of a job ClassAd attribute.

        :param job_attribute: The job attribute to query
        :returns: The value of the job attribute as a string

        """

        length = int(
            self._simple_command("get_job_attr {0}\n".format(quote(job_attribute)))
        )
        result = self._get_fixed_data(length).decode()

        return result

    def get_job_attr_delayed(self, job_attribute):
        """Get the value of a job ClassAd attribute from the local Starter.

        This may differ from the value in the Schedd.

        :param job_attribute: The job attribute to query
        :returns: The value of the job attribute as a string

        """

        length = int(
            self._simple_command(
                "get_job_attr_delayed {0}\n".format(quote(job_attribute))
            )
        )
        result = self._get_fixed_data(length).decode()

        return result

    def set_job_attr(self, job_attribute, attribute_value):
        """Set the value of a job ClassAd attribute.

        :param job_attribute: The job attribute to set
        :param attribute_value: The job attribute's new value

        """

        self._simple_command(
            "set_job_attr {0} {1}\n".format(
                quote(job_attribute), quote(attribute_value)
            )
        )

    def set_job_attr_delayed(self, job_attribute, attribute_value):
        """Set the value of a job ClassAd attribute.

        This variant of set_job_attr will not push the update immediately, but
        rather as a non-durable update during the next communication between
        starter and shadow.

        :param job_attribute: The job attribute to set
        :param attribute_value: The job attribute's new value

        """

        self._simple_command(
            "set_job_attr_delayed {0} {1}\n".format(
                quote(job_attribute), quote(attribute_value)
            )
        )

    def ulog(self, text):
        """Log a generic string to the job log.

        :param text: String to log

        """

        self._simple_command("ulog {0}\n".format(quote(text)))

    def phase(self, phasestring):
        """Tell HTCondor that the job is changing phases.

        :param phasestring: New phase

        """

        self._simple_command("phase {0}\n".format(quote(phasestring)))

    # Wrappers around methods that use a file descriptor

    def read(
        self, remote_path, length, offset=None, stride_length=None, stride_skip=None
    ):
        """Read up to 'length' bytes from a file on the remote machine.

        Optionally, start at an offset and/or retrieve data in strides.

        :param remote_path: Path to file
        :param length: Number of bytes to read
        :param offset: Number of bytes to offset from beginning of file
        :param stride_length: Number of bytes to read per stride
        :param stride_skip: Number of bytes to skip per stride
        :returns: Data read from file

        """

        fd = self._open(remote_path, "r")
        data = self._read(fd, length, offset, stride_length, stride_skip)
        self._close(fd)

        return data

    def write(
        self,
        data,
        remote_path,
        flags="w",
        mode=None,
        length=None,
        offset=None,
        stride_length=None,
        stride_skip=None,
    ):
        """Write bytes to a file on the remote matchine.

        Optionally, specify the number of bytes to write,
        start at an offset, and/or write data in strides.

        :param data: Bytes to write
        :param remote_path: Path to file
        :param flags: File open modes (one or more of 'rwatcx') [default: 'w']
        :param mode: Permission mode to set [default: 0777]
        :param length: Number of bytes to write [default: len(data)]
        :param offset: Number of bytes to offset from beginning of file
        :param stride_length: Number of bytes to write per stride
        :param stride_skip: Number of bytes to skip per stride
        :returns: Number of bytes written

        """

        # Set default flags
        if flags is None:
            flags = "w"

        flags = set(flags)
        if not ("w" in flags):
            raise ValueError(
                "'w' is not included in flags '{0}'".format("".join(flags))
            )

        if length is None:
            length = len(data)
        else:
            data = data[:length]

        fd = self._open(remote_path, flags, mode)
        bytes_sent = self._write(fd, data, length, offset, stride_length, stride_skip)
        self._fsync(fd)  # force the file to be written to disk
        self._close(fd)

        return bytes_sent

    # Chirp protocol standard methods

    def rename(self, old_path, new_path):
        """Rename (move) a file on the remote machine.

        :param old_path: Path to file to be renamed
        :param new_path: Path to new file name

        """

        self._simple_command(
            "rename {0} {1}\n".format(quote(old_path), quote(new_path))
        )

    def unlink(self, remote_file):
        """Delete a file on the remote machine.

        :param remote_file: Path to file

        """

        self._simple_command("unlink {0}\n".format(quote(remote_file)))

    def rmdir(self, remote_path, recursive=False):
        """Delete a directory on the remote machine.

        The directory must be empty unless recursive is set to True.

        :param remote_path: Path to directory
        :param recursive: If set to True, recursively delete remote_path

        """

        if recursive:
            self.rmall(remote_path)
        else:

            self._simple_command("rmdir {0}\n".format(quote(remote_path)))

    def rmall(self, remote_path):
        """Recursively delete an entire directory on the remote machine.

        :param remote_path: Path to directory

        """

        self._simple_command("rmall {0}\n".format(quote(remote_path)))

    def mkdir(self, remote_path, mode=None):
        """Create a new directory on the remote machine.

        :param remote_path: Path to new directory
        :param mode: Permission mode to set [default: 0777]

        """

        # set the default permission
        if mode is None:
            mode = self.__class__.DEFAULT_MODE

        self._simple_command("mkdir {0} {1}\n".format(quote(remote_path), int(mode)))

    def getfile(self, remote_file, local_file):
        """Retrieve an entire file efficiently from the remote machine.

        :param remote_file: Path to file to be sent from remote machine
        :param local_file: Path to file to be written to on local machine
        :returns: Bytes written

        """

        length = int(self._simple_command("getfile {0}\n".format(quote(remote_file))))
        bytes_recv = self._get_fixed_data(length, local_file)

        return bytes_recv

    def putfile(self, local_file, remote_file, mode=None):
        """Store an entire file efficiently to the remote machine.

        This method will create or overwrite the file on the remote machine. If
        you want to append to a file, use the write() method.

        :param local_file: Path to file to be sent from local machine
        :param remote_file: Path to file to be written to on remote machine
        :param mode: Permission mode to set [default: 0777]
        :returns: Size of written file

        """

        # check that client is connected
        self._check_connection()

        # set the default permission
        if mode is None:
            mode = self.__class__.DEFAULT_MODE

        # get file size
        length = os.stat(local_file).st_size
        bytes_sent = 0

        # send the file
        self._simple_command(
            "putfile {0} {1} {2}\n".format(quote(remote_file), int(mode), int(length))
        )
        wfd = self.socket.makefile("wb")  # open socket as a file object
        with open(local_file, "rb") as rfd:
            data = rfd.read(self.__class__.CHIRP_LINE_MAX)
            while data:  # write to socket CHIRP_LINE_MAX bytes at a time
                wfd.write(data)
                bytes_sent += len(data)
                data = rfd.read(self.__class__.CHIRP_LINE_MAX)
        wfd.close()

        # the chirp server will return the number of bytes it received
        bytes_recv = int(self._simple_response())

        # check bytes
        if (bytes_recv != bytes_sent) or (bytes_recv != length):
            raise RuntimeWarning(
                "File on disk is {0} B, chirp client sent {1} B, chirp server received {2} B".format(
                    length, bytes_sent, bytes_recv
                )
            )

        return bytes_recv

    def getlongdir(self, remote_path):
        """List a directory and all its file metadata on the remote machine.

        :param remote_path: Path to directory
        :returns: A dict of file metadata

        """

        names = [
            "device",
            "inode",
            "mode",
            "nlink",
            "uid",
            "gid",
            "rdevice",
            "size",
            "blksize",
            "blocks",
            "atime",
            "mtime",
            "ctime",
        ]

        length = int(
            self._simple_command("getlongdir {0}\n".format(quote(remote_path)))
        )
        result = self._get_fixed_data(length).decode()

        results = result.rstrip().split("\n")
        files = results[::2]
        stat_dicts = [
            dict(zip(names, [int(x) for x in s.split()])) for s in results[1::2]
        ]
        return dict(zip(files, stat_dicts))

    def getdir(self, remote_path, stat_dict=False):
        """List a directory on the remote machine.

        :param remote_path: Path to directory
        :param stat_dict: If set to True, return a dict of file metadata
        :returns: List of files, unless stat_dict is True

        """

        if stat_dict == True:
            return self.getlongdir(remote_path)
        else:

            length = int(
                self._simple_command("getdir {0}\n".format(quote(remote_path)))
            )
            result = self._get_fixed_data(length).decode()

            files = result.rstrip().split("\n")
            return files

    def whoami(self):
        """Get the user's current identity with respect to this server.

        :returns: The user's identity

        """

        length = int(
            self._simple_command("whoami {0}\n".format(self.__class__.CHIRP_LINE_MAX))
        )
        result = self._get_fixed_data(length).decode()

        return result

    def whoareyou(self, remote_host):
        """Get the server's identity with respect to the remote host.

        :param remote_host: Remote host
        :returns: The server's identity

        """

        length = int(
            self._simple_command(
                "whoareyou {0} {1}\n".format(
                    quote(remote_host), self.__class__.CHIRP_LINE_MAX
                )
            )
        )
        result = self._get_fixed_data(length).decode()

        return result

    def link(self, old_path, new_path, symbolic=False):
        """Create a link on the remote machine.

        :param old_path: File path to link from on the remote machine
        :param new_path: File path to link to on the remote machine
        :param symbolic: If set to True, use a symbolic link

        """

        if symbolic:
            self.symlink(old_path, new_path)
        else:
            self._simple_command(
                "link {0} {1}\n".format(quote(old_path), quote(new_path))
            )

    def symlink(self, old_path, new_path):
        """Create a symbolic link on the remote machine.

        :param old_path: File path to symlink from on the remote machine
        :param new_path: File path to symlink to on the remote machine

        """

        self._simple_command(
            "symlink {0} {1}\n".format(quote(old_path), quote(new_path))
        )

    def readlink(self, remote_path):
        """Read the contents of a symbolic link.

        :param remote_path: File path on the remote machine
        :returns: Contents of the link

        """

        length = self._simple_command(
            "readlink {0} {1}\n".format(
                quote(remote_path), self.__class__.CHIRP_LINE_MAX
            )
        )
        result = self._get_fixed_data(length)

        return result

    def stat(self, remote_path):
        """Get metadata for file on the remote machine.

        If remote_path is a symbolic link, examine its target.

        :param remote_path: Path to file
        :returns: Dict of file metadata

        """

        names = [
            "device",
            "inode",
            "mode",
            "nlink",
            "uid",
            "gid",
            "rdevice",
            "size",
            "blksize",
            "blocks",
            "atime",
            "mtime",
            "ctime",
        ]

        response = self._simple_command("stat {0}\n".format(quote(remote_path)))
        result = str(self._get_line_data()).rstrip()
        while len(result.split()) < len(names):
            result += " " + str(self._get_line_data()).rstrip()

        results = [int(x) for x in result.split()]
        return dict(zip(names, results))

    def lstat(self, remote_path):
        """Get metadata for file on the remote machine.

        If remote path is a symbolic link, examine the link.

        :param remote_path: Path to file
        :returns: Dict of file metadata

        """

        names = [
            "device",
            "inode",
            "mode",
            "nlink",
            "uid",
            "gid",
            "rdevice",
            "size",
            "blksize",
            "blocks",
            "atime",
            "mtime",
            "ctime",
        ]

        response = self._simple_command("lstat {0}\n".format(quote(remote_path)))
        result = str(self._get_line_data()).rstrip()
        while len(result.split()) < len(names):
            result += " " + str(self._get_line_data()).rstrip()

        results = [int(x) for x in result.split()]
        stats = dict(zip(names, results))
        return stats

    def statfs(self, remote_path):
        """Get metadata for a file system on the remote machine.

        :param remote_path: Path to examine
        :returns: Dict of filesystem metadata

        """

        names = ["type", "bsize", "blocks", "bfree", "bavail", "files", "free"]
        names = ["f_" + x for x in names]

        response = self._simple_command("statfs {0}\n".format(quote(remote_path)))
        result = self._get_line_data().rstrip()
        while len(result.split()) < len(names):
            result += " " + self._get_line_data().rstrip()

        results = [int(x) for x in result.split()]
        stats = dict(zip(names, results))
        return stats

    def access(self, remote_path, mode_str):
        """Check access permissions.

        :param remote_path: Path to examine
        :param mode_str: Mode to check (one or more of 'frwx')
        :raises NotAuthorized: If any access mode is not authorized

        """

        modes = {"f": 0, "r": stat.S_IROTH, "w": stat.S_IWOTH, "x": stat.S_IXOTH}

        mode = 0
        for m in mode_str:
            if m not in modes:
                raise ValueError("mode '{0}' not in (fxwr)".format(m))
            mode = mode | modes[m]

        self._simple_command("access {0} {1}\n".format(quote(remote_path), int(mode)))

    def chmod(self, remote_path, mode):
        """Change permission mode of a path on the remote machine.

        :param remote_path: Path
        :param mode: Permission mode to set

        """

        self._simple_command("chmod {0} {1}\n".format(quote(remote_path), int(mode)))

    def chown(self, remote_path, uid, gid):
        """Change the UID and/or GID of a path on the remote machine.

        If remote_path is a symbolic link, change its target.

        :param remote_path: Path
        :param uid: UID
        :param gid: GID

        """

        self._simple_command(
            "chown {0} {1} {2}\n".format(quote(remote_path), int(uid), int(gid))
        )

    def lchown(self, remote_path, uid, gid):
        """Changes the ownership of a file or directory.

        If the path is a symbolic link, change the link.

        :param remote_path: Path
        :param uid: UID
        :param gid: GID

        """

        self._simple_command(
            "lchown {0} {1} {2}\n".format(quote(remote_path), int(uid), int(gid))
        )

    def truncate(self, remote_path, length):
        """Truncates a file on the remote machine to a given number of bytes.

        :param remote_path: Path to file
        :param length: Truncated length

        """

        self._simple_command(
            "truncate {0} {1}\n".format(quote(remote_path), int(length))
        )

    def utime(self, remote_path, actime, mtime):
        """Change the access and modification times of a file
        on the remote machine.

        :param remote_path: Path to file
        :param actime: Access time, in seconds (Unix epoch)
        :param mtime: Modification time, in seconds (Unix epoch)

        """

        self._simple_command(
            "utime {0} {1} {2}\n".format(quote(remote_path), int(actime), int(mtime))
        )

    ## custom exceptions

    class ChirpError(Exception):
        """Base class for all chirp errors."""

        pass

    class NotAuthenticated(ChirpError):
        pass

    class NotAuthorized(ChirpError):
        pass

    class DoesntExist(ChirpError):
        pass

    class AlreadyExists(ChirpError):
        pass

    class TooBig(ChirpError):
        pass

    class NoSpace(ChirpError):
        pass

    class NoMemory(ChirpError):
        pass

    class InvalidRequest(ChirpError):
        pass

    class TooManyOpen(ChirpError):
        pass

    class Busy(ChirpError):
        pass

    class TryAgain(ChirpError):
        pass

    class BadFD(ChirpError):
        pass

    class IsDir(ChirpError):
        pass

    class NotDir(ChirpError):
        pass

    class NotEmpty(ChirpError):
        pass

    class CrossDeviceLink(ChirpError):
        pass

    class Offline(ChirpError):
        pass

    class UnknownError(ChirpError):
        pass


def condor_chirp(chirp_args, return_exit_code=False):
    """Call HTChirp methods using condor_chirp-style commands

    See https://htcondor.readthedocs.io/en/latest/man-pages/condor_chirp.html
    for a list of commands, or use a Python interpreter to run ``htchirp.py --help``.

    :param chirp_args: List or string of arguments as would be passed to condor_chirp
    :param return_exit_code: If ``True``, format and print return value in condor_chirp-style,
        and return 0 (success) or 1 (failure) (defaults to ``False``).

    :returns: Return value from the HTChirp method called,
        unless ``return_exit_code=True`` (see above).

    """

    CONDOR_CHIRP_METHODS = [
        "access",
        "chmod",
        "chown",
        "fetch",
        "get_job_attr",
        "get_job_attr_delayed",
        "getdir",
        "lchown",
        "link",
        "lstat",
        "phase",
        "put",
        "read",
        "readlink",
        "remove",
        "rmdir",
        "set_job_attr",
        "set_job_attr_delayed",
        "stat",
        "statfs",
        "truncate",
        "ulog",
        "utime",
        "whoami",
        "whoareyou",
        "write",
    ]

    usage = "%(prog)s command [command-arguments]"
    epilog = """supported commands:

    fetch RemoteFileName LocalFileName
      Copy the RemoteFileName from the submit machine to the execute machine,
      naming it LocalFileName.

    put [-mode mode] [-perm UnixPerm] LocalFileName RemoteFileName
      Copy the LocalFileName from the execute machine to the submit machine,
      naming it RemoteFileName. The optional -perm UnixPerm argument describes
      the file access permissions in a Unix format; 660 is an example Unix
      format.

      The optional -mode mode argument is one or more of the following
      characters describing the RemoteFileName file:
        w, open for writing;
        a, force all writes to append;
        t, truncate before use;
        c, create the file, if it does not exist;
        x, fail if c is given and the file already exists.

    remove RemoteFileName
      Remove the RemoteFileName file from the submit machine.

    get_job_attr JobAttributeName
      Prints the named job ClassAd attribute to standard output.

    set_job_attr JobAttributeName AttributeValue
      Sets the named job ClassAd attribute with the given attribute value.

    get_job_attr_delayed JobAttributeName
      Prints the named job ClassAd attribute to standard output, potentially
      reading the cached value from a recent set_job_attr_delayed.

    set_job_attr_delayed JobAttributeName AttributeValue
      Sets the named job ClassAd attribute with the given attribute value, but
      does not immediately synchronize the value with the submit side. It can
      take 15 minutes before the synchronization occurs. This has much less
      overhead than the non delayed version. With this option, jobs do not need
      ClassAd attribute WantIOProxy set. With this option, job attribute names
      are restricted to begin with the case sensitive substring Chirp.

    ulog Message
      Appends Message to the job event log.

    phase Phasestring
      Tell HTCondor that the job is changing phases.

    read [-offset offset] [-stride length skip] RemoteFileName Length
      Read Length bytes from RemoteFileName. Optionally, implement a stride by
      starting the read at offset and reading length bytes with a stride of
      skip bytes.

    write [-offset offset] [-stride length skip] RemoteFileName LocalFileName [numbytes]
      Write the contents of LocalFileName to RemoteFileName. Optionally, start
      writing to the remote file at offset and write length bytes with a stride
      of skip bytes. If the optional numbytes follows LocalFileName, then the
      write will halt after numbytes input bytes have been written. Otherwise,
      the entire contents of LocalFileName will be written.

    rmdir [-r] RemotePath
      Delete the directory specified by RemotePath. If the optional -r is
      specified, recursively delete the entire directory.

    getdir [-l] RemotePath
      List the contents of the directory specified by RemotePath. If -l is
      specified, list all metadata as well.

    whoami
      Get the user's current identity.

    whoareyou RemoteHost
      Get the identity of RemoteHost.

    link [-s] OldRemotePath NewRemotePath
      Create a hard link from OldRemotePath to NewRemotePath. If the optional -s
      is specified, create a symbolic link instead.

    readlink RemoteFileName
      Read the contents of the file defined by the symbolic link RemoteFileName.

    stat RemotePath
      Get metadata for RemotePath. Examines the target, if it is a symbolic link.

    lstat RemotePath
      Get metadata for RemotePath. Examines the file, if it is a symbolic link.

    statfs RemotePath
      Get file system metadata for RemotePath.

    access RemotePath Mode
      Check access permissions for RemotePath. Mode is one or more of the
      characters r, w, x, or f, representing read, write, execute, and
      existence, respectively.

    chmod RemotePath UnixPerm
      Change the permissions of RemotePath to UnixPerm. UnixPerm describes the
      file access permissions in a Unix format; 660 is an example Unix format.

    chown RemotePath UID GID
      Change the ownership of RemotePath to UID and GID. Changes the target, if
      RemotePath is a symbolic link.

    lchown RemotePath UID GID
      Change the ownership of RemotePath to UID and GID. Changes the link, if
      RemotePath is a symbolic link.

    truncate RemoteFileName Length
      Truncates RemoteFileName to Length bytes.

    utime RemotePath AccessTime ModifyTime
      Change the access to AccessTime and modification time to ModifyTime of
      RemotePath.
"""

    # Base args
    parser = argparse.ArgumentParser(
        usage=usage, epilog=epilog, formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument("command", nargs=1, help=argparse.SUPPRESS)
    parser.add_argument("args", nargs=argparse.REMAINDER, help=argparse.SUPPRESS)

    # Specific method args
    subparser = argparse.ArgumentParser(add_help=False)
    subparser.add_argument("args", nargs="*")
    subparser.add_argument("-mode", dest="mode")
    subparser.add_argument("-perm", dest="perm")
    subparser.add_argument("-offset", dest="offset")
    subparser.add_argument("-stride", dest="stride", type=int, nargs=2)
    subparser.add_argument("-r", dest="recursive", action="store_true")
    subparser.add_argument("-l", dest="long", action="store_true")
    subparser.add_argument("-s", dest="symbolic", action="store_true")

    # Parse args
    if len(chirp_args) > 0:
        if isinstance(chirp_args, str):
            chirp_args = shlex.split(chirp_args)
        base_args = parser.parse_args(chirp_args)
        cmd_args = subparser.parse_args(base_args.args)
    elif return_exit_code:
        parser.print_help(sys.stderr)
        return 1
    else:
        raise TypeError("Command must be one of: " + ", ".join(CONDOR_CHIRP_METHODS))

    # Verify that command is indeed one of the condor_chirp supported commands
    if base_args.command[0] not in CONDOR_CHIRP_METHODS:
        if return_exit_code:
            error_str = "Command {0} not supported\n".format(base_args.command[0])
            error_str += "Run {0} --help for a list of supported commands\n".format(
                parser.prog
            )
            sys.stderr.write(error_str)
            return 1
        else:
            raise TypeError(
                "Command must be one of: " + ", ".join(CONDOR_CHIRP_METHODS)
            )

    # Prepare command
    command = base_args.command[0]
    args = cmd_args.args
    kwargs = {}

    # Munge commands for inconsistencies between HTChirp and condor_chirp
    if command == "put":
        kwargs = {
            "flags": cmd_args.mode,
        }
        if cmd_args.perm is not None:
            kwargs["mode"] = int(cmd_args.perm, 8)
    elif command == "read":
        if len(args) >= 2:
            args[1] = int(args[1])  # length
        kwargs = {
            "offset": cmd_args.offset,
        }
        if cmd_args.stride is not None:
            kwargs["stride_length"] = cmd_args.stride[0]
            kwargs["stride_skip"] = cmd_args.stride[1]
    elif command == "write":
        # swap order of args and pass raw data
        if len(args) >= 2:
            args[0], args[1] = args[1], args[0]
            args[0] = open(os.path.realpath(args[0]), "r").read()
        length = None
        kwargs = {
            "offset": cmd_args.offset,
        }
        if cmd_args.stride is not None:
            kwargs["stride_length"] = cmd_args.stride[0]
            kwargs["stride_skip"] = cmd_args.stride[1]
        if (
            len(args) >= 3
        ):  # condor_chirp parses length as an optional positional argument
            kwargs["length"] = int(args.pop(2))
    elif command == "rmdir":
        kwargs = {
            "recursive": cmd_args.recursive,
        }
    elif command == "getdir":
        kwargs = {
            "stat_dict": cmd_args.long,
        }
    elif command == "link":
        kwargs = {
            "symbolic": cmd_args.symbolic,
        }
    elif command == "chmod":
        if len(args) >= 2:
            args[1] = int(args[1], 8)  # mode
    elif command == "truncate":
        if len(args) >= 2:
            args[1] = int(args[1])  # length
    elif command == "utime":
        if len(args) >= 3:
            args[1] = int(args[1])  # actime
            args[2] = int(args[2])  # mtime

    # Run the command
    try:
        with HTChirp() as chirp:
            result = getattr(chirp, command)(*args, **kwargs)
    except Exception as e:
        if return_exit_code:
            sys.stderr.write(str(e) + "\n")
            return 1
        else:
            raise

    # Return result
    if return_exit_code and (command in ["fetch", "put", "write"]):
        # These HTChirp methods return bytes read/written
        # But condor_chirp returns nothing for these commands
        return 0
    elif return_exit_code:
        _condor_chirp_print(result)
        return 0
    else:
        return result


if __name__ == "__main__":
    exit_code = condor_chirp(sys.argv[1:], True)
    sys.exit(exit_code)
