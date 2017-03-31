##**************************************************************
##
## Copyright (C) 1990-2017, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
##
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
##
##    http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************

import socket
import struct

### Helper functions

def send_cedar(sock, command):
    """Sends a HTCondor CEDAR-encoded DaemonCore Command"""

    # Network byte order
    header = struct.pack('!B', 1) # header is 1 byte w/ value 1
    length = struct.pack('!I', 8) # length is 4 bytes
    command = struct.pack('!Q', command) # command is 8 bytes

    # Send the command
    sock.send(header)
    sock.send(length)
    sock.send(command)

def read_cedar(sock):
    """Reads a HTCondor CEDAR-encoded response"""

    # First byte is the header (should always be 1)
    header = sock.recv(1)
    if not header:
        raise TypeError('No CEDAR header received from server')
    header = struct.unpack('!B', header)[0]
    if header != 1:
        raise ValueError('Unexpected CEDAR header value returned: {0}'.format(header))

    # Next four bytes are length of the payload
    length = sock.recv(4)
    if not length:
        raise TypeError('No CEDAR length received from server.')
    length = struct.unpack('!I', length)[0]

    # Now retrieve the payload, which is returned RAW
    if length > 0:
        message = sock.recv(length)
        if not message:
            raise TypeError('No message received from server')
    else:
        message = ''

    return message

### AWS Lambda handler function

def condor_ping_handler(event, context):
    """Queries a HTCondor (SharedPort or Collector) daemon with an
    unauthenticated request for a random number. For use with AWS Lambda when
    setting up condor_annex. Primary use case is testing for open firewall.

    Input
    -----
    IP/hostname, port, and timeout are passed via event data
    with the keys:
        condor_host
        condor_port
        timeout

    condor_host : hostname or IP address of the HTCondor central manager
    condor_port : port on which the SharedPort or Collector daemon is running
    timeout : number of seconds to try connecting to the daemon

    Example event data:
    {
        "condor_host": "mycondor.mydomain.edu",
        "condor_port": "9618",
        "timeout": "5"
    }

    Output
    ------
    The return data includes keys:
        description
        result
        success

    description : text describing the outcome, including errors
    result : the response from the condor daemon (empty string if errors)
    success : 0 if any errors were encountered, 1 if no errors encountered
    """

    success = '0'
    result = ''

    # Get the HTCondor hostname/ip
    try:
        CONDOR_HOST = socket.gethostbyname(event['condor_host'])

    except Exception as e:
        description = ('Error: {0}.  '
                           'Could not find host {1}. '
                           'Provide a valid public hostname '
                           'or IP address.').format(e, event['condor_host'])
        return {'description': description, 'result': result, 'success': success}

    # Get the HTCondor shared/collector port
    try:
        CONDOR_PORT = int(event['condor_port'])
        if (CONDOR_PORT < 0) or (CONDOR_PORT > 65535):
            raise OverflowError('Port must be in range 0-65535')

    except Exception as e:
        description = ('Error: {0}.  '
                           'Provide a valid port number.').format(e)
        return {'description': description, 'result': result, 'success': success}

    # Get the timeout for socket methods
    try:
        TIMEOUT = int(event['timeout'])
    except Exception as e:
        description = ('Error: {0}.  Provide a valid timeout '
                           'in integer seconds.').format(e)
        return {'description': description, 'result': result, 'success': success}

    # HTCondor DaemonCore Command (DC_QUERY_INSTANCE)
    CONDOR_DC_QUERY_INSTANCE = 60045


    # Get socket object and set the timeout
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(TIMEOUT)

    # Connect to the HTCondor shared port/collector and send the command
    try:
        sock.connect((CONDOR_HOST, CONDOR_PORT))

        send_cedar(sock, CONDOR_DC_QUERY_INSTANCE)
        result = read_cedar(sock)

        success = '1'
        description = 'Request sent to {0}:{1}.'.format(CONDOR_HOST, CONDOR_PORT)

    except (TypeError, ValueError) as e: # HTCondor sent something weird back
        description = ('Error: {0}.  '
                           'Invalid or no response from {1}:{2}. '
                           'Is it running HTCondor 8.7.1+?').format(e, CONDOR_HOST, CONDOR_PORT)

    except socket.error as e:

        if e.errno == 54: # Connection reset by peer. HTCondor didn't like what we sent.
            description = ('Error: {0}.  '
                               'Invalid or no response from {1}:{2}. '
                               'Is it running HTCondor 8.7.1+?').format(e, CONDOR_HOST, CONDOR_PORT)

        else: # Catch all other socket errors
            description = ('Error: {0}.  '
                               'Could not connect to {1}:{2}.').format(e, CONDOR_HOST, CONDOR_PORT)

    except Exception as e: # Catch all other exceptions
        description = ('Error: {0}.  '
                           'Could not connect to {1}:{2}.').format(e, CONDOR_HOST, CONDOR_PORT)

    finally:
        sock.close()

    return {'description': description, 'result': result, 'success': success}
