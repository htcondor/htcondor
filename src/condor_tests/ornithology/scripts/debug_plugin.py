#!/usr/bin/env python3
############################################################################
## This debug plugin takes URL's encoded with how to function via a main
## command and potentially encoded information about what ClassAd information
## to return. URL information encoding is only applicable to two commands
## (see below). All encoding is specified via a keyword option, an option
## source type, and the encoding details wrapped in square brackets.
##
## Available Commands:
##     - SLEEP   -> sleep for specified time
##     - SIGNAL  -> Send specified signal to self  (as defined by python signal module)
##     - EXIT    -> Exit immediately with specified code
##     - ERROR   -> Mimic a transfer failure using information encoded in
##                  URL to create returned ClassAd
##     - SUCCESS -> As ERROR but successful transfer
##
## Available option encodings (excluding delete option):
##     - None: option -> ../resolution/..
##     - Inline: option[ClassAd] -> ../resolution[foo="bar";baz=false;DeveloperData=[A=1;B=2.45;];]/..
##     - File: option~[path/filename] -> ../resolution~[/home/miron/test.ad]/..
##     - Script: option#[./executable] -> ../resolution#[./generate_dynamic_ad.py]/..
##
## Encoding options as detailed via: https://docs.google.com/document/d/1Gbb7XDOtmLX_xcfmCwP8KAZ_s6PDDGgCamGoDl145Ww/edit?usp=sharing
##     - general        -> Update full result ad with encoded ClassAd
##     - delete         -> Delete listed attributes from full result ad (delete[foo,bar,...])
##     - parameter      -> Add parameter failure attempt to TransferErrorData
##     - resolution     -> Add resolution failure attempt to TransferErrorData
##     - contact        -> Add contact failure attempt to TransferErrorData
##     - authentication -> Add autentication failure attempt to TransferErrorData
##     - specification  -> Add specification failure attempt to TransferErrorData
##     - transfer       -> Add transfer failure attempt to TransferErrorData
############################################################################

import sys
import os
import argparse
import textwrap
import time
import signal
import re
import enum
import subprocess


# ----------------------------------------------------------------------------
EXIT_SUCCESS = 0
EXIT_SETUP_FAILURE = 125
EXIT_INVALID_URL = 124

# ----------------------------------------------------------------------------
try:
    import classad2 as classad
except ImportError:
    sys.stderr.write("Failed to import HTCondor classad2 module\n")
    sys.exit(EXIT_SETUP_FAILURE)


# ----------------------------------------------------------------------------
class DebugUrlParseError(Exception):
    """Custom URL parse failure exception"""

    def __init__(self, msg=None):
        self.message = msg if msg is not None else "Failed to parse debug plugin encoded URL"
        super().__init__(self.message)


# ----------------------------------------------------------------------------
class ActionType(enum.Enum):
    """Enumeration of encoding actions"""

    NONE = 0  # Do nothing (i.e. use defaults)
    INLINE = 1  # Information encoded inline in URL
    FILE = 2  # Information is provided from provided file path
    SCRIPT = 3  # Information returned via stdout of provided executable


# ----------------------------------------------------------------------------
PROGRAM_NAME = "debug_plugin.py"
PLUGIN_VERSION = "1.0.0"
SUPPORTED_SCHEMAS = "debug,encode"

DEFAULT_HOSTNAME = "default.test.hostname"

OPT_ERR_PARAMETER = "parameter"
OPT_ERR_RESOLUTION = "resolution"
OPT_ERR_CONTACT = "contact"
OPT_ERR_AUTH = "authentication"
OPT_ERR_SPECIFICATION = "specification"
OPT_ERR_XFER = "transfer"
OPT_GENERAL = "general"
OPT_DELETE = "delete"

