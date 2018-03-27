# CurlPluginTestHTTPServer
# Allows us to test error conditions in curl requests by introducing unexpected behavior.

from random import randint
from SimpleHTTPServer import SimpleHTTPRequestHandler
import BaseHTTPServer
import logging
import sys
import time

class CurlPluginTestHTTPServer(SimpleHTTPRequestHandler):

    num_partial_norange_requests = 0
    num_retry_requests = 0

    # On GET requests, check the URL path and proceed accordingly
    def do_GET(self):

        # Requesting /servererror returns a 500 error code
        if self.path == "/server-error":
            self.protocol_version = "HTTP/1.1"
            self.send_response(500)

        # Requesting /partial tests HTTP resume requests
        elif self.path == "/partial":

            self.protocol_version = "HTTP/1.1"
            partial_content = "<html>Content to return on partial requests</html>"

            # If the HTTP "Range" header is not set, return a random substring
            # of the partial_content string, but set Content-Length to the full
            # length of the partial_content string. This will be recognized as
            # a partial file transfer.
            if self.headers.getheader("Range", 0) == 0:
                self.send_response(200, "OK")
                self.send_header("Content-Length", len(partial_content))
                self.end_headers()
                partial_offset = randint(1, len(partial_content) - 1);
                self.wfile.write(partial_content[:partial_offset])

            # If the HTTP "Range" header is set, return the specified byte range
            # from the partial_content string. Set Content-Length and
            # Content-Range accordingly.
            else:
                range_header = self.headers.getheader("Range", 0)
                request_range = range_header[range_header.index("=")+1:]

                # Parse the range requested. If no end index is specified, set
                # this by default to the length of partial_content
                request_range_tokens = request_range.split("-")
                request_range_start = request_range_tokens[0]
                request_range_end = str(len(partial_content))
                if request_range_tokens[1] != "":
                    request_range_end = request_range_tokens[1]

                # Now send the response with appropriate HTTP headers
                response_range = "bytes " + request_range_start + "-" + str(int(request_range_end)-1) + "/" + request_range_end
                response_length = int(request_range_end) - int(request_range_start)
                self.send_response(206, "Partial Content")
                self.send_header("Content-Range", response_range)
                self.send_header("Content-Length", str(response_length))
                self.end_headers()
                self.wfile.write(partial_content[int(request_range_start):int(request_range_end)])

        # Requesting /partial-norange is similar to the /partial request above,
        # although it mimics a server that does not support the "Range" HTTP 
        # header. This is supposed to encourage curl_plugin to just redownload 
        # the entire file.
        elif self.path == "/partial-norange":

            self.protocol_version = "HTTP/1.1"
            partial_content = "<html>Content to return on partial-norange requests</html>"

            # Step 1: If this is the first time requesting /partial-norange, 
            # return a random substring of partial_content
            if CurlPluginTestHTTPServer.num_partial_norange_requests == 0:
                self.send_response(200, "OK")
                self.send_header("Content-Length", len(partial_content))
                self.end_headers()
                partial_offset = randint(1, len(partial_content) - 1);
                self.wfile.write(partial_content[:partial_offset])
                CurlPluginTestHTTPServer.num_partial_norange_requests = 1

            # Step 2: When curl_plugin sends a request with the "Range" header 
            # set with "bytes=0-0" to test if Range is supported, return a 200 OK. 
            # This will indicate that Range is not supported. Send the full
            # content.
            elif self.headers.getheader("Range", 0) != 0:
                self.send_response(200, "OK")
                self.send_header("Content-Length", len(partial_content))
                self.end_headers()
                self.wfile.write(partial_content)

            # Step 3: At this point the client should just submit a brand new
            # request without a Range specified. Send the full content. Note
            # that wget and other http clients typically don't neeed this step
            # because they already get the full content in step 2. 
            elif CurlPluginTestHTTPServer.num_partial_norange_requests == 1:
                self.send_response(200, "OK")
                self.send_header("Content-Length", len(partial_content))
                self.end_headers()
                self.wfile.write(partial_content)
                CurlPluginTestHTTPServer.num_partial_norange_requests = 0

        # All other HTTP requests return success codes        
        else:
            self.protocol_version = "HTTP/1.1"
            self.send_response(200, "OK")
            self.send_header("Content-type", "text/html")
            self.end_headers()

if __name__ == "__main__":

    # Read in the name of the address file
    if len(sys.argv) != 2:
        sys.exit("Error: Invalid syntax.\nUsage: python curl_plugin_test_http_server.py [address-filename]")
    address_filename = sys.argv[1]

    # Start the CurlPluginTestHTTPServer. Let the system determine an available port.
    flaky_httpd = BaseHTTPServer.HTTPServer(("127.0.0.1", 0), CurlPluginTestHTTPServer)

    # Output the server address to a file, which will be read in by the Perl test.
    server_address = flaky_httpd.socket.getsockname()
    address_file = open(address_filename, "w")
    address_file.write("127.0.0.1:{0}".format(server_address[1]))
    print("127.0.0.1:{0}".format(server_address[1])) 
    address_file.close()

    # Now serve up flaky responses!
    flaky_httpd.serve_forever()
