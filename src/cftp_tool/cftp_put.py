#!/usr/bin/python

'''Uses cftp tools and condor to handle transferring a file to many nodes.'''

CFTP_SERVER = "./server"
CFTP_CLIENT = "./client"


import sys
if sys.version_info < (2, 6):
    print sys.version_info
    raise "must use python 2.6 or greater. you are running %d.%d" % sys.version_info


from condor_starter import start_job
from multiprocessing import Process, Queue
from Queue import Empty as QEmpty
import socket
from optparse import OptionParser
import subprocess
import uuid



def serverCollector( interface, port, max_servers, uuid_list, out_queue, command_queue ):

    uuid_list = uuid_list[:]
    collector_socket = socket.socket( socket.AF_INET,
                                      socket.SOCK_DGRAM )
    collector_socket.setsockopt( socket.SOL_SOCKET,
                                 socket.SO_REUSEADDR,
                                 1 )
    collector_socket.bind( (interface, port) )
    server_count = 0
    
    while server_count < max_servers:
        try:
            command = command_queue.get( False )
        except QEmpty:
            command = ""
            
        if command == "QUIT":
            break

        data, address = collector_socket.recvfrom( 4096 )
        
        try:
            data_bits = data.split()
            server = { 'host':data_bits[0], 'port':int(data_bits[1]), 'uuid':str(data_bits[2]) }
        except Exception, e:
            print "Error recieving server announcement: %s" % data
            print e
        else:
            if server["uuid"] in uuid_list:  
                server_count += 1
                out_queue.put( server )
                uuid_list.remove( server["uuid"] )
            else:
                # Ignore announcements from uuids we are not looking for
                # They may be duplicates or zombies
                pass
        
    return 0





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
    



if __name__ == "__main__":
    
    usage = "Usage: %prog [options] [machine 1] [machine 2] ..."
    parser = OptionParser(usage)

    # Transfer options
    parser.add_option("-f", "--file", dest="filename",
                      help="transfer FILE to remote machines.", metavar="FILE")
    
    parser.add_option("-c", "--count", dest="count",
                      type="int", metavar="NUM_REPLICA", 
                      help="transfer NUM_REPLICA copies to unique machines. Overrides machine list." )
    
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

    parser.add_option( "-S", "--collect-server-statistics", dest='collectstats',
                       help = "Collect statistics on server placement afterwards",
                       action = "store_true")
    
    (options, args) = parser.parse_args()

    if not options.filename:
        print "ERROR: Must specify a file to transfer. Did you remember --file?"
        exit(1)

    if options.count:
        machine_list = set()
        machine_count = options.count
    else:
        machine_list = set( args )
        machine_count = len( machine_list )

    if options.host:
        host = options.host
    else:
        host = socket.gethostbyname( socket.gethostname() )
    
    if options.port:
        port = options.port
    else:
        port = 9999

    # Start server collector
    
    q_out = Queue()
    q_cmd = Queue()
    m_id_list = list(( uuid.uuid4().hex for machine in range(machine_count) ))
    p = Process(target=serverCollector, args=(host, port, machine_count, m_id_list, q_out, q_cmd))
    p.start()
    
    # Generate submit data and start condor jobs

    print "\n\nSubmitting server instances to condor for starting...\n\n"

    submit_args = [ '-verbose' ]

    submit = {}
    submit["Executable"] = CFTP_SERVER 
    submit["Error"] =  "/tmp/logs/server_$$([TARGET.Machine]).output"
    submit["Output"] = "/tmp/logs/server_$$([TARGET.Machine]).output"
    submit["Log"] = "/tmp/logs/server_$$([TARGET.Machine]).log"
    submit["Universe"] = "vanilla"
    submit["should_transfer_files"] = "YES"
    submit["when_to_transfer_output"] = "ON_EXIT"

    m_list= list( machine_list )
    for machine_index in range(machine_count):

        submit["Arguments"] = "-a %s:%d -i 60 -l $$([TARGET.Machine]) -d -u %s" % (host, port, m_id_list[machine_index])
        submit["Requirements"] = ''
        #submit["Requirements"] = submit["Requirements"] + '(target.FileSystemDomain == "*.chtc.wisc.edu")'

        if m_list :
            machine = m_list[machine_index]
            submit["Requirements"] = submit["Requirements"]  + ' && (Machine == "%s")' % machine

        start_job( submit, submit_args, queue=1 , save=( options.savesubmit == True) )

    # Display list of entered jobs to prove we are done
        
    #print "\n\n"
    #subprocess.call( ["condor_q",] )
    #print "\n\n"


    # Respond to servers starting up
    print "\n\nWaiting for servers to comeup to begin file transfer...\n\n"

    seen_servers = 0
    seen_server_buckets = {}
        
    while p.is_alive and seen_servers < machine_count:
        server = q_out.get(True)
        print "We have seen server %s come up at %s:%d" % ( server["uuid"],
                                                            server["host"],
                                                            server["port"] )
        seen_servers += 1

        # collect history of server placement
        if server["host"] in seen_server_buckets:
            seen_server_buckets[ server["host"] ][0] += 1
        else:
            seen_server_buckets[ server["host"] ] = [1, 0]


        retcode = handle_server( server, options.filename )
        if retcode == 0:
            seen_server_buckets[ server["host"] ][1] += 1
        else:
            print "Warning. Transfer failed to server instance %d" % (seen_servers)
        
        if seen_servers == machine_count:
            q_cmd.put( "QUIT" )
            
    if seen_servers < machine_count:
        print "Collector died unexpectedly. Quitting now, but there may be issues."
    else:
        #Wait for collector to quit
        print "\nAll servers seen. Killing collector..."
        p.join()
        print "Done."
    


    if options.collectstats:

        print "\n\nTransfer Statistics\n"

        total_unique = len( seen_server_buckets.keys() )
        total_servers, successful_transfers = map( sum, zip(*seen_server_buckets.values()))
        maxName = max( [len(key) for key in seen_server_buckets.keys() ] ) + 2

        print "Total Unique Servers: ", total_unique
        print "Total Successful Transfers: ", successful_transfers
        print ""
        print "Server Name%s%sCount%s%%%sTransfers%sT%%" % ( ' '*(maxName-11),
                                                             ' '*(6),
                                                             ' '*(6),
                                                             ' '*(6),
                                                             ' '*(6),)
        print "-"*(maxName+6+6)
        for key in seen_server_buckets.keys():
            print "%(server_name)s%(spacer)s%(count) 11d%(percent) 7d%%%(transfers) 15d%(transfer_percent)5d" % { 'server_name':key,
                                                                                                                  'spacer': ' '*(maxName-len(key)),
                                                                                                                  'count':int(seen_server_buckets[key][0]),
                                                                                                                  'percent':int(float(seen_server_buckets[key][0]) / total_servers * 100),
                                                                                                                  'transfers':int(seen_server_buckets[key][1]),
                                                                                                                  'transfer_percent':float(float(seen_server_buckets[key][1]) / float(seen_server_buckets[key][0]) * 100)
                                                                       }
        



            

        
    
                                      
