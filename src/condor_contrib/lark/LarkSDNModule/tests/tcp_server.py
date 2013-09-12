#!/usr/bin/python

import socket
import threading
import SocketServer
import fcntl
import struct

class ThreadedTCPRequestHandler(SocketServer.BaseRequestHandler):

    def handle(self):
        data = self.request.recv(1024)
        print data
        cur_thread = threading.current_thread()
        response = "{0}: {1}".format(cur_thread.name, data)
        self.request.sendall(response)

class ThreadedTCPServer(SocketServer.ThreadingMixIn, SocketServer.TCPServer):
    pass

if __name__ == "__main__":
    
    # Get the internal interface name for htcondor job
    f = open('/proc/net/dev')
    lines = f.readlines()
    words = lines[2].split()
    eth_dev = words[0][:-1]
    if eth_dev == 'lo':
        words = lines[3].split()
        eth_dev = words[0][:-1]
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # Get the IPv4 address for indicated ethernet interface
    HOST = socket.inet_ntoa(fcntl.ioctl(
        s.fileno(),
        0x8915, # SIOCGIFADDR
        struct.pack('256s', eth_dev[:15])
    )[20:24])

    PORT = 8866

    # Start a TCP server that binds to indicated Host and Port
    server = ThreadedTCPServer((HOST, PORT), ThreadedTCPRequestHandler)
    server_thread = threading.Thread(target=server.serve_forever)
    #server_thread.daemon = True
    server_thread.start()

    print "Server loop running in thread:", server_thread.name
