'''Data channels for calls.
'''

from utils import *
from enums import *
from errors import ISkypeError
import time


class ICallChannel(object):
    '''Represents a call channel.
    '''

    def __init__(self, Manager, Call, Stream, Type):
        '''__init__.

        @param Manager: Manager
        @type Manager: L{ICallChannelManager}
        @param Call: Call
        @type Call: L{ICall}
        @param Stream: Stream
        @type Stream: L{IApplicationStream}
        @param Type: Type
        @type Type: L{Call channel type<enums.cctUnknown>}
        '''
        self._Manager = Manager
        self._Call = Call
        self._Stream = Stream
        self._Type = Type

    def __repr__(self):
        return '<%s with Manager=%s, Call=%s, Stream=%s>' % (object.__repr__(self)[1:-1], repr(self.Manager), repr(self.Call), repr(self.Stream))

    def SendTextMessage(self, Text):
        '''Sends text message over channel.

        @param Text: Text
        @type Text: unicode
        '''
        if self._Type == cctReliable:
            self._Stream.Write(Text)
        elif self._Type == cctDatagram:
            self._Stream.SendDatagram(Text)
        else:
            raise ISkypeError(0, 'Cannot send using %s channel type' & repr(self._Type))

    def _GetCall(self):
        return self._Call

    Call = property(_GetCall,
    doc='''Call.

    @type: L{ICall}
    ''')

    def _GetManager(self):
        return self._Manager

    Manager = property(_GetManager,
    doc='''Manager.

    @type: L{ICallChannelManager}
    ''')

    def _GetStream(self):
        return self._Stream

    Stream = property(_GetStream,
    doc='''Stream.

    @type: L{IApplicationStream}
    ''')

    def _GetType(self):
        return self._Type

    Type = property(_GetType,
    doc='''Type.

    @type: L{Call channel type<enums.cctUnknown>}
    ''')


