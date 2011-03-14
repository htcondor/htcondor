#! /usr/bin/python
#
# Sends your own mood messages to Twitter.
#
# Requires the Twitter Python support from http://code.google.com/p/python-twitter/
# (included in the package).
# Twitter Python wrapper itself requires the python-simplejson package.
#
# To use pass your Twitter username and password to the script and leave running.
#
# The script will also show on the command-line when one of your contact
# changed its mood.
#
# (c) Copyright 2007, Vincent Oberle, vincent@oberle.org
#
# This software may be used and distributed according to the terms
# of the GNU Public License, incorporated herein by reference.


import os
import sys
import re
import time
from optparse import OptionParser

import Skype4Py

import twitter


parser = OptionParser('%prog username password', version="%prog 0.1")

options, args = parser.parse_args()
if len(args) != 2:
	parser.print_help()
	sys.exit(0)

username = args[0]
password = args[1]

skype = Skype4Py.Skype()
skype.FriendlyName = 'mood2twitter'
skype.Attach() # attach to Skype client

twitter_api = twitter.Api(username = username, password = password)


def user_mood(User, MoodText):
	# Ignore empty moods
	if not len(MoodText): return
	
	print User.Handle, 'changed its mood to', MoodText
	
	## My own mood to Twitter
	if User == skype.CurrentUser:
		try:
			status = twitter_api.PostUpdate(MoodText)
		except UnicodeDecodeError:
			print "Your message could not be encoded.  Perhaps it contains non-ASCII characters? "
		else:
			print "%s just posted: %s" % (status.user.name, status.text)


skype.OnUserMood = user_mood

while(True):
	time.sleep(1) # Infinite loop, ctrl^C to break
