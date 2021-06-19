#!/usr/bin/env python3

import argparse
import htcondor
import os
import re
import sys
import stat

from .conf import *
from .dagman import DAGMan
from .job import Job


def print_help(stream=sys.stderr):
    help_msg = """htcondor is a tool for managing HTCondor jobs and resources.

Usage: htcondor <object> <action> [<target>] [<option1> <option2> ...]

Managing jobs:
    htcondor job submit <filename.sub>      Submits filename.sub
    htcondor job status <job-id>            Shows current status of job-id
    htcondor job resources <job-id>         Shows resources used by job-id

    Options:
        --resource=<type>                   Resource to run this job. Supports Slurm and EC2.
        --runtime=<time-seconds>            Runtime for this resource (seconds)
        --node_count=<count>                Number of nodes to provision
        --email=<address@domain.com>        Email address to receive notifications

"""
    stream.write(help_msg.format(sys.argv[0]))


def parse_args():

    # The only arguments that are acceptable are
    # htcondor <object> <action>
    # htcondor <object> <action> <target>
    # htcondor <object> <action> <target> [<option1> <option2> ...]

    if len(sys.argv) < 3:
        print_help()
        sys.exit(1)

    parser = argparse.ArgumentParser()
    command = {}
    options = {}

    # Command arguments: up to 3 unflagged arguments at the beginning
    parser.add_argument("command", nargs="*")
    # Options arguments: optional flagged arguments following the command args
    parser.add_argument("--resource", help="Type of compute resource")
    parser.add_argument("--runtime", type=int, action="store", help="Runtime for provisioned Slurm glideins (seconds)")
    parser.add_argument("--node_count", type=int, action="store", help="Number of Slurm nodes to provision")
    parser.add_argument("--email", type=str, action="store", help="Email address for notifications")
    #parser.add_argument("--schedd", help="Address to remote schedd to query")

    args = parser.parse_args()

    command_object = args.command[0]
    try:
        command["object"] = command_object.lower()
    except:
        print(f"Error: Object must be a string")
        sys.exit(1)

    command_action = args.command[1]
    try:
        command["action"] = command_action.lower()
    except:
        print(f"Error: Action must be a string")
        sys.exit(1)

    if len(args.command) > 2:
        command["target"] = args.command[2]

    if args.resource is not None:
        options["resource"] = args.resource.lower()

    if args.email is not None:
        options["email"] = args.email.lower()
    else:
        options["email"] = None

    if args.runtime is not None:
        try:
            options["runtime"] = int(args.runtime)
        except:
            print(f"Error: The --runtime argument must take a numeric value")
            sys.exit(1)
    
    if args.node_count is not None:
        try:
            options["node_count"] = int(args.node_count)
        except:
            print(f"Error: The --node_count argument must take a numeric value")
            sys.exit(1)

    return {
        "command": command,
        "options": options
    }


def main():

    # Parse arguments and set default values if undeclared
    try:
        args = parse_args()
    except Exception as err:
        print(f"Failed to parse arguments: {err}", file=sys.stderr)

    # Assume any error checking on the inputs happens during parsing
    command = args["command"]
    options = args["options"]

    # Make sure we have a schedd!
    try:
        schedd = htcondor.Schedd()
    except:
        print(f"Could not access local schedd. This tool must be a run from an HTCondor submit machine.")
        sys.exit(1)

    # Figure out what the user is asking for and provide it
    if command["object"] == "job":
        if command["action"] == "submit":
            Job.submit(command["target"], options)
        elif command["action"] == "status":
            Job.status(command["target"], options)
        elif command["action"] == "resources":
            Job.resources(command["target"], options)
        else:
            print(f"Error: The '{command['action']}' action is not supported for jobs")
    else:
        print(f"Error: '{command['object']}' is not a valid object") 

    # All done
    sys.exit(0)


if __name__ == "__main__":
    main()