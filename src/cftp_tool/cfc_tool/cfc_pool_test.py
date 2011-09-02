import subprocess
import os.path as osp
import os

node_cache_dir = "/tmp/cache"

boot_port = 9000
boot_host = "127.0.0.1"

pool_size = 20

node_set = []
log_set = []
pid_map = {}

try:
    os.mkdir(  osp.join(node_cache_dir,"bootstrap") )
except:
    pass

try:
    os.mkdir(  osp.join(node_cache_dir,"bootstrap_db") )
except:
    pass

boot_log = open( osp.join( node_cache_dir, "boot.log" ), 'w' )
boot_node = subprocess.Popen( ["./cfcnode",
                               str(boot_port),
                               osp.join(node_cache_dir,"bootstrap"),
                               osp.join(node_cache_dir,"bootstrap_db")],
                              stdout= boot_log,
                              stderr= boot_log )

print "Boot node created at PID:", boot_node.pid
pid_map[boot_node.pid] = boot_port

node_set.append( boot_node )
log_set.append( boot_log )


for i in range( pool_size ):
    try:
        os.mkdir(  osp.join(node_cache_dir,"node"+str(boot_port+1+i)))
    except:
        pass

    try:
        os.mkdir(  osp.join(node_cache_dir,"node"+str(boot_port+1+i)+"_db"))
    except:
        pass
    log = open( osp.join( node_cache_dir, "Node_" + str(boot_port+1+i) + ".log" ), 'w' )
    node_set.append( subprocess.Popen( [ "./cfcnode",
                                        str(boot_port+1+i),
                                        osp.join(node_cache_dir,"node"+str(boot_port+1+i)),
                                        osp.join(node_cache_dir,"node"+str(boot_port+1+i)+"_db"),
                                        boot_host,
                                        str(boot_port)],
                                       stdout=log,
                                       stderr=log
                                       ) )
    print "Node on port ", str(boot_port+1+i), "created with PID:", node_set[-1].pid
    pid_map[node_set[-1].pid] = boot_port+1+i
    log_set.append( log )
    
try:
    while( True ):
        for node in node_set[:]:
            node.poll()
            
            if node.returncode != None:
                print "Node", pid_map[node.pid], "has terminated with code", node.returncode
                node_set.remove( node )


except KeyboardInterrupt:
    print "Killing node processes..."
    for node in node_set:
        node.kill()
    print "Closing logs..."
    for log in log_set:
        log.close()
    print "Done. Quitting."


