#!/usr/bin/env pytest

"""
Integration test for Pelican input sandbox transfers.

This test creates:
1. A mock Pelican server (HTTP with TLS) that serves tarballs of input files
2. A test file transfer plugin that downloads and unpacks tarballs
3. OpenSSL-generated CA and certificates for TLS (with intentional hostname mismatch)
4. HTCondor job that uses Pelican for input file transfer
"""

from ornithology import *
import htcondor2
import http.server
import socketserver
import threading
import socket
import tempfile
import tarfile
import io
import json
import subprocess
import time
import ssl
import os
from pathlib import Path
from urllib.parse import urlparse, parse_qs


class PelicanHTTPHandler(http.server.BaseHTTPRequestHandler):
    """HTTP handler for mock Pelican server"""
    
    # Class variable to store tarballs
    tarballs = {}
    # Class variable to store job Iwd (Initial Working Directory) for extraction
    job_iwds = {}
    # Class variables to store stdout/stderr paths for each job
    job_stdout_paths = {}
    job_stderr_paths = {}
    # Class variable to store bearer tokens for each job
    job_tokens = {}
    
    def log_message(self, format, *args):
        """Override to reduce noise in test output"""
        pass
    
    def do_GET(self):
        """Handle GET requests for tarball downloads"""
        parsed = urlparse(self.path)
        path = parsed.path
        
        # Handle tarball download: /sandboxes/{cluster}.{proc}/input
        if path.startswith('/sandboxes/') and path.endswith('/input'):
            # Extract job ID from path
            parts = path.split('/')
            if len(parts) >= 4:
                job_id = parts[2]  # cluster.proc

                # Check bearer token
                auth_header = self.headers.get('Authorization', '')
                expected_token = self.job_tokens.get(job_id, '')
                if not auth_header.startswith('Bearer ') or auth_header[7:] != expected_token:
                    self.send_response(401)
                    self.send_header('Content-Type', 'text/plain')
                    self.end_headers()
                    self.wfile.write(b'Unauthorized: Invalid or missing bearer token')
                    return

                if job_id in self.tarballs:
                    tarball_data = self.tarballs[job_id]
                    self.send_response(200)
                    self.send_header('Content-Type', 'application/x-tar')
                    self.send_header('Content-Length', str(len(tarball_data)))
                    self.end_headers()
                    self.wfile.write(tarball_data)
                    return

        # Not found
        self.send_response(404)
        self.send_header('Content-Type', 'text/plain')
        self.end_headers()
        self.wfile.write(b'Not found')

    def do_PUT(self):
        """Handle PUT requests for output file uploads"""
        parsed = urlparse(self.path)
        path = parsed.path

        # Handle output upload: /sandboxes/{cluster}.{proc}/output
        if path.startswith('/sandboxes/') and path.endswith('/output'):
            # Extract job ID from path
            parts = path.split('/')
            if len(parts) >= 4:
                job_id = parts[2]  # cluster.proc

                # Check bearer token
                auth_header = self.headers.get('Authorization', '')
                expected_token = self.job_tokens.get(job_id, '')
                if not auth_header.startswith('Bearer ') or auth_header[7:] != expected_token:
                    self.send_response(401)
                    self.send_header('Content-Type', 'text/plain')
                    self.end_headers()
                    self.wfile.write(b'Unauthorized: Invalid or missing bearer token')
                    return

                # Read uploaded tarball
                content_length = int(self.headers.get('Content-Length', 0))
                tarball_data = self.rfile.read(content_length)
                
                # Store the uploaded tarball (can be verified in tests)
                if not hasattr(self.__class__, 'uploaded_tarballs'):
                    self.__class__.uploaded_tarballs = {}
                self.uploaded_tarballs[job_id] = tarball_data
                
                # Extract tarball contents to job's Iwd directory
                iwd = self.job_iwds.get(job_id, None)
                if not iwd:
                    self.send_response(400)
                    self.send_header('Content-Type', 'text/plain')
                    self.end_headers()
                    self.wfile.write(f'Job {job_id} not registered or Iwd unknown'.encode('utf-8'))
                    return
                
                try:
                    with tarfile.open(fileobj=io.BytesIO(tarball_data), mode='r:gz') as tar:
                        # Get stdout/stderr paths for special handling
                        stdout_path = self.job_stdout_paths.get(job_id, '')
                        stderr_path = self.job_stderr_paths.get(job_id, '')
                        
                        # Extract to the job's Iwd, applying remaps
                        # Files in the tarball are stored with their destination names (arcname)
                        # We need to extract them to the Iwd, creating any necessary subdirectories
                        for member in tar.getmembers():
                            if member.isfile():
                                # Handle special stdout/stderr remapping
                                if member.name == '_condor_stdout' and stdout_path:
                                    # Extract _condor_stdout to Out path (relative to Iwd)
                                    dest_path = os.path.join(iwd, stdout_path)
                                elif member.name == '_condor_stderr' and stderr_path:
                                    # Extract _condor_stderr to Err path (relative to Iwd)
                                    dest_path = os.path.join(iwd, stderr_path)
                                else:
                                    # Normal file extraction
                                    dest_path = os.path.join(iwd, member.name)
                                
                                # Create parent directories if needed
                                dest_dir = os.path.dirname(dest_path)
                                if dest_dir:
                                    os.makedirs(dest_dir, exist_ok=True)
                                
                                # Extract the file
                                file_obj = tar.extractfile(member)
                                if file_obj:
                                    with open(dest_path, 'wb') as f:
                                        f.write(file_obj.read())
                                    # Preserve file permissions
                                    os.chmod(dest_path, member.mode)
                        
                        # Store extracted files for verification
                        if not hasattr(self.__class__, 'uploaded_files'):
                            self.__class__.uploaded_files = {}
                        self.uploaded_files[job_id] = {}
                        
                        # Read back the extracted files for verification
                        for member in tar.getmembers():
                            if member.isfile():
                                # Use the remapped path for stdout/stderr
                                if member.name == '_condor_stdout' and stdout_path:
                                    extracted_path = os.path.join(iwd, stdout_path)
                                elif member.name == '_condor_stderr' and stderr_path:
                                    extracted_path = os.path.join(iwd, stderr_path)
                                else:
                                    extracted_path = os.path.join(iwd, member.name)
                                
                                if os.path.exists(extracted_path):
                                    with open(extracted_path, 'rb') as f:
                                        content = f.read()
                                    self.uploaded_files[job_id][member.name] = content
                except Exception as e:
                    self.send_response(500)
                    self.send_header('Content-Type', 'text/plain')
                    self.end_headers()
                    self.wfile.write(f'Failed to extract tarball: {str(e)}'.encode('utf-8'))
                    return
                
                # Return success
                self.send_response(200)
                self.send_header('Content-Type', 'text/plain')
                self.end_headers()
                self.wfile.write(b'Upload successful')
                return
        
        # Not found
        self.send_response(404)
        self.send_header('Content-Type', 'text/plain')
        self.end_headers()
        self.wfile.write(b'Not found')
    
    def do_POST(self):
        """Handle POST requests for sandbox registration"""
        parsed = urlparse(self.path)
        path = parsed.path
        
        # Handle sandbox registration via Unix socket: /api/v1/sandbox/register
        # or via HTTP: /api/v1.0/registry
        if path == '/api/v1/sandbox/register' or path == '/api/v1.0/registry':
            content_length = int(self.headers.get('Content-Length', 0))
            body = self.rfile.read(content_length)
            
            try:
                # Parse registration request (ClassAd JSON format)
                request = json.loads(body)
                
                # Extract job information from ClassAd
                # The job ad will have cluster, proc, owner, etc.
                cluster = request.get('ClusterId', request.get('cluster', 0))
                proc = request.get('ProcId', request.get('proc', 0))
                job_id = f"{cluster}.{proc}"
                
                # For token requests, return token with URLs
                if path == '/api/v1/sandbox/register':
                    # Get the actual server port from the server instance
                    # We need to return URLs that point to THIS server
                    server_port = getattr(self.server, '_http_port', None)
                    if not server_port:
                        self.send_response(500)
                        self.send_header('Content-Type', 'application/json')
                        error_msg = json.dumps({'error': 'HTTP port not available'}).encode('utf-8')
                        self.send_header('Content-Length', str(len(error_msg)))
                        self.end_headers()
                        self.wfile.write(error_msg)
                        return
                    
                    input_url = f'pelican://localhost:{server_port}/sandboxes/{job_id}/input'
                    output_url = f'pelican://localhost:{server_port}/sandboxes/{job_id}/output'
                    
                    # Create tarball from actual files listed in the job ad
                    # Look for Iwd (initial working directory) and input files in the request
                    iwd = request.get('Iwd', '')
                    transfer_input_files = request.get('TransferInput', '')
                    executable = request.get('Cmd', '')
                    
                    # Parse the file list and create tarball from actual files
                    files_to_transfer = []
                    
                    # Add the executable first
                    if executable:
                        exec_path = os.path.join(iwd, executable) if iwd else executable
                        if os.path.exists(exec_path):
                            with open(exec_path, 'rb') as f:
                                content = f.read()
                            # Keep executable bit by checking source file
                            files_to_transfer.append({
                                'name': os.path.basename(executable),
                                'content': content.decode('utf-8', errors='replace'),
                                'mode': 0o755
                            })
                    
                    # Add input files
                    if transfer_input_files:
                        for filename in transfer_input_files.split(','):
                            filename = filename.strip()
                            if filename:
                                filepath = os.path.join(iwd, filename) if iwd else filename
                                if os.path.exists(filepath):
                                    with open(filepath, 'rb') as f:
                                        content = f.read().decode('utf-8', errors='replace')
                                    files_to_transfer.append({'name': filename, 'content': content})
                    
                    tarball_data = self._create_tarball(files_to_transfer)
                    self.tarballs[job_id] = tarball_data
                    # Store Iwd for this job so we can extract uploads to the correct location
                    self.job_iwds[job_id] = iwd
                    # Store stdout/stderr paths for output extraction
                    stdout_path = request.get('Out', '')
                    stderr_path = request.get('Err', '')
                    self.job_stdout_paths[job_id] = stdout_path
                    self.job_stderr_paths[job_id] = stderr_path

                    # Generate and store token for this job
                    token = f'mock-pelican-token-{job_id}'
                    self.job_tokens[job_id] = token

                    response = {
                        'token': token,
                        'expires_at': int(time.time()) + 3600,  # 1 hour from now
                        # Array of URLs; may split sandbox across multiple URLs in the future!
                        'input_urls': [input_url],
                        'output_urls': [output_url]
                    }
                    
                    response_body = json.dumps(response).encode('utf-8')
                    self.send_response(200)
                    self.send_header('Content-Type', 'application/json')
                    self.send_header('Content-Length', str(len(response_body)))
                    self.end_headers()
                    self.wfile.write(response_body)
                    return
                
                # For registry requests, handle file registration
                files = request.get('files', [])
                
                # Create in-memory tarball
                tarball_data = self._create_tarball(files)
                self.tarballs[job_id] = tarball_data
                
                # Return success
                response = {
                    'status': 'success',
                    'job_id': job_id,
                    'url': f'pelican://localhost/sandboxes/{job_id}/input'
                }
                
                response_body = json.dumps(response).encode('utf-8')
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.send_header('Content-Length', str(len(response_body)))
                self.end_headers()
                self.wfile.write(response_body)
                return
            except Exception as e:
                self.send_response(500)
                self.send_header('Content-Type', 'text/plain')
                self.end_headers()
                self.wfile.write(f'Error: {str(e)}'.encode('utf-8'))
                return
        
        # Unknown endpoint
        self.send_response(404)
        self.send_header('Content-Type', 'text/plain')
        self.end_headers()
        self.wfile.write(b'Not found')
    
    def _create_tarball(self, files):
        """Create an in-memory tarball from file specifications"""
        tarball_io = io.BytesIO()
        
        with tarfile.open(fileobj=tarball_io, mode='w:gz') as tar:
            for file_spec in files:
                filename = file_spec.get('name', 'file')
                content = file_spec.get('content', '').encode('utf-8')
                mode = file_spec.get('mode', 0o644)
                
                # Create TarInfo
                tarinfo = tarfile.TarInfo(name=filename)
                tarinfo.size = len(content)
                tarinfo.mode = mode
                
                # Add to tarball
                tar.addfile(tarinfo, io.BytesIO(content))
        
        tarball_io.seek(0)
        return tarball_io.read()


