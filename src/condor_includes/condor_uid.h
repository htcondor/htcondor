/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/



/*
** This module implements all process privilege switching in Condor.  No
** other code in the system should call seteuid and friends directly.
** Privilege classes are defined in the priv_state enumeration.  To
** switch to a given privilege class, call set_priv(new_priv_class).
** This call will return the privilege class in place at the time of
** the call.  The set_condor_priv(), set_user_priv(), set_user_priv_final(),
** and set_root_priv() macros are provided for convenience, which in
** turn call set_priv().  Either init_user_ids() or set_user_ids() should be
** called before set_user_priv() or set_user_priv_final().
**
** Privilege classes designated as "final" set real uid/gid so that
** the process can not later switch to another privilege class.
**
** The display_priv_log() function writes a log of the recent privilege
** class switches using dprintf().
*/

#ifndef _UID_H
#define _UID_H

#if defined(__cplusplus)
extern "C" {
#endif

/*
  Our code depends on _priv_state_threshold being last, so if you add
  another priv state here, put it *BEFORE* _priv_state_threshold!!!!
*/
typedef enum {
	PRIV_UNKNOWN,
	PRIV_ROOT,
	PRIV_CONDOR,
	PRIV_USER,
	PRIV_USER_FINAL,
	_priv_state_threshold
} priv_state;

#define set_priv(s)	_set_priv(s, __FILE__, __LINE__, 1)
#define set_condor_priv() _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 1)
#define set_user_priv()	_set_priv(PRIV_USER, __FILE__, __LINE__, 1)
#define set_user_priv_final() _set_priv(PRIV_USER_FINAL, __FILE__, __LINE__, 1)
#define set_root_priv()	_set_priv(PRIV_ROOT, __FILE__, __LINE__, 1)

#ifdef WIN32
typedef int uid_t;
typedef int gid_t;
HANDLE priv_state_get_handle();
void init_user_nobody_loginname(const char *);
const char *get_user_nobody_loginname();
#endif

void _condor_disable_uid_switching();
void init_condor_ids();
void uninit_user_ids();
int init_user_ids(const char username[]);
int init_user_ids_quiet(const char username[]);
int set_user_ids(uid_t uid, gid_t gid);
int set_user_ids_quiet(uid_t uid, gid_t gid);
priv_state _set_priv(priv_state s, char file[], int line, int dologging);
uid_t get_my_uid();
gid_t get_my_gid();
priv_state get_priv();
const char* priv_to_string( priv_state s );

#if !defined(WIN32)
uid_t get_condor_uid();
gid_t get_condor_gid();
uid_t get_user_uid();
gid_t get_user_gid();
uid_t get_real_condor_uid();
gid_t get_real_condor_gid();
#else
uid_t getuid(); /* getuid stub for WINNT */
#endif

int is_root( void );

const char* get_condor_username();
void display_priv_log();

#if defined(__cplusplus)
}
#endif

#endif /* _UID_H */
