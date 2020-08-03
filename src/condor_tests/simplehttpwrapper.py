import sys
import re
import socket
import http.server
from http.server import SimpleHTTPRequestHandler

parentpid  = int(sys.argv[1])
print('sys.argv[0]', sys.argv[0])
print('sys.argv[1]', sys.argv[1])
print("parentpid:", parentpid)

HandlerClass = SimpleHTTPRequestHandler
ServerClass  = http.server.HTTPServer
Protocol     = "HTTP/1.0"

port = 0

server_address = ('127.0.0.1', port)
	
HandlerClass.protocol_version = Protocol
httpd = ServerClass(server_address, HandlerClass)
	
sa = httpd.socket.getsockname()
portfile = "../portsavefile.%d" % (parentpid)
print('portfile',portfile)
p = open(portfile,'w')
p.write(str(sa[1]))
p.close()
print("Serving HTTP on", sa[0], "port", sa[1], "...")
httpd.serve_forever()