class PelicanServer:
    """Mock Pelican server that handles sandbox registration and downloads"""
    
    def __init__(self, cert_file, key_file):
        self.cert_file = cert_file
        self.key_file = key_file
        self.http_port = None
        self.unix_socket_path = None
        self.http_server = None
        self.unix_server = None
        self.http_thread = None
        self.unix_thread = None
    
    def start(self, test_dir):
        """Start both HTTP (random port) and Unix socket servers"""
        # Start HTTP server on random port
        self.http_server = socketserver.TCPServer(('localhost', 0), PelicanHTTPHandler)
        self.http_port = self.http_server.server_address[1]
        
        # Store port in server instance so handlers can access it
        self.http_server._http_port = self.http_port
        
        # Wrap with TLS
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(self.cert_file, self.key_file)
        self.http_server.socket = context.wrap_socket(self.http_server.socket, server_side=True)
        
        self.http_thread = threading.Thread(target=self.http_server.serve_forever)
        self.http_thread.daemon = True
        self.http_thread.start()
        
        # Start Unix socket server in /tmp to avoid path length issues
        import tempfile
        temp_dir = Path(tempfile.gettempdir())
        self.unix_socket_path = str(temp_dir / f"pelican_{os.getpid()}.sock")
        
        # Remove socket if it exists
        if Path(self.unix_socket_path).exists():
            Path(self.unix_socket_path).unlink()

        # Create custom Unix socket server class that wraps connections with TLS
        class TLSUnixStreamServer(socketserver.UnixStreamServer):
            def __init__(self, server_address, RequestHandlerClass, cert_file, key_file):
                self.cert_file = cert_file
                self.key_file = key_file
                super().__init__(server_address, RequestHandlerClass)

            def get_request(self):
                """Override to wrap socket with TLS"""
                sock, addr = super().get_request()
                context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
                context.load_cert_chain(self.cert_file, self.key_file)
                ssl_sock = context.wrap_socket(sock, server_side=True)
                return ssl_sock, addr

        self.unix_server = TLSUnixStreamServer(
            self.unix_socket_path,
            PelicanHTTPHandler,
            self.cert_file,
            self.key_file
        )

        # Share the HTTP port with the Unix socket server so handlers can access it
        self.unix_server._http_port = self.http_port
        self.unix_thread = threading.Thread(target=self.unix_server.serve_forever)
        self.unix_thread.daemon = True
        self.unix_thread.start()

        # Give servers time to start
        time.sleep(0.5)
        
        return self.http_port, self.unix_socket_path
    
    def stop(self):
        """Stop both servers"""
        if self.http_server:
            self.http_server.shutdown()
            self.http_server.server_close()
        
        if self.unix_server:
            self.unix_server.shutdown()
            self.unix_server.server_close()
        
        # Clean up Unix socket
        if self.unix_socket_path and Path(self.unix_socket_path).exists():
            Path(self.unix_socket_path).unlink()


