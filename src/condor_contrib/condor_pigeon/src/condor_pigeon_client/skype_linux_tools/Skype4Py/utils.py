'''Utility functions and classes used internally by Skype4Py.
'''

import sys
import weakref
import threading
from new import instancemethod


def chop(s, n=1, d=None):
    '''Chops initial words from a string and returns a list of them and the rest of the string.

    @param s: String to chop from.
    @type s: str or unicode
    @param n: Number of words to chop.
    @type n: int
    @param d: Optional delimeter. Any white-char by default.
    @type d: str or unicode
    @return: A list of n first words from the string followed by the rest of the string
    (C{[w1, w2, ..., wn, rest_of_string]}).
    @rtype: list of str or unicode
    '''

    spl = s.split(d, n)
    if len(spl) == n:
        spl.append(s[:0])
    if len(spl) != n + 1:
        raise ValueError('chop: Could not chop %d words from \'%s\'' % (n, s))
    return spl


def args2dict(s):
    '''Converts a string in 'ARG="value", ARG2="value2"' format into a dictionary.

    @param s: Input string with comma-separated 'ARG="value"' strings.
    @type s: str or unicode
    @return: C{{'ARG': 'value'}} dictionary.
    @rtype: dict
    '''

    d = {}
    while s:
        t, s = chop(s, 1, '=')
        if s.startswith('"'):
            i = 0
            while True:
                i = s.find('"', i+1)
                # XXX How are the double-quotes escaped? The code below implements VisualBasic technique.
                try:
                    if s[i+1] != '"':
                        break
                    else:
                        i += 1
                except IndexError:
                    break
            if i > 0:
                d[t] = s[1:i]
                s = s[i+1:]
            else:
                d[t] = s
                break
        else:
            i = s.find(', ')
            if i >= 0:
                d[t] = s[:i]
                s = s[i+2:]
            else:
                d[t] = s
                break
    return d


def quote(s, always=False):
    '''Adds double-quotes to string if needed.

    @param s: String to add double-quotes to.
    @type s: str or unicode
    @param always: If True, adds quotes even if the input string contains no spaces.
    @type always: bool
    @return: If the given string contains spaces or always=True, returns the string enclosed
    in double-quotes (if it contained quotes too, they are preceded with a backslash).
    Otherwise returns the string unchnaged.
    @rtype: str or unicode
    '''

    if always or ' ' in s:
        return '"%s"' % s.replace('"', '\\"')
    return s


def esplit(s, d=None):
    '''Splits a string into words.

    @param s: String to split.
    @type s: str or unicode
    @param d: Optional delimeter. Any white-char by default.
    @type d: str or unicode
    @return: A list of words or C{[]} if the string was empty.
    @rtype: list of str or unicode
    @note: This function works like C{s.split(d)} except that it always returns an
    empty list instead of C{['']} for empty strings.
    '''

    if s:
        return s.split(d)
    return []


def cndexp(condition, truevalue, falsevalue):
    '''Simulates a conditional expression known from C or Python 2.5+.

    @param condition: Boolean value telling what should be returned.
    @type condition: bool, see note
    @param truevalue: Value returned if condition was True.
    @type truevalue: any
    @param falsevalue: Value returned if condition was False.
    @type falsevalue: any
    @return: Either truevalue or falsevalue depending on condition.
    @rtype: same as type of truevalue or falsevalue
    @note: The type of condition parameter can be anything as long as
    C{bool(condition)} returns a bool value.
    '''

    if condition:
        return truevalue
    return falsevalue


class _WeakMethod(object):
    '''Helper class for WeakCallableRef function (see below).
    Don't use directly.
    '''

    def __init__(self, method, callback=None):
        '''__init__.

        @param method: Method to be referenced.
        @type method: method
        @param callback: Callback to be called when the method is collected.
        @type callback: callable
        '''
        self.im_func = method.im_func
        try:
            self.weak_im_self = weakref.ref(method.im_self, self._dies)
        except TypeError:
            self.weak_im_self = None
        self.im_class = method.im_class
        self.callback = callback

    def __call__(self):
        if self.weak_im_self:
            im_self = self.weak_im_self()
            if im_self == None:
                return None
        else:
            im_self = None
        return instancemethod(self.im_func, im_self, self.im_class)

    def __repr__(self):
        obj = self()
        objrepr = repr(obj)
        if obj == None:
            objrepr = 'dead'
        return '<weakref at 0x%x; %s>' % (id(self), objrepr)
    def _dies(self, ref):
        # weakref to im_self died
        self.im_func = self.im_class = None
        if self.callback != None:
            self.callback(self)

def WeakCallableRef(c, callback=None):
    '''Creates and returns a new weak reference to a callable object.

    In contrast to weakref.ref() works on all kinds of callables.
    Usage is same as weakref.ref().

    @param c: A callable that the weak reference should point at.
    @type c: callable
    @param callback: Callback called when the callable is collected (freed).
    @type callback: callable
    @return: A weak callable reference.
    @rtype: weakref
    '''

    try:
        return _WeakMethod(c, callback)
    except AttributeError:
        return weakref.ref(c, callback)