OPTION_DEFAULTS = {
    OPT_GENERAL: {},
    OPT_DELETE: [],
    OPT_ERR_PARAMETER: {
        "ErrorCode": -1,
        "ErrorString": "Invalid plugin parameters",
        "PluginVersion": PLUGIN_VERSION,
        "PluginLaunched": True,
    },
    OPT_ERR_RESOLUTION: {
        "ErrorCode": -2,
        "ErrorString": "Failed to resolve server name to contact",
        "FailedName": DEFAULT_HOSTNAME,
        "FailureType": "Definitive",
    },
    OPT_ERR_CONTACT: {
        "ErrorCode": -3,
        "ErrorString": "Failed to contact server",
        "FailedServer": DEFAULT_HOSTNAME,
    },
    OPT_ERR_AUTH: {
        "ErrorCode": -4,
        "ErrorString": "Failed authorization with server",
        "FailedServer": DEFAULT_HOSTNAME,
        "FailureType": "Authentication",
        "ShouldRefresh": True,
    },
    OPT_ERR_SPECIFICATION: {
        "ErrorCode": -5,
        "ErrorString": "Speficied file DNE",
        "FailedServer": DEFAULT_HOSTNAME,
    },
    OPT_ERR_XFER: {
        "ErrorCode": -6,
        "ErrorString": "Failed to transfer file",
        "FailedServer": DEFAULT_HOSTNAME,
        "FailureType": "ToSlow",
    },
}


ACTION_TYPE_STR = {
    ActionType.NONE: "DetailsFromDefaults",
    ActionType.INLINE: "DetailsFromInline",
    ActionType.FILE: "DetailsFromFile",
    ActionType.SCRIPT: "DetailsFromScript",
}


# ----------------------------------------------------------------------------
def format_error(error: Exception) -> str:
    """Format an exception into error string"""
    return "{0}: {1}".format(type(error).__name__, str(error))


# ----------------------------------------------------------------------------
def get_error_dict(error, url="") -> dict:
    """Return runtime plugin failure ClassAd"""
    error_string = format_error(error)
    error_dict = {
        "TransferSuccess": False,
        "TransferError": error_string,
        "TransferUrl": url,
    }

    return error_dict


# ----------------------------------------------------------------------------
def parse_portion(url: str):
    """Parse out and yield the next portion of the URL to process"""
    option = ""
    details = ""
    action = ActionType.NONE
    in_brackets = 0
    trailing = False
    specifier = None

    # Parse URL one character at a time
    for c in url:
        # Always expect a '[' after an action specifier ('~','#')
        if specifier is not None and c != "[":
            raise DebugUrlParseError(f"Special file path delimeter token '{specifier}' not followed by '['")
        else:
            specifier = None

        # If not in brackets and '/' then yield stored information and rest
        if in_brackets == 0 and c == "/":
            yield (option, action, details)
            option = details = ""
            action = ActionType.NONE
            trailing = False
        # Ignore all information post closing bracket ']'
        elif trailing:
            continue
        # Process start of specified encoding details
        elif c == "[":
            # If no action type has been provided then info is inline
            if action == ActionType.NONE:
                action = ActionType.INLINE
            # Count number of open brackets for nested information
            in_brackets += 1
            if action == ActionType.INLINE:
                details += c
        # Process end of specified encoding details
        elif c == "]":
            # Invalid URL if no start character ('[') found
            if in_brackets <= 0:
                raise DebugUrlParseError("Invalid URL has close bracket with no open.")
            # Found a pair of brackets '[]' reduce count
            in_brackets -= 1
            # If all bracket pairs found ignore trailing characters
            if in_brackets == 0:
                trailing = True
            if action == ActionType.INLINE:
                details += c
        # Store the details if in brackets (i.e. found open but not close)
        elif in_brackets > 0:
            details += c
        # Tilde specifies details is a file path to read
        elif c == "~":
            action = ActionType.FILE
            specifier = c
        # Hashtag specifies details is an executable to run
        elif c == "#":
            action = ActionType.SCRIPT
            specifier = c
        # If here then must be the start of the portion which is the sub option
        else:
            option += c

    # Went through full URL and has a mismatch of brackets
    if in_brackets != 0:
        raise DebugUrlParseError("Invalid URL has open bracket with no close.")

    # yield the final portion information
    yield (option, action, details)


