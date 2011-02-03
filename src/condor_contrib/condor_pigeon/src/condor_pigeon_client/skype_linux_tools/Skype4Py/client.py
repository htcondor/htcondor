'''Skype client user interface control.
'''

from enums import *
from errors import ISkypeError
from utils import *
import weakref


class IClient(object):
    '''Represents a Skype client. Access using L{ISkype.Client<skype.ISkype.Client>}.
    '''

    def __init__(self, Skype):
        '''__init__.

        @param Skype: Skype
        @type Skype: L{ISkype}
        '''
        self._SkypeRef = weakref.ref(Skype)

    def ButtonPressed(self, Key):
        '''Sends button button pressed to client.

        @param Key: Key
        @type Key: unicode
        '''
        self._Skype._DoCommand('BTN_PRESSED %s' % Key)

    def ButtonReleased(self, Key):
        '''Sends button released event to client.

        @param Key: Key
        @type Key: unicode
        '''
        self._Skype._DoCommand('BTN_RELEASED %s' % Key)

    def CreateEvent(self, EventId, Caption, Hint):
        '''Creates a custom event displayed in Skype client's events pane.

        @param EventId: Unique identifier for the event.
        @type EventId: unicode
        @param Caption: Caption text.
        @type Caption: unicode
        @param Hint: Hint text. Shown when mouse hoovers over the event.
        @type Hint: unicode
        @return: Event object.
        @rtype: L{IPluginEvent}
        '''
        self._Skype._DoCommand('CREATE EVENT %s CAPTION %s HINT %s' % (EventId, quote(Caption), quote(Hint)))
        return IPluginEvent(EventId, self._Skype)

    def CreateMenuItem(self, MenuItemId, PluginContext, CaptionText, HintText=u'', IconPath='', Enabled=True,
                       ContactType=pluginContactTypeAll, MultipleContacts=False):
        '''Creates custom menu item in Skype client's "Do More" menus.

        @param MenuItemId: Unique identifier for the menu item.
        @type MenuItemId: unicode
        @param PluginContext: Menu item context. Allows to choose in which client windows will
        the menu item appear.
        @type PluginContext: L{Plug-in context<enums.pluginContextUnknown>}
        @param CaptionText: Caption text.
        @type CaptionText: unicode
        @param HintText: Hint text (optional). Shown when mouse hoovers over the menu item.
        @type HintText: unicode
        @param IconPath: Path to the icon (optional).
        @type IconPath: unicode
        @param Enabled: Initial state of the menu item. True by default.
        @type Enabled: bool
        @param ContactType: In case of L{pluginContextContact<enums.pluginContextContact>} tells which contacts
        the menu item should appear for. Defaults to L{pluginContactTypeAll<enums.pluginContactTypeAll>}.
        @type ContactType: L{Plug-in contact type<enums.pluginContactTypeUnknown>}
        @param MultipleContacts: Set to True if multiple contacts should be allowed (defaults to False).
        @type MultipleContacts: bool
        @return: Menu item object.
        @rtype: L{IPluginMenuItem}
        '''
        com = 'CREATE MENU_ITEM %s CONTEXT %s CAPTION %s ENABLED %s' % (MenuItemId, PluginContext, quote(CaptionText), cndexp(Enabled, 'true', 'false'))
        if HintText:
            com += ' HINT %s' % quote(HintText)
        if IconPath:
            com += ' ICON %s' % quote(IconPath)
        if MultipleContacts:
            com += ' ENABLE_MULTIPLE_CONTACTS true'
        if PluginContext == pluginContextContact:
            com += ' CONTACT_TYPE_FILTER %s' % ContactType
        self._Skype._DoCommand(com)
        return IPluginMenuItem(MenuItemId, self._Skype, CaptionText, HintText, Enabled)

    def Focus(self):
        '''Brings the client window into focus.
        '''
        self._Skype._DoCommand('FOCUS')

    def Minimize(self):
        '''Hides Skype application window.
        '''
        self._Skype._DoCommand('MINIMIZE')

    def OpenAddContactDialog(self, Username=''):
        '''Opens "Add a Contact" dialog.

        @param Username: Optional Skypename of the contact.
        @type Username: unicode
        '''
        self.OpenDialog('ADDAFRIEND', Username)

    def OpenAuthorizationDialog(self, Username):
        '''Opens authorization dialog.

        @param Username: Skypename of the user to authenticate.
        @type Username: unicode
        '''
        self.OpenDialog('AUTHORIZATION', Username)

    def OpenBlockedUsersDialog(self):
        '''Opens blocked users dialog.
        '''
        self.OpenDialog('BLOCKEDUSERS')

    def OpenCallHistoryTab(self):
        '''Opens call history tab.
        '''
        self.OpenDialog('CALLHISTORY')

    def OpenConferenceDialog(self):
        '''Opens create conference dialog.
        '''
        self.OpenDialog('CONFERENCE')

    def OpenContactsTab(self):
        '''Opens contacts tab.
        '''
        self.OpenDialog('CONTACTS')

    def OpenDialog(self, Name, *Params):
        '''Open dialog. Use this method to open dialogs added in newer Skype versions if there is no
        dedicated method in Skype4Py.

        @param Name: Dialog name.
        @type Name: unicode
        @param Params: One or more optional parameters.
        @type Params: unicode
        '''
        self._Skype._DoCommand('OPEN %s %s' % (Name, ' '.join(Params)))

    def OpenDialpadTab(self):
        '''Opens dial pad tab.
        '''
        self.OpenDialog('DIALPAD')

    def OpenFileTransferDialog(self, Username, Folder):
        '''Opens file transfer dialog.

        @param Username: Skypename of the user.
        @type Username: unicode
        @param Folder: Path to initial directory.
        @type Folder: unicode
        '''
        self.OpenDialog('FILETRANSFER', Username, 'IN %s' % Folder)

    def OpenGettingStartedWizard(self):
        '''Opens getting started wizard.
        '''
        self.OpenDialog('GETTINGSTARTED')

    def OpenImportContactsWizard(self):
        '''Opens import contacts wizard.
        '''
        self.OpenDialog('IMPORTCONTACTS')

    def OpenLiveTab(self):
        '''OpenLiveTab.
        '''
        self.OpenDialog('LIVETAB')

    def OpenMessageDialog(self, Username, Text=''):
        '''Opens "Send an IM Message" dialog.

        @param Username: Message target.
        @type Username: unicode
        @param Text: Message text.
        @type Text: unicode
        '''
        self.OpenDialog('IM', Username, Text)

    def OpenOptionsDialog(self, Page=''):
        '''Opens options dialog.

        @param Page: Page name to open.
        @type Page: unicode
        '''
        self.OpenDialog('OPTIONS', Page)

    def OpenProfileDialog(self):
        '''Opens current user profile dialog.
        '''
        self.OpenDialog('PROFILE')

    def OpenSearchDialog(self):
        '''Opens search dialog.
        '''
        self.OpenDialog('SEARCH')

    def OpenSendContactsDialog(self, Username=''):
        '''Opens send contacts dialog.

        @param Username: Optional Skypename of the user.
        @type Username: unicode
        '''
        self.OpenDialog('SENDCONTACTS', Username)

    def OpenSmsDialog(self, SmsId):
        '''Opens SMS window

        @param SmsId: SMS message Id.
        @type SmsId: int
        '''
        self.OpenDialog('SMS', SmsId)

    def OpenUserInfoDialog(self, Username):
        '''Opens user information dialog.

        @param Username: Skypename of the user.
        @type Username: unicode
        '''
        self.OpenDialog('USERINFO', Username)

    def OpenVideoTestDialog(self):
        '''Opens video test dialog.
        '''
        self.OpenDialog('VIDEOTEST')

    def Shutdown(self):
        '''Closes Skype application.
        '''
        self._Skype._API.Shutdown()

    def Start(self, Minimized=False, Nosplash=False):
        '''Starts Skype application.

        @param Minimized: If True, Skype is started minized in system tray.
        @type Minimized: bool
        @param Nosplash: If True, no splash screen is displayed upon startup.
        @type Nosplash: bool
        '''
        self._Skype._API.Start(Minimized, Nosplash)

    def _Get_Skype(self):
        skype = self._SkypeRef()
        if skype:
            return skype
        raise ISkypeError('Skype4Py internal error')

    _Skype = property(_Get_Skype)

    def _GetIsRunning(self):
        return self._Skype._API.IsRunning()

    IsRunning = property(_GetIsRunning,
    doc='''Tells if Skype client is running.

    @type: bool
    ''')

    def _GetWallpaper(self):
        return self._Skype.Variable('WALLPAPER')

    def _SetWallpaper(self, value):
        self._Skype.Variable('WALLPAPER', value)

    Wallpaper = property(_GetWallpaper, _SetWallpaper,
    doc='''Path to client wallpaper bitmap.

    @type: unicode
    ''')

    def _GetWindowState(self):
        return self._Skype.Variable('WINDOWSTATE')

    def _SetWindowState(self, value):
        self._Skype.Variable('WINDOWSTATE', value)

    WindowState = property(_GetWindowState, _SetWindowState,
    doc='''Client window state.

    @type: L{Window state<enums.wndUnknown>}
    ''')


