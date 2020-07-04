import http.server, http.server
import socket
import _thread

from .Globals import *
from random import randint
from .Utils import Utils

class StoppableHTTPServerHandler(http.server.BaseHTTPRequestHandler):

    urls = {}

    def do_GET(s):
        request_url = s.path
        if request_url in StoppableHTTPServerHandler.urls:
            handler_function = StoppableHTTPServerHandler.urls[request_url]
            handler_function(s)
        else:
            s.send_response(200)
            s.send_header("Content-type", "text/html")
            s.end_headers()


class StoppableHTTPServer(http.server.HTTPServer):

    def __del__(self):
        self.stop()

    def server_bind(self):
        http.server.HTTPServer.server_bind(self)
        self.socket.settimeout(1)
        self.run = True

    def get_request(self):
        while self.run:
            try:
                sock, addr = self.socket.accept()
                sock.settimeout(None)
                return (sock, addr)
            except socket.timeout:
                pass

    def start(self):
        _thread.start_new_thread(self.serve, ())

    def stop(self):
        self.run = False

    def serve(self):
        while self.run:
            self.handle_request()


class HTTPServer():

    def __init__(self):
        self._httpd = StoppableHTTPServer(("127.0.0.1", 0), StoppableHTTPServerHandler)
        server_address = self._httpd.socket.getsockname()
        self._host_address = server_address[0]
        self._port = server_address[1]

    def Start(self):
        self._httpd.start()
        Utils.TLog("HTTP server started at " + str(self._host_address) + ":" + str(self._port))

    def Stop(self):
        Utils.TLog("HTTP server shutting down")
        self._httpd.stop()

    def RegisterUrlHandler(self, url, handler):
        StoppableHTTPServerHandler.urls[url] = handler