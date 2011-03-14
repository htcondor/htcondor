'''
Low level Skype for Windows interface implemented
using Windows messaging. Uses direct WinAPI calls
through ctypes module.

This module handles the options that you can pass to L{ISkype.__init__<skype.ISkype.__init__>}
for Windows machines.

No options are currently supported.
'''

import threading
import time
import weakref
from ctypes import *
from Skype4Py.API import ICommand, _ISkypeAPIBase
from Skype4Py.enums import *
from Skype4Py.errors import ISkypeAPIError

try:
    WNDPROC = WINFUNCTYPE(c_long, c_int, c_uint, c_int, c_int)
except NameError:
    # This will allow importing of this module on non-Windows machines. It won't work
    # of course but this will allow building documentation on any platform.
    WNDPROC = c_void_p

class _WNDCLASS(Structure):
    _fields_ = [('style', c_uint),
                ('lpfnWndProc', WNDPROC),
                ('cbClsExtra', c_int),
                ('cbWndExtra', c_int),
                ('hInstance', c_int),
                ('hIcon', c_int),
                ('hCursor', c_int),
                ('hbrBackground', c_int),
                ('lpszMenuName', c_char_p),
                ('lpszClassName', c_char_p)]


class _MSG(Structure):
    _fields_ = [('hwnd', c_int),
                ('message', c_uint),
                ('wParam', c_int),
                ('lParam', c_int),
                ('time', c_int),
                ('pointX', c_long),
                ('pointY', c_long)]


class _COPYDATASTRUCT(Structure):
    _fields_ = [('dwData', POINTER(c_uint)),
                ('cbData', c_uint),
                ('lpData', c_char_p)]
PCOPYDATASTRUCT = POINTER(_COPYDATASTRUCT)

_WM_QUIT = 0x12
_WM_COPYDATA = 0x4A

_HWND_BROADCAST = 0xFFFF



