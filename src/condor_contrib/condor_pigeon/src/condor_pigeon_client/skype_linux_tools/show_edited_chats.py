#! /usr/bin/python
#
# Shows Skype edited chat messages
#
# (c) Copyright 2007, Vincent Oberle, vincent@oberle.org
#
# This software may be used and distributed according to the terms
# of the GNU Public License, incorporated herein by reference.


import sys
import re
import datetime
from optparse import OptionParser

from skype_api import *

# KDE users
kdialog = True
# Gnome users
gmessage = False
# If both are false, we use xmessage


appname = 'show_edited_chats'

# tables by chat id
edited_by = {}
edited_timestamp = {}
body = {}

# Callback for property change events
def edited_onchange(event, api):
	r = re.search (r'CHATMESSAGE (\d+) (\w+) (.*)', event)
	if not r: return   # event is not for us
	id = r.group(1).strip()
	prop = r.group(2).strip()
	params = r.group(3).strip()
	
	if prop == 'EDITED_BY':
		# Comes only when changed
		edited_by[id] = params
	elif prop == 'EDITED_TIMESTAMP':
		edited_timestamp[id] = params
	elif prop == 'BODY':
		body[id] = params
	
	# If we have all 3 info, print and clear
	if edited_by.has_key(id) and edited_timestamp.has_key(id) and body.has_key(id):
		timestamp = datetime.datetime.fromtimestamp(int(edited_timestamp[id])).ctime()
		ret = api.send_and_block('GET CHATMESSAGE ' + str(id) + ' FROM_DISPNAME')
		r = re.search (r'CHATMESSAGE ' + str(id) + ' FROM_DISPNAME (.+)', ret)
		if r:
			dispname = r.group(1).strip()

		dispname
		if kdialog:
			title = '"Edited Chat message"'
			msg = '"[' + timestamp + '] ' + dispname + '\n' + body[id] + '"'
			os.system('kdialog --title ' + title + ' --msgbox ' + msg)
		else:
			message = '[' + timestamp + '] ' + dispname + ' edited a message, new text: ' + body[id]
			if gmessage:
				# Could probably be made prettier
				os.system('gmessage ' + message)
			else:
				os.system('xmessage ' + message)
			print message
		# Clear to avoid showing duplicates
		del edited_timestamp[id]
		del body[id]


if __name__ == "__main__":
	parser = OptionParser('%prog [options]')

	parser.add_option('-d', '--debug', action='store_true', dest='debug',
		default = False, help='Print debug messages')

	options, args = parser.parse_args()
	if len(args) > 0:
		parser.print_help()
		sys.exit(0)


	try:
		api = SkypeAPI(appname, options.debug)
	except StandardError:
		print 'Could not connect to Skype. Check if "' + appname + '" is authorized to connect to Skype (Options - Public API)'
		sys.exit(0)

	api.set_callback(edited_onchange, api)

	print 'Running...'
	try:
		while True:
			api.poll_events(1)
	except KeyboardInterrupt: # hides error message on control C
		print 'Exited'