class _EventHandlingThread(threading.Thread):
    def __init__(self, name=None):
        '''__init__.

        @param name: name
        @type name: unicode
        '''
        threading.Thread.__init__(self, name='%s event handler' % name)
        self.setDaemon(False)
        self.lock = threading.Lock()
        self.queue = []

    def enqueue(self, target, args, kwargs):
        '''enqueue.

        @param target: Callable to be called.
        @type target: callable
        @param args: Positional arguments for the callable.
        @type args: tuple
        @param kwargs: Keyword arguments for the callable.
        @type kwargs: dict
        '''
        self.queue.append((target, args, kwargs))

    def run(self):
        '''Executes all enqueued targets.
        '''
        while True:
            try:
                try:
                    self.lock.acquire()
                    h = self.queue[0]
                    del self.queue[0]
                except IndexError:
                    break
            finally:
                self.lock.release()
            h[0](*h[1], **h[2])

class EventHandlingBase(object):
    '''This class is used as a base by all classes implementing event handlers.

    Look at known subclasses (above in epydoc) to see which classes will allow you to
    attach your own callables (event handlers) to certain events occuring in them.

    Read the respective classes documentations to learn what events are provided by them. The
    events are always defined in a class whose name consist of the name of the class it provides
    events for followed by C{Events}). For example class L{ISkype} provides events defined in
    L{ISkypeEvents}. The events class is always defined in the same submodule as the main class.

    The events class is just informative. It tells you what events you can assign your event
    handlers to, when do they occur and what arguments lists should your event handlers
    accept.

    There are three ways of attaching an event handler to an event.

      1. C{Events} object.

         Use this method if you need to attach many event handlers to many events.

         Write your event handlers as methods of a class. The superclass of your class
         doesn't matter, Skype4Py will just look for methods with apropriate names.
         The names of the methods and their arguments lists can be found in respective
         events classes (see above).

         Pass an instance of this class as the C{Events} argument to the constructor of
         a class whose events you are interested in. For example::

             import Skype4Py

             class MySkypeEvents:
                 def UserStatus(self, Status):
                     print 'The status of the user changed'

             skype = Skype4Py.Skype(Events=MySkypeEvents())

         The C{UserStatus} method will be called when the status of the user currently logged
         into skype is changed.

      2. C{On...} properties.

         This method lets you use any callables as event handlers. Simply assign them to C{On...}
         properties (where "C{...}" is the name of the event) of the object whose events you are
         interested in. For example::

             import Skype4Py

             def user_status(Status):
                 print 'The status of the user changed'

             skype = Skype4Py.Skype()
             skype.OnUserStatus = user_status

         The C{user_status} function will be called when the status of the user currently logged
         into skype is changed.

         The names of the events and their arguments lists should be taken from respective events
         classes (see above). Note that there is no C{self} argument (which can be seen in the events
         classes) simply because our event handler is a function, not a method.

      3. C{RegisterEventHandler} / C{UnregisterEventHandler} methods.

         This method, like the second one, also let you use any callables as event handlers. However,
         it additionally let you assign many event handlers to a single event.

         In this case, you use L{RegisterEventHandler} and L{UnregisterEventHandler} methods
         of the object whose events you are interested in. For example::

             import Skype4Py

             def user_status(Status):
                 print 'The status of the user changed'

             skype = Skype4Py.Skype()
             skype.RegisterEventHandler('UserStatus', user_status)

         The C{user_status} function will be called when the status of the user currently logged
         into skype is changed.

         The names of the events and their arguments lists should be taken from respective events
         classes (see above). Note that there is no C{self} argument (which can be seen in the events
         classes) simply because our event handler is a function, not a method.

    B{Important notes!}

    The event handlers are always called on a separate thread. At any given time, there is at most
    one handling thread per event type. This means that when a lot of events of the same type are
    generated at once, handling of an event will start only after the previous one is handled.
    Handling of events of different types may happen simultaneously.

    In case of second and third method, only weak references to the event handlers are stored. This
    means that you must make sure that Skype4Py is not the only one having a reference to the callable
    or else it will be garbage collected and silently removed from Skype4Py's handlers list. On the
    other hand, it frees you from worrying about cyclic references.
    '''

    _EventNames = []

    def __init__(self):
        '''Initializes the object.
        '''
        self._EventHandlerObj = None
        self._DefaultEventHandlers = {}
        self._EventHandlers = {}
        self._EventThreads = {}

        for event in self._EventNames:
            self._EventHandlers[event] = []

    def _CallEventHandler(self, Event, *args, **kwargs):
        '''Calls all event handlers defined for given Event (str), additional parameters
        will be passed unchanged to event handlers, all event handlers are fired on
        separate threads.
        '''
        # get list of relevant handlers
        handlers = dict([(x, x()) for x in self._EventHandlers[Event]])
        if None in handlers.values():
            # cleanup
            self._EventHandlers[Event] = list([x[0] for x in handlers.items() if x[1] != None])
        handlers = filter(None, handlers.values())
        # try the On... handlers
        try:
            h = self._DefaultEventHandlers[Event]()
            if h:
                handlers.append(h)
        except KeyError:
            pass
        # try the object handlers
        try:
            handlers.append(getattr(self._EventHandlerObj, Event))
        except AttributeError:
            pass
        # if no handlers, leave
        if not handlers:
            return
        # initialize event handling thread if needed
        if Event in self._EventThreads:
            t = self._EventThreads[Event]
            t.lock.acquire()
            if not self._EventThreads[Event].isAlive():
                t = self._EventThreads[Event] = _EventHandlingThread(Event)
        else:
            t = self._EventThreads[Event] = _EventHandlingThread(Event)
        # enqueue handlers in thread
        for h in handlers:
            t.enqueue(h, args, kwargs)
        # start serial event processing
        try:
            t.lock.release()
        except:
            t.start()

    def RegisterEventHandler(self, Event, Target):
        '''Registers any callable as an event handler.

        @param Event: Name of the event. For event names, see the respective C{...Events} class.
        @type Event: str
        @param Target: Callable to register as the event handler.
        @type Target: callable
        @return: True is callable was successfully registered, False if it was already registered.
        @rtype: bool
        @see: L{EventHandlingBase}
        '''

        if not callable(Target):
            raise TypeError('%s is not callable' % repr(Target))
        if Event not in self._EventHandlers:
            raise ValueError('%s is not a valid %s event name' % (Event, self.__class__.__name__))
        # get list of relevant handlers
        handlers = dict([(x, x()) for x in self._EventHandlers[Event]])
        if None in handlers.values():
            # cleanup
            self._EventHandlers[Event] = list([x[0] for x in handlers.items() if x[1] != None])
        if Target in handlers.values():
            return False
        self._EventHandlers[Event].append(WeakCallableRef(Target))
        return True

    def UnregisterEventHandler(self, Event, Target):
        '''Unregisters a previously registered event handler (a callable).

        @param Event: Name of the event. For event names, see the respective C{...Events} class.
        @type Event: str
        @param Target: Callable to unregister.
        @type Target: callable
        @return: True if callable was successfully unregistered, False if it wasn't registered first.
        @rtype: bool
        @see: L{EventHandlingBase}
        '''

        if not callable(Target):
            raise TypeError('%s is not callable' % repr(Target))
        if Event not in self._EventHandlers:
            raise ValueError('%s is not a valid %s event name' % (Event, self.__class__.__name__))
        # get list of relevant handlers
        handlers = dict([(x, x()) for x in self._EventHandlers[Event]])
        if None in handlers.values():
            # cleanup
            self._EventHandlers[Event] = list([x[0] for x in handlers.items() if x[1] != None])
        for wref, trg in handlers.items():
            if trg == Target:
                self._EventHandlers[Event].remove(wref)
                return True
        return False

    def _SetDefaultEventHandler(self, Event, Target):
        if Target:
            if not callable(Target):
                raise TypeError('%s is not callable' % repr(Target))
            self._DefaultEventHandlers[Event] = WeakCallableRef(Target)
        else:
            try:
                del self._DefaultEventHandlers[Event]
            except KeyError:
                pass

    def _GetDefaultEventHandler(self, Event):
        try:
            return self._DefaultEventHandlers[Event]()
        except KeyError:
            pass

    def _SetEventHandlerObj(self, Obj):
        '''Registers an object (Obj) as event handler, object should contain methods with names
        corresponding to event names, only one obj is allowed at a time.
        '''
        self._EventHandlerObj = Obj

    @staticmethod
    def __AddEvents_make_event(Event):
        # TODO: rework to make compatible with cython
        return property(lambda self: self._GetDefaultEventHandler(Event),
                        lambda self, value: self._SetDefaultEventHandler(Event, value))

    @classmethod
    def _AddEvents(cls, klass):
        '''Adds events to class based on 'klass' attributes.'''
        for event in dir(klass):
            if not event.startswith('_'):
                setattr(cls, 'On%s' % event, cls.__AddEvents_make_event(event))
                cls._EventNames.append(event)


class Cached(object):
    '''Base class for all cached objects.

    Every object is identified by an Id specified as first parameter of the constructor.
    Trying to create two objects with same Id yields the same object. Uses weak references
    to allow the objects to be deleted normally.

    @warning: C{__init__()} is always called, don't use it to prevent initializing an already
    initialized object. Use C{_Init()} instead, it is called only once.
    '''
    _cache_ = weakref.WeakValueDictionary()

    def __new__(cls, Id, *args, **kwargs):
        h = cls, Id
        try:
            return cls._cache_[h]
        except KeyError:
            o = object.__new__(cls)
            cls._cache_[h] = o
            if hasattr(o, '_Init'):
                o._Init(Id, *args, **kwargs)
            return o

    def __copy__(self):
        return self
