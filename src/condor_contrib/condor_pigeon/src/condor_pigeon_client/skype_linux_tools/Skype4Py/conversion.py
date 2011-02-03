'''Conversion between constants and text.
'''

import enums
import os


# Following code is needed when building executable files using py2exe.
# Together with the Languages.__init__ it makes sure that all languages
# are included in the package built by py2exe. The tool lookes just at
# the imports, it ignores the 'if' statement.
#
# More about py2exe: http://www.py2exe.org/

if False:
    import Languages
    

class IConversion(object):
    '''Allows conversion between constants and text. Access using L{ISkype.Convert<skype.ISkype.Convert>}.
    '''

    def __init__(self, Skype):
        '''__init__.

        @param Skype: Skype object.
        @type Skype: L{ISkype}
        '''
        self._Language = u''
        self._Module = None
        self._SetLanguage('en')

    def _TextTo(self, prefix, value):
        enum = [z for z in [(y, getattr(enums, y)) for y in [x for x in dir(enums) if x.startswith(prefix)]] if z[1] == value]
        if enum:
            return str(value)
        raise ValueError('Bad text')

    def _ToText(self, prefix, value):
        enum = [z for z in [(y, getattr(enums, y)) for y in [x for x in dir(enums) if x.startswith(prefix)]] if z[1] == value]
        if enum:
            try:
                return unicode(getattr(self._Module, enum[0][0]))
            except AttributeError:
                pass
        raise ValueError('Bad identifier')

    def AttachmentStatusToText(self, Status):
        '''Returns attachment status as text.

        @param Status: Attachment status.
        @type Status: L{Attachment status<enums.apiAttachUnknown>}
        @return: Text describing the attachment status.
        @rtype: unicode
        '''
        return self._ToText('api', Status)

    def BuddyStatusToText(self, Status):
        '''Returns buddy status as text.

        @param Status: Buddy status.
        @type Status: L{Buddy status<enums.budUnknown>}
        @return: Text describing the buddy status.
        @rtype: unicode
        '''
        return self._ToText('bud', Status)

    def CallFailureReasonToText(self, Reason):
        '''Returns failure reason as text.

        @param Reason: Call failure reason.
        @type Reason: L{Call failure reason<enums.cfrUnknown>}
        @return: Text describing the call failure reason.
        @rtype: unicode
        '''
        return self._ToText('cfr', Reason)

    def CallStatusToText(self, Status):
        '''Returns call status as text.

        @param Status: Call status.
        @type Status: L{Call status<enums.clsUnknown>}
        @return: Text describing the call status.
        @rtype: unicode
        '''
        return self._ToText('cls', Status)

    def CallTypeToText(self, Type):
        '''Returns call type as text.

        @param Type: Call type.
        @type Type: L{Call type<enums.cltUnknown>}
        @return: Text describing the call type.
        @rtype: unicode
        '''
        return self._ToText('clt', Type)

    def CallVideoSendStatusToText(self, Status):
        '''Returns call video send status as text.

        @param Status: Call video send status.
        @type Status: L{Call video send status<enums.vssUnknown>}
        @return: Text describing the call video send status.
        @rtype: unicode
        '''
        return self._ToText('vss', Status)

    def CallVideoStatusToText(self, Status):
        '''Returns call video status as text.

        @param Status: Call video status.
        @type Status: L{Call video status<enums.cvsUnknown>}
        @return: Text describing the call video status.
        @rtype: unicode
        '''
        return self._ToText('cvs', Status)

    def ChatLeaveReasonToText(self, Reason):
        '''Returns leave reason as text.

        @param Reason: Chat leave reason.
        @type Reason: L{Chat leave reason<enums.leaUnknown>}
        @return: Text describing the chat leave reason.
        @rtype: unicode
        '''
        return self._ToText('lea', Reason)

    def ChatMessageStatusToText(self, Status):
        '''Returns message status as text.

        @param Status: Chat message status.
        @type Status: L{Chat message status<enums.cmsUnknown>}
        @return: Text describing the chat message status.
        @rtype: unicode
        '''
        return self._ToText('cms', Status)

    def ChatMessageTypeToText(self, Type):
        '''Returns message type as text.

        @param Type: Chat message type.
        @type Type: L{Chat message type<enums.cmeUnknown>}
        @return: Text describing the chat message type.
        @rtype: unicode
        '''
        return self._ToText('cme', Type)

    def ChatStatusToText(self, Status):
        '''Returns chatr status as text.

        @param Status: Chat status.
        @type Status: L{Chat status<enums.chsUnknown>}
        @return: Text describing the chat status.
        @rtype: unicode
        '''
        return self._ToText('chs', Status)

    def ConnectionStatusToText(self, Status):
        '''Returns connection status as text.

        @param Status: Connection status.
        @type Status: L{Connection status<enums.conUnknown>}
        @return: Text describing the connection status.
        @rtype: unicode
        '''
        return self._ToText('con', Status)

    def GroupTypeToText(self, Type):
        '''Returns group type as text.

        @param Type: Group type.
        @type Type: L{Group type<enums.grpUnknown>}
        @return: Text describing the group type.
        @rtype: unicode
        '''
        return self._ToText('grp', Type)

    def OnlineStatusToText(self, Status):
        '''Returns online status as text.

        @param Status: Online status.
        @type Status: L{Online status<enums.olsUnknown>}
        @return: Text describing the online status.
        @rtype: unicode
        '''
        return self._ToText('ols', Status)

    def SmsMessageStatusToText(self, Status):
        '''Returns SMS message status as text.

        @param Status: SMS message status.
        @type Status: L{SMS message status<enums.smsMessageStatusUnknown>}
        @return: Text describing the SMS message status.
        @rtype: unicode
        '''
        return self._ToText('smsMessageStatus', Status)

    def SmsMessageTypeToText(self, Type):
        '''Returns SMS message type as text.

        @param Type: SMS message type.
        @type Type: L{SMS message type<enums.smsMessageTypeUnknown>}
        @return: Text describing the SMS message type.
        @rtype: unicode
        '''
        return self._ToText('smsMessageType', Type)

    def SmsTargetStatusToText(self, Status):
        '''Returns SMS target status as text.

        @param Status: SMS target status.
        @type Status: L{SMS target status<enums.smsTargetStatusUnknown>}
        @return: Text describing the SMS target status.
        @rtype: unicode
        '''
        return self._ToText('smsTargetStatus', Status)

    def TextToAttachmentStatus(self, Text):
        '''Returns attachment status code.

        @param Text: Text, one of 'UNKNOWN', 'SUCCESS', 'PENDING_AUTHORIZATION', 'REFUSED',
        'NOT_AVAILABLE', 'AVAILABLE'.
        @type Text: unicode
        @return: Attachment status.
        @rtype: L{Attachment status<enums.apiAttachUnknown>}
        '''
        conv = {'UNKNOWN': enums.apiAttachUnknown,
                'SUCCESS': enums.apiAttachSuccess,
                'PENDING_AUTHORIZATION': enums.apiAttachPendingAuthorization,
                'REFUSED': enums.apiAttachRefused,
                'NOT_AVAILABLE': enums.apiAttachNotAvailable,
                'AVAILABLE': enums.apiAttachAvailable}
        try:
            return self._TextTo('api', conv[Text.upper()])
        except KeyError:
            raise ValueError('Bad text')

    def TextToBuddyStatus(self, Text):
        '''Returns buddy status code.

        @param Text: Text, one of 'UNKNOWN', 'NEVER_BEEN_FRIEND', 'DELETED_FRIEND',
        'PENDING_AUTHORIZATION', 'FRIEND'.
        @type Text: unicode
        @return: Buddy status.
        @rtype: L{Buddy status<enums.budUnknown>}
        '''
        conv = {'UNKNOWN': enums.budUnknown,
                'NEVER_BEEN_FRIEND': enums.budNeverBeenFriend,
                'DELETED_FRIEND': enums.budDeletedFriend,
                'PENDING_AUTHORIZATION': enums.budPendingAuthorization,
                'FRIEND': enums.budFriend}
        try:
            return self._TextTo('bud', conv[Text.upper()])
        except KeyError:
            raise ValueError('Bad text')

    def TextToCallStatus(self, Text):
        '''Returns call status code.

        @param Text: Text, one of L{Call status<enums.clsUnknown>}.
        @type Text: unicode
        @return: Call status.
        @rtype: L{Call status<enums.clsUnknown>}
        @note: Currently, this method only checks if the given string is one of the allowed ones
        and returns it or raises a C{ValueError}.
        '''
        return self._TextTo('cls', Text)

    def TextToCallType(self, Text):
        '''Returns call type code.

        @param Text: Text, one of L{Call type<enums.cltUnknown>}.
        @type Text: unicode
        @return: Call type.
        @rtype: L{Call type<enums.cltUnknown>}
        @note: Currently, this method only checks if the given string is one of the allowed ones
        and returns it or raises a C{ValueError}.
        '''
        return self._TextTo('clt', Text)

    def TextToChatMessageStatus(self, Text):
        '''Returns message status code.

        @param Text: Text, one of L{Chat message status<enums.cmsUnknown>}.
        @type Text: unicode
        @return: Chat message status.
        @rtype: L{Chat message status<enums.cmsUnknown>}
        @note: Currently, this method only checks if the given string is one of the allowed ones
        and returns it or raises a C{ValueError}.
        '''
        return self._TextTo('cms', Text)

    def TextToChatMessageType(self, Text):
        '''Returns message type code.

        @param Text: Text, one of L{Chat message type<enums.cmeUnknown>}.
        @type Text: unicode
        @return: Chat message type.
        @rtype: L{Chat message type<enums.cmeUnknown>}
        @note: Currently, this method only checks if the given string is one of the allowed ones
        and returns it or raises a C{ValueError}.
        '''
        return self._TextTo('cme', Text)

    def TextToConnectionStatus(self, Text):
        '''Retunes connection status code.

        @param Text: Text, one of L{Connection status<enums.conUnknown>}.
        @type Text: unicode
        @return: Connection status.
        @rtype: L{Connection status<enums.conUnknown>}
        @note: Currently, this method only checks if the given string is one of the allowed ones
        and returns it or raises a C{ValueError}.
        '''
        return self._TextTo('con', Text)

    def TextToGroupType(self, Text):
        '''Returns group type code.

        @param Text: Text, one of L{Group type<enums.grpUnknown>}.
        @type Text: unicode
        @return: Group type.
        @rtype: L{Group type<enums.grpUnknown>}
        @note: Currently, this method only checks if the given string is one of the allowed ones
        and returns it or raises a C{ValueError}.
        '''
        return self._TextTo('grp', Text)

    def TextToOnlineStatus(self, Text):
        '''Returns online status code.

        @param Text: Text, one of L{Online status<enums.olsUnknown>}.
        @type Text: unicode
        @return: Online status.
        @rtype: L{Online status<enums.olsUnknown>}
        @note: Currently, this method only checks if the given string is one of the allowed ones
        and returns it or raises a C{ValueError}.
        '''
        return self._TextTo('ols', Text)

    def TextToUserSex(self, Text):
        '''Returns user sex code.

        @param Text: Text, one of L{User sex<enums.usexUnknown>}.
        @type Text: unicode
        @return: User sex.
        @rtype: L{User sex<enums.usexUnknown>}
        @note: Currently, this method only checks if the given string is one of the allowed ones
        and returns it or raises a C{ValueError}.
        '''
        return self._TextTo('usex', Text)

    def TextToUserStatus(self, Text):
        '''Returns user status code.

        @param Text: Text, one of L{User status<enums.cusUnknown>}.
        @type Text: unicode
        @return: User status.
        @rtype: L{User status<enums.cusUnknown>}
        @note: Currently, this method only checks if the given string is one of the allowed ones
        and returns it or raises a C{ValueError}.
        '''
        return self._TextTo('cus', Text)

    def TextToVoicemailStatus(self, Text):
        '''Returns voicemail status code.

        @param Text: Text, one of L{Voicemail status<enums.vmsUnknown>}.
        @type Text: unicode
        @return: Voicemail status.
        @rtype: L{Voicemail status<enums.vmsUnknown>}
        @note: Currently, this method only checks if the given string is one of the allowed ones
        and returns it or raises a C{ValueError}.
        '''
        return self._TextTo('vms', Text)

    def UserSexToText(self, Sex):
        '''Returns user sex as text.

        @param Sex: User sex.
        @type Sex: L{User sex<enums.usexUnknown>}
        @return: Text describing the user sex.
        @rtype: unicode
        '''
        return self._ToText('usex', Sex)

    def UserStatusToText(self, Status):
        '''Returns user status as text.

        @param Status: User status.
        @type Status: L{User status<enums.cusUnknown>}
        @return: Text describing the user status.
        @rtype: unicode
        '''
        return self._ToText('cus', Status)

    def VoicemailFailureReasonToText(self, Reason):
        '''Returns voicemail failure reason as text.

        @param Reason: Voicemail failure reason.
        @type Reason: L{Voicemail failure reason<enums.vmrUnknown>}
        @return: Text describing the voicemail failure reason.
        @rtype: unicode
        '''
        return self._ToText('vmr', Reason)

    def VoicemailStatusToText(self, Status):
        '''Returns voicemail status as text.

        @param Status: Voicemail status.
        @type Status: L{Voicemail status<enums.vmsUnknown>}
        @return: Text describing the voicemail status.
        @rtype: unicode
        '''
        return self._ToText('vms', Status)

    def VoicemailTypeToText(self, Type):
        '''Returns voicemail type as text.

        @param Type: Voicemail type.
        @type Type: L{Voicemail type<enums.vmtUnknown>}
        @return: Text describing the voicemail type.
        @rtype: unicode
        '''
        return self._ToText('vmt', Type)

    def _GetLanguage(self):
        return self._Language

    def _SetLanguage(self, Language):
        try:
            self._Module = __import__('Languages.%s' % Language, globals(), locals(), ['Languages'])
            self._Language = unicode(Language)
        except ImportError:
            raise ValueError('Unknown language: %s' % Language)

    Language = property(_GetLanguage, _SetLanguage,
    doc='''Language used for all "ToText" conversions.

    Currently supported languages: ar, bg, cs, cz, da, de, el, en, es, et, fi, fr, he, hu, it, ja, ko,
    lt, lv, nl, no, pl, pp, pt, ro, ru, sv, tr, x1.

    @type: unicode
    ''')
