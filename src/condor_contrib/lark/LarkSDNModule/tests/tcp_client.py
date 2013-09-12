#!/usr/bin/python

import socket
import SocketServer

# This is going to be the IP address where the tcp server htcondor job is running
HOST = "129.93.244.249"
PORT = 8866
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((HOST, PORT))
try:
    sock.sendall("hello world!")
    response = sock.recv(1024)
    print "Received: {0}".format(response)
finally:
    sock.close()