class ICallChannelManager(EventHandlingBase):
    '''Instatinate this class to create a call channel manager. A call channel manager will
    automatically create a data channel for voice calls based on the APP2APP protocol.

      1. Usage.

         You should access this class using the alias at the package level::

             import Skype4Py

             skype = Skype4Py.Skype()

             ccm = Skype4Py.CallChannelManager()
             ccm.Connect(skype)

         For possible constructor arguments, read the L{ICallChannelManager.__init__} description.

      2. Events.

         This class provides events.

         The events names and their arguments lists can be found in L{ICallChannelManagerEvents} class.

         The usage of events is described in L{EventHandlingBase} class which is a superclass of
         this class. Follow the link for more information.

    @ivar OnChannels: Event handler for L{ICallChannelManagerEvents.Channels} event. See L{EventHandlingBase} for more information on events.
    @type OnChannels: callable

    @ivar OnMessage: Event handler for L{ICallChannelManagerEvents.Message} event. See L{EventHandlingBase} for more information on events.
    @type OnMessage: callable

    @ivar OnCreated: Event handler for L{ICallChannelManagerEvents.Created} event. See L{EventHandlingBase} for more information on events.
    @type OnCreated: callable
    '''

    def __del__(self):
        if self._Application:
            self._Application.Delete()
            self._Application = None
            self._Skype.UnregisterEventHandler('ApplicationStreams', self._OnApplicationStreams)
            self._Skype.UnregisterEventHandler('ApplicationReceiving', self._OnApplicationReceiving)
            self._Skype.UnregisterEventHandler('ApplicationDatagram', self._OnApplicationDatagram)

    def __init__(self, Events=None):
        '''__init__.

        @param Events: Events
        @type Events: An optional object with event handlers. See L{EventHandlingBase} for more information on events.
        '''
        EventHandlingBase.__init__(self)
        if Events:
            self._SetEventHandlerObj(Events)

        self._Skype = None
        self._CallStatusEventHandler = None
        self._ApplicationStreamsEventHandler = None
        self._ApplicationReceivingEventHandler = None
        self._ApplicationDatagramEventHandler = None
        self._Application = None
        self._Name = u'CallChannelManager'
        self._ChannelType = cctReliable
        self._Channels = []

    def _OnApplicationDatagram(self, pApp, pStream, Text):
        if pApp == self._Application:
            for ch in self_Channels:
                if ch.Stream == pStream:
                    msg = ICallChannelMessage(Text)
                    self._CallEventHandler('Message', self, ch, msg)
                    break

    def _OnApplicationReceiving(self, pApp, pStreams):
        if pApp == self._Application:
            for ch in self._Channels:
                if ch.Stream in pStreams:
                    msg = ICallChannelMessage(ch.Stream.Read())
                    self._CallEventHandler('Message', self, ch, msg)

    def _OnApplicationStreams(self, pApp, pStreams):
        if pApp == self._Application:
            for ch in self._Channels:
                if ch.Stream not in pStreams:
                    self._Channels.remove(ch)
                    self._CallEventHandler('Channels', self, tuple(self._Channels))

    def _OnCallStatus(self, pCall, Status):
        if Status == clsRinging:
            if self._Application == None:
                self.CreateApplication()
            self._Application.Connect(pCall.PartnerHandle, True)
            for stream in self._Application.Streams:
                if stream.PartnerHandle == pCall.PartnerHandle:
                    self._Channels.append(ICallChannel(self, pCall, stream, self._ChannelType))
                    self._CallEventHandler('Channels', self, tuple(self._Channels))
                    break
        elif Status in (clsCancelled, clsFailed, clsFinished, clsRefused, clsMissed):
            for ch in self._Channels:
                if ch.Call == pCall:
                    self._Channels.remove(ch)
                    self._CallEventHandler('Channels', self, tuple(self._Channels))
                    try:
                        ch.Stream.Disconnect()
                    except ISkypeError:
                        pass
                    break

    def Connect(self, Skype):
        '''Connects this call channel manager instance to Skype. This is the first thing you should
        do after creating this object.

        @param Skype: Skype object
        @type Skype: L{ISkype}
        @see: L{Disconnect}
        '''
        self._Skype = Skype
        self._Skype.RegisterEventHandler('CallStatus', self._OnCallStatus)

    def CreateApplication(self, ApplicationName=None):
        '''Creates an APP2APP application context. The application is automatically created using
        L{IApplication.Create<application.IApplication.Create>}.

        @param ApplicationName: Application name
        @type ApplicationName: unicode
        '''
        if ApplicationName != None:
            self.Name = ApplicationName
        self._Application = self._Skype.Application(self.Name)
        self._Skype.RegisterEventHandler('ApplicationStreams', self._OnApplicationStreams)
        self._Skype.RegisterEventHandler('ApplicationReceiving', self._OnApplicationReceiving)
        self._Skype.RegisterEventHandler('ApplicationDatagram', self._OnApplicationDatagram)
        self._Application.Create()
        self._CallEventHandler('Created', self)

    def Disconnect(self):
        '''Disconnects from Skype.
        @see: L{Connect}
        '''
        self._Skype.UnregisterEventHandler('CallStatus', self._OnCallStatus)
        self._Skype = None

    def _GetChannels(self):
        return tuple(self._Channels)

    Channels = property(_GetChannels,
    doc='''All call data channels.

    @type: tuple of L{ICallChannel}
    ''')

    def _GetChannelType(self):
        return self._ChannelType

    def _SetChannelType(self, ChannelType):
        self._ChannelType = ChannelType

    ChannelType = property(_GetChannelType, _SetChannelType,
    doc='''Queries/sets the default channel type.

    @type: L{Call channel type<enums.cctUnknown>}
    ''')

    def _GetCreated(self):
        return bool(self._Application)

    Created = property(_GetCreated,
    doc='''Returns True if the application context has been created.

    @type: bool
    ''')

    def _GetName(self):
        return self._Name

    def _SetName(self, Name):
        self._Name = unicode(Name)

    Name = property(_GetName, _SetName,
    doc='''Queries/sets the application context name.

    @type: unicode
    ''')


class ICallChannelManagerEvents(object):
    '''Events defined in L{ICallChannelManager}.

    See L{EventHandlingBase} for more information on events.
    '''

    def Channels(self, Manager, Channels):
        '''This event is triggered when list of call channels changes.

        @param Manager: Manager
        @type Manager: L{ICallChannelManager}
        @param Channels: Channels
        @type Channels: tuple of L{ICallChannel}
        '''

    def Created(self, Manager):
        '''This event is triggered when the application context has successfuly been created.

        @param Manager: Manager
        @type Manager: L{ICallChannelManager}
        '''

    def Message(self, Manager, Channel, Message):
        '''This event is triggered when a call channel message has been received.

        @param Manager: Manager
        @type Manager: L{ICallChannelManager}
        @param Channel: Channel
        @type Channel: L{ICallChannel}
        @param Message: Message
        @type Message: L{ICallChannelMessage}
        '''


ICallChannelManager._AddEvents(ICallChannelManagerEvents)


class ICallChannelMessage(object):
    '''Represents a call channel message.
    '''

    def __init__(self, Text):
        '''__init__.

        @param Text: Text
        @type Text: unicode
        '''
        self._Text = Text

    def _GetText(self):
        return self._Text

    def _SetText(self, Text):
        self._Text = Text

    Text = property(_GetText, _SetText,
    doc='''Queries/sets message text.

    @type: unicode
    ''')
