#!/usr/bin/env python

import Skype4Py
import re
import time

skype = Skype4Py.Skype()
skype.Attach()  # Attach to Skype client

def message_status(Message, Status):
    if Status != Skype4Py.cmsSent: return
    if Message.Sender != skype.CurrentUser: return
    r = re.search (r'/all (.+)', Message.Body)
    if r:
        msg = r.group(1).strip()
        for member in Message.Chat.MemberObjects:
            if member.Handle == skype.CurrentUserHandle:
                continue # don't sent to myself
            skype.SendMessage(member.Handle, msg)

skype.OnMessageStatus = message_status

while(True):
	time.sleep(1) # Infinite loop, ctrl^C to break
