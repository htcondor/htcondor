'''Chats.
'''

from utils import *
from user import *
from errors import ISkypeError


class IChat(Cached):
    '''Represents a Skype chat.
    '''

    def __repr__(self):
        return '<%s with Name=%s>' % (Cached.__repr__(self)[1:-1], repr(self.Name))

    def _Alter(self, AlterName, Args=None):
        return self._Skype._Alter('CHAT', self._Name, AlterName, Args, 'ALTER CHAT %s' % AlterName)

    def _Init(self, Name, Skype):
        self._Skype = Skype
        self._Name = Name

    def _Property(self, PropName, Value=None, Cache=True):
        return self._Skype._Property('CHAT', self._Name, PropName, Value, Cache)

    def AcceptAdd(self):
        '''Accepts a shared group add request.
        '''
        self._Alter('ACCEPTADD')

    def AddMembers(self, *Members):
        '''Adds new members to the chat.

        @param Members: One or more users to add.
        @type Members: L{IUser}
        '''
        self._Alter('ADDMEMBERS', ', '.join([x.Handle for x in Members]))

    def Bookmark(self):
        '''Bookmarks the chat in Skype client.
        '''
        self._Alter('BOOKMARK')

    def ClearRecentMessages(self):
        '''Clears recent chat messages.
        '''
        self._Alter('CLEARRECENTMESSAGES')

    def Disband(self):
        '''Ends the chat.
        '''
        self._Alter('DISBAND')

    def EnterPassword(self, Password):
        '''Enters chat password.

        @param Password: Password
        @type Password: unicode
        '''
        self._Alter('ENTERPASSWORD', Password)

    def Join(self):
        '''Joins the chat.
        '''
        self._Alter('JOIN')

    def Kick(self, Handle):
        '''Kicks a member from chat.

        @param Handle: Handle
        @type Handle: unicode
        '''
        self._Alter('KICK', Handle)

    def KickBan(self, Handle):
        '''Kicks and bans a member from chat.

        @param Handle: Handle
        @type Handle: unicode
        '''
        self._Alter('KICKBAN', Handle)

    def Leave(self):
        '''Leaves the chat.
        '''
        self._Alter('LEAVE')

    def OpenWindow(self):
        '''Opens the chat window.
        '''
        self._Skype.Client.OpenDialog('CHAT', self._Name)

    def SendMessage(self, MessageText):
        '''Sends a chat message.

        @param MessageText: Message text
        @type MessageText: unicode
        @return: Message object
        @rtype: L{IChatMessage}
        '''
        return IChatMessage(chop(self._Skype._DoCommand('CHATMESSAGE %s %s' % (self._Name, MessageText)), 2)[1], self._Skype)

    def SetPassword(self, Password, Hint=''):
        '''Sets the chat password.

        @param Password: Password
        @type Password: unicode
        @param Hint: Password hint
        @type Hint: unicode
        '''
        if ' ' in Password:
            raise ValueError('Password mut be one word')
        self._Alter('SETPASSWORD', '%s %s' % (Password, Hint))

    def Unbookmark(self):
        '''Unbookmarks the chat.
        '''
        self._Alter('UNBOOKMARK')

    def _GetActiveMembers(self):
        return tuple([IUser(x, self._Skype) for x in esplit(self._Property('ACTIVEMEMBERS', Cache=False))])

    ActiveMembers = property(_GetActiveMembers,
    doc='''Active members of a chat.

    @type: tuple of L{IUser}
    ''')

    def _GetActivityDatetime(self):
        from datetime import datetime
        return datetime.fromtimestamp(self.ActivityTimestamp)

    ActivityDatetime = property(_GetActivityDatetime,
    doc='''Returns chat activity timestamp as datetime.

    @type: datetime.datetime
    ''')

    def _GetActivityTimestamp(self):
        return float(self._Property('ACTIVITY_TIMESTAMP'))

    ActivityTimestamp = property(_GetActivityTimestamp,
    doc='''Returns chat activity timestamp.

    @type: float
    @see: L{ActivityDatetime}
    ''')

    def _GetAdder(self):
        return IUser(self._Property('ADDER'), self._Skype)

    Adder = property(_GetAdder,
    doc='''Returns the user that added current user to the chat.

    @type: L{IUser}
    ''')

    def _SetAlertString(self, value):
        self._Alter('SETALERTSTRING', quote('=%s' % value))

    AlertString = property(fset=_SetAlertString,
    doc='''Chat alert string. Only messages containing words from this string will cause a
    notification to pop up on the screen.

    @type: unicode
    ''')

    def _GetApplicants(self):
        return tuple([IUser(x, self._Skype) for x in esplit(self._Property('APPLICANTS'))])

    Applicants = property(_GetApplicants,
    doc='''Chat applicants.

    @type: tuple of L{IUser}
    ''')

    def _GetBlob(self):
        return self._Property('BLOB')

    Blob = property(_GetBlob,
    doc='''Chat blob.

    @type: unicode
    ''')

    def _GetBookmarked(self):
        return self._Property('BOOKMARKED') == 'TRUE'

    Bookmarked = property(_GetBookmarked,
    doc='''Tells if this chat is bookmarked.

    @type: bool
    ''')

    def _GetDatetime(self):
        from datetime import datetime
        return datetime.fromtimestamp(self.Timestamp)

    Datetime = property(_GetDatetime,
    doc='''Chat timestamp as datetime.

    @type: datetime.datetime
    ''')

    def _GetDescription(self):
        return self._Property('DESCRIPTION')

    def _SetDescription(self, value):
        self._Property('DESCRIPTION', value)

    Description = property(_GetDescription, _SetDescription,
    doc='''Chat description.

    @type: unicode
    ''')

    def _GetDialogPartner(self):
        return self._Property('DIALOG_PARTNER')

    DialogPartner = property(_GetDialogPartner,
    doc='''Skypename of the chat dialog partner.

    @type: unicode
    ''')

    def _GetFriendlyName(self):
        return self._Property('FRIENDLYNAME')

    FriendlyName = property(_GetFriendlyName,
    doc='''Friendly name of the chat.

    @type: unicode
    ''')

    def _GetGuideLines(self):
        return self._Property('GUIDELINES')

    def _SetGuideLines(self, value):
        self._Alter('SETGUIDELINES', value)

    GuideLines = property(_GetGuideLines, _SetGuideLines,
    doc='''Chat guidelines.

    @type: unicode
    ''')

    def _GetMemberObjects(self):
        return tuple([IChatMember(x, self._Skype) for x in esplit(self._Property('MEMBEROBJECTS'), ', ')])

    MemberObjects = property(_GetMemberObjects,
    doc='''Chat members as member objects.

    @type: tuple of L{IChatMember}
    ''')

    def _GetMembers(self):
        return tuple([IUser(x, self._Skype) for x in esplit(self._Property('MEMBERS'))])

    Members = property(_GetMembers,
    doc='''Chat members.

    @type: tuple of L{IUser}
    ''')

    def _GetMessages(self):
        return tuple([IChatMessage(x ,self._Skype) for x in esplit(self._Property('CHATMESSAGES', Cache=False), ', ')])

    Messages = property(_GetMessages,
    doc='''All chat messages.

    @type: tuple of L{IChatMessage}
    ''')

    def _GetMyRole(self):
        return self._Property('MYROLE')

    MyRole = property(_GetMyRole,
    doc='''My chat role in a public chat.

    @type: L{Chat member role<enums.chatMemberRoleUnknown>}
    ''')

    def _GetMyStatus(self):
        return self._Property('MYSTATUS')

    MyStatus = property(_GetMyStatus,
    doc='''My status in a public chat.

    @type: L{My chat status<enums.chatStatusUnknown>}
    ''')

    def _GetName(self):
        return self._Name

    Name = property(_GetName,
    doc='''Chat name as used by Skype to identify this chat.

    @type: unicode
    ''')

    def _GetOptions(self):
        return int(self._Property('OPTIONS'))

    def _SetOptions(self, value):
        self._Alter('SETOPTIONS', value)

    Options = property(_GetOptions, _SetOptions,
    doc='''Chat options. A mask.

    @type: L{Chat options<enums.chatOptionJoiningEnabled>}
    ''')

    def _GetPasswordHint(self):
        return self._Property('PASSWORDHINT')

    PasswordHint = property(_GetPasswordHint,
    doc='''Chat password hint.

    @type: unicode
    ''')

    def _GetPosters(self):
        return tuple([IUser(x, self._Skype) for x in esplit(self._Property('POSTERS'))])

    Posters = property(_GetPosters,
    doc='''Users who have posted messages to this chat.

    @type: tuple of L{IUser}
    ''')

    def _GetRecentMessages(self):
        return tuple([IChatMessage(x, self._Skype) for x in esplit(self._Property('RECENTCHATMESSAGES', Cache=False), ', ')])

    RecentMessages = property(_GetRecentMessages,
    doc='''Most recent chat messages.

    @type: tuple of L{IChatMessage}
    ''')

    def _GetStatus(self):
        return self._Property('STATUS')

    Status = property(_GetStatus,
    doc='''Status.

    @type: L{Chat status<enums.chsUnknown>}
    ''')

    def _GetTimestamp(self):
        return float(self._Property('TIMESTAMP'))

    Timestamp = property(_GetTimestamp,
    doc='''Chat timestamp.

    @type: float
    @see: L{Datetime}
    ''')

    def _GetTopic(self):
        try:
            topicxml = self._Property('TOPICXML')
            if topicxml:
                return topicxml
        except ISkypeError:
            pass
        return self._Property('TOPIC')

    def _SetTopic(self, value):
        try:
            self._Alter('SETTOPICXML', value)
        except ISkypeError:
            self._Alter('SETTOPIC', value)

    Topic = property(_GetTopic, _SetTopic,
    doc='''Chat topic.

    @type: unicode
    ''')

    def _GetTopicXML(self):
        return self._Property('TOPICXML')

    def _SetTopicXML(self, value):
        self._Property('TOPICXML', value)

    TopicXML = property(_GetTopicXML, _SetTopicXML,
    doc='''Chat topic in XML format.

    @type: unicode
    ''')

    def _GetType(self):
        return self._Property('TYPE')

    Type = property(_GetType,
    doc='''Chat type.

    @type: L{Chat type<enums.chatTypeUnknown>}
    ''')


