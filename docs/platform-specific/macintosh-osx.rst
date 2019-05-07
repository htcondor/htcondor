Macintosh OS X
==============

:index:`Macintosh OS X<single: Macintosh OS X; platform-specific information>`

This section provides information specific to the Macintosh OS X port of
HTCondor. The Macintosh port of HTCondor is more accurately a port of
HTCondor to Darwin, the BSD core of OS X. HTCondor uses the Carbon
library only to detect keyboard activity, and it does not use Cocoa at
all. HTCondor on the Macintosh is a relatively new port, and it is not
yet well-integrated into the Macintosh environment.

HTCondor on the Macintosh has a few shortcomings:

-  Users connected to the Macintosh via *ssh* are not noticed for
   console activity.
-  The memory size of threaded programs is reported incorrectly.
-  No Macintosh-based installer is provided.
-  The example start up scripts do not follow Macintosh conventions.
-  Kerberos is not supported.
