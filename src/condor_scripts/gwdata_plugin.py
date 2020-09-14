#!/usr/bin/env python3

import argparse
import classad
import gwdatafind
import io
import json
import os
import pathlib
import posixpath
import pycurl
import shutil
import socket
import sys
import time

try:
    from urllib.parse import urlparse # Python 3
except ImportError:
    from urlparse import urlparse # Python 2

DEFAULT_TIMEOUT = 30
GWDATA_PLUGIN_VERSION = '1.0.0'

def print_help(stream = sys.stderr):
    help_msg = '''Usage: {0} -infile <input-filename> -outfile <output-filename>
       {0} -classad

Options:
  -classad                    Print a ClassAd containing the capablities of this
                              file transfer plugin.
  -infile <input-filename>    Input ClassAd file
  -outfile <output-filename>  Output ClassAd file
  -upload                     Not supported
'''
    stream.write(help_msg.format(sys.argv[0]))

def print_capabilities():
    capabilities = {
         'MultipleFileSupport': True,
         'PluginType': 'FileTransfer',
         'SupportedMethods': 'gwdata',
         'Version': GWDATA_PLUGIN_VERSION,
    }
    sys.stdout.write(classad.ClassAd(capabilities).printOld())

def parse_args():
    '''The optparse library can't handle the types of arguments that the file
    transfer plugin sends, the argparse library can't be expected to be
    found on machines running EL 6 (Python 2.6), and a plugin should not
    reach outside the standard library, so the plugin must roll its own argument
    parser. The expected input is very rigid, so this isn't too awful.'''

    # The only argument lists that are acceptable are
    # <this> -classad
    # <this> -infile <input-filename> -outfile <output-filename>
    # <this> -outfile <output-filename> -infile <input-filename>
    if not len(sys.argv) in [2, 5, 6]:
        print_help()
        sys.exit(-1)

    # If -classad, print the capabilities of the plugin and exit early
    if (len(sys.argv) == 2) and (sys.argv[1] == '-classad'):
        print_capabilities()
        sys.exit(0)

    # Upload is not supported
    if '-upload' in sys.argv[1:]:
        print("Upload is not supported. You should not be seeing this.")
        sys.exit(-1)

    # -infile and -outfile must be in the first and third position
    if not (
            ('-infile' in sys.argv[1:]) and
            ('-outfile' in sys.argv[1:]) and
            (sys.argv[1] in ['-infile', '-outfile']) and
            (sys.argv[3] in ['-infile', '-outfile']) and
            (len(sys.argv) == 5)):
        print_help()
        sys.exit(-1)
    infile = None
    outfile = None
    try:
        for i, arg in enumerate(sys.argv):
            if i == 0:
                continue
            elif arg == '-infile':
                infile = sys.argv[i+1]
            elif arg == '-outfile':
                outfile = sys.argv[i+1]
    except IndexError:
        print_help()
        sys.exit(-1)

    return {'infile': infile, 'outfile': outfile}

def format_error(error):
    return '{0}: {1}'.format(type(error).__name__, str(error))

def get_error_dict(error, url = ''):
    error_string = format_error(error)
    error_dict = {
        'TransferSuccess': False,
        'TransferError': error_string,
        'TransferUrl': url,
    }

    return error_dict

