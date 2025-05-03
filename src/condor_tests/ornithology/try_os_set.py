import os

from pwd import getpwnam
from grp import getgrnam


def try_os_setegid(number=None, name=None):
    try:
        egid = os.getegid()
        if number is None:
            number = getgrnam(name).gr_gid
        os.setegid(number)
        return egid
    except PermissionError:
        return None
    except KeyError:
        return None


def try_os_seteuid(number=None, name=None):
    try:
        euid=os.geteuid()
        if number is None:
            number = getpwnam(name).pw_uid
        os.seteuid(number)
        return euid
    except PermissionError:
        return None
    except KeyError:
        return None
