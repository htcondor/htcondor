'''
Low level Skype for Mac OS X interface implemented
using Carbon distributed notifications. Uses direct
Carbon/CoreFoundation calls through ctypes module.

This module handles the options that you can pass to
L{ISkype.__init__<skype.ISkype.__init__>} for Mac OS X
machines.

No further options are currently supported.

Thanks to Eion Robb for reversing Skype for Mac API protocol.
'''

from Skype4Py.API import ICommand, _ISkypeAPIBase
from ctypes import *
from ctypes.util import find_library
from Skype4Py.errors import ISkypeAPIError
from Skype4Py.enums import *
import threading, time


class Carbon(object):
    '''Represents the Carbon.framework.
    '''

    def __init__(self):
        path = find_library('Carbon')
        if path == None:
            raise ImportError('Could not find Carbon.framework')
        self.lib = cdll.LoadLibrary(path)
        self.lib.RunCurrentEventLoop.argtypes = (c_double,)

    def RunCurrentEventLoop(self, timeout=-1):
        # timeout=-1 means forever
        return self.lib.RunCurrentEventLoop(timeout)

    def GetCurrentEventLoop(self):
        return EventLoop(self, c_void_p(self.lib.GetCurrentEventLoop()))


class EventLoop(object):
    '''Represents an EventLoop from Carbon.framework.

    http://developer.apple.com/documentation/Carbon/Reference/Carbon_Event_Manager_Ref/
    '''

    def __init__(self, carb, handle):
        self.carb = carb
        self.handle = handle

    def quit(self):
        self.carb.lib.QuitEventLoop(self.handle)


class CoreFoundation(object):
    '''Represents the CoreFoundation.framework.
    '''

    def __init__(self):
        path = find_library('CoreFoundation')
        if path == None:
            raise ImportError('Could not find CoreFoundation.framework')
        self.lib = cdll.LoadLibrary(path)
        self.cfstrs = []

    def CFType(self, handle=0):
        return CFType(self, handle=handle)

    def CFStringFromHandle(self, handle=0):
        return CFString(self, handle=handle)

    def CFString(self, s=u''):
        return CFString(self, string=s)

    def CFSTR(self, s):
        for cfs in self.cfstrs:
            if unicode(cfs) == s:
                return cfs
        cfs = self.CFString(s)
        self.cfstrs.append(cfs)
        return cfs

    def CFNumberFromHandle(self, handle=0):
        return CFNumber(self, handle=handle)

    def CFNumber(self, n=0):
        return CFNumber(self, number=n)

    def CFDictionaryFromHandle(self, handle=0):
        return CFDictionary(self, handle=handle)

    def CFDictionary(self, d={}):
        return CFDictionary(self, dictionary=d)

    def CFDistributedNotificationCenter(self):
        return CFDistributedNotificationCenter(self)


class CFType(object):
    '''Fundamental type for all CoreFoundation types.

    http://developer.apple.com/documentation/CoreFoundation/Reference/CFTypeRef/
    '''

    def __init__(self, cf, **kwargs):
        if isinstance(cf, CFType):
            self.cf = cf.cf
            self.handle = cf.get_handle()
            if len(kwargs):
                raise TypeError('unexpected additional arguments')
        else:
            self.cf = cf
            self.handle = None
        self.owner = False
        self.__dict__.update(kwargs)
        if self.handle != None and not isinstance(self.handle, c_void_p):
            self.handle = c_void_p(self.handle)

    def retain(self):
        if not self.owner:
            self.cf.lib.CFRetain(self)
            self.owner = True

    def is_owner(self):
        return self.owner

    def get_handle(self):
        return self.handle

    def __del__(self):
        if self.owner:
            self.cf.lib.CFRelease(self)
        self.handle = None
        self.cf = None

    def __repr__(self):
        return '%s(handle=%s)' % (self.__class__.__name__, repr(self.handle))

    # allows passing CF types as ctypes function parameters
    _as_parameter_ = property(get_handle)


