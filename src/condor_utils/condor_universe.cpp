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


#include "condor_common.h"
#include "condor_debug.h"
#include "condor_universe.h"
#include "MyString.h"
#include "tokener.h"

/*
Warning: This table should stay in sync
with the symbols in condor_universe.h
*/

typedef struct {
	const char	*uc;
	const char	*ucfirst;
	unsigned int flags;
} UniverseName;

// values for universe flags
#define OBSOLETE_UNI  0x0001
#define CAN_RECONNECT 0x0002
#define HAS_TOPPINGS  0x0004

static const UniverseName names [] =
{
	{ "NULL",      "NULL",      0 },
	{ "STANDARD",  "Standard",  0 },
	{ "PIPE",      "Pipe",      OBSOLETE_UNI },
	{ "LINDA",     "Linda",     OBSOLETE_UNI },
	{ "PVM",       "PVM",       0 },
	{ "VANILLA",   "Vanilla",   CAN_RECONNECT | HAS_TOPPINGS },
	{ "PVMD",      "PVMD",      0 },
	{ "SCHEDULER", "Scheduler", 0 },
	{ "MPI",       "MPI",       0 },
	{ "GRID",      "Grid",      0 },
	{ "JAVA",      "Java",      CAN_RECONNECT },
	{ "PARALLEL",  "Parallel",  CAN_RECONNECT },
	{ "LOCAL",     "Local",     0  },
	{ "VM",        "VM",        CAN_RECONNECT },
};


const char*
CondorUniverseName( int u )
{
	if( u<=CONDOR_UNIVERSE_MIN || u>=CONDOR_UNIVERSE_MAX ) {
		return "UNKNOWN";
	} else {
		return names[u].uc;
	}
}

const char*
CondorUniverseNameUcFirst( int u )
{
	if( u<=CONDOR_UNIVERSE_MIN || u>=CONDOR_UNIVERSE_MAX ) {
		return "Unknown";
	} else {
		return names[u].ucfirst;
	}
}

bool
universeCanReconnect( int universe )
{
	if( universe <= CONDOR_UNIVERSE_MIN || universe >= CONDOR_UNIVERSE_MAX ) {
		EXCEPT( "Unknown universe (%d) in universeCanReconnect()", universe );
	} else {
		return (names[universe].flags & CAN_RECONNECT) != 0;
	}
}

// toppings on vanilla universe
static const UniverseName toppings [] = {
	{ "NONE", "None", 0 },
	{ "DOCKER", "Docker", 0 },
};

const char *CondorUniverseOrToppingName( int universe, int topping )
{
	if( universe <= CONDOR_UNIVERSE_MIN || universe >= CONDOR_UNIVERSE_MAX ) {
		return "Unknown";
	}
	if (topping > 0 && (names[universe].flags & HAS_TOPPINGS)) {
		if (topping >= (int)COUNTOF(toppings)) {
			return "Unknown";
		}
		return toppings[topping].ucfirst;
	}
	return names[universe].ucfirst;
}

// do lookups of universe by name
// 
typedef struct {
	const char * key;
	char  universe_id;
	char  topping_id;
} UniverseInfo;

// Do some mappings
#define CONDOR_TOPPING_DOCKER 1

// Lookup table for universe name to Id.
// MUST BE SORTED BY FIRST FIELD (case insensitively)
#define UNI(u) { #u, CONDOR_UNIVERSE_##u, 0 }
#define TOP(t, u) { #t, CONDOR_UNIVERSE_##u, CONDOR_TOPPING_##t }
static const UniverseInfo UniverseItems[] = {
	TOP(DOCKER, VANILLA),
	UNI(GRID),
	UNI(JAVA),
	UNI(LINDA),
	UNI(LOCAL),
	UNI(MPI),
	UNI(PARALLEL),
	UNI(PIPE),
	UNI(PVM),
	UNI(PVMD),
	UNI(SCHEDULER),
	UNI(STANDARD),
	UNI(VANILLA),
	UNI(VM),
};
#undef UNI
#undef TOP
static const nocase_sorted_tokener_lookup_table<UniverseInfo> Universes = SORTED_TOKENER_TABLE(UniverseItems);

int
CondorUniverseNumber( const char* univ )
{
	if( ! univ ) {
		return 0;
	}

	const UniverseInfo * puni = Universes.lookup(univ);
	if ( ! puni) {
		return 0;
	}
	// don't match toppings in this function
	if (puni->topping_id) {
		return 0;
	}

	return puni->universe_id;
}

int
CondorUniverseInfo( const char* univ, int * topping_id, int * is_obsolete )
{
	if( ! univ ) {
		return 0;
	}

	const UniverseInfo * puni = Universes.lookup(univ);
	if ( ! puni) {
		return 0;
	}
	if (is_obsolete) *is_obsolete = names[(int)(puni->universe_id)].flags & OBSOLETE_UNI;
	if (topping_id) *topping_id = puni->topping_id;
	return puni->universe_id;
}
