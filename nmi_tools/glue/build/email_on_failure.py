#!/usr/bin/python

# The purpose of this script is to email users when an automated build breaks.
# Caveats:
#  - it has a few hardcoded spots that require the build user to be cndrauto.
#  - it only works for builds.  I think tests are too flaky and would generate too much mail
#
# Written by Scot Kronenfeld 2011-06


# The current list of wranglers:
#  - All failures will go to them, even if they are not in the author list.
#  - Error emails will go to them and nobody else.
#WRANGLERS = ["kronenfe@cs.wisc.edu", "gthain@cs.wisc.edu", "johnkn@cs.wisc.edu"]
WRANGLERS = ["kronenfe@cs.wisc.edu", "johnkn@cs.wisc.edu", "gthain@cs.wisc.edu"]

EMAIL_FROM_ADDRESS = 'noreply@cs.wisc.edu'

###############

import re
import os
import sys
import time
import socket

# For connecting to the DB
import MySQLdb

# For sending email
import smtplib
from email.MIMEText import MIMEText

#################################################
# Helper functions that implement all the logic #
#################################################

def validate_input():
    """ Validate the input """
    if len(sys.argv) != 2:
        die_nice("Must pass in exactly one argument, the NMI Run ID that failed")

    if re.search("\D", sys.argv[1]):
        die_nice("Argument supplied does not look like an NMI Run ID.  It contains a non-digit: %s" % sys.argv[1])

    return sys.argv[1]


def load_config(file):
    """ Load a config file of the format key=value. """
    
    config = {}

    f = open(file, 'r')
    for line in f.readlines():
        if re.match("\s*$", line) or re.match("\s*\#", line):
            continue

        m = re.match("\s*(\S+)\s*=\s*(.+)$", line)
        if m:
            config[m.group(1)] = m.group(2).strip()

    return config

def check_runid(config, runid):
    """ Connect to MySQL and determine the SHA1 value for the run and the previous run """

    # Connect to MySQL using credentials from config file
    conn = MySQLdb.connect(host = config["WEB_DB_HOST"], user = config["DB_READER_USER"], passwd = config["DB_READER_PASS"], db = "nmi_history")
    cursor = conn.cursor()

    # To get the last two runs we will first get details about the RunID that we know, then use that info to constrain the second query
    cursor.execute("SELECT description FROM Run WHERE runid = " + runid + ";")
    description = cursor.fetchone()[0]

    # If this is a continuous build then we can leave the description alone.
    # But if it is a branch build we need to strip the date off the end of it.
    if re.search("-2\d\d\d-\d\d?-\d\d?$", description):
        description = re.sub("-2\d\d\d-\d\d?-\d\d?$", "", description)

    cursor.execute("SELECT result,project_version FROM Run WHERE user = \"cndrauto\" AND runid <= " + runid + " AND description LIKE \"" + description + "%\" ORDER BY runid DESC LIMIT 2;")
    (this_result, this_sha1) = cursor.fetchone()
    (prev_result, prev_sha1) = cursor.fetchone()
    cursor.close()
    conn.close()

    print "Current result: %s - Current SHA1: %s" % (this_result, this_sha1)
    print "Previous result: %s - Previous SHA1: %s" % (prev_result, prev_sha1)

    # Check the status of the previous run.  NMI status reference:
    #     Pass - 0
    #     Fail - >0
    #     Pending - NULL
    if this_result == 0:
        die_nice("This script was called for a run that succeeded", runid)
    elif prev_result == None:
        die_nice("The previous run is not complete yet", runid)
    elif prev_result == 0:
        # We have a failure and the previous run succeeded.  Now is our time to shine!
        authors = get_authors(this_sha1, prev_sha1)
        msg = create_email(this_sha1, prev_sha1, description, runid, authors)
        send_mail(authors.values(), msg, runid, description)
            
    else:
        # We don't need to do anything if there are two failures in a row.
        # Maybe someday we will change this behavior.
        print "Previous run failed - no need to send email"
    
    return


def get_authors(new_sha1, old_sha1):
    """ Given two sha1 hashes, get the list of Git authors and emails in between """

    cmd = 'cd /space/git/CONDOR_SRC.git && git log --pretty=format:\'%an || %ae\' ' + old_sha1 + '..' + new_sha1 + ' 2>&1 | sort | uniq -c | sort -nr'

    authors = {}
    for line in os.popen(cmd).read().split("\n"):
        m = re.match("\s*(.+) \|\| (.+)$", line)
        if m:
            authors[m.group(1)] = m.group(2).strip()

    return authors


def create_email(this_sha1, prev_sha1, description, runid, authors):
    """ Create the email to send out """

    # Form a detailed message
    msg = "A build of type '%s' failed.  The previous build of this type succeeded.\n\n" % description
    msg += "Build SHA1 - %s\n" % this_sha1
    msg += "Previous SHA1 - %s\n\n\n" % prev_sha1

    msg += "Committers in between:\n"
    for author in authors.keys():
        msg += "%s <%s>\n" % (author, authors[author])

    msg += "\n"
    msg += "Link to dashboard:\n"
    msg += "http://nmi-s006.cs.wisc.edu/results/Run-condor-details.php?runid=%s&type=build&user=cndrauto\n\n" % runid
    msg += "Log of commits between these two builds:\n"
    msg += "http://condor-git.cs.wisc.edu/?p=condor.git;a=log;h=%s;hp=%s\n\n" % (this_sha1, prev_sha1)

    msg += message_footer()

    return msg

def message_footer():
    """ Add some information at the bottom of the message """

    footer = "\n\n---------- Script Info -------------\n"
    footer += "Script that sent this mail: %s\n" % sys.argv[0]
    footer += "CWD: %s\n" % os.getcwd()
    footer += "Current host: %s\n" % socket.gethostname()
    footer += "Current time: %s\n" % time.strftime("%X %x %Z")
    return footer


def send_mail(emails, msg, runid, description):
    """ Send the email to the supplied people """

    msg = MIMEText(msg)
    msg['Subject'] = "Build error - Run ID %s - Description '%s'" % (runid, description)
    msg['From']    = EMAIL_FROM_ADDRESS
    msg['To']      = str.join(",", WRANGLERS)
    # Uncomment this line to go live.  During testing we will just send to wranglers
    #msg['To']      = str.join(",", emails) + "," + str.join(",", WRANGLERS)

    s = smtplib.SMTP()
    s.connect()
    s.sendmail(EMAIL_FROM_ADDRESS, msg['To'], msg.as_string())
    s.quit()


def die_nice(msg, runid="?"):
    """ Email out an error message to peeps """

    # We will send email with no extra addresses.  This will just go to the wranglers.
    send_mail({}, msg, runid, "Error occurred")

    # Also print this to the screen so it shows up in the NMI build output
    print msg
    print "Exiting with error from %s" % sys.argv[0]

    sys.exit(1)


################
# Control flow #
################

print "Beginning %s" % sys.argv[0]

try:
    # Validate input RunID
    nmi_runid = validate_input()

    # Get the DB connection info from the config file
    config = load_config("/nmi/etc/nmi.conf")

    # Get the last runs
    check_runid(config, nmi_runid)
except:
    import traceback
    # TODO: it would be nice to email this error message, but traceback.format_exc is missing in Python 2.3 :(
    # So for now I'll just print it and then figure out a workaround at some unspecified point in the future.
    traceback.print_exc()
    

print "Exiting successfully from %s" % sys.argv[0]
sys.exit(0)
