/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/




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

/*
  Our code depends on _priv_state_threshold being last, so if you add
  another priv state here, put it *BEFORE* _priv_state_threshold!!!!
*/
typedef enum {
	PRIV_UNKNOWN,
	PRIV_ROOT,
	PRIV_CONDOR,
	PRIV_CONDOR_FINAL,
	PRIV_USER,
	PRIV_USER_FINAL,
	PRIV_FILE_OWNER,
	_priv_state_threshold
} priv_state;

// special flag passed as dologging value to _set_priv()
#define NO_PRIV_MEMORY_CHANGES 999

#define set_priv(s)	_set_priv(s, __FILE__, __LINE__, 1)
#define set_priv_no_memory_changes(s) _set_priv(s, __FILE__, __LINE__, NO_PRIV_MEMORY_CHANGES)
#define set_condor_priv() _set_priv(PRIV_CONDOR, __FILE__, __LINE__, 1)
#define set_condor_priv_final() _set_priv(PRIV_CONDOR_FINAL, __FILE__, __LINE__, 1)
#define set_user_priv()	_set_priv(PRIV_USER, __FILE__, __LINE__, 1)
#define set_user_priv_final() _set_priv(PRIV_USER_FINAL, __FILE__, __LINE__, 1)
#define set_file_owner_priv() _set_priv(PRIV_FILE_OWNER, __FILE__, __LINE__, 1)
#define set_root_priv()	_set_priv(PRIV_ROOT, __FILE__, __LINE__, 1)

#ifdef WIN32
typedef int uid_t;
typedef int gid_t;
HANDLE priv_state_get_handle();
#else
#include <sys/types.h>
#endif

const char *get_user_loginname(void);
#ifdef WIN32
const char *get_user_domainname(void);
#endif
int can_switch_ids( void );
int set_priv_initialize(void); // called by deamonCore to conditionally enable priv switching, returns TRUE if switching is enabled
void clear_passwd_cache(void);
void delete_passwd_cache(void);
void init_condor_ids(void);
void uninit_user_ids(void);
void uninit_file_owner_ids(void);
int set_file_owner_ids( uid_t uid, gid_t gid );
bool user_ids_are_inited();
int init_user_ids(const char username[], const char domain[]);
int init_user_ids_quiet(const char username[]);
int set_user_ids(uid_t uid, gid_t gid);
int set_user_ids_quiet(uid_t uid, gid_t gid);
void set_user_tracking_gid(gid_t);
void unset_user_tracking_gid(void);
priv_state _set_priv(priv_state s, const char *file, int line, int dologging);
priv_state get_priv_state(void);
uid_t get_my_uid(void);
gid_t get_my_gid(void);
priv_state get_priv(void);
const char* priv_to_string( priv_state s );
const char* priv_identifier( priv_state s );


#if !defined(WIN32)
uid_t get_condor_uid(void);
gid_t get_condor_gid(void);
// returns true if condor uid is already known and sets uid and gid
bool get_condor_uid_if_inited(uid_t &uid,gid_t &gid);
uid_t get_user_uid(void);
gid_t get_user_gid(void);
uid_t get_file_owner_uid(void);
gid_t get_file_owner_gid(void);
uid_t get_real_condor_uid(void);
gid_t get_real_condor_gid(void);
#else
uid_t getuid( void ); /* getuid stub for WINNT */
#endif


int is_root( void );

const char* get_real_username(void);
const char* get_condor_username(void);
void display_priv_log(void);

typedef enum {
   COMPARE_DOMAIN_DEFAULT=0,  // use default flags options for username compare
   COMPARE_IGNORE_DOMAIN,    // compare users only, ingnore domains
   COMPARE_DOMAIN_PREFIX,    // compare users & domains, allow one domain to be a prefix of the other
   COMPARE_DOMAIN_FULL,      // usernames and domains must be the same (domain is case-insensitive)
   DOMAIN_MASK = 0x0F,      // mask to strip compare type from options
   ASSUME_UID_DOMAIN = 0x10, // if this bit is set, assume UID_DOMAIN when domain is unspecified.
   CASELESS_USER = 0x20,     // if this bit is set, do case-insensitive comparison of user (domain is always caseless)
   } CompareUsersOpt;
bool is_same_user(const char user1[], const char user2[], CompareUsersOpt opt, const char * uid_domain);
bool is_same_domain(const char * domain1, const char * domain2, CompareUsersOpt opt, const char * uid_domain);

// given a bare user name "bob" or fully-qualified user name "bob@domain" return "bob". buffer used only if needed.
const char * name_of_user(const char user[], std::string & buf);
// given a bare username "bob" or fully-qualified user name "bob@domain" return "domain" or def_domain
const char * domain_of_user(const char user[], const char * def_domain);


#if ! defined WIN32
#include "passwd_cache.unix.h"
extern passwd_cache* pcache(void);
#endif

// An object that automatically returns the previous privilege level when destroyed
// Optionally, also undo user id initialization.
class TemporaryPrivSentry {

public:
	TemporaryPrivSentry() : TemporaryPrivSentry(!user_ids_are_inited()) {}

	TemporaryPrivSentry(bool clear_user_ids) {
		m_orig_state = get_priv_state();
		m_clear_user_ids = clear_user_ids;
	}

	TemporaryPrivSentry(priv_state dest_state) : TemporaryPrivSentry(dest_state, !user_ids_are_inited()) {}

	TemporaryPrivSentry(priv_state dest_state, bool clear_user_ids) {
		m_orig_state = set_priv(dest_state);
		m_clear_user_ids = clear_user_ids;
	}

	~TemporaryPrivSentry() {
		if (m_orig_state != PRIV_UNKNOWN) {
			set_priv(m_orig_state);
		}
		if (m_clear_user_ids) {
			uninit_user_ids();
		}
	}

	priv_state orig_priv() const {return m_orig_state;}

private:
	// non-copyable.
	TemporaryPrivSentry(const TemporaryPrivSentry&);
	TemporaryPrivSentry& operator=(const TemporaryPrivSentry&);

	priv_state m_orig_state;
	bool m_clear_user_ids;
};

#endif /* _UID_H */
