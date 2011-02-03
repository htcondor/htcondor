#
# Skype4Py
#

'''
Skype4Py is a multiplatform Skype API wrapper for Python.

  1. Usage.

     C{Skype4Py} is the package that you should import in your scripts to be able to access Skype.
     You won't need to import any submodules. Everything you may need will be available at the
     package level. This includes:

       - Classes
         - C{Skype4Py.Skype = L{Skype4Py.skype.ISkype}}
         - C{Skype4Py.CallChannelManager = L{Skype4Py.callchannel.ICallChannelManager}}
       - Constants
         - C{Skype4Py.* = L{Skype4Py.enums}.*}
       - Errors
         - C{Skype4Py.SkypeError = L{Skype4Py.errors.ISkypeError}}
         - C{Skype4Py.SkypeAPIError = L{Skype4Py.errors.ISkypeAPIError}}

     The first two are the only classes that you will be instantiating directly. Calling their methods/properties
     will give you the access to instances of all other classes, you won't have to instantiate them yourself.
     The two classes are also the only ones that provide event handlers (for more information about events,
     see the L{EventHandlingBase} class which is a baseclass of the above two classes).

     Every Skype4Py script instatinates the C{Skype4Py.Skype} class at least once. That's what you want to do
     first in your script. Then follow the L{Skype4Py.skype.ISkype} reference to see where you can get from
     there.

  2. Quick example.

     This short example connects to Skype client and prints the user's full name and the names of all the
     contacts from the contacts list::

         import Skype4Py

         # Create Skype instance
         skype = Skype4Py.Skype()

         # Connect Skype object to Skype client
         skype.Attach()

         print 'Your full name:', skype.CurrentUser.FullName
         print 'Your contacts:'
         for user in skype.Friends:
             print '    ', user.FullName

@author: Arkadiusz Wahlig (arkadiusz.wahlig at googlemail)
@requires: Python 2.4 or newer (not yet 3.0)
@see: U{The Skype4Py webpage<https://developer.skype.com/wiki/Skype4Py>}
@license: BSD License (see the accompanying LICENSE file for more information)
@copyright: S{copy} 2007-2008 Arkadiusz Wahlig
'''

from skype import ISkype as Skype
from callchannel import ICallChannelManager as CallChannelManager
from errors import ISkypeError as SkypeError, ISkypeAPIError as SkypeAPIError
from enums import *


__version__ = '1.0.31.0'
'''The version of Skype4Py.'''