def create_ca_and_certs(test_dir):
    """
    Create a CA and host certificate using openssl CLI.
    
    Uses a full hostname (not localhost) which will cause verification
    failures when libcurl tries to validate against 'localhost'.
    """
    cert_dir = test_dir / "certs"
    cert_dir.mkdir(exist_ok=True)
    
    # Use a simple hostname that won't be too long for certificate CN field
    # (socket.getfqdn() can return very long IPv6 reverse DNS names)
    try:
        hostname = socket.gethostname()
        # If hostname is too long or looks like an IP address, use a fallback
        if len(hostname) > 50 or '.' not in hostname or hostname.count('.') > 5:
            hostname = "test.example.com"
    except:
        hostname = "test.example.com"
    
    # CA private key
    ca_key = cert_dir / "ca.key"
    subprocess.run([
        'openssl', 'genrsa',
        '-out', str(ca_key),
        '2048'
    ], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    
    # CA certificate (self-signed)
    ca_cert = cert_dir / "ca.crt"
    subprocess.run([
        'openssl', 'req', '-new', '-x509',
        '-key', str(ca_key),
        '-out', str(ca_cert),
        '-days', '365',
        '-subj', '/CN=Test CA'
    ], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    
    # Server private key
    server_key = cert_dir / "server.key"
    subprocess.run([
        'openssl', 'genrsa',
        '-out', str(server_key),
        '2048'
    ], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    
    # Server CSR (using full hostname, not localhost)
    server_csr = cert_dir / "server.csr"
    subprocess.run([
        'openssl', 'req', '-new',
        '-key', str(server_key),
        '-out', str(server_csr),
        '-subj', f'/CN={hostname}'
    ], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    
    # Sign server certificate with CA
    server_cert = cert_dir / "server.crt"
    subprocess.run([
        'openssl', 'x509', '-req',
        '-in', str(server_csr),
        '-CA', str(ca_cert),
        '-CAkey', str(ca_key),
        '-CAcreateserial',
        '-out', str(server_cert),
        '-days', '365'
    ], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    # Append CA certificate to server certificate to create a chain
    # This allows clients to auto-detect the CA during TLS handshake
    with open(ca_cert, 'r') as ca_file:
        ca_pem = ca_file.read()
    with open(server_cert, 'a') as cert_file:
        cert_file.write(ca_pem)

    return {
        'ca_cert': ca_cert,
        'ca_key': ca_key,
        'server_cert': server_cert,
        'server_key': server_key,
        'hostname': hostname
    }


@standup
def pelican_certs(test_dir):
    """Create CA and certificates for Pelican server"""
    return create_ca_and_certs(test_dir)


@standup
def pelican_server(test_dir, pelican_certs):
    """Start mock Pelican server with TLS"""
    server = PelicanServer(
        pelican_certs['server_cert'],
        pelican_certs['server_key']
    )
    http_port, unix_socket = server.start(test_dir)
    
    yield {
        'server': server,
        'http_port': http_port,
        'unix_socket': unix_socket,
        'hostname': pelican_certs['hostname']
    }
    
    server.stop()


@standup
def pelican_plugin(test_dir):
    """Create test Pelican file transfer plugin"""
    plugin_path = test_dir / "pelican_plugin"
    
    # Get the Python bindings path from environment (set by ctest_driver.py)
    pythonpath_env = os.environ.get('PYTHONPATH', '')
    python_bindings = ''
    for path in pythonpath_env.split(':'):
        if 'python3' in path:
            python_bindings = path
            break
    
    if not python_bindings:
        # Fallback: try to find it relative to test directory
        python_bindings = str((test_dir / '../../release_dir/lib/python3').resolve())

    plugin_code = f'''#!/usr/bin/env python3
"""
Test Pelican file transfer plugin.

Downloads tarballs from Pelican server and unpacks them into the job scratch directory.
"""

import sys
import os

# Manually add PYTHONPATH since environment variables are lost when plugin is executed
sys.path.insert(0, "{python_bindings}")

import urllib.request
import tarfile
import io
import classad2

def print_capabilities():
    """Print plugin capabilities as ClassAd for HTCondor"""
    capabilities = {{
        'MultipleFileSupport': True,
        'PluginType': 'FileTransfer',
        'SupportedMethods': 'pelican',
        'Version': '1.0.0',
    }}
    # Use .printOld() to output in old (line-oriented) ClassAd format
    sys.stdout.write(classad2.ClassAd(capabilities).printOld())
    sys.exit(0)

def main():
    # Check if this is a capability query
    if len(sys.argv) == 2 and sys.argv[1] == '-classad':
        print_capabilities()
        return
    
    # Parse command-line arguments
    infile = None
    outfile = None
    upload = False
    i = 1
    while i < len(sys.argv):
        if sys.argv[i] == '-infile' and i + 1 < len(sys.argv):
            infile = sys.argv[i + 1]
            i += 2
        elif sys.argv[i] == '-outfile' and i + 1 < len(sys.argv):
            outfile = sys.argv[i + 1]
            i += 2
        elif sys.argv[i] == '-upload':
            upload = True
            i += 1
        else:
            i += 1
    
    if not infile:
        print("TransferError = \\"Missing -infile argument\\"", file=sys.stderr)
        sys.exit(1)
    
    # Handle upload vs download
    if upload:
        return handle_upload(infile, outfile)
    else:
        return handle_download(infile, outfile)

def handle_download(infile, outfile):
    """Handle download operation - download and extract tarball"""
    # Read the input ClassAd with URL
    try:
        with open(infile, 'r') as f:
            ad_str = f.read()
        ad = classad2.parseOne(ad_str)
    except Exception as e:
        print("TransferError = \\"Failed to read input ad: " + str(e) + "\\"", file=sys.stderr)
        sys.exit(1)
    
    # Get the URL
    url = ad.get('Url', None)
    if not url:
        print("TransferError = \\"No Url in input ad\\"", file=sys.stderr)
        sys.exit(1)

    # Get the Capability (bearer token) if present
    capability = ad.get('Capability', None)

    # Get CA contents for TLS verification if present
    ca_contents = ad.get('PelicanCAContents', None)

    # Convert pelican:// to https://
    if url.startswith('pelican://'):
        url = 'https://' + url[len('pelican://'):]

    # Download tarball
    try:
        import ssl
        import tempfile

        # Create SSL context with verification enabled
        ctx = ssl.create_default_context()

        # Use CA contents for verification
        if not ca_contents:
            print("TransferError = \\"No PelicanCAContents in input ad\\"", file=sys.stderr)
            sys.exit(1)

        # Write CA to temporary file
        with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.pem') as ca_file:
            ca_file.write(ca_contents)
            ca_file_path = ca_file.name

        try:
            ctx.load_verify_locations(cafile=ca_file_path)
            # For test: disable hostname check since we're using localhost
            ctx.check_hostname = False
        finally:
            # Clean up temp file
            try:
                os.unlink(ca_file_path)
            except:
                pass

        req = urllib.request.Request(url)
        if capability:
            req.add_header('Authorization', f'Bearer {{capability}}')

        with urllib.request.urlopen(req, context=ctx, timeout=4) as response:
            tarball_data = response.read()
    except Exception as e:
        # Write error to outfile if available
        if outfile:
            error_ad = classad2.ClassAd({{
                'TransferSuccess': False,
                'TransferError': 'Failed to download: ' + str(e)
            }})
            try:
                with open(outfile, 'w') as f:
                    f.write(str(error_ad))
            except:
                pass
        sys.exit(1)
    
    # Unpack tarball into current directory
    try:
        with tarfile.open(fileobj=io.BytesIO(tarball_data), mode='r:gz') as tar:
            tar.extractall(path='.')
    except Exception as e:
        # Write error to outfile if available
        if outfile:
            error_ad = classad2.ClassAd({{
                'TransferSuccess': False,
                'TransferError': 'Failed to extract tarball: ' + str(e)
            }})
            try:
                with open(outfile, 'w') as f:
                    f.write(str(error_ad))
            except:
                pass
        sys.exit(1)
    
    # Create marker file to prove plugin successfully transferred files
    marker_file = ".pelican_plugin_executed"
    try:
        with open(marker_file, 'w') as f:
            f.write("Plugin executed successfully\\n")
    except Exception as e:
        print("Warning: Could not create marker file: " + str(e), file=sys.stderr)
    
    # Write success stats to outfile
    # For downloads, we have one file (the tarball itself)
    result_ad = classad2.ClassAd({{
        'TransferFileName': url,
        'TransferSuccess': True,
        'TransferUrl': url,
        'TransferTotalBytes': len(tarball_data),
    }})
    
    if outfile:
        try:
            with open(outfile, 'w') as f:
                f.write(str(result_ad))
        except Exception as e:
            print("Warning: Could not write output file: " + str(e), file=sys.stderr)
    
    sys.exit(0)

def handle_upload(infile, outfile):
    """Handle upload operation - create tarball and upload to Pelican"""
    # Read the input file which contains a single ClassAd with tarfiles list
    try:
        with open(infile, 'r') as f:
            content = f.read()
        
        ad = classad2.parseOne(content)
    except Exception as e:
        print("TransferError = \\"Failed to read input file: " + str(e) + "\\"", file=sys.stderr)
        sys.exit(1)
    
    if not ad:
        print("TransferError = \\"No ClassAd found in input file\\"", file=sys.stderr)
        sys.exit(1)
    
    # Get the upload URL
    url = ad.get('Url', None)
    if not url:
        print("TransferError = \\"No Url in ClassAd\\"", file=sys.stderr)
        sys.exit(1)

    # Get the Capability (bearer token) if present
    capability = ad.get('Capability', None)

    # Get CA contents for TLS verification if present
    ca_contents = ad.get('PelicanCAContents', None)

    # Get the base directory (scratch directory where files are located)
    base_dir = ad.get('LocalFileName', '.')
    
    # Get the list of files to include in tarball
    tarfiles = ad.get('tarfiles', [])
    if not tarfiles:
        print("TransferError = \\"No tarfiles in ClassAd\\"", file=sys.stderr)
        sys.exit(1)
    
    # Create tarball containing all files, using basename as tarball name
    # Resolve file paths relative to base_dir
    try:
        tarball_io = io.BytesIO()
        with tarfile.open(fileobj=tarball_io, mode='w:gz') as tar:
            for filename in tarfiles:
                # Resolve full path relative to base_dir
                file_path = os.path.join(base_dir, filename)
                if os.path.exists(file_path):
                    # Use basename as the name in the tarball
                    arcname = os.path.basename(file_path)
                    tar.add(file_path, arcname=arcname)
        
        tarball_data = tarball_io.getvalue()
    except Exception as e:
        if outfile:
            error_ad = classad2.ClassAd({{
                'TransferSuccess': False,
                'TransferError': 'Failed to create tarball: ' + str(e)
            }})
            try:
                with open(outfile, 'w') as f:
                    f.write(str(error_ad))
            except:
                pass
        sys.exit(1)
    
    # Convert pelican:// to https://
    if url.startswith('pelican://'):
        url = 'https://' + url[len('pelican://'):]
    
    # Upload tarball via PUT request
    try:
        import ssl
        import tempfile

        # Create SSL context with verification enabled
        ctx = ssl.create_default_context()

        # Use CA contents for verification
        if not ca_contents:
            if outfile:
                error_ad = classad2.ClassAd({{
                    'TransferSuccess': False,
                    'TransferError': 'No PelicanCAContents in ClassAd'
                }})
                try:
                    with open(outfile, 'w') as f:
                        f.write(str(error_ad))
                except:
                    pass
            sys.exit(1)

        # Write CA to temporary file
        with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.pem') as ca_file:
            ca_file.write(ca_contents)
            ca_file_path = ca_file.name

        try:
            ctx.load_verify_locations(cafile=ca_file_path)
            # For test: disable hostname check since we're using localhost
            ctx.check_hostname = False
        finally:
            # Clean up temp file
            try:
                os.unlink(ca_file_path)
            except:
                pass

        req = urllib.request.Request(url, data=tarball_data, method='PUT')
        req.add_header('Content-Type', 'application/x-tar')
        if capability:
            req.add_header('Authorization', f'Bearer {{capability}}')

        with urllib.request.urlopen(req, context=ctx, timeout=4) as response:
            response_data = response.read()
    except Exception as e:
        if outfile:
            error_ad = classad2.ClassAd({{
                'TransferSuccess': False,
                'TransferError': 'Failed to upload: ' + str(e)
            }})
            try:
                with open(outfile, 'w') as f:
                    f.write(str(error_ad))
            except:
                pass
        sys.exit(1)

    # Write sentry file to test directory to prove upload happened via plugin
    test_dir = "{test_dir}"
    sentry_path = os.path.join(test_dir, '.pelican_upload_executed')
    try:
        with open(sentry_path, 'w') as f:
            f.write('Plugin upload executed successfully\\n')
    except Exception as e:
        print('Warning: Could not create upload sentry file: ' + str(e), file=sys.stderr)
    
    # Write success stats to outfile - one ClassAd per file in tarfiles
    if outfile:
        try:
            with open(outfile, 'w') as f:
                for filename in tarfiles:
                    file_path = os.path.join(base_dir, filename)
                    result_ad = classad2.ClassAd({{
                        'TransferFileName': file_path,
                        'TransferSuccess': True,
                        'TransferUrl': url,
                        'TransferTotalBytes': len(tarball_data),
                    }})
                    f.write(str(result_ad))
                    f.write('\\\\n')
        except Exception as e:
            print('Warning: Could not write output file: ' + str(e), file=sys.stderr)
    
    sys.exit(0)

if __name__ == '__main__':
    main()
'''
    
    plugin_path.write_text(plugin_code)
    plugin_path.chmod(0o755)
    
    return plugin_path


@standup
def condor(test_dir, pelican_server, pelican_plugin):
    """Set up HTCondor with Pelican plugin enabled"""
    with Condor(
        local_dir=test_dir / "condor",
        config={
            "ENABLE_URL_TRANSFERS": "TRUE",
            "ENABLE_PELICAN_FILE_TRANSFER": "TRUE",
            "FILETRANSFER_PLUGINS": f"$(LIBEXEC)/curl_plugin, {pelican_plugin}",
            # Enable Pelican functionality
            "ENABLE_PELICAN_TRANSFERS": "TRUE",
            "PELICAN_REGISTRATION_SOCKET": pelican_server['unix_socket'],
            # Since auto-detection of CAs is working, we don't need to set CA file
            # "PELICAN_CA_FILE": str(pelican_certs['ca_cert']),
            # Set verbose logging for debugging
            "SHADOW_DEBUG": "D_FULLDEBUG",
            "STARTER_DEBUG": "D_FULLDEBUG",
        }
    ) as condor:
        yield condor


@standup
def pelican_plugin_configured(pelican_plugin):
    """Marker fixture indicating plugin is configured"""
    return pelican_plugin


@action
def test_job_handle(condor, test_dir, pelican_server, pelican_plugin_configured):
    """Submit a job that uses Pelican for input transfer"""
    
    # Create input files for the job
    input1 = test_dir / "input1.txt"
    input1.write_text("Hello from input1")
    
    input2 = test_dir / "input2.txt"
    input2.write_text("Hello from input2")
    
    # Create job that reads input files and checks for marker
    job_script = test_dir / "test_job.sh"
    job_script.write_text("""#!/bin/bash
echo "Reading input files..."
cat input1.txt
cat input2.txt
echo "Job completed successfully"

# Check if plugin marker exists
if [ -f .pelican_plugin_executed ]; then
    echo "PELICAN_PLUGIN_MARKER_FOUND"
fi

# Create an output file for output transfer testing
echo "This is output data from the job" > output.txt
echo "Created output file"
""")
    job_script.chmod(0o755)
    
    # Submit job
    # The shadow will automatically transform the input files to use Pelican
    # based on the FILETRANSFER_PLUGINS configuration
    sub = htcondor2.Submit({
        "executable": str(job_script),
        "transfer_input_files": "input1.txt,input2.txt",
        "transfer_output_files": "output.txt",
        "output": "test.out",
        "error": "test.err",
        "log": str(test_dir / "test.log"),
        "should_transfer_files": "YES",
        "when_to_transfer_output": "ON_EXIT",
    })
    
    handle = condor.submit(sub)
    
    return handle


class TestPelicanInputTransfer:
    """Test Pelican input sandbox transfers"""
    
    def test_pelican_input_transfer(self, test_job_handle, condor, test_dir):
        """Test that job successfully transfers input via Pelican"""
        
        # Wait for job to complete (timeout after 60 seconds)
        # The wait() returns True if condition was met, False if timeout
        completed = test_job_handle.wait(condition=ClusterState.all_terminal, timeout=60)
        assert completed, "Job did not complete within timeout"
        
        # Check output file exists
        output_file = test_dir / "test.out"
        assert output_file.exists(), "Job output file not found"
        
        output_content = output_file.read_text()
        
        # Verify input files were transferred
        assert "Hello from input1" in output_content, "input1.txt content not found in output"
        assert "Hello from input2" in output_content, "input2.txt content not found in output"
        assert "Job completed successfully" in output_content, "Job completion message not found"
        assert "PELICAN_PLUGIN_MARKER_FOUND" in output_content, "Pelican plugin was not executed for input transfer"
        
        # Verify output file was created by job
        assert "Created output file" in output_content, "Job did not create output file"
        
        # Verify output file was transferred back
        output_data_file = test_dir / "output.txt"
        assert output_data_file.exists(), "Output file was not transferred back"
        
        output_data_content = output_data_file.read_text()
        assert "This is output data from the job" in output_data_content, "Output file content incorrect"
        
        # Verify Pelican plugin was used for output transfer (not CEDAR fallback)
        upload_sentry_file = test_dir / ".pelican_upload_executed"
        assert upload_sentry_file.exists(), "Pelican plugin was not executed for output transfer - CEDAR fallback may have been used"
        
        # Verify the uploaded tarball was received by the mock server
        from test_pelican_input_transfer import PelicanHTTPHandler
        assert hasattr(PelicanHTTPHandler, 'uploaded_files'), "No uploaded files tracked by mock server"
        
        # Find our job ID in the uploaded files
        job_id = None
        for jid in PelicanHTTPHandler.uploaded_files.keys():
            job_id = jid
            break
        
        assert job_id is not None, "No job uploads found in mock server"
        
        # Verify output.txt was in the uploaded tarball
        uploaded_files = PelicanHTTPHandler.uploaded_files[job_id]
        assert 'output.txt' in uploaded_files, "output.txt was not in uploaded tarball"
        
        # Verify content matches
        uploaded_content = uploaded_files['output.txt'].decode('utf-8')
        assert "This is output data from the job" in uploaded_content, "Uploaded output.txt content incorrect"
