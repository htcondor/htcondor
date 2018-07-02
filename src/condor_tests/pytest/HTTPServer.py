import SimpleHTTPServer, BaseHTTPServer
import socket
import thread

from Utils import Utils

class StoppableHTTPServerHandler(BaseHTTPServer.BaseHTTPRequestHandler):

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


class StoppableHTTPServer(BaseHTTPServer.HTTPServer):

    def __del__(self):
        self.stop()

    def server_bind(self):
        BaseHTTPServer.HTTPServer.server_bind(self)
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
        thread.start_new_thread(self.serve, ())
        server_address = self.socket.getsockname()
        Utils.TLog("HTTP server started at " + str(server_address[0]) + ":" + str(server_address[1]))

    def stop(self):
        self.run = False

    def serve(self):
        while self.run:
            self.handle_request()


class HTTPServer():

    def __init__(self):
        self._httpd = StoppableHTTPServer(("127.0.0.1", 8080), StoppableHTTPServerHandler)

    def start(self):
        self._httpd.start()

    def stop(self):
        self._httpd.stop()

    def register_url(self, url, callback):
        StoppableHTTPServerHandler.urls[url] = callback