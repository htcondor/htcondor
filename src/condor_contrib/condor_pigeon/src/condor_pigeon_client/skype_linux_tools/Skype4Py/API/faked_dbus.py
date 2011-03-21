'''
This faked module is used while building docs on windows
where dbus package isn't available.
'''

class dbus(object):
    class service(object):
        class Object(object):
            pass
        @staticmethod
        def method(*args, **kwargs):
            return lambda *args, **kwargs: None