# ----------------------------------------------------------------------------
def extract_details(action: ActionType, details: str) -> dict:
    """Extract encoded information from a string"""
    ad = {}

    # Action type is none so do nothing (use default)
    if action == ActionType.NONE:
        pass
    # Action type is inline ClassAd
    elif action == ActionType.INLINE:
        try:
            # Replace any '+' w/ spaces (%20's) already replaced and attempt to parse ClassAd
            details = details.replace("+", " ")
            inline_ad = classad.ClassAd(details)
            ad.update(inline_ad)
        except Exception as e:
            raise DebugUrlParseError(f"Failed to parse inline information '{details}' into ClassAd: {e}")
    # Action type is file
    elif action == ActionType.FILE:
        try:
            # Attempt to parse one ClassAd from the file
            with open(details, "r") as f:
                f_ad = classad.parseOne(f.read())
                ad.update(f_ad)
        except Exception as e:
            raise DebugUrlParseError(f"Failed to parse file '{details}' into ClassAd: {e}")
    # Action type is a script
    elif action == ActionType.SCRIPT:
        try:
            # Replace any '&&' w/ spaces (%20's) already replaced
            details = details.replace("&&", " ")
            # Attempt to execute the script. Non-zero exit code is failure
            p = subprocess.run(details.split(" "), stdout=subprocess.PIPE)
            if p.returncode != EXIT_SUCCESS:
                raise RuntimeError(f"Failed to excute script")
            # Attempt to parse one ClassAd from stdout of script
            output = p.stdout.rstrip().decode()
            p_ad = classad.parseOne(output)
            ad.update(p_ad)
        except Exception as e:
            raise DebugUrlParseError(f"Failed to parse script '{details}' output into ClassAd: {e}")
    # Unknown action type...
    else:
        raise DebugUrlParseError(f"Unknown specified option action: {action}")

    return ad


