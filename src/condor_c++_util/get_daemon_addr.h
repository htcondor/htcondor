#ifndef CONDOR_GET_DAEMON_ADDR_H
#define CONDOR_GET_DAEMON_ADDR_H

#ifdef __cplusplus
extern "C" {
#endif

extern char* get_schedd_addr(const char* name);
extern char* get_startd_addr(const char* name);
extern char* get_master_addr(const char* name);
extern char* get_daemon_name(const char* name);
extern const char* get_host_part(const char* name);

#ifdef __cplusplus
}
#endif

#endif /* CONDOR_GET_DAEMON_ADDR_H */