class IChatMessage(Cached):
    '''Represents a single chat message.
    '''

    def __repr__(self):
        return '<%s with Id=%s>' % (Cached.__repr__(self)[1:-1], repr(self.Id))

    def _Init(self, Id, Skype):
        self._Skype = Skype
        self._Id = int(Id)

    def _Property(self, PropName, Value=None, Cache=True):
        return self._Skype._Property('CHATMESSAGE', self._Id, PropName, Value, Cache)

    def MarkAsSeen(self):
        '''Marks a missed chat message as seen.
        '''
        self._Skype._DoCommand('SET CHATMESSAGE %d SEEN' % self._Id, 'CHATMESSAGE %d STATUS READ' % self._Id)

    def _GetBody(self):
        return self._Property('BODY')

    def _SetBody(self, value):
        self._Property('BODY', value)

    Body = property(_GetBody, _SetBody,
    doc='''Chat message body.

    @type: unicode
    ''')

    def _GetChat(self):
        return IChat(self.ChatName, self._Skype)

    Chat = property(_GetChat,
    doc='''Chat this message was posted on.

    @type: L{IChat}
    ''')

    def _GetChatName(self):
        return self._Property('CHATNAME')

    ChatName = property(_GetChatName,
    doc='''Name of the chat this message was posted on.

    @type: unicode
    ''')

    def _GetDatetime(self):
        from datetime import datetime
        return datetime.fromtimestamp(self.Timestamp)

    Datetime = property(_GetDatetime,
    doc='''Chat message timestamp as datetime.

    @type: datetime.datetime
    ''')

    def _GetEditedBy(self):
        return self._Property('EDITED_BY')

    EditedBy = property(_GetEditedBy,
    doc='''Skypename of the user who edited this message.

    @type: unicode
    ''')

    def _GetEditedDatetime(self):
        from datetime import datetime
        return datetime.fromtimestamp(self.EditedTimestamp)

    EditedDatetime = property(_GetEditedDatetime,
    doc='''Message editing timestamp as datetime.

    @type: datetime.datetime
    ''')

    def _GetEditedTimestamp(self):
        return float(self._Property('EDITED_TIMESTAMP'))

    EditedTimestamp = property(_GetEditedTimestamp,
    doc='''Message editing timestamp.

    @type: float
    ''')

    def _GetFromDisplayName(self):
        return self._Property('FROM_DISPNAME')

    FromDisplayName = property(_GetFromDisplayName,
    doc='''DisplayName of the message sender.

    @type: unicode
    ''')

    def _GetFromHandle(self):
        return self._Property('FROM_HANDLE')

    FromHandle = property(_GetFromHandle,
    doc='''Skypename of the message sender.

    @type: unicode
    ''')

    def _GetId(self):
        return self._Id

    Id = property(_GetId,
    doc='''Chat message Id.

    @type: int
    ''')

    def _GetIsEditable(self):
        return self._Property('IS_EDITABLE') == 'TRUE'

    IsEditable = property(_GetIsEditable,
    doc='''Tells if message body is editable.

    @type: bool
    ''')

    def _GetLeaveReason(self):
        return self._Property('LEAVEREASON')

    LeaveReason = property(_GetLeaveReason,
    doc='''LeaveReason.

    @type: L{Chat leave reason<enums.leaUnknown>}
    ''')

    def _SetSeen(self, value):
        from warnings import warn
        warn('IChatMessage.Seen = x: Use IChatMessage.MarkAsSeen() instead.', DeprecationWarning, stacklevel=2)
        if value:
            self.MarkAsSeen()
        else:
            raise ISkypeError(0, 'Seen can only be set to True')

    Seen = property(fset=_SetSeen,
    doc='''Marks a missed chat message as seen.

    @type: bool
    @deprecated: Unpythonic, use L{MarkAsSeen} instead.
    ''')

    def _GetSender(self):
        return IUser(self.FromHandle, self._Skype)

    Sender = property(_GetSender,
    doc='''Sender of the chat message.

    @type: L{IUser}
    ''')

    def _GetStatus(self):
        return self._Property('STATUS')

    Status = property(_GetStatus,
    doc='''Status of the chat messsage.

    @type: L{Chat message status<enums.cmsUnknown>}
    ''')

    def _GetTimestamp(self):
        return float(self._Property('TIMESTAMP'))

    Timestamp = property(_GetTimestamp,
    doc='''Chat message timestamp.

    @type: float
    @see: L{Datetime}
    ''')

    def _GetType(self):
        return self._Property('TYPE')

    Type = property(_GetType,
    doc='''Type of chat message.

    @type: L{Chat message type<enums.cmeUnknown>}
    ''')

    def _GetUsers(self):
        return tuple([IUser(self._Skype, x) for x in esplit(self._Property('USERS'))])

    Users = property(_GetUsers,
    doc='''Users added to the chat.

    @type: tuple of L{IUser}
    ''')


