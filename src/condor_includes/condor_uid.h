#ifndef _UID_H
#define _UID_H

#include <sys/types.h>

#if defined(__cplusplus)
extern "C" {
#endif

int set_fatal_uid_sets(int flag);
uid_t get_user_uid(char *user_name);
int init_condor_uid();
int get_condor_uid();
int set_condor_euid();
int set_condor_ruid();
int set_root_euid();
int set_effective_uid(uid_t new_uid);

#if defined(__cplusplus)
}
#endif

#endif /* _UID_H */
