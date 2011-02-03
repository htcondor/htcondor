'''Skype settings.
'''

import weakref
import sys
from utils import *


class ISettings(object):
    '''Represents Skype settings. Access using L{ISkype.Settings<skype.ISkype.Settings>}.
    '''

    def __init__(self, Skype):
        '''__init__.

        @param Skype: Skype
        @type Skype: L{ISkype}
        '''
        self._SkypeRef = weakref.ref(Skype)

    def Avatar(self, Id=1, Set=None):
        '''Sets user avatar picture from file.

        @param Id: Optional avatar Id.
        @type Id: int
        @param Set: New avatar file name.
        @type Set: unicode
        @deprecated: Use L{LoadAvatarFromFile} instead.
        '''
        from warnings import warn
        warn('ISettings.Avatar: Use ISettings.LoadAvatarFromFile instead.', DeprecationWarning, stacklevel=2)
        if Set == None:
            raise TypeError('Argument \'Set\' is mandatory!')
        self.LoadAvatarFromFile(Set, Id)

    def LoadAvatarFromFile(self, Filename, AvatarId=1):
        '''Loads user avatar picture from file.

        @param Filename: Name of the avatar file.
        @type Filename: unicode
        @param AvatarId: Optional avatar Id.
        @type AvatarId: int
        '''
        s = 'AVATAR %s %s' % (AvatarId, Filename)
        self._Skype._DoCommand('SET %s' % s, s)

    def ResetIdleTimer(self):
        '''Reset Skype idle timer.
        '''
        self._Skype._DoCommand('RESETIDLETIMER')

    def RingTone(self, Id=1, Set=None):
        '''Returns/sets a ringtone.

        @param Id: Ringtone Id
        @type Id: int
        @param Set: Path to new ringtone or None if the current path should be queried.
        @type Set: unicode
        @return: Current path if Set=None, None otherwise.
        @rtype: unicode or None
        '''
        return self._Skype._Property('RINGTONE', Id, '', Set)

    def RingToneStatus(self, Id=1, Set=None):
        '''Enables/disables a ringtone.

        @param Id: Ringtone Id
        @type Id: int
        @param Set: True/False if the ringtone should be enabled/disabled or None if the current
        status should be queried.
        @type Set: bool
        @return: Current status if Set=None, None otherwise.
        @rtype: bool
        '''
        if Set == None:
            return self._Skype._Property('RINGTONE', Id, 'STATUS') == 'ON'
        return self._Skype._Property('RINGTONE', Id, 'STATUS', cndexp(Set, 'ON', 'OFF'))

    def SaveAvatarToFile(self, Filename, AvatarId=1):
        '''Saves user avatar picture to file.

        @param Filename: Destination path.
        @type Filename: unicode
        @param AvatarId: Avatar Id
        @type AvatarId: int
        '''
        s = 'AVATAR %s %s' % (AvatarId, Filename)
        self._Skype._DoCommand('GET %s' % s, s)

    def _Get_Skype(self):
        skype = self._SkypeRef()
        if skype:
            return skype
        raise Exception()

    _Skype = property(_Get_Skype)

    def _GetAEC(self):
        return self._Skype.Variable('AEC') == 'ON'

    def _SetAEC(self, value):
        self._Skype.Variable('AEC', cndexp(value, 'ON', 'OFF'))

    AEC = property(_GetAEC, _SetAEC,
    doc='''Automatic echo cancellation state.

    @type: bool
    @warning: Starting with Skype for Windows 3.6, this property has no effect.
    It can still be set for backwards compatibility reasons.
    ''')

    def _GetAGC(self):
        return self._Skype.Variable('AGC') == 'ON'

    def _SetAGC(self, value):
        self._Skype.Variable('AGC', cndexp(value, 'ON', 'OFF'))

    AGC = property(_GetAGC, _SetAGC,
    doc='''Automatic gain control state.

    @type: bool
    @warning: Starting with Skype for Windows 3.6, this property has no effect.
    It can still be set for backwards compatibility reasons.
    ''')

    def _GetAudioIn(self):
        return self._Skype.Variable('AUDIO_IN')

    def _SetAudioIn(self, value):
        self._Skype.Variable('AUDIO_IN', value)

    AudioIn = property(_GetAudioIn, _SetAudioIn,
    doc='''Name of an audio input device.

    @type: unicode
    ''')

    def _GetAudioOut(self):
        return self._Skype.Variable('AUDIO_OUT')

    def _SetAudioOut(self, value):
        self._Skype.Variable('AUDIO_OUT', value)

    AudioOut = property(_GetAudioOut, _SetAudioOut,
    doc='''Name of an audio output device.

    @type: unicode
    ''')

    def _GetAutoAway(self):
        return self._Skype.Variable('AUTOAWAY') == 'ON'

    def _SetAutoAway(self, value):
        self._Skype.Variable('AUTOAWAY', cndexp(value, 'ON', 'OFF'))

    AutoAway = property(_GetAutoAway, _SetAutoAway,
    doc='''Auto away status.

    @type: bool
    ''')

    def _GetLanguage(self):
        return self._Skype.Variable('UI_LANGUAGE')

    def _SetLanguage(self, value):
        self._Skype.Variable('UI_LANGUAGE', value)

    Language = property(_GetLanguage, _SetLanguage,
    doc='''Language of the Skype client as an ISO code.

    @type: unicode
    ''')

    def _GetPCSpeaker(self):
        return self._Skype.Variable('PCSPEAKER') == 'ON'

    def _SetPCSpeaker(self, value):
        self._Skype.Variable('PCSPEAKER', cndexp(value, 'ON', 'OFF'))

    PCSpeaker = property(_GetPCSpeaker, _SetPCSpeaker,
    doc='''PCSpeaker status.

    @type: bool
    ''')

    def _GetRinger(self):
        return self._Skype.Variable('RINGER')

    def _SetRinger(self, value):
        self._Skype.Variable('RINGER', value)

    Ringer = property(_GetRinger, _SetRinger,
    doc='''Name of a ringer device.

    @type: unicode
    ''')

    def _GetVideoIn(self):
        return self._Skype.Variable('VIDEO_IN')

    def _SetVideoIn(self, value):
        self._Skype.Variable('VIDEO_IN', value)

    VideoIn = property(_GetVideoIn, _SetVideoIn,
    doc='''Name of a video input device.

    @type: unicode
    ''')
