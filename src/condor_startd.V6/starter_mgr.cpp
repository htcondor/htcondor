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
  ! ! ! BEWARE ! ! !

  The ShadowMgr object in the schedd is based pretty heavily on this
  object.  There was no easy way to share the code via common base
  classes, and the Starter and Shadow objects are very different from
  each other.  However, most of the code in
  condor_schedd.V6/shadow_mgr.[Ch] was lifted from here.  If you
  change anything in here, you should check in those two files, too,
  and make sure you don't need to make the same change on that side.

  ! ! ! BEWARE ! ! !
*/


#include "condor_common.h"
#include "startd.h"
#include "my_popen.h"


StarterMgr::StarterMgr() : _haveStandardUni(false)
{
}


StarterMgr::~StarterMgr()
{
	Starter* tmp_starter;
	starters.Rewind();
	while( starters.Next(tmp_starter) ) {
		delete( tmp_starter );
		starters.DeleteCurrent();
	}
}


void
StarterMgr::init( void )
{
	static bool did_warning = false;
	StringList starter_list;
	StringList checked_starter_list;
	Starter* tmp_starter;
	starters.Rewind();
	while( starters.Next(tmp_starter) ) {
		delete( tmp_starter );
		starters.DeleteCurrent();
	}

	char *tmp, *starter_path = NULL;
	tmp = param( "STARTER_LIST" );
	if( ! tmp ) {
			// Not defined, give them a default
		tmp = strdup( "STARTER, STARTER_1, STARTER_2, "
					  "ALTERNATE_STARTER_1, ALTERNATE_STARTER_2" );
		if( ! did_warning ) {
			dprintf(D_ALWAYS,
					"WARNING: STARTER_LIST not defined in config file, "
					"using default: %s\n", tmp);
			did_warning = true;
		}
	}
	starter_list.initializeFromString( tmp );
	free( tmp );

	starter_list.rewind();
	while( (tmp = starter_list.next()) ) {
		starter_path = param( tmp );
		if( ! starter_path ) {
			dprintf( D_ALWAYS, "Starter specified in STARTER_LIST "
					 "\"%s\" not found in config file, ignoring.\n",
					 tmp ); 
			continue;
		}
		if( checked_starter_list.contains(starter_path) ) {
			dprintf( D_ALWAYS, "Starter pointed to by \"%s\" (%s) is "
					 "in STARTER_LIST more than once, ignoring.\n", 
					 tmp, starter_path );
			free(starter_path);
			continue;
		}
		
			// now, we've got a path to a binary we want to use as a
			// starter.  try to run it with a -classad option and grab
			// the output (which should be a classad), and construct
			// the appropriate Starter object for it.
		tmp_starter = registerStarter( starter_path );
		if( tmp_starter ) {
			starters.Append( tmp_starter );
		}
			// record the fact that we've already considered this
			// starter, even if it failed to give us a classad. 
		checked_starter_list.append( starter_path );
		free( starter_path );
	}

	if( starters.IsEmpty() ) {
		dprintf(D_ALWAYS,"WARNING WARNING WARNING: No valid starters were found!  Is something wrong with your Condor installation?  This startd will not be able to run jobs.\n");
	}
}


void
StarterMgr::printStarterInfo( int debug_level )
{
	Starter *tmp_starter;
	starters.Rewind();
	while( starters.Next(tmp_starter) ) {
		tmp_starter->printInfo( debug_level );
	}
}


void
StarterMgr::publish(ClassAd* ad)  // all of this should be IS_STATIC()
{
	Starter *tmp_starter;
	StringList ability_list;
	starters.Rewind();
	while( starters.Next(tmp_starter) ) {
		tmp_starter->publish( ad, &ability_list );
	}
		// finally, print out all the abilities we added into the
		// classad so that other folks can know what we did.
	char* ability_str = ability_list.print_to_string();

	// If our ability list is NULL it means that we have no starters.
	// This is ok for hawkeye; nothing more to do here!
	if ( NULL == ability_str ) {
		ability_str = strdup("\0");
	}
	ASSERT(ability_str);
	ad->Assign( ATTR_STARTER_ABILITY_LIST, ability_str );
	free( ability_str );
}


Starter*
StarterMgr::newStarter( ClassAd* job_ad, ClassAd* mach_ad, bool &no_starter,
						 int starter_num ) 
{
	Starter *new_starter, *tmp_starter;

	no_starter = true;

	starters.Rewind();
	while( starters.Next(tmp_starter) ) {
		switch( starter_num ) {
		case -1:
				// no type specified, just use the job_ad directly
			break;
		case 0:
				// non-dc standard/vanilla starter
			if( tmp_starter->is_dc() ||
				!(tmp_starter->provides(ATTR_HAS_REMOTE_SYSCALLS)) ) {
				dprintf( D_FULLDEBUG, 
						 "Job wants old RSC/Ckpt starter, skipping %s\n",
						 tmp_starter->path() );
				continue;
			}
			break;
		case 1:
				// non-dc pvm starter
			dprintf( D_ALWAYS, "ERROR: PVM starter is not supported, "
					 "returning failure\n" );
			return NULL;
			break;
		case 2:
				// dc starter (mpi, java, etc)
			if( ! tmp_starter->is_dc() ) {
				dprintf( D_FULLDEBUG, 
						 "Job wants DaemonCore starter, skipping %s\n",
						 tmp_starter->path() );
				continue;
			}
			break;
		default:
			dprintf( D_ALWAYS, "ERROR: unknown starter type (%d) in "
					 "StarterMgr::newStarter(), returning failure\n",
					 starter_num );
			return NULL;
			break;
		}

		no_starter = false;

		if( tmp_starter->satisfies(job_ad, mach_ad) ) {
			new_starter = new Starter( *tmp_starter );
			return new_starter;
		}
	}
	return NULL;
}


Starter*
StarterMgr::registerStarter( const char* path )
{
	Starter* new_starter;
	FILE* fp;
	const char *args[] = { path,
					 "-classad",
					 NULL };
	char buf[1024];

		// first, try to execute the given path with a "-classad"
		// option, and grab the output as a ClassAd
		// note we run the starter here as root if possible,
		// since that is how the starter will be invoked for real,
		// and the real uid of the starter may influence the
		// list of capabilities the "-classad" option returns.
	{
		TemporaryPrivSentry sentry(PRIV_ROOT);
		fp = my_popenv( args, "r", FALSE );
	}

	if( ! fp ) {
		dprintf( D_ALWAYS, "Failed to execute %s, ignoring\n", path );
		return NULL;
	}
	ClassAd* ad = new ClassAd;
	bool read_something = false;
	while( fgets(buf, 1024, fp) ) {
		read_something = true;
		if( ! ad->Insert(buf) ) {
			dprintf( D_ALWAYS, "Failed to insert \"%s\" into ClassAd, "
					 "ignoring invalid starter\n", buf );
			delete( ad );
			pclose( fp );
			return NULL;
		}
	}
	my_pclose( fp );
	if( ! read_something ) {
		dprintf( D_ALWAYS, 
				 "\"%s -classad\" did not produce any output, ignoring\n", 
				 path ); 
		delete( ad );
		return NULL;
	}

	new_starter = new Starter();
	new_starter->setAd( ad );
	new_starter->setPath( path );
	bool is_dc = false;
	ad->LookupBool( ATTR_IS_DAEMON_CORE, is_dc );
	new_starter->setIsDC( is_dc );

	bool has_checkpoint = false;
	ad->LookupBool( ATTR_HAS_CHECKPOINTING, has_checkpoint);
	if (has_checkpoint) {
		_haveStandardUni = true;
	}

	return new_starter;
}


