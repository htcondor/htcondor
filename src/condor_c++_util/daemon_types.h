#ifndef _CONDOR_DAEMON_TYPES_H
#define _CONDOR_DAEMON_TYPES_H

enum daemonType {MASTER, SCHEDD, STARTD, COLLECTOR, NEGOTIATOR, KBDD};

#ifdef __cplusplus
extern "C" {
#endif

const char* daemon_string( daemonType dt );

#ifdef __cplusplus
}
#endif


#endif /* _CONDOR_DAEMON_TYPES_H */
