#! /usr/bin/python
#
# Changes your Skype online status (availability).
#
# Usefull for adjusting the status from some script. For example, you can
# have a script that locks the screen and sets you to not available.
#
# (c) Copyright 2007, Vincent Oberle, vincent@oberle.org
#
# This software may be used and distributed according to the terms
# of the GNU Public License, incorporated herein by reference.


import os
import sys
import re
from optparse import OptionParser

import Skype4Py


parser = OptionParser('%prog user [user*]', version="%prog 0.1")

options, args = parser.parse_args()
if len(args) < 1:
	parser.print_help()
	sys.exit(0)

stat_str = args[0]

skype = Skype4Py.Skype()
#skype.FriendlyName = 'change_availability'
skype.Attach() # attach to Skype client


if (stat_str == 'UNKNOWN'): status = Skype4Py.cusUnknown
elif (stat_str == 'OFFLINE'): status = Skype4Py.cusOffline
elif (stat_str == 'ONLINE'): status = Skype4Py.cusOnline
elif (stat_str == 'AWAY'): status = Skype4Py.cusAway
elif (stat_str == 'NA'): status = Skype4Py.cusNotAvailable
elif (stat_str == 'DND'): status = Skype4Py.cusDoNotDisturb
elif (stat_str == 'INVISIBLE'): status = Skype4Py.cusInvisible
elif (stat_str == 'LOGGEDOUT'): status = Skype4Py.cusLoggedOut
elif (stat_str == 'SKYPEME'): status = Skype4Py.cusSkypeMe
else:
	print 'Unsupported status'
	sys.exit(0)


skype.ChangeUserStatus(status)

print 'Status changed to', status