class _ISkypeAPI(_ISkypeAPIBase):
    def __init__(self, handler, opts):
        _ISkypeAPIBase.__init__(self, opts)
        if opts:
            raise TypeError('Unexpected parameter(s): %s' % ', '.join(opts.keys()))
        self.hwnd = None
        self.Skype = None
        self.RegisterHandler(handler)
        self._SkypeControlAPIDiscover = windll.user32.RegisterWindowMessageA('SkypeControlAPIDiscover')
        self._SkypeControlAPIAttach = windll.user32.RegisterWindowMessageA('SkypeControlAPIAttach')
        windll.user32.GetWindowLongA.restype = c_ulong
        self.start()
        # wait till the thread initializes
        while not self.hwnd:
            time.sleep(0.01)

    def run(self):
        self.DebugPrint('thread started')
        if not self.CreateWindow():
            self.hwnd = None
            return

        Msg = _MSG()
        pMsg = pointer(Msg)
        while self.hwnd and windll.user32.GetMessageA(pMsg, self.hwnd, 0, 0):
            windll.user32.TranslateMessage(pMsg)
            windll.user32.DispatchMessageA(pMsg)

        self.DestroyWindow()
        self.hwnd = None
        self.DebugPrint('thread finished')

    def Close(self):
        # if there are no active handlers
        if self.NumOfHandlers() == 0:
            if self.hwnd:
                windll.user32.PostMessageA(self.hwnd, _WM_QUIT, 0, 0)
                while self.hwnd:
                    time.sleep(0.01)
            self.Skype = None
            self.DebugPrint('closed')

    def SetFriendlyName(self, FriendlyName):
        self.FriendlyName = FriendlyName
        if self.Skype:
            self.SendCommand(ICommand(-1, 'NAME %s' % FriendlyName))

    def GetForegroundWindow(self):
        fhwnd = windll.user32.GetForegroundWindow()
        if fhwnd:
            # awahlig (7.05.2008):
            # I've found at least one app (RocketDock) that had window style 8 set.
            # This is odd since windows header files do not contain such a style.
            # Doing message exchange while this window is a foreground one, causes
            # lockups if some operations on client UI are involved (for example
            # sending a 'FOCUS' command). Therefore, we will set our window as
            # the foreground one for the transmission time.
            if windll.user32.GetWindowLongA(fhwnd, -16) & 8 == 0:
                fhwnd = None
        return fhwnd
        
    def __Attach_ftimeout(self):
        self.Wait = False

    def Attach(self, Timeout=30000, Wait=True):
        if self.Skype:
            return
        if not self.isAlive():
            raise ISkypeAPIError('Skype API closed')
        self.DebugPrint('->', 'SkypeControlAPIDiscover')
        fhwnd = self.GetForegroundWindow()
        try:
            if fhwnd:
                windll.user32.SetForegroundWindow(self.hwnd)
            if not windll.user32.SendMessageTimeoutA(_HWND_BROADCAST, self._SkypeControlAPIDiscover,
                                                     self.hwnd, None, 2, 5000, None):
                raise ISkypeAPIError('Could not broadcast Skype discover message')
            # wait (with timeout) till the WindProc() attaches
            self.Wait = True
            t = threading.Timer(Timeout / 1000, self.__Attach_ftimeout)
            if Wait:
                t.start()
            while self.Wait and self.AttachmentStatus not in (apiAttachSuccess, apiAttachRefused):
                if self.AttachmentStatus == apiAttachPendingAuthorization:
                    # disable the timeout
                    t.cancel()
                elif self.AttachmentStatus == apiAttachAvailable:
                    # rebroadcast
                    self.DebugPrint('->', 'SkypeControlAPIDiscover')
                    windll.user32.SetForegroundWindow(self.hwnd)
                    if not windll.user32.SendMessageTimeoutA(_HWND_BROADCAST, self._SkypeControlAPIDiscover,
                                                             self.hwnd, None, 2, 5000, None):
                        raise ISkypeAPIError('Could not broadcast Skype discover message')
                time.sleep(0.01)
            t.cancel()
        finally:
            if fhwnd:
                windll.user32.SetForegroundWindow(fhwnd)
        # check if we got the Skype window's hwnd
        if self.Skype:
            self.SendCommand(ICommand(-1, 'PROTOCOL %s' % self.Protocol))
        elif not self.Wait:
            raise ISkypeAPIError('Skype attach timeout')

    def IsRunning(self):
        # TZap is for Skype 4.0, tSk for 3.8 series
        return bool(windll.user32.FindWindowA('TZapMainForm.UnicodeClass', None) or \
            windll.user32.FindWindowA('tSkMainForm.UnicodeClass', None))

    def get_skype_path(self):
        key = c_long()
        # try to find Skype in HKEY_CURRENT_USER registry tree
        if windll.advapi32.RegOpenKeyA(0x80000001, 'Software\\Skype\\Phone', byref(key)) != 0:
            # try to find Skype in HKEY_LOCAL_MACHINE registry tree
            if windll.advapi32.RegOpenKeyA(0x80000002, 'Software\\Skype\\Phone', byref(key)) != 0:
                raise ISkypeAPIError('Skype not installed')
        pathlen = c_long(512)
        path = create_string_buffer(pathlen.value)
        if windll.advapi32.RegQueryValueExA(key, 'SkypePath', None, None, path, byref(pathlen)) != 0:
            windll.advapi32.RegCloseKey(key)
            raise ISkypeAPIError('Cannot find Skype path')
        windll.advapi32.RegCloseKey(key)
        return path.value

    def Start(self, Minimized=False, Nosplash=False):
        args = []
        if Minimized:
            args.append('/MINIMIZED')
        if Nosplash:
            args.append('/NOSPLASH')
        try:
            if self.hwnd:
                fhwnd = self.GetForegroundWindow()
                if fhwnd:
                    windll.user32.SetForegroundWindow(self.hwnd)
            if windll.shell32.ShellExecuteA(None, 'open', self.get_skype_path(), ' '.join(args), None, 0) <= 32:
                raise ISkypeAPIError('Could not start Skype')
        finally:
            if self.hwnd and fhwnd:
                windll.user32.SetForegroundWindow(fhwnd)
        
    def Shutdown(self):
        try:
            if self.hwnd:
                fhwnd = self.GetForegroundWindow()
                if fhwnd:
                    windll.user32.SetForegroundWindow(self.hwnd)
            if windll.shell32.ShellExecuteA(None, 'open', self.get_skype_path(), '/SHUTDOWN', None, 0) <= 32:
                raise ISkypeAPIError('Could not shutdown Skype')
        finally:
            if self.hwnd and fhwnd:
                windll.user32.SetForegroundWindow(fhwnd)

    def CreateWindow(self):
        # window class has to be saved as property to keep reference to self.WinProc
        self.window_class = _WNDCLASS(3, WNDPROC(self.WinProc), 0, 0,
                                      windll.kernel32.GetModuleHandleA(None),
                                      0, 0, 0, None, 'Skype4Py.%d' % id(self))

        wclass = windll.user32.RegisterClassA(byref(self.window_class))
        if wclass == 0:
            return False

        self.hwnd = windll.user32.CreateWindowExA(0, 'Skype4Py.%d' % id(self), 'Skype4Py',
                                                  0xCF0000, 0x80000000, 0x80000000,
                                                  0x80000000, 0x80000000, None, None,
                                                  self.window_class.hInstance, 0)
        if self.hwnd == 0:
            windll.user32.UnregisterClassA('Skype4Py.%d' % id(self), None)
            return False

        return True

    def DestroyWindow(self):
        if not windll.user32.DestroyWindow(self.hwnd):
            return False

        if not windll.user32.UnregisterClassA('Skype4Py.%d' % id(self), None):
            return False

        return True

    def WinProc(self, hwnd, uMsg, wParam, lParam):
        if uMsg == self._SkypeControlAPIAttach:
            self.DebugPrint('<-', 'SkypeControlAPIAttach', lParam)
            if lParam == apiAttachSuccess:
                self.Skype = wParam
            elif lParam in (apiAttachRefused, apiAttachNotAvailable, apiAttachAvailable):
                self.Skype = None
            self.SetAttachmentStatus(lParam)
            return 1
        elif uMsg == _WM_COPYDATA and wParam == self.Skype and lParam:
            copydata = cast(lParam, PCOPYDATASTRUCT).contents
            com8 = copydata.lpData[:copydata.cbData - 1]
            com = com8.decode('utf-8')
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
            return 1
        elif uMsg == apiAttachAvailable:
            self.DebugPrint('<-', 'apiAttachAvailable')
            self.Skype = None
            self.SetAttachmentStatus(uMsg)
            return 1
        return windll.user32.DefWindowProcA(c_int(hwnd), c_int(uMsg), c_int(wParam), c_int(lParam))

    def SendCommand(self, Command):
        for retry in xrange(2):
            if not self.Skype:
                self.Attach(Command.Timeout)
            self.CommandsStackPush(Command)
            self.CallHandler('send', Command)
            com = u'#%d %s' % (Command.Id, Command.Command)
            com8 = com.encode('utf-8') + '\0'
            copydata = _COPYDATASTRUCT(None, len(com8), com8)
            if Command.Blocking:
                Command._event = event = threading.Event()
            else:
                Command._timer = timer = threading.Timer(Command.Timeout / 1000.0, self.CommandsStackPop, (Command.Id,))
            self.DebugPrint('->', repr(com))
            fhwnd = self.GetForegroundWindow()
            try:
                if fhwnd:
                    windll.user32.SetForegroundWindow(self.hwnd)
                if windll.user32.SendMessageA(self.Skype, _WM_COPYDATA, self.hwnd, byref(copydata)):
                    if Command.Blocking:
                        event.wait(Command.Timeout / 1000.0)
                        if not event.isSet():
                            raise ISkypeAPIError('Skype command timeout')
                    else:
                        timer.start()
                    break
                else:
                    self.CommandsStackPop(Command.Id)
                    self.Skype = None
                    # let the loop go back and try to reattach but only once
            finally:
                if fhwnd:
                    windll.user32.SetForegroundWindow(fhwnd)
        else:
            raise ISkypeAPIError('Skype API error, check if Skype wasn\'t closed')
