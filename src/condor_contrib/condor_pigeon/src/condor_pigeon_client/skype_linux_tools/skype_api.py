#! /usr/bin/python
#
# Some tools to use the Skype Public API under Linux.
# Uses X11 as communication protocol. Tested with Skype version 1.4.
#
# Requires python-xlib
#
#
# (c) Copyright 2007, Vincent Oberle, vincent@oberle.org
#
# Initial code from Amarok Skype mood poster by Tadas Subonis
# http://www.kde-apps.org/content/show.php/Amarok+Skype+mood+poster?content=55794
#
# This software may be used and distributed according to the terms
# of the GNU Public License, incorporated herein by reference.


from Xlib.protocol.event import ClientMessage
import Xlib.display
import Xlib.error
import select
import os
import sys
import re


class SkypeAPI(object):
	# Skype Linux 1.4 supports protocol version 7
	protocol_version = 7

	def __init__(self, appname, _debug = False):
		self.next_id = 0
		self.event_list = []

		"""Sets up all the required Xlib objects"""
		# Basic settings
		self.debug = _debug
		self.appname = appname
		self.partial_message = ''

		# X Objects
		self.display = Xlib.display.Display(":0")
		self.win = {}
		self.win['skype'] = None
		self.win['root'] = self.display.screen()['root']
		self.win['self'] = self.win['root'].create_window(0, 0, 1, 1, 0, 
														self.display.screen()['root_depth'],
														background_pixel = self.display.screen()['black_pixel'])
		self.atoms = {}
		self.atoms['msg'] = self.display.intern_atom("SKYPECONTROLAPI_MESSAGE", False)
		self.atoms['msgbegin'] = self.display.intern_atom("SKYPECONTROLAPI_MESSAGE_BEGIN", False)
		# Try to initialize the Skype object
		self.discover_skype(True)

	def __del__(self):
		if self.debug and self.event_list:
			print 'Unprocessed events:'
			for e in self.event_list:
				print e

	def status(self):
		"""Return the connected status of the api object"""
		return (self.win['skype'] is not None)

	def handle_error(self, error):
		"""Handles an error"""
		if error:
			print "Info: Skype exited"
			if isinstance(error, Xlib.error.BadWindow):
				self.win['skype'] = None
				return False

	def read(self, event):
		"""Receive an event"""
		d = event.data[1]
		pos = d.find("\x00")
		if pos < 0:
			pos = len(d)
		curchunk = d[:pos]
		self.partial_message += curchunk
		if pos != len(d):
			msg = self.partial_message
			self.partial_message = ''
			if self.debug:
				print "Received:", msg
			return msg
		return None

	def send(self, msg):
		"""Used to send a message to Skype"""
		if self.win['skype'] is None:
			return False
		ec = Xlib.error.CatchError(Xlib.error.BadWindow)
		ctype = self.atoms['msgbegin']
		for i in range(0, len(msg) - 1, 20):
			event = ClientMessage(window = self.win['self'],
								client_type = ctype,
								data = (8, msg[i:i+20] + (20 - len(msg[i:i+20])) * "\0"))
			ctype = self.atoms['msg']
			self.win['skype'].send_event(event, onerror = ec)
		if self.debug:
			print "Sent:", msg
		self.display.sync()
		self.handle_error(ec.get_error())

	def response_wait(self, timeout = 0):
		"""Waits for a response, or a timeout"""
		msgs = []
		while 1:
			r, _w, _e = select.select([self.display], [], [], timeout)
			if len(r) < 1:
				break
			elif self.display in r:
				# TODO Do we ever get more than one here???
				i = self.display.pending_events()
				while i > 0:
					msg = self.read(self.display.next_event())
					if msg is not None:
						msgs.append(msg)
					i = i - 1
				break
		return msgs


	def queue_event(self, event):
		"""Queues an event"""
		if not event: return
		self.event_list.append(event)

	def run_poll_for_events(self, wait = 0):
		"""Poll for some events.
		   wait is in seconds. If wait is None, block until something is here"""
		for reply in self.response_wait(wait):
			if reply[0] != '#':
				self.queue_event(reply)
			else:
				raise StandardError('Got a reply we never asked for')

	def run_poll_for_reply(self, reply_prefix):
		"""Blocks until we got the reply corresponding to the prefic"""
		got_it = None
		while True:
			for reply in self.response_wait():
				if not len(reply): continue
				if reply.startswith(reply_prefix):
					got_it = reply
				elif reply[0] != '#':
					self.queue_event(reply)
				else:
					raise StandardError('Got a reply we never asked for')
			if got_it:
				return got_it.lstrip('#0123456789')

	def make_id(self):
		"""Prefix for blocking calls"""
		self.next_id = self.next_id + 1
		return '#' + str(self.next_id) + ' '

	def send_and_block(self, msg):
		"""Sends a message to Skype and waits for an answer"""
		i = self.make_id()
		self.send(i + msg)
		return self.run_poll_for_reply(i)

	def poll_events(self, wait = 0):
		"""Polls for events from Skype"""
		# TODO disconnection handling
		self.run_queue()
		self.run_poll_for_events(wait)
		self.run_queue()

	def set_callback(self, func, userdata):
		if self.debug: print 'Info: Set callback', func
		self.onchange_callback = func
		self.onchange_userdata = userdata

	def run_queue(self):
		if self.debug: print 'Info: run_queue', self.event_list
		while self.event_list:
			e = self.event_list.pop()
			self.onchange_callback(e, self.onchange_userdata)

	def discover_skype(self, initialize = False, timeout = 10):
		"""Returns the skype window if one exists"""
		self.win['skype'] = None
		for prop in self.win['root'].list_properties():
			if self.display.get_atom_name(prop) == "_SKYPE_INSTANCE":
				fullprop = self.win['root'].get_full_property(prop, 0)
				skypewinid = fullprop.value[0]
				if self.debug: print "Info: Skype is running with id", skypewinid
				self.win['skype'] = self.display.create_resource_object('window', skypewinid)
		if (initialize and self.win['skype'] is not None):
			if self.debug: print "Info: Initializing"
			self.send("NAME " + self.appname)
			#response = []
			#while len([ x for x in response if x == 'OK' or x.startswith('ERROR 1')]) == 0:
				#response = self.response_wait(10)
			# We should wait long enough to let the user confirm the access request
			response = self.response_wait(timeout)
			if 'OK' in response:
				if self.debug: print "Info: Initialized"
				self.send('PROTOCOL ' + str(self.protocol_version))
			else:
				if self.debug: print "Info: Initialization Rejected"
				self.win['skype'] = None
		if not self.win['skype']:
			raise StandardError('Initialization failed: Have you authorized ' + self.appname + ' to connect to Skype?')
		else:
			print 'Connected!'



if __name__ == '__main__':
	def test_cb(event, userdata):
		print ' Got ', event
	
	api = SkypeAPI('test-api', True)
	api.set_callback(test_cb, None)

	api.send_and_block('SET PROFILE MOOD_TEXT oberle.org')
	
	api.send_and_block('DISCONNECT')

	while True:
		print 'polling..'
		api.poll_events(1)
