'''Voicemails.
'''

from utils import *
from enums import *


class IVoicemail(Cached):
    '''Represents a voicemail.
    '''

    def __repr__(self):
        return '<%s with Id=%s>' % (Cached.__repr__(self)[1:-1], repr(self.Id))

    def _Alter(self, AlterName, Args=None):
        return self._Skype._Alter('VOICEMAIL', self._Id, AlterName, Args)

    def _Init(self, Id, Skype):
        self._Id = int(Id)
        self._Skype = Skype

    def _Property(self, PropName, Set=None, Cache=True):
        return self._Skype._Property('VOICEMAIL', self._Id, PropName, Set, Cache)

    def CaptureMicDevice(self, DeviceType=None, Set=None):
        '''Queries or sets the mic capture device.

        @param DeviceType: Mic capture device type or None.
        @type DeviceType: L{Call IO device type<enums.callIoDeviceTypeUnknown>} or None
        @param Set: Value the device should be set to or None.
        @type Set: unicode, int or None
        @return: If DeviceType and Set are None, returns a dictionary of device types and their
        values. Dictionary contains only those device types, whose values were set. If the
        DeviceType is not None but Set is None, returns the current value of the device or
        None if the device wasn't set. If Set is not None, sets a new value for the device.
        @rtype: unicode, dict or None
        '''
        if Set == None: # get
            args = args2dict(self._Property('CAPTURE_MIC', Cache=False))
            for t in args:
                if t == callIoDeviceTypePort:
                    args[t] = int(args[t])
            if DeviceType == None: # get active devices
                return args
            return args.get(DeviceType, None)
        elif DeviceType != None: # set
            self._Alter('SET_CAPTURE_MIC', '%s=%s' % (DeviceType, quote(unicode(Set), True)))
        else:
            raise TypeError('DeviceType must be specified if Set is used')

    def Delete(self):
        '''Deletes this voicemail.
        '''
        self._Alter('DELETE')

    def Download(self):
        '''Downloads this voicemail object from the voicemail server to a local computer.
        '''
        self._Alter('DOWNLOAD')

    def InputDevice(self, DeviceType=None, Set=None):
        '''Queries or sets the sound input device.

        @param DeviceType: Sound input device type or None.
        @type DeviceType: L{Call IO device type<enums.callIoDeviceTypeUnknown>} or None
        @param Set: Value the device should be set to or None.
        @type Set: unicode, int or None
        @return: If DeviceType and Set are None, returns a dictionary of device types and their
        values. Dictionary contains only those device types, whose values were set. If the
        DeviceType is not None but Set is None, returns the current value of the device or
        None if the device wasn't set. If Set is not None, sets a new value for the device.
        @rtype: unicode, dict or None
        '''
        if Set == None: # get
            args = args2dict(self._Property('INPUT', Cache=False))
            for t in args:
                if t == callIoDeviceTypePort:
                    args[t] = int(args[t])
            if DeviceType == None: # get active devices
                return args
            return args.get(DeviceType, None)
        elif DeviceType != None: # set
            self._Alter('SET_INPUT', '%s=%s' % (DeviceType, quote(unicode(Set), True)))
        else:
            raise TypeError('DeviceType must be specified if Set is used')

    def Open(self):
        '''Opens and plays this voicemail.
        '''
        self._Skype._DoCommand('OPEN VOICEMAIL %s' % self._Id)

    def OutputDevice(self, DeviceType=None, Set=None):
        '''Queries or sets the sound output device.

        @param DeviceType: Sound output device type or None.
        @type DeviceType: L{Call IO device type<enums.callIoDeviceTypeUnknown>} or None
        @param Set: Value the device should be set to or None.
        @type Set: unicode, int or None
        @return: If DeviceType and Set are None, returns a dictionary of device types and their
        values. Dictionary contains only those device types, whose values were set. If the
        DeviceType is not None but Set is None, returns the current value of the device or
        None if the device wasn't set. If Set is not None, sets a new value for the device.
        @rtype: unicode, dict or None
        '''
        if Set == None: # get
            args = args2dict(self._Property('OUTPUT', Cache=False))
            for t in args:
                if t == callIoDeviceTypePort:
                    args[t] = int(args[t])
            if DeviceType == None: # get active devices
                return args
            return args.get(DeviceType, None)
        elif DeviceType != None: # set
            self._Alter('SET_OUTPUT', '%s=%s' % (DeviceType, quote(unicode(Set), True)))
        else:
            raise TypeError('DeviceType must be specified if Set is used')

    def SetUnplayed(self):
        '''Changes the status of a voicemail from played to unplayed.
        '''
        # Note. Due to a bug in Skype (tested using 3.8.0.115) the reply from
        # [ALTER VOICEMAIL <id> SETUNPLAYED] is [ALTER VOICEMAIL <id> DELETE]
        # causing the _Alter method to fail. Therefore we have to use a direct
        # _DoCommand instead.
        
        #self._Alter('SETUNPLAYED')
        self._Skype._DoCommand('ALTER VOICEMAIL %d SETUNPLAYED' % self._Id,
                               'ALTER VOICEMAIL %d' % self._Id)

    def StartPlayback(self):
        '''Starts playing downloaded voicemail.
        '''
        self._Alter('STARTPLAYBACK')

    def StartPlaybackInCall(self):
        '''Starts playing downloaded voicemail during a call.
        '''
        self._Alter('STARTPLAYBACKINCALL')

    def StartRecording(self):
        '''Stops playing a voicemail greeting and starts recording a voicemail message.
        '''
        self._Alter('STARTRECORDING')

    def StopPlayback(self):
        '''Stops playing downloaded voicemail.
        '''
        self._Alter('STOPPLAYBACK')

    def StopRecording(self):
        '''Ends the recording of a voicemail message.
        '''
        self._Alter('STOPRECORDING')

    def Upload(self):
        '''Uploads recorded voicemail from a local computer to the voicemail server.
        '''
        self._Alter('UPLOAD')

    def _GetAllowedDuration(self):
        return int(self._Property('ALLOWED_DURATION'))

    AllowedDuration = property(_GetAllowedDuration,
    doc='''Maximum voicemail duration in seconds allowed to leave to partner

    @type: int
    ''')

    def _GetDatetime(self):
        from datetime import datetime
        return datetime.fromtimestamp(self.Timestamp)

    Datetime = property(_GetDatetime,
    doc='''Timestamp of this voicemail expressed using datetime.

    @type: datetime.datetime
    ''')

    def _GetDuration(self):
        return int(self._Property('DURATION'))

    Duration = property(_GetDuration,
    doc='''Actual voicemail duration in seconds.

    @type: int
    ''')

    def _GetFailureReason(self):
        return self._Property('FAILUREREASON')

    FailureReason = property(_GetFailureReason,
    doc='''Voicemail failure reason. Read if L{Status} == L{vmsFailed<enums.vmsFailed>}.

    @type: L{Voicemail failure reason<enums.vmrUnknown>}
    ''')

    def _GetId(self):
        return self._Id

    Id = property(_GetId,
    doc='''Unique voicemail Id.

    @type: int
    ''')

    def _GetPartnerDisplayName(self):
        return self._Property('PARTNER_DISPNAME')

    PartnerDisplayName = property(_GetPartnerDisplayName,
    doc='''DisplayName for voicemail sender (for incoming) or recipient (for outgoing).

    @type: unicode
    ''')

    def _GetPartnerHandle(self):
        return self._Property('PARTNER_HANDLE')

    PartnerHandle = property(_GetPartnerHandle,
    doc='''Skypename for voicemail sender (for incoming) or recipient (for outgoing).

    @type: unicode
    ''')

    def _GetStatus(self):
        return self._Property('STATUS')

    Status = property(_GetStatus,
    doc='''Voicemail status.

    @type: L{Voicemail status<enums.vmsUnknown>}
    ''')

    def _GetTimestamp(self):
        return float(self._Property('TIMESTAMP'))

    Timestamp = property(_GetTimestamp,
    doc='''Timestamp of this voicemail.

    @type: float
    ''')

    def _GetType(self):
        return self._Property('TYPE')

    Type = property(_GetType,
    doc='''Voicemail type.

    @type: L{Voicemail type<enums.vmtUnknown>}
    ''')
