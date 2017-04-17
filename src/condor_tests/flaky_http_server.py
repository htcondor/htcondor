# FlakyHTTPServer
# Allows us to test error conditions in curl requests by introducing unexpected behavior.

from SimpleHTTPServer import SimpleHTTPRequestHandler
import BaseHTTPServer
import sys

class FlakyHTTPServer(SimpleHTTPRequestHandler):
    # On GET requests, check the URL path and proceed accordingly
    def do_GET(self):
        # Requesting /servererror returns a 500 error code
        if self.path == "/servererror":
            self.protocol_version = "HTTP/1.1"
            self.send_response(500)
        # All other HTTP requests return success codes        
        else:
            self.protocol_version = "HTTP/1.1"
            self.send_response(200, "OK")
            self.send_header('Content-type', 'text/html')
            self.end_headers()

if __name__ == "__main__":

    # Start the FlakyHTTPServer. Let the system determine an available port.
    flaky_httpd = BaseHTTPServer.HTTPServer(('127.0.0.1', 0), FlakyHTTPServer)

    # Output the server address to a file, which will be read in by the Perl test.
    server_address = flaky_httpd.socket.getsockname()
    address_file = open('flaky-http-address', 'w')
    address_file.write("127.0.0.1:{0}".format(server_address[1]))
    print("127.0.0.1:{0}".format(server_address[1])) 
    address_file.close()

    # Now serve up flaky responses!
    flaky_httpd.serve_forever()
