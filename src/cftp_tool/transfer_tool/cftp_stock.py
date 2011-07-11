#!/usr/bin/python


import sys
if sys.version_info < (2, 6):
    print "You must use python 2.6 or greater. You are running Python %d.%d" % sys.version_info[:2]
    sys.exit(1)


from datetime import datetime, timedelta
import select
import socket
import uuid
from condor_starter import start_job
import subprocess
from optparse import OptionParser

def handle_server( server, filename ):

    client_out_log = open( "/tmp/logs/client_%(host)s_%(port)d.out" % server,'w')
    client_err_log = open( "/tmp/logs/client_%(host)s_%(port)d.err" % server,'w')

    client_args = [ CFTP_CLIENT, server["host"], str(server["port"]), filename ]
    retCode = subprocess.call( client_args,
                               stdout=client_out_log,
                               stderr=client_err_log)

    client_out_log.close()
    client_err_log.close()

    return retCode

def prune_collection( server_collection, max_age ):
    
    for uuid in server_collection.copy():
        if server_collection[uuid].last_heard_from:
            if server_collection[uuid].last_heard_from < datetime.now() - timedelta(seconds=max_age):
                print "We have not heard from server %s in over %d seconds. Removing." % ( uuid,
                                                                                           max_age )
                del server_collection[uuid]
        elif server_collection[uuid].will_terminate:
            if server_collection[uuid].will_terminate < datetime.now():
                print "Server %s expected termination time passed. Removing." % uuid
                del server_collection[uuid]
        else:
            if server_collection[uuid].started < datetime.now() - timedelta(seconds=max_age):
                print "We have never heard from server %s in over %d seconds. Removing." % ( uuid,
                                                                                           max_age )
                del server_collection[uuid]


class ServerMaintainanceRecord(object):
    server_addr = None
    server_port = None
    uuid = None
    started = None
    will_terminate = None
    last_heard_from = None
    
    has_payload = False


#--------------------------------
#--------------------------------
#--------------------------------

usage = "Usage: %prog [options]"
parser = OptionParser(usage)

# Transfer options
parser.add_option("-f", "--file", dest="filename",
                  help="transfer FILE to remote machines.", metavar="FILE")

parser.add_option("-c", "--count", dest="count",
                  type="int", metavar="NUM_REPLICA", 
                  help="maintain NUM_REPLICA instances of the server in the pool." )

parser.add_option("-t", "--timeout", dest="timeout",
                  type="int", metavar="TIMEOUT",
                  help="wait for TIMEOUT seconds before dropping a AWOL instance.")

# Collector options
parser.add_option("-H", "--host", dest="host",
                  help="Interface under which to run the collector daemon.",
                  metavar="ADDR" )
parser.add_option("-P", "--port", dest="port", type="int",
                  help="Port under which to run the collector daemon.",
                  metavar="PORT" )


# Debug Options
parser.add_option( "-s", "--save-submit", dest="savesubmit",
                   help = "Save a copy of each submit file to the current directory for debugging.",
                   action="store_true")

#parser.add_option( "-S", "--collect-server-statistics", dest='collectstats',
#                   help = "Collect statistics on server placement afterwards",
#                   action = "store_true")

(options, args) = parser.parse_args()



#--------------------------------
#--------------------------------
#--------------------------------

if not options.filename:
    print "ERROR: Must specify a file to transfer. Did you remember --file?"
    exit(1)
else:
    filename = options.filename

if not options.count:
    print "ERROR: Must specify an instance count to maintain. Did you remember --count?"
    exit(1)
else:
    COUNT = int(options.count)

if options.timeout:
    max_age = options.timeout
else:
    max_age = 300

#-----

if options.host:
    interface = options.host
else:
    interface = socket.gethostbyname( socket.gethostname() )

if options.port:
    port = options.port
else:
    port = 9999

#-----



CFTP_SERVER = "./server"
CFTP_CLIENT = "./client"
server_collection = dict();



submit_args = [ '-verbose' ]
submit = {}
submit["Executable"] = CFTP_SERVER 
submit["Error"] =  "/tmp/logs/server_$$([TARGET.Machine]).output"
submit["Output"] = "/tmp/logs/server_$$([TARGET.Machine]).output"
submit["Log"] = "/tmp/logs/server_$$([TARGET.Machine]).log"
submit["Universe"] = "vanilla"
submit["should_transfer_files"] = "YES"
submit["when_to_transfer_output"] = "ON_EXIT"


# Set up UDP collector socket for server notifications
collector_socket = socket.socket( socket.AF_INET,
                                  socket.SOCK_DGRAM )
collector_socket.setsockopt( socket.SOL_SOCKET,
                             socket.SO_REUSEADDR,
                             1 )
collector_socket.bind( (interface, port) )


Working = True
# Main loop
while Working:
    try:
        if len(server_collection) != COUNT:
            uuid_str = uuid.uuid4().hex
            submit["Arguments"] = "-a %s:%d -i 60 -I 300:300 -l $$([TARGET.Machine]) -d -u %s" % (interface,
                                                                                       port,
                                                                                       uuid_str)
            
            print "Submitting new server instance with UUID=%s" % uuid_str
            start_job( submit, submit_args, queue=1, save=options.savesubmit )
        
            smr = ServerMaintainanceRecord()
            smr.uuid = uuid_str
            smr.started = datetime.now()
            server_collection[uuid_str] = smr
            
        # Wait for the collector socket or until 2 seconds, whichever comes first
        ready_r, ready_w, ready_e = select.select( ( collector_socket, ), (), (), 2.0 )

        prune_collection( server_collection, max_age )
            
        if not ready_r:
            continue

    
        data, address = collector_socket.recvfrom( 4096 )

        try:
            data_bits = data.split()
            if len( data_bits ) == 3:
                server = { 'host':data_bits[0],
                           'port':int( data_bits[1] ),
                           'uuid':str( data_bits[2] ) }
            elif len( data_bits ) == 5:
                server = { 'host':data_bits[0],
                           'port':int( data_bits[1] ),
                           'uuid':str( data_bits[2] ),
                           'payload':str( data_bits[3] ),
                           'ttl':str( data_bits[4] ) }

            else:
                raise Exception
        except:
            print "Bad data recieved from server communication. Ignoring."

        smr = server_collection.get( server["uuid"], None )
        if not smr:
            print "Unknown server is attempting to contact us. Ignoring."
            continue

        # Update the server record time field
        smr.last_heard_from = datetime.now()


        if smr.server_addr == None and smr.server_port == None:
            # This means that the server has not yet been handled    
            retcode = handle_server( server, filename )
            if retcode == 0:
                smr.server_addr = server["host"]
                smr.server_port = server["port"]
            else:
                print "Failed to transfer file to server instance at %s:%s" % ( server["host"],
                                                                        server["port"] )
        else:
            # This means that the server has the file and is ticking down to die
            try:
                ttl = int(server["ttl"])
            except ValueError:
                if server["ttl"] == "KILLED":
                    print "Server instance %s has announced termination. Removing from collection." % smr.uuid
                    del server_collection[server["uuid"]]
            except KeyError:
                pass
                # TODO: Fix this and handle it.
                # print "We are in a bad state. We should do something about it."
            else:
                smr.will_terminate = datetime.now() + timedelta(seconds=ttl)
                print "Server %s will terminate in %d seconds." % ( smr.uuid, ttl )  


    except KeyboardInterrupt:
        Working = False
        print "Quitting..."




    
    
        