# ----------------------------------------------------------------------------
class DebugPlugin:
    def __init__(self):
        self.exit_code = EXIT_SUCCESS
        self.sub_cmd_counts = {
            OPT_ERR_PARAMETER: 0,
            OPT_ERR_RESOLUTION: 0,
            OPT_ERR_CONTACT: 0,
            OPT_ERR_AUTH: 0,
            OPT_ERR_SPECIFICATION: 0,
            OPT_ERR_XFER: 0,
            OPT_GENERAL: 0,
            OPT_DELETE: 0,
        }
        self.details_counts = {
            ActionType.NONE: 0,
            ActionType.INLINE: 0,
            ActionType.FILE: 0,
            ActionType.SCRIPT: 0,
        }

    def reset_per(self):
        """Reset per URL statistics/data"""
        self.sub_cmd_counts = {key: 0 for key in self.sub_cmd_counts.keys()}
        self.details_counts = {key: 0 for key in self.details_counts}

    def get_dev_data(self) -> dict:
        """Return nested ClassAd of data pertaining to plugin execution"""
        data = {
            "CurrentPluginExitCode": self.exit_code,
        }
        data.update({f"Num{key.capitalize()}Option": val for key, val in self.sub_cmd_counts.items() if val > 0})
        data.update({ACTION_TYPE_STR[key]: val for key, val in self.details_counts.items() if val > 0})
        return data

    def process_url(self, url: str, is_success: bool = False) -> dict:
        """Process full URL for encoded information"""
        info = {}
        attempts = []
        err_msg = "URL transfer encoded to fail"

        # Loop through custom parsed URL portions
        for option, action, details in parse_portion(url):
            # Replace all %20 w/ spaces
            details = details.replace("%20", " ")
            # Check if portion has specified inner option
            if option.lower() not in OPTION_DEFAULTS:
                continue

            # Copy default option ad
            ad = OPTION_DEFAULTS.get(option.lower()).copy()

            # Update internal statistics
            self.sub_cmd_counts[option.lower()] += 1
            self.details_counts[action] += 1

            # If 'general' option: update main result ad
            if re.match(OPT_GENERAL, option, re.I):
                info.update(extract_details(action, details))
                continue

            # If 'delete' option: delete specified keys from main result ad
            if re.match(OPT_DELETE, option, re.I):
                # Expects: delete[attr{,attr,...}]
                if action != ActionType.INLINE or details.strip("[").strip("]") == "":
                    raise DebugUrlParseError(f"Invalid {OPT_DELETE} option specified: No inline details.")
                for attr in details[1:-1].split(","):
                    if attr in info:
                        del info[attr]
                continue

            # If specified as a successful transfer then ignore failure options
            if is_success:
                continue

            # Set the ErrorType attr in failure ad
            ad["ErrorType"] = option.lower().capitalize()
            # ad["DeveloperData"] = {}

            # Update this ad with encoded information
            ad.update(extract_details(action, details))

            # Add this ad to list of failure attempts
            attempts.append(ad)

        # If failure attempts have been specified then add ads to result ad
        if len(attempts) > 0:
            info["TransferErrorData"] = attempts
            info["TransferError"] = err_msg  # We should do something better here

        return info

    def transfer_file(self, url: str, local_file: str, upload: bool) -> dict:
        """Mock transfer a file to produce return ClassAd from information encoded from URL"""
        self.reset_per()
        now = int(time.time())

        schema, data = url.split("://")

        # Default result ad
        RESULT = {
            "TransferProtocol": schema,
            "TransferType": "upload" if upload else "download",
            "TransferFileName": local_file,
            "TransferFileBytes": 0,
            "TransferTotalBytes": 0,
            "TransferStartTime": now,
            "TransferEndTime": now,
            "ConnectionTimeSeconds": 0,
            "TransferUrl": url,
        }

        try:
            # Parse main command from start of URL: cmd/.../...
            cmd, info = data.split("/", 1) if "/" in data else (data, "")
            cmd = cmd.upper()

            # Check main command

            # SLEEP (debug://sleep/<time>) -> Sleep for specified time (likely to invoke timeout)
            if cmd == "SLEEP":
                try:
                    timeout = int(info.split("/")[0])
                except Exception as e:
                    raise DebugUrlParseError(f"Invalid {cmd} command: {e}")
                time.sleep(timeout)
                raise RuntimeError("Timeout delay has ended and plugin is still executing")
            # SIGNAL (debug://signal/SIGTERM) -> Send a signal to self
            elif cmd == "SIGNAL":
                value = info.split("/")[0]
                SIGNAL = getattr(signal, value, signal.SIGTERM)
                os.kill(os.getpid(), SIGNAL)
            # ERROR (debug://error/...) -> Specify a transfer failure with encoded return ad information
            elif cmd == "ERROR":
                RESULT["TransferSuccess"] = False
                RESULT.update(self.process_url(info))
                if self.exit_code == EXIT_SUCCESS:
                    self.exit_code = 1
            # SUCCESS (debug://success/...) -> Specify a successful transfer with encoded return ad information
            elif cmd == "SUCCESS":
                RESULT["TransferSuccess"] = True
                RESULT.update(self.process_url(info, True))
            # EXIT (debug://exit/<code>) -> Inform plugin to exit right now with the specific code
            elif cmd == "EXIT":
                try:
                    # Limit exit code range 0-123
                    code = [0, int(info.split("/")[0]), 123][1]
                except Exception as e:
                    raise DebugUrlParseError(f"Invalid {cmd} command: {e}")
                # Only override exit code if no invalid URL's were parsed
                if self.exit_code != EXIT_INVALID_URL:
                    self.exit_code = code
                RESULT["TransferSuccess"] = (self.exit_code == EXIT_SUCCESS)
            else:
                raise DebugUrlParseError(f"Unknown debug URL command '{cmd}'")
        # Catch all exceptions and fail due to an invalid encoded URL provided
        except Exception as e:
            RESULT.update(get_error_dict(e, url))
            self.exit_code = EXIT_INVALID_URL

        # Add general debugging Developer Data to output
        RESULT["DeveloperData"] = self.get_dev_data()

        return RESULT


# ----------------------------------------------------------------------------
def recursive(l: list):
    """Recursively loop through nested lists"""
    for item in l:
        if type(item) is list:
            yield from recursive(item)
        else:
            yield item


