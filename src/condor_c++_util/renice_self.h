#ifndef CONDOR_RENICE_SELF_H
#define CONDOR_RENICE_SELF_H

#ifdef __cplusplus
extern "C" {
#endif

/* Note:  this file is now only referenced by the starter.V5 and 
   starter.jim.  Once they change to DaemonCore, this function 
   can go away.  I moved the functionality into DaemonCore->
   CreateProcess(), but the person who calls CreateProcess has
   to do the param'ing themself.  -MEY 4-16-1999 */

int renice_self(char*);

#ifdef __cplusplus
}
#endif

#endif /* CONDOR_RENICE_SELF_H */
