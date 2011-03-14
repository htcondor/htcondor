#! /usr/bin/python
#
# Sends a SMS with Skype.
# Uses the Skype4Py Skype API Wrapper. See https://developer.skype.com/wiki/Skype4Py
# for download and installation instructions.
#
# (c) Copyright 2007, Vincent Oberle, vincent@oberle.org
#
# This software may be used and distributed according to the terms
# of the GNU Public License, incorporated herein by reference.


import sys
import re
from optparse import OptionParser

import Skype4Py


appname = 'sms_sender'


# Limitations: Can only handle the sending of one SMS at a time
class SkypeSMS:
	def __init__(self):
		# The ISmsMessage we are trying to send
		self.message = None
		# To poll until the SMS has been actually sent
		self.done = False
		self.count_success_delivered = 0
		
		self.api = Skype4Py.Skype(Events = self)
		self.api.FriendlyName = appname
		self.api.Attach()  # Attach to Skype client

	def OnSmsMessageStatusChanged(self, message, status):
		#print 'Got OnSmsMessageStatusChanged'
		if message != self.message: return # event from another SMS
		if status == Skype4Py.smsMessageStatusReceived:
			print 'the message has been received' # (but not tagged as read)
			self.done = True
		elif status == Skype4Py.smsMessageStatusRead:
			print 'the message has been read'
			self.done = True
		#elif status == Skype4Py.smsMessageStatusComposing: print 'the message has been created but not yet sent'
		elif status == Skype4Py.smsMessageStatusSendingToServer:
			print 'the message is in process of being sent to server'
		elif status == Skype4Py.smsMessageStatusSentToServer:
			print 'the message has been sent to server'
		elif status == Skype4Py.smsMessageStatusDelivered:
			print 'server has confirmed that the message is sent out to recipient'
			self.done = True
		elif status == Skype4Py.smsMessageStatusSomeTargetsFailed:
			print 'server reports failure to deliver the message to one of the recipients within 24h'
			self.done = True
		elif status == Skype4Py.smsMessageStatusFailed:
			print 'the message has failed' # possible reason may be found in FAILUREREASON property
			fr = self.message.FailureReason
			if fr == Skype4Py.smsFailureReasonMiscError:
				print 'Failure reason: Misc failure'
			elif fr == Skype4Py.smsFailureReasonServerConnectFailed:
				print 'Failure reason: unable to connect to SMS server'
			elif fr == Skype4Py.smsFailureReasonNoSmsCapability:
				print 'Failure reason: recipient is unable to receive SMS messages'
			elif fr == Skype4Py.smsFailureReasonInsufficientFunds:
				print 'Failure reason: insufficient Skype Credit to send an SMS message'
			elif fr == Skype4Py.smsFailureReasonInvalidConfirmationCode:
				print 'Failure reason: an erroneous code was submitted in a CONFIRMATION_CODE_SUBMIT message'
			elif fr == Skype4Py.smsFailureReasonUserBlocked or fr == Skype4Py.smsFailureReasonIPBlocked or fr == Skype4Py.smsFailureReasonNodeBlocked:
				print 'Failure reason: user is blocked ' + fr
			self.done = True

	def SmsTargetStatusChanged(self, target, status):
		if status == Skype4Py.smsTargetStatusUndefined: print target.Number, 'is undefined'
		elif status == Skype4Py.smsTargetStatusNotRoutable: print target.Number, 'cannot be routed'
		elif status == Skype4Py.smsTargetStatusDeliveryFailed: print target.Number, 'could not be deliveted'
		#elif status == Skype4Py.smsTargetStatusAnalyzing: print target.Number, 'is being analized'
		#elif status == Skype4Py.smsTargetStatusAcceptable: print target.Number, 'is acceptable'
		#elif status == Skype4Py.smsTargetStatusDeliveryPending: print target.Number, ''
		elif status == Skype4Py.smsTargetStatusDeliverySuccessful:
			print target.Number, 'has been successfully delivered'
			self.count_success_delivered = self.count_success_delivered + 1
			if self.count_success_delivered >= self.number_of_targets:
				print 'All targets successfully delivered'
				self.done = True

	def create_sms(self, phonenumbers):
		if self.message: raise StandardError('SMS already created?!?')
		# we get also PRICE, PRICE_PRECISION, PRICE_CURRENCY, TARGET_STATUSES
		if not phonenumbers: return False
		self.message = self.api.CreateSms(Skype4Py.smsMessageTypeOutgoing, phonenumbers)
		if self.message:
			self.number_of_targets = len(self.message.Targets)
			return True
		return False

	def delete_sms(self):
		if not self.message: return
		self.message.Delete()

	def set_body(self, text):
		if not self.message or not text: return
		self.message.Body = text

	def send_sms(self):
		if not self.message: return
		self.message.Send()

	def set_replyto(self, pstn):
		if not self.message or not pstn: return
		self.message.ReplyToNumber = pstn

	def print_chunking(self):
		if not self.message: return
		print 'Your SMS has', len(self.message.Chunks), 'chunk(s).'

	def print_val_numbers(self):
		numbers = api.CurrentUserProfile.ValidatedSmsNumbers
		if numbers:
			print 'Validated numbers:'
			for n in numbers:
				print '\t', n
			return
		print 'No validated numbers'

	def wait_until_done(self):
		while not self.done:
			pass



if __name__ == "__main__":
	parser = OptionParser('%prog [options] phonenumber[,phonenumber*] text')

	parser.add_option('-r', '--replyto', dest='replyto',
		default = None, help='Reply-to number to use')
	parser.add_option('-n', '--dontsend', action='store_true', dest='dontsend',
		default = False, help='Do not send the SMS, only print info about it')
	parser.add_option('-u', '--valnums', action='store_true', dest='valnums',
		default = False, help='Prints the validated numbers')
	parser.add_option('-d', '--debug', action='store_true', dest='debug',
		default = False, help='Deprecated, unused')

	options, args = parser.parse_args()
	if len(args) < 2:
		parser.print_help()
		sys.exit(0)
	phonenumbers = args[0] #.split(',')
	text = ' '.join(args[1:])

	#print 'Phone numbers:', phonenumbers
	#print 'Text:', text
	
	api = SkypeSMS()

	if options.valnums:
		api.print_val_numbers()

	if not api.create_sms(phonenumbers):
		print 'SMS could not be created'
		sys.exit(0)
	
	api.set_body(text)
	api.print_chunking()

	if options.replyto:
		api.set_replyto(options.replyto)
	
	if not options.dontsend:
		api.send_sms()

		api.wait_until_done()
	else:
		# Since we didn't send it we delete it
		api.delete_sms()