class CFString(CFType):
    '''CoreFoundation string type.

    Supports Python unicode type only.

    http://developer.apple.com/documentation/CoreFoundation/Reference/CFStringRef/
    '''

    def __init__(self, *args, **kwargs):
        CFType.__init__(self, *args, **kwargs)
        if 'string' in kwargs:
            s = unicode(kwargs['string']).encode('utf8')
            self.handle = c_void_p(self.cf.lib.CFStringCreateWithBytes(None,
                s, len(s), 0x08000100, False))
            self.owner = True

    def __str__(self):
        i = self.cf.lib.CFStringGetLength(self)
        size = c_long()
        if self.cf.lib.CFStringGetBytes(self, 0, i, 0x08000100, 0, False, None, 0, byref(size)) > 0:
            buf = create_string_buffer(size.value)
            self.cf.lib.CFStringGetBytes(self, 0, i, 0x08000100, 0, False, buf, size, None)
            return buf.value
        else:
            raise UnicodeError('CFStringGetBytes() failed')

    def __unicode__(self):
        return self.__str__().decode('utf8')

    def __len__(self):
        return self.cf.lib.CFStringGetLength(self)

    def __repr__(self):
        return 'CFString(%s)' % repr(unicode(self))


class CFNumber(CFType):
    '''CoreFoundation number type.

    Supports Python int type only.

    http://developer.apple.com/documentation/CoreFoundation/Reference/CFNumberRef/
    '''

    def __init__(self, *args, **kwargs):
        CFType.__init__(self, *args, **kwargs)
        if 'number' in kwargs:
            n = int(kwargs['number'])
            self.handle = c_void_p(self.cf.lib.CFNumberCreate(None, 3, byref(c_int(n))))
            self.owner = True

    def __int__(self):
        n = c_int()
        if self.cf.lib.CFNumberGetValue(self, 3, byref(n)):
            return n.value
        return 0

    def __repr__(self):
        return 'CFNumber(%s)' % repr(int(self))


class CFDictionary(CFType):
    '''CoreFoundation immutable dictionary type.

    http://developer.apple.com/documentation/CoreFoundation/Reference/CFDictionaryRef/
    '''

    def __init__(self, *args, **kwargs):
        CFType.__init__(self, *args, **kwargs)
        if 'dictionary' in kwargs:
            d = dict(kwargs['dictionary'])
            keys = (c_void_p * len(d))()
            values = (c_void_p * len(d))()
            i = 0
            for k, v in d.items():
                if not isinstance(k, CFType):
                    raise TypeError('CFDictionary: key is not a CFType')
                if not isinstance(v, CFType):
                    raise TypeError('CFDictionary: value is not a CFType')
                keys[i] = k.get_handle()
                values[i] = v.get_handle()
                i += 1
            self.handle = c_void_p(self.cf.lib.CFDictionaryCreate(None, keys, values, len(d),
                self.cf.lib.kCFTypeDictionaryKeyCallBacks, self.cf.lib.kCFTypeDictionaryValueCallBacks))
            self.owner = True

    def get_dict(self):
        n = len(self)
        keys = (c_void_p * n)()
        values = (c_void_p * n)()
        self.cf.lib.CFDictionaryGetKeysAndValues(self, keys, values)
        d = dict()
        for i in xrange(n):
            d[self.cf.CFType(keys[i])] = self.cf.CFType(values[i])
        return d

    def __getitem__(self, key):
        return self.cf.CFType(c_void_p(self.cf.lib.CFDictionaryGetValue(self, key)))

    def __len__(self):
        return self.cf.lib.CFDictionaryGetCount(self)


class CFDistributedNotificationCenter(CFType):
    '''CoreFoundation distributed notification center type.

    http://developer.apple.com/documentation/CoreFoundation/Reference/CFNotificationCenterRef/
    '''

    CFNotificationCallback = CFUNCTYPE(None, c_void_p, c_void_p, c_void_p, c_void_p, c_void_p)

    def __init__(self, *args, **kwargs):
        CFType.__init__(self, *args, **kwargs)
        self.handle = c_void_p(self.cf.lib.CFNotificationCenterGetDistributedCenter())
        # there is only one distributed notification center per application
        self.owner = False
        self.callbacks = {}
        self._callback = self.CFNotificationCallback(self._notification_callback)

    def _notification_callback(self, center, observer, name, obj, userInfo):
        observer = self.cf.CFStringFromHandle(observer)
        name = self.cf.CFStringFromHandle(name)
        if obj:
            obj = self.cf.CFStringFromHandle(obj)
        callback = self.callbacks[(unicode(observer), unicode(name))]
        callback(self, observer, name, obj,
            self.cf.CFDictionaryFromHandle(userInfo))

    def add_observer(self, observer, callback, name=None, obj=None,
            drop=False, coalesce=False, hold=False, immediate=False):
        if not isinstance(observer, CFString):
            observer = self.cf.CFString(observer)
        if not callable(callback):
            raise TypeError('callback must be callable')
        self.callbacks[(unicode(observer), unicode(name))] = callback
        if name != None and not isinstance(name, CFString):
            name = self.cf.CFSTR(name)
        if obj != None and not isinstance(obj, CFString):
            obj = self.cf.CFSTR(obj)
        if drop:
            behaviour = 1
        elif coalesce:
            behaviour = 2
        elif hold:
            behaviour = 3
        elif immediate:
            behaviour = 4
        else:
            behaviour = 0
        self.cf.lib.CFNotificationCenterAddObserver(self, observer, self._callback, name, obj, behaviour)

    def remove_observer(self, observer, name=None, obj=None):
        if not isinstance(observer, CFString):
            observer = self.cf.CFString(observer)
        if name != None and not isinstance(name, CFString):
            name = self.cf.CFSTR(name)
        if obj != None and not isinstance(obj, CFString):
            obj = self.cf.CFSTR(obj)
        self.cf.lib.CFNotificationCenterRemoveObserver(self, observer, name, obj)
        try:
            del self.callbacks[(unicode(observer), unicode(name))]
        except KeyError:
            pass

    def post_notification(self, name, obj=None, userInfo=None, immediate=False):
        if not isinstance(name, CFString):
            name = self.cf.CFSTR(name)
        if obj != None and not isinstance(obj, CFString):
            obj = self.cf.CFSTR(obj)
        if userInfo != None and not isinstance(userInfo, CFDictionary):
            userInfo = self.cf.CFDictionary(userInfo)
        self.cf.lib.CFNotificationCenterPostNotification(self, name, obj, userInfo, immediate)


