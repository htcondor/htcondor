#ifndef _CONDOR_DAEMON_TYPES_H
#define _CONDOR_DAEMON_TYPES_H

enum daemonType { DT_NONE, DT_MASTER, DT_SCHEDD, DT_STARTD,
				  DT_COLLECTOR, DT_NEGOTIATOR, DT_KBDD, 
				  DT_DAGMAN };

#ifdef __cplusplus
extern "C" {
#endif

const char* daemon_string( daemonType dt );

#ifdef __cplusplus
}
#endif


#endif /* _CONDOR_DAEMON_TYPES_H */
