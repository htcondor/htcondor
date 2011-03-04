#!/usr/bin/python

'''Uses cftp tools and condor to handle transferring a file to many nodes.'''

CFTP_SERVER = "/scratch/CONDOR_SRC/src/cftp_tool/server"
CFTP_CLIENT = "/scratch/CONDOR_SRC/src/cftp_tool/client"


import sys
if sys.version_info < (2, 6):
    raise "must use python 2.6 or greater"


from condor_starter import start_job
from multiprocessing import Process, Queue
from Queue import Empty as QEmpty
import socket
from optparse import OptionParser
import subprocess






def serverCollector( interface, port, max_servers, out_queue, command_queue ):
    
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
            server = { 'host':data_bits[0], 'port':int(data_bits[1]) }
        except Exception, e:
            print "Error recieving server announcement: %s" % data
            print e
        else:
            server_count += 1
            out_queue.put( server )
        
    return 0





def handle_server( server, filename ):

    client_out_log = open( "logs/client_%(host)s_%(port)d.out" % server,'w')
    client_err_log = open( "logs/client_%(host)s_%(port)d.err" % server,'w')

    client_args = [ CFTP_CLIENT, server["host"], str(server["port"]), filename ]
    subprocess.call( client_args,
                     stdout=client_out_log,
                     stderr=client_err_log)
   
    




if __name__ == "__main__":
    
    usage = "Usage: %prog [options] [machine 1] [machine 2] ..."
    parser = OptionParser(usage)

    # Transfer options
    parser.add_option("-f", "--file", dest="filename",
                      help="transfer FILE to remote machines.", metavar="FILE")
    
    # We can't support this yet
    #parser.add_option("-c", "--count", dest="count",
    #                  type="int", metavar="NUM_REPLICA", 
    #                  help="transfer NUM_REPLICA copies to unique machines. Overrides machine list." )
    
    # Collector options
    parser.add_option("-H", "--host", dest="host",
                      help="Interface under which to run the collector daemon.",
                      metavar="ADDR" )
    parser.add_option("-P", "--port", dest="port", type="int",
                      help="Port under which to run the collector daemon.",
                      metavar="PORT" )
        

    
    
    (options, args) = parser.parse_args()

    if not options.filename:
        print "ERROR: Must specify a file to transfer. Did you remember --file?"
        exit(1)

    #if options.count:
    #    machine_list = set()
    #    machine_count = options.count
    #else:
    #    machine_list = set( args )
    #    machine_count = len( machine_list )

    machine_list = set( args )
    machine_count = len( machine_list ) 

    if options.host:
        host = options.host
    else:
        host = socket.gethostbyname( socket.gethostname )
    
    if options.port:
        port = options.port
    else:
        port = 9999

    # Start server collector
    
    q_out = Queue()
    q_cmd = Queue()
    p = Process(target=serverCollector, args=(host, port, machine_count, q_out, q_cmd))
    p.start()
    
    # Generate submit data and start condor jobs

    print "\n\nSubmitting server instances to condor for starting...\n\n"

    submit_args = [ '-verbose', '-pool' , 'chopin.cs.wisc.edu:15396']

    submit = {}
    submit["Executable"] = CFTP_SERVER 
    submit["Arguments"] = "-a %s:%d -i 60 -l $$([TARGET.Machine])" % (host, port)
    submit["Error"] =  "logs/server_$$([TARGET.Machine]).error"
    submit["Output"] = "logs/server_$$([TARGET.Machine]).output"
    submit["Log"] = "logs/server_$$([TARGET.Machine]).log"
    submit["Universe"] = "vanilla"
    for machine in machine_list:
        submit["Requirements"] = 'Machine == "%s"' % machine
        start_job( submit, submit_args, queue=1 )

    # Display list of entered jobs to prove we are done
        
    print "\n\n"
    subprocess.call( ["condor_q",] )
    print "\n\n"


    # Respond to servers starting up
    print "\n\nWaiting for servers to comeup to begin file transfer...\n\n"

    seen_servers = 0
        
    while p.is_alive and seen_servers < machine_count:
        server = q_out.get(True)
        print "We have seen server %d come up at %s:%d" % ( seen_servers+1,
                                                            server["host"],
                                                            server["port"] )
        seen_servers += 1

        handle_server( server, options.filename )
        
        if seen_servers == machine_count:
            q_cmd.put( "QUIT" )
            
    if seen_servers < machine_count:
        print "Collector died unexpectedly. Quitting now, but there may be issues."
    else:
        #Wait for collector to quit
        print "All servers seen. Killing collector..."
        p.join()
        print "Done."
    




            

        
    
                                      
