import os


def try_os_setegid(number=None, name=None):
    # This can happen if we're calling this function with is previous result.
    if number is None and name is None:
        return None

    try:
        from grp import getgrnam

        egid = os.getegid()
        if number is None:
            number = getgrnam(name).gr_gid
        os.setegid(number)
        return egid
    except ModuleNotFoundError:
        return None
    except PermissionError:
        return None
    except KeyError:
        return None


def try_os_seteuid(number=None, name=None):
    # This can happen if we're calling this function with is previous result.
    if number is None and name is None:
        return None


    try:
        from pwd import getpwnam

        euid=os.geteuid()
        if number is None:
            number = getpwnam(name).pw_uid
        os.seteuid(number)
        return euid
    except ModuleNotFoundError:
        return None
    except PermissionError:
        return None
    except KeyError:
        return None
