#ifndef _UID_H
#define _UID_H

#include "_condor_fix_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum { PRIV_UNKNOWN,
			   PRIV_ROOT,
			   PRIV_CONDOR,
			   PRIV_USER,
			   PRIV_USER_FINAL
} priv_state;

void init_condor_ids();
void init_user_ids(char username[]);
void set_user_ids(uid_t uid, gid_t gid);
priv_state set_priv(priv_state s);
priv_state set_condor_priv();
priv_state set_user_priv();
void set_user_priv_final();
priv_state set_root_priv();
uid_t get_condor_uid();
gid_t get_condor_gid();
uid_t get_user_uid();
gid_t get_user_gid();


#if defined(__cplusplus)
}
#endif

#endif /* _UID_H */
