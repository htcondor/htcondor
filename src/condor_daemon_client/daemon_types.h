#ifndef _CONDOR_DAEMON_TYPES_H
#define _CONDOR_DAEMON_TYPES_H


// if you add another type to this list, make sure to edit
// daemon_types.C and add the string equivilant.

enum daemon_t { DT_NONE, DT_ANY,  DT_MASTER, DT_SCHEDD, DT_STARTD,
				DT_COLLECTOR, DT_NEGOTIATOR, DT_KBDD, 
				DT_DAGMAN, DT_VIEW_COLLECTOR, DT_CLUSTER,  
				DT_SHADOW, DT_STARTER, 
				_dt_threshold_ };

#ifdef __cplusplus
extern "C" {
#endif

const char* daemonString( daemon_t dt );
daemon_t stringToDaemonType( char* name );

#ifdef __cplusplus
}
#endif


#endif /* _CONDOR_DAEMON_TYPES_H */