class IPluginEvent(Cached):
    '''Represents an event displayed in Skype client's events pane.
    '''

    def __repr__(self):
        return '<%s with Id=%s>' % (Cached.__repr__(self)[1:-1], repr(self.Id))

    def _Init(self, Id, Skype):
        self._Skype = Skype
        self._Id = unicode(Id)

    def Delete(self):
        '''Deletes the event from the events pane in the Skype client.
        '''
        self._Skype._DoCommand('DELETE EVENT %s' % self._Id)

    def _GetId(self):
        return self._Id

    Id = property(_GetId,
    doc='''Unique event Id.

    @type: unicode
    ''')


class IPluginMenuItem(Cached):
    '''Represents a menu item displayed in Skype client's "Do More" menus.
    '''

    def __repr__(self):
        return '<%s with Id=%s>' % (Cached.__repr__(self)[1:-1], repr(self.Id))

    def _Init(self, Id, Skype, Caption=None, Hint=None, Enabled=None):
        self._Skype = Skype
        self._Id = unicode(Id)
        self._CacheDict = {}
        if Caption != None:
            self._CacheDict['CAPTION'] = unicode(Caption)
        if Hint != None:
            self._CacheDict['HINT'] = unicode(Hint)
        if Enabled != None:
            self._CacheDict['ENABLED'] = cndexp(Enabled, u'TRUE', u'FALSE')

    def _Property(self, PropName, Set=None):
        if Set == None:
            return self._CacheDict[PropName]
        self._Skype._Property('MENU_ITEM', self._Id, PropName, Set)
        self._CacheDict[PropName] = unicode(Set)

    def Delete(self):
        '''Removes the menu item from the "Do More" menus.
        '''
        self._Skype._DoCommand('DELETE MENU_ITEM %s' % self._Id)

    def _GetCaption(self):
        return self._Property('CAPTION')

    def _SetCaption(self, value):
        self._Property('CAPTION', value)

    Caption = property(_GetCaption, _SetCaption,
    doc='''Menu item caption text.

    @type: unicode
    ''')

    def _GetEnabled(self):
        return self._Property('ENABLED') == 'TRUE'

    def _SetEnabled(self, value):
        self._Property('ENABLED', cndexp(value, 'TRUE', 'FALSE'))

    Enabled = property(_GetEnabled, _SetEnabled,
    doc='''Defines whether the menu item is enabled when a user launches Skype. If no value is defined,
    the menu item will be enabled.

    @type: bool
    ''')

    def _GetHint(self):
        return self._Property('HINT')

    def _SetHint(self, value):
        self._Property('HINT', value)

    Hint = property(_GetHint, _SetHint,
    doc='''Menu item hint text.

    @type: unicode
    ''')

    def _GetId(self):
        return self._Id

    Id = property(_GetId,
    doc='''Unique menu item Id.

    @type: unicode
    ''')