class GWDataPlugin:

    #def __init__(self):

    def get_urls(self, host, args):

        urls = []

        # Parse the input arguments
        for arg in args:
            attr, value = arg.split("=")
            if attr == "observatory": observatory = value
            if attr == "type": type = value
            if attr == "s": start_frame = value
            if attr == "e": end_frame = value

        # Retrieve the list of URLs
        try:
            urls = gwdatafind.find_urls(
                host=host,
                site=observatory,
                frametype=type,
                gpsstart=int(start_frame),
                gpsend=int(end_frame)
            )
        except Exception as e:
            print("Error retrieving gwdatafind URLs: " + str(sys.exc_info()[0]) + ": " + str(e))

        return urls

    def create_cache(self, args, urls):

        metadata_filename = "metadata.txt"

        # Parse the input arguments
        for arg in args:
            attr, value = arg.split("=")
            if attr == "e": end_frame = int(value)
            if attr == "cache": cache = value
            if attr == "metadata_file": metadata_filename = value

        metadata_file = open(metadata_filename, "w")

        if cache == "lal" or cache == "lal-cache":
            for url in urls:
                file_name = url.split("/")[-1]
                metadata_tokens = file_name.split("-")
                metadata_file.write("{0} {1} {2} {3} {4}\n".format(
                    metadata_tokens[0],
                    metadata_tokens[1],
                    metadata_tokens[2],
                    metadata_tokens[3].split(".")[0],
                    pathlib.Path().absolute()
                ))
        elif cache == "frame" or cache == "frame-cache":
            for url in urls:

                # Filename and metadata specific to this url
                file_name = url.split("/")[-1]
                metadata_tokens = file_name.split("-")

                # If this is the first url in the list, set a couple gps tracking variables
                if url == urls[0]:
                    gps_start = int(metadata_tokens[2])
                    gps_counter = gps_start
                else:
                    gps_current = int(metadata_tokens[2])
                    gps_interval = int(metadata_tokens[3].split(".")[0])
                    gps_counter += gps_interval

                    # If this url represents a gap, write data to file
                    if (gps_current != gps_counter) or (url == urls[-1]):
                        gps_end = gps_counter
                        if url == urls[-1]:
                            gps_end = end_frame

                        metadata_file.write("{0} {1} {2} {3} {4} {5}\n".format(
                            metadata_tokens[0],
                            metadata_tokens[1],
                            gps_start,
                            gps_end,
                            gps_interval,
                            pathlib.Path().absolute()
                        ))
                        # Now reset reset gps tracking variables
                        gps_start = gps_current
                        gps_counter = gps_start
        else:
            metadata_file.write("Error: {0} is not a valid cache type. Allowed types are \"lal\", \"lal-cache\", \"frame\" and \"frame-cache\".\n".format(cache))

        metadata_file.close()


    def download_data(self, url):

        transfer_stats = []
        
        gwdatafind_server = url[(url.find("://") + 3):url.find("?")]
        gwdata_args = url[(url.find("?") + 1):].split("&")

        # First retrieve a list of urls to download from the gwdatafind server
        gwdata_urls = self.get_urls(gwdatafind_server, gwdata_args)

        # Now transfer each url
        curl = pycurl.Curl()
        for gwdata_url in gwdata_urls:

            start_time = time.time()

            # Setup the transfer
            transfer_success = True
            transfer_error = ""
            file_name = gwdata_url.split("/")[-1]
            file_size = 0
            file = open(file_name, "wb")

            # Use pycurl to actually grab the file
            curl.setopt(curl.URL, gwdata_url)
            curl.setopt(curl.WRITEDATA, file)
            try:
                curl.perform()
                file_size = file.tell()
                file.close()
            except Exception as error:
                transfer_success = False
                errno, errstr = error.args
                transfer_error = str(errstr + " (Error " + str(errno) + ")")

            end_time = time.time()

            # Add statistics for this transfer
            this_transfer_stats = {
                'TransferSuccess': transfer_success,
                'TransferProtocol': 'gwdata',
                'TransferType': 'download',
                'TransferFileName': file_name,
                'TransferFileBytes': file_size,
                'TransferTotalBytes': file_size,
                'TransferStartTime': int(start_time),
                'TransferEndTime': int(end_time),
                'ConnectionTimeSeconds': end_time - start_time,
                'TransferUrl': gwdata_url,
            }
            if transfer_error:
                this_transfer_stats['TransferError'] = transfer_error

            transfer_stats.append(this_transfer_stats)

            # If this transfer failed, exit immediately
            if not transfer_success:
                curl.close()
                return transfer_stats, False

        # Check if the args list is requesting a cache file; if so, create it
        if "cache" in url:
            self.create_cache(gwdata_args, gwdata_urls)


        # Looks like everything downloaded correctly. Exit success.
        curl.close()
        return transfer_stats, True



if __name__ == '__main__':

    # Start by parsing input arguments
    try:
        args = parse_args()
    except Exception:
        sys.exit(-1)

    gwdata = GWDataPlugin()

    # Parse in the classads stored in the input file. 
    # Each ad represents a single file to be transferred.
    try:
        infile_ads = classad.parseAds(open(args['infile'], 'r'))
    except Exception as err:
        try:
            with open(args['outfile'], 'w') as outfile:
                outfile_dict = get_error_dict(err)
                outfile.write(str(classad.ClassAd(outfile_dict)))
        except Exception:
            pass
        sys.exit(-1)
    
    # Now iterate over the list of classads and perform the transfers.
    try:
        with open(args['outfile'], 'w') as outfile:
            for ad in infile_ads:
                try:
                    result_ads, transfer_success = gwdata.download_data(ad['Url'])
                    # Output the results to file
                    for ad in result_ads:
                        outfile.write(str(classad.ClassAd(ad)))

                    if not transfer_success:
                        sys.exit(-1)

                except Exception as err:
                    print(err)
                    try:
                        outfile_dict = get_error_dict(err, url = ad['Url'])
                        outfile.write(str(classad.ClassAd(outfile_dict)))
                    except Exception:
                        pass
                    sys.exit(-1)

    except Exception:
        sys.exit(-1)
