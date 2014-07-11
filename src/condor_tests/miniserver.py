import sys
import BaseHTTPServer
from SimpleHTTPServer import SimpleHTTPRequestHandler


HandlerClass = SimpleHTTPRequestHandler
ServerClass  = BaseHTTPServer.HTTPServer
Protocol     = "HTTP/1.0"


port = 0

server_address = ('127.0.0.1', port)

HandlerClass.protocol_version = Protocol
httpd = ServerClass(server_address, HandlerClass)

sa = httpd.socket.getsockname()
f = open('pythonurl', 'w')
f.write ("127.0.0.1:{0}".format(sa[1]))
print "127.0.0.1:{0}".format(sa[1])
f.close()
httpd.serve_forever()
