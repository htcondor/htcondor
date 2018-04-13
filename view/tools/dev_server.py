#!usr/bin/env python3

import http.server
import socketserver
import argparse

HANDLER = http.server.SimpleHTTPRequestHandler


def parse_args():
    parser = argparse.ArgumentParser(
        description = 'Run a local HTTP server for developing and testing HTCondor View\n\n     DO NOT USE IN PRODUCTION',
        formatter_class = argparse.RawTextHelpFormatter,
    )
    parser.add_argument(
        '-p', '--port',
        action = 'store',
        type = int,
        default = 8000,
        help = 'the port to serve on',
    )

    return parser.parse_args()


if __name__ == '__main__':
    args = parse_args()

    with socketserver.TCPServer(('127.0.0.1', args.port), HANDLER) as httpd:
        print('DO NOT USE IN PRODUCTION')
        print(f'Serving HTCondor View at localhost:{args.port}')
        print('ctrl-C to exit...')
        httpd.serve_forever()