class IChatMember(Cached):
    '''Represents a member of a public chat.
    '''

    def __repr__(self):
        return '<%s with Id=%s>' % (Cached.__repr__(self)[1:-1], repr(self.Id))

    def _Alter(self, AlterName, Args=None):
        return self._Skype._Alter('CHATMEMBER', self._Id, AlterName, Args, 'ALTER CHATMEMBER %s' % AlterName)

    def _Init(self, Id, Skype):
        self._Skype = Skype
        self._Id = int(Id)

    def _Property(self, PropName, Value=None, Cache=True):
        return self._Skype._Property('CHATMEMBER', self._Id, PropName, Value, Cache)

    def CanSetRoleTo(self, Role):
        '''Checks if the new role can be applied to the member.

        @param Role: New chat member role.
        @type Role: L{Chat member role<enums.chatMemberRoleUnknown>}
        @return: True if the new role can be applied, False otherwise.
        @rtype: bool
        '''
        return self._Alter('CANSETROLETO', Role) == 'TRUE'

    def _GetChat(self):
        return IChat(self._Property('CHATNAME'), self._Skype)

    Chat = property(_GetChat,
    doc='''Chat this member belongs to.

    @type: L{IChat}
    ''')

    def _GetHandle(self):
        return self._Property('IDENTITY')

    Handle = property(_GetHandle,
    doc='''Member Skypename.

    @type: unicode
    ''')

    def _GetId(self):
        return self._Id

    Id = property(_GetId,
    doc='''Chat member Id.

    @type: int
    ''')

    def _GetIsActive(self):
        return self._Property('IS_ACTIVE') == 'TRUE'

    IsActive = property(_GetIsActive,
    doc='''Member activity status.

    @type: bool
    ''')

    def _GetRole(self):
        return self._Property('ROLE')

    def _SetRole(self, value):
        self._Alter('SETROLETO', value)

    Role = property(_GetRole, _SetRole,
    doc='''Chat Member role.

    @type: L{Chat member role<enums.chatMemberRoleUnknown>}
    ''')