class _ISkypeAPI(_ISkypeAPIBase):
    '''Skype for Mac API wrapper.

    Code based on Pidgin Skype Plugin source.
    http://code.google.com/p/skype4pidgin/
    Permission was granted by the author.
    '''

    def __init__(self, handler, opts):
        _ISkypeAPIBase.__init__(self, opts)
        self.RegisterHandler(handler)
        self.carbon = Carbon()
        self.coref = CoreFoundation()
        self.center = self.coref.CFDistributedNotificationCenter()
        self.is_available = False
        self.client_id = -1

    def run(self):
        self.DebugPrint('thread started')
        self.loop = self.carbon.GetCurrentEventLoop()
        self.carbon.RunCurrentEventLoop()
        self.DebugPrint('thread finished')

    def Close(self):
        if hasattr(self, 'loop'):
            self.loop.quit()
            self.client_id = -1
            self.DebugPrint('closed')

    def SetFriendlyName(self, FriendlyName):
        self.FriendlyName = FriendlyName
        if self.AttachmentStatus == apiAttachSuccess:
            # reattach with the new name
            self.SetAttachmentStatus(apiAttachUnknown)
            self.Attach()

    def __Attach_ftimeout(self):
        self.wait = False

    def Attach(self, Timeout=30000, Wait=True):
        if self.AttachmentStatus in (apiAttachPendingAuthorization, apiAttachSuccess):
            return
        try:
            self.start()
        except AssertionError:
            pass
        t = threading.Timer(Timeout / 1000.0, self.__Attach_ftimeout)
        try:
            self.init_observer()
            self.client_id = -1
            self.SetAttachmentStatus(apiAttachPendingAuthorization)
            self.post('SKSkypeAPIAttachRequest')
            self.wait = True
            if Wait:
                t.start()
            while self.wait and self.AttachmentStatus == apiAttachPendingAuthorization:
                time.sleep(1.0)
        finally:
            t.cancel()
        if not self.wait:
            self.SetAttachmentStatus(apiAttachUnknown)
            raise ISkypeAPIError('Skype attach timeout')
        self.SendCommand(ICommand(-1, 'PROTOCOL %s' % self.Protocol))

    def IsRunning(self):
        try:
            self.start()
        except AssertionError:
            pass
        self.init_observer()
        self.is_available = False
        self.post('SKSkypeAPIAvailabilityRequest')
        time.sleep(1.0)
        return self.is_available

    def Start(self, Minimized=False, Nosplash=False):
        if not self.IsRunning():
            from subprocess import Popen
            nul = file('/dev/null')
            Popen(['/Applications/Skype.app/Contents/MacOS/Skype'], stdin=nul, stdout=nul, stderr=nul)

    def SendCommand(self, Command):
        if not self.AttachmentStatus == apiAttachSuccess:
            self.Attach(Command.Timeout)
        self.CommandsStackPush(Command)
        self.CallHandler('send', Command)
        com = u'#%d %s' % (Command.Id, Command.Command)
        if Command.Blocking:
            Command._event = event = threading.Event()
        else:
            Command._timer = timer = threading.Timer(Command.Timeout / 1000.0, self.CommandsStackPop, (Command.Id,))

        self.DebugPrint('->', repr(com))
        userInfo = self.coref.CFDictionary({self.coref.CFSTR('SKYPE_API_COMMAND'): self.coref.CFString(com),
                                            self.coref.CFSTR('SKYPE_API_CLIENT_ID'): self.coref.CFNumber(self.client_id)})
        self.post('SKSkypeAPICommand', userInfo)

        if Command.Blocking:
            event.wait(Command.Timeout / 1000.0)
            if not event.isSet():
                raise ISkypeAPIError('Skype command timeout')
        else:
            timer.start()

    def init_observer(self):
        if self.has_observer():
            self.delete_observer()
        self.observer = self.coref.CFString(self.FriendlyName)
        self.center.add_observer(self.observer, self.SKSkypeAPINotification, 'SKSkypeAPINotification', immediate=True)
        self.center.add_observer(self.observer, self.SKSkypeWillQuit, 'SKSkypeWillQuit', immediate=True)
        self.center.add_observer(self.observer, self.SKSkypeBecameAvailable, 'SKSkypeBecameAvailable', immediate=True)
        self.center.add_observer(self.observer, self.SKAvailabilityUpdate, 'SKAvailabilityUpdate', immediate=True)
        self.center.add_observer(self.observer, self.SKSkypeAttachResponse, 'SKSkypeAttachResponse', immediate=True)

    def delete_observer(self):
        if not self.has_observer():
            return
        self.center.remove_observer(self.observer, 'SKSkypeAPINotification')
        self.center.remove_observer(self.observer, 'SKSkypeWillQuit')
        self.center.remove_observer(self.observer, 'SKSkypeBecameAvailable')
        self.center.remove_observer(self.observer, 'SKAvailabilityUpdate')
        self.center.remove_observer(self.observer, 'SKSkypeAttachResponse')
        del self.observer

    def has_observer(self):
        return hasattr(self, 'observer')

    def post(self, name, userInfo=None):
        if not self.has_observer():
            self.init_observer()
        self.center.post_notification(name, self.observer, userInfo, immediate=True)

    def SKSkypeAPINotification(self, center, observer, name, obj, userInfo):
        client_id = int(CFNumber(userInfo[self.coref.CFSTR('SKYPE_API_CLIENT_ID')]))
        if client_id != 999 and (client_id == 0 or client_id != self.client_id):
            return
        com = unicode(CFString(userInfo[self.coref.CFSTR('SKYPE_API_NOTIFICATION_STRING')]))
        self.DebugPrint('<-', repr(com))

        if com.startswith(u'#'):
            p = com.find(u' ')
            Command = self.CommandsStackPop(int(com[1:p]))
            if Command:
                Command.Reply = com[p + 1:]
                if Command.Blocking:
                    Command._event.set()
                    del Command._event
                else:
                    Command._timer.cancel()
                    del Command._timer
                self.CallHandler('rece', Command)
            else:
                self.CallHandler('rece_api', com[p + 1:])
        else:
            self.CallHandler('rece_api', com)

    def SKSkypeWillQuit(self, center, observer, name, obj, userInfo):
        self.DebugPrint('<-', 'SKSkypeWillQuit')
        self.SetAttachmentStatus(apiAttachNotAvailable)

    def SKSkypeBecameAvailable(self, center, observer, name, obj, userInfo):
        self.DebugPrint('<-', 'SKSkypeBecameAvailable')
        self.SetAttachmentStatus(apiAttachAvailable)

    def SKAvailabilityUpdate(self, center, observer, name, obj, userInfo):
        self.DebugPrint('<-', 'SKAvailabilityUpdate')
        self.is_available = bool(int(CFNumber(userInfo[self.coref.CFSTR('SKYPE_API_AVAILABILITY')])))

    def SKSkypeAttachResponse(self, center, observer, name, obj, userInfo):
        self.DebugPrint('<-', 'SKSkypeAttachResponse')
        # It seems that this notification is not called if the access is refused. Therefore we can't
        # distinguish between attach timeout and access refuse.
        if unicode(CFString(userInfo[self.coref.CFSTR('SKYPE_API_CLIENT_NAME')])) == self.FriendlyName:
            response = int(CFNumber(userInfo[self.coref.CFSTR('SKYPE_API_ATTACH_RESPONSE')]))
            if response and self.client_id == -1:
                self.client_id = response
                self.SetAttachmentStatus(apiAttachSuccess)
