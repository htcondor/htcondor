/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef _CONDOR_ENVIRON_H
#define _CONDOR_ENVIRON_H

#include "condor_common.h"
#include "condor_auth_x509.h"

// ---------------------------------------------------------------
// Ok.  Here's how you add a new environment variable:
//  1 -	Add an enum to the CONDOR_ENVIRON enumeration
//  2 -	Add it to the CondorEnvironList array (at the end, duh)
//		If you intend to have the string CONDOR in the name anywhere,
//		you should set the flag to ENVIRON_FLAG_DISTRO and put a "%s"
//		where you want the substitution to take place.  If we later have
//		a need for more magic strings, we'll create new flags.  In this
//		case, condor_environ.C will need to be modified, as well.

// Enumerated list of environment variables
typedef enum
{
	ENV_INHERIT = 0,
	ENV_STACKSIZE,
	ENV_CORESIZE,
	ENV_GZIP,
	ENV_PVM_ROOT,
	ENV_UG_IDS,
	ENV_PATH,
	ENV_ID,
	ENV_SCHEDD_NAME,
	ENV_CORE_SIZE,
	ENV_NOCHECK,
	ENV_PARALLEL_SHADOW_SINFUL,
	ENV_LOWPORT,
	ENV_HIGHPORT,
	ENV_CONFIG,
	ENV_CONFIG_ROOT,
	ENV_RENDEZVOUS,
    ENV_DAEMON_DEATHTIME,
	ENV_PARENT_ID,
	// ....
} CONDOR_ENVIRON;

// Prototypes
int EnvInit( void );
const char *EnvGetName( CONDOR_ENVIRON );


// ------------------------------------------------------
// Stuff private to the environment variable manager
// ------------------------------------------------------
#if defined _CONDOR_ENVIRON_MAIN

// Flags available
typedef enum
{
	ENV_FLAG_NONE = 0,			// No special treatment
	ENV_FLAG_DISTRO,			// Plug in the distribution name
	ENV_FLAG_DISTRO_UC,			// Plug in the UPPER CASE distribution name
} CONDOR_ENVIRON_FLAGS;

// Data on each env variable
typedef struct
{
	CONDOR_ENVIRON			sanity;		// Used to sanity check
	const char				*string;	// My format string
	CONDOR_ENVIRON_FLAGS	flag;		// Flags
	const char				*cached;	// Cached answer
} CONDOR_ENVIRON_ELEM;

// The actual list of variables indexed by CONDOR_ENVIRON
static CONDOR_ENVIRON_ELEM CondorEnvironList[] =
{
	{ ENV_INHERIT,			"%s_INHERIT",			ENV_FLAG_DISTRO_UC, NULL },
	{ ENV_STACKSIZE,		"%s_STACK_SIZE",		ENV_FLAG_DISTRO_UC, NULL },
	{ ENV_CORESIZE,			"%s_CORESIZE",			ENV_FLAG_DISTRO_UC, NULL },
	{ ENV_GZIP,				"GZIP",					ENV_FLAG_NONE, NULL },
	{ ENV_PVM_ROOT,			"PVM_ROOT",				ENV_FLAG_NONE, NULL },
	{ ENV_UG_IDS,			"%s_IDS",				ENV_FLAG_DISTRO_UC, NULL },
	{ ENV_PATH,				"PATH",					ENV_FLAG_NONE, NULL },
	{ ENV_ID,				"%s_ID",				ENV_FLAG_DISTRO_UC, NULL },
	{ ENV_SCHEDD_NAME,		"SCHEDD_NAME",			ENV_FLAG_NONE, NULL },
	{ ENV_CORE_SIZE,		"%s_CORE_SIZE",			ENV_FLAG_DISTRO_UC, NULL },
	{ ENV_NOCHECK,			"_%s_NOCHECK",			ENV_FLAG_DISTRO_UC, NULL },
	{ ENV_PARALLEL_SHADOW_SINFUL,"PARALLEL_SHADOW_SINFUL",ENV_FLAG_NONE, NULL },
	{ ENV_LOWPORT,			"_%s_LOWPORT",			ENV_FLAG_DISTRO, NULL },
	{ ENV_HIGHPORT,			"_%s_HIGHPORT",			ENV_FLAG_DISTRO, NULL },
	{ ENV_CONFIG,			"%s_CONFIG",			ENV_FLAG_DISTRO_UC, NULL },
	{ ENV_CONFIG_ROOT,		"%s_CONFIG_ROOT",		ENV_FLAG_DISTRO_UC, NULL },
	{ ENV_RENDEZVOUS,		"RENDEZVOUS_DIRECTORY", ENV_FLAG_NONE, NULL },
	{ ENV_DAEMON_DEATHTIME,	"DAEMON_DEATHTIME",     ENV_FLAG_NONE, NULL },
	{ ENV_PARENT_ID,		"%s_PARENT_ID",			ENV_FLAG_DISTRO_UC, NULL },
};
#endif		// _CONDOR_ENV_MAIN

#endif /* _CONDOR_ENVIRON_H */
