'''Error classes.
'''

class ISkypeAPIError(Exception):
    '''Exception raised whenever there is a problem with connection between Skype4Py and Skype client.
    It can be subscripted in which case following information can be obtained::

        Index | Meaning
        ------+-------------------------------------------------------------------------------
            0 | (unicode) Error description string.
    '''

    def __init__(self, errstr):
        '''__init__.

        @param errstr: Error description.
        @type errstr: unicode
        '''
        Exception.__init__(self, str(errstr))


class ISkypeError(Exception):
    '''Raised whenever Skype client reports an error back to Skype4Py. It can be subscripted in which
    case following information can be obtained::

        Index | Meaning
        ------+-------------------------------------------------------------------------------
            0 | (int) Error number. See below.
            1 | (unicode) Error description string.

    See U{Error codes<https://developer.skype.com/Docs/ApiDoc/Error_codes>} for more information
    about Skype error code numbers. Additionally error code 0 can be raised by Skype4Py itself.
    '''

    def __init__(self, errno, errstr):
        '''__init__.

        @param errno: Error number.
        @type errno: int
        @param errstr: Error description.
        @type errstr: unicode
        '''
        Exception.__init__(self, int(errno), str(errstr))

    def __str__(self):
        return '[Errno %d] %s' % (self[0], self[1])
