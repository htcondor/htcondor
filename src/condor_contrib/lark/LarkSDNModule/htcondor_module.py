#!/usr/bin/python

#
# This htcondor module receives job and machine classad as 
# strings from lark setup callout script and parses these
# received strings to job and machine classads. It looks for 
# interesting classad attributes value and uses it for network 
# scheduling.
#

import sys
import time
import socket
import threading
import SocketServer
import classad
import htcondor
from pox.core import core

log = core.getLogger()
try:
    serverlog = log.getChild("server")
except:
    serverlog = core.getLogger("htcondor_module.server")

threadLock = threading.Lock()

# use a dictionary to store all the network classads, internal IPv4 address
# is used as the key
classadDict = {}

class ThreadedTCPRequestHandler(SocketServer.BaseRequestHandler):
    def handle(self):
        # there are two possible kinds of request to handle
        # 1. lark setup script sends full job and machine classad
        # 2. pox controller component ask for network classad for specific IP
        # they are represented as "SEND" and "REQUEST" respectively
        
        data = self.request.recv(16384).strip()
        cur_thread = threading.current_thread()
        lines = data.split("\n")

        #serverlog.debug("Message type is: %s", lines[0])

        if (lines[0] == "SEND"):
            job_ad = classad.ClassAd(lines[1])
            machine_ad = classad.ClassAd(lines[2])
            # parse out the IP address of internal eth device and the job owner
            ip_src = machine_ad.eval("LarkInnerAddressIPv4")
            job_owner = job_ad.eval("Owner")
            serverlog.debug("IP address of internal ethernet device is: %s", ip_src)
            serverlog.debug("The owner of submitted job is: %s", job_owner)

            self.request.close()

            network_classad = classad.ClassAd()
            # insert all the network policy related classad attr to network classad
            network_classad["Owner"] = job_owner
            network_classad["LarkInnerAddressIPv4"] = ip_src
        
            threadLock.acquire()
            classadDict[ip_src] = network_classad.__str__()
            threadLock.release()
        elif (lines[0] == "REQUEST"):
            network_classad = None
            ip_src = lines[1]
            threadLock.acquire()
            # first check whether classadDict has the given key
            if ip_src in classadDict:
                network_classad = classadDict[ip_src]
            threadLock.release()
            if network_classad is not None:
                serverlog.debug("Network classad is %s", network_classad)
                serverlog.debug("Found network classad for IP %s, send it back.", ip_src)
                self.request.sendall("FOUND" + network_classad)
            else:
                #serverlog.debug("Could not find network classad for IP %s, send back no found.", ip_src)
                self.request.sendall("NOFOUND" + "\n")
            self.request.close()
        elif (lines[0] == "CLEAN"):
            ip_src = lines[1]
            threadLock.acquire()
            # check whether classadDict has the given key and delete corresponding
            # network classad if it is in the dictionary
            log.debug("Delete network classad in classad dictionary for IP address %s", ip_src)
            if ip_src in classadDict:
                del classadDict[ip_src]
            threadLock.release()
        else:
            serverlog.debug("Unknown message type, ignoring...")
            self.request.close()

class ThreadedTCPServer(SocketServer.ThreadingMixIn, SocketServer.TCPServer):
    # Ctrl-C will cleanly kill all spawned threads
    daemon_threads = True
    # faster rebinding
    allow_reuse_address = True

    def __init__(self, server_address, RequestHandlerClass):
        SocketServer.TCPServer.__init__(self, server_address, RequestHandlerClass)

def launch():
    #threadLock = threading.Lock()
    # make HOST to be IPv4 address of the host where the pox controller is running
    HOST = htcondor.param["HTCONDOR_MODULE_HOST"]
    PORT = int(htcondor.param["HTCONDOR_MODULE_PORT"])
    server = ThreadedTCPServer((HOST, PORT), ThreadedTCPRequestHandler)
    core.register("TCPServer", server)
    
    def run():
        try:
            log.debug("Server starts and listens on %s:%i", HOST, PORT)
            server.serve_forever()
        except:
            pass
        log.info("Server quit")

    thread = threading.Thread(target=run)
    thread.daemon = True
    thread.start()