# ----------------------------------------------------------------------------
def parse_args():
    """Function to handle all CLI argument parsing"""

    parser = argparse.ArgumentParser(
        prog=PROGRAM_NAME,
        description=textwrap.dedent(
            f"""
            Center for High Throughput Computing, Computer Sciences
            Department, University of Wisconsin-Madison, Madison, WI, US

            Debug Plugin
                This is a custom HTCondor file transfer plugin that parses
                provided URL's to determine how the plugin writes output
                data and exits to provide a testing infrastucture for how
                the file transfer acts provided specific scenarios.
            """
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    parser.add_argument(
        "-classad",
        dest="classad",
        action="store_true",
        default=False,
        help="Print a ClassAd containing the capablities of this file transfer plugin.",
    )

    parser.add_argument(
        "-infile",
        dest="infile",
        action="store",
        type=str,
        default=None,
        help="Input ClassAd file",
    )

    parser.add_argument(
        "-outfile",
        dest="outfile",
        action="store",
        type=str,
        default=None,
        help="Output ClassAd file",
    )

    parser.add_argument(
        "-upload",
        dest="upload",
        action="store_true",
        default=False,
        help="Indicates this transfer is an upload (default is download).",
    )

    parser.add_argument(
        "-test",
        dest="testing",
        action="append",
        nargs="+",
        default=None,
        help="Return plugin result dictionary for each provided test url.",
    )

    parser.add_argument(
        "-v",
        "-verbose",
        dest="verbosity",
        action="count",
        default=0,
        help="Increase plugin verbosity",
    )

    args = parser.parse_args()

    # Special -test flag directly parses provided url and prints resulting ClassAd
    if args.testing is not None:
        plugin = DebugPlugin()
        for url in recursive(args.testing):
            if args.verbosity > 0:
                print(f"{url} yields:")
            result = plugin.transfer_file(url, "TESTING-DUMMY", args.upload)
            try:
                print(str(classad.ClassAd(result)))
            except Exception as e:
                if args.verbosity > 0:
                    print(f"Error: Failed to convert results to ClassAd: {e}")
                    print(result)
        sys.exit(plugin.exit_code)

    # Ensure either -classad or (-infile and -outfile) specified
    if not args.classad and None in [args.infile, args.outfile]:
        parser.print_usage()
        print(f"{PROGRAM_NAME}: error: invlaid args: expects either -classad or both -infile and -outfile")
        raise RuntimeError

    # Return capabilities ClassAd and exit
    if args.classad:
        capabilities = {
            "MultipleFileSupport": True,
            "PluginType": "FileTransfer",
            "SupportedMethods": SUPPORTED_SCHEMAS,
            "Version": PLUGIN_VERSION,
        }
        sys.stdout.write(classad.ClassAd(capabilities).printOld())
        sys.exit(EXIT_SUCCESS)

    return args


# ----------------------------------------------------------------------------
def main():
    try:
        args = parse_args()
    except Exception:
        sys.exit(EXIT_SETUP_FAILURE)

    plugin = DebugPlugin()

    # Parse in the classads stored in the input file.
    # Each ad represents a single file to be transferred.
    try:
        infile_ads = classad.parseAds(open(args.infile, "r"))
    except Exception as err:
        try:
            with open(args.outfile, "w") as outfile:
                outfile_dict = get_error_dict(err)
                outfile.write(str(classad.ClassAd(outfile_dict)))
        except Exception:
            pass
        sys.exit(EXIT_SETUP_FAILURE)

    # Now iterate over the list of classads and perform the transfers.
    try:
        with open(args.outfile, "w") as outfile:
            for ad in infile_ads:
                try:
                    outfile_dict = plugin.transfer_file(ad["Url"], ad["LocalFileName"], args.upload)
                    outfile.write(str(classad.ClassAd(outfile_dict)))
                except Exception as err:
                    try:
                        outfile_dict = get_error_dict(err, url=ad["Url"])
                        outfile.write(str(classad.ClassAd(outfile_dict)))
                    except Exception:
                        pass
                    sys.exit(EXIT_SETUP_FAILURE)

    except Exception:
        sys.exit(EXIT_SETUP_FAILURE)

    sys.exit(plugin.exit_code)


# ----------------------------------------------------------------------------
if __name__ == "__main__":
    main()
