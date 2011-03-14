#! /usr/bin/python
#
# Displays all multi-chats and allows to open them.
#
# (c) Copyright 2007, Vincent Oberle, vincent@oberle.org
#
# This software may be used and distributed according to the terms
# of the GNU Public License, incorporated herein by reference.


import sys
import re
from optparse import OptionParser

from skype_api import *


appname = 'chat_finder'

class SkypeChat:
	def __init__(self, _chunk_size = 5, debug = False):
		self.ids = None
		self.chunk = 0
		self.chunk_size = _chunk_size
		self.topics = {}
		self.members = {}
		self.friendlyname = {}
		self.api = SkypeAPI(appname, debug)

	def init_chat_ids(self):
		ret = self.api.send_and_block('SEARCH CHATS')
		r = re.search (r'CHATS (.*)', ret)
		if r:
			self.ids = r.group(1).strip().split(', ')
		# convert percentage to actual value
		self.chunk_size = len(self.ids) * self.chunk_size / 100

	# Gets chat info by chunks
	def get_all_chats(self):
		if not self.ids:
			self.init_chat_ids()

		lo = self.chunk
		self.hi = min([self.chunk + self.chunk_size - 1, len(self.ids) - 1])

		for i in self.ids[lo:self.hi]:
			self.api.send('GET CHAT ' + i + ' TOPIC')
			self.api.send('GET CHAT ' + i + ' MEMBERS')
			self.api.send('GET CHAT ' + i + ' FRIENDLYNAME')

		while True:
			msgs = self.api.response_wait(1)
			if not msgs: break
			for reply in msgs:
				r = re.search (r'CHAT (\S+) TOPIC (.+)', reply)
				if r: self.topics[r.group(1).strip()] = r.group(2).strip()
				r = re.search (r'CHAT (\S+) MEMBERS (.+)', reply)
				if r: self.members[r.group(1).strip()] = r.group(2).strip()
				r = re.search (r'CHAT (\S+) FRIENDLYNAME (.+)', reply)
				if r: self.friendlyname[r.group(1).strip()] = r.group(2).strip()
		
		self.chunk = min([self.chunk + self.chunk_size, len(self.ids) - 1])

	def open_chat(self, name):
		self.api.send('OPEN CHAT ' + name)

	def open_chat_by_index(self, index):
		self.open_chat(self.ids[index])

	def print_chat_list(self, filter):
		for i in self.ids[:self.hi]:
			# display only multi-chats
			if not self.members.has_key(i) or len(self.members[i].split()) < 3:
				continue
			# string we filter on
			search_str = ''
			if self.topics.has_key(i):
				search_str = search_str + self.topics[i]
			elif self.friendlyname.has_key(i):
				search_str = search_str + self.friendlyname[i]
			if self.members.has_key(i):
				search_str = search_str + self.members[i]
			search_str = search_str.lower()
			
			if not filter or search_str.find(filter) >= 0:
				t = ''
				if self.topics.has_key(i):           t = '**' + self.topics[i] + '**'
				elif self.friendlyname.has_key(i):   t = '"' + self.friendlyname[i] + '"'
				if self.members.has_key(i):          t = t + ' ' + self.members[i]
				if t:
					print str(self.ids.index(i)) + ': ' + t

	def perc_loaded(self):
		return str( int(float(self.chunk) / len(self.ids) * 100) )



if __name__ == "__main__":
	parser = OptionParser('%prog [options]')

	parser.add_option('-c', '--chunk', dest='chunk',
		default = 5, help='Percentage of total number of chats to load each time (100 for all)')
	parser.add_option('-d', '--debug', action='store_true', dest='debug',
		default = False, help='Print debug messages')

	options, args = parser.parse_args()
	if len(args):
		parser.print_help()
		sys.exit(0)


	try:
		api = SkypeChat(int(options.chunk), options.debug)
	except StandardError:
		print 'Could not connect to Skype. Check if "' + appname + '" is authorized to connect to Skype (Options - Public API)'
		sys.exit(0)
	
	print 'Please patient while chats are being loaded...'
	api.get_all_chats()
	
	filter = None
	refresh = True
	while True:
		if refresh:
			print ''
			api.print_chat_list(filter)
		refresh = False
		
		print 'Loaded: ' + api.perc_loaded() + ' %'
		print 'Quick help: "/word" filter by word; "/" clean filter; "m" load more chats; number to open a chat; "q" to quit'
		print '> ',
		sys.stdout.flush()
		result = sys.stdin.readline().strip().lower()
		
		if not result: continue

		elif result == '/':
			filter = None
			refresh = True
		elif result[0] == '/':
			filter = result[1:]
			refresh = True
		elif result.isdigit():
			api.open_chat_by_index(int(result))
		elif result[0] == '#':
			api.open_chat(result)
		elif result == 'm':
			print 'Loading more chats...'
			api.get_all_chats()
			refresh = True
		elif result == 'h':
			print 'Skype Chat Finder: Finds old multi-chats and allows to open them'
			print 'Commands:'
			print '    /word         Filters chat topic and member list with "word"'
			print '    /             Show all chats (reset filter)'
			print '    m             Loads more chats'
			print '    Chat number   Opens the chat window in Skype'
			print '    q             Quits'
		elif result == 'q':
			break
