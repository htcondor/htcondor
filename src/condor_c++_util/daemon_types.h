#ifndef _CONDOR_DAEMON_TYPES_H
#define _CONDOR_DAEMON_TYPES_H

enum daemon_t { DT_NONE, DT_MASTER, DT_SCHEDD, DT_STARTD,
				  DT_COLLECTOR, DT_NEGOTIATOR, DT_KBDD, 
				  DT_DAGMAN, DT_VIEW_COLLECTOR, _dt_threshold_ };

#ifdef __cplusplus
extern "C" {
#endif

const char* daemonString( daemon_t dt );
daemon_t stringToDaemonType( char* name );

#ifdef __cplusplus
}
#endif


#endif /* _CONDOR_DAEMON_TYPES_H */
