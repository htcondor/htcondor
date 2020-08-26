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
#include "shadow_mgr.h"
#include "simplelist.h"
#include "string_list.h"
#include "condor_attributes.h"
#include "condor_config.h"
#include "classad_merge.h"
#include "my_popen.h"

/*

  ! ! ! BEWARE ! ! !

  These two objects are based pretty heavily on the Starter and
  StarterMgr objects in the startd.  There was no easy way to share
  the code via common base classes, and the Starter and Shadow objects
  are very different from each other.  However, most of the code in
  here was lifted from condor_startd.V6/Starter.C and
  condor_startd.V6/starter_mgr.C.  If you change anything in here, you
  should check in those two files, too, and make sure you don't need
  to make the same change on that side.  

  ! ! ! BEWARE ! ! !

*/


//--------------------------------------------------
// Shadow object
//--------------------------------------------------


Shadow::Shadow( const char* path_arg, ClassAd* ad )
{
	s_path = strdup( path_arg );
	s_ad = ad;
	s_is_dc = false;
	m_version_info = NULL;

	if ( s_ad ) {
		s_ad->LookupBool( ATTR_IS_DAEMON_CORE, s_is_dc );

		char* version_string = NULL;
		if (s_ad->LookupString(ATTR_VERSION, &version_string)) {
			m_version_info = new CondorVersionInfo(version_string, "SHADOW", NULL);
			free(version_string);
		}
	}
}


Shadow::Shadow( const Shadow& s )
{
	if( s.s_path ) {
		s_path = strdup( s.s_path );
	} else {
		s_path = NULL;
	}

	if( s.s_ad ) {
		s_ad = new ClassAd( *(s.s_ad) );
	} else {
		s_ad = NULL;
	}

	s_is_dc = s.s_is_dc;

	char* version_string = NULL;
	if (s_ad && s_ad->LookupString(ATTR_VERSION, &version_string)) {
		m_version_info = new CondorVersionInfo(version_string, "SHADOW", NULL);
		free(version_string);
	} else {
		m_version_info = NULL;
	}
}


Shadow::~Shadow()
{
	if( s_path ) {
		free(s_path);
	}
	if( s_ad ) {
		delete( s_ad );
	}
	if (m_version_info) {
		delete(m_version_info);
	}
}


bool
Shadow::provides( const char* ability )
{
	bool has_it = false;
	if( ! s_ad ) {
		return false;
	}
	if( ! s_ad->LookupBool(ability, has_it) ) {
		has_it = false;
	}
	return has_it;
}


void
Shadow::printInfo( int debug_level )
{
	dprintf( debug_level, "Info for \"%s\":\n", s_path );
	dprintf( debug_level | D_NOHEADER, "IsDaemonCore: %s\n", 
			 s_is_dc ? "True" : "False" );
	if( ! s_ad ) {
		dprintf( debug_level | D_NOHEADER, 
				 "No ClassAd available!\n" ); 
	} else {
		dPrintAd( debug_level, *s_ad );
	}
	dprintf( debug_level | D_NOHEADER, "*** End of shadow info ***\n" ); 
}


bool
Shadow::builtSinceVersion(int major, int minor, int sub_minor) {
	if (m_version_info) {
		return m_version_info->built_since_version(major, minor, sub_minor);
	}
		/*
		  It doesn't even have a version string, we should return
		  false and assume it's older than whatever we care about
		  (e.g. this functionality was added for 6.8.5, and we *know*
		  the shadow is returning a version string there).
		*/
	dprintf(D_ALWAYS, "ERROR: shadow classad does not contain version "
			"information, it must be REALLY OLD.  "
			"Please upgrade your shadow immediately.");
	return false;
}


//--------------------------------------------------
// ShadowMgr:: object
//--------------------------------------------------


ShadowMgr::ShadowMgr()
{
}


ShadowMgr::~ShadowMgr()
{
	Shadow* tmp_shadow;
	shadows.Rewind();
	while( shadows.Next(tmp_shadow) ) {
		delete( tmp_shadow );
		shadows.DeleteCurrent();
	}

}


void
ShadowMgr::init( void )
{
	StringList shadow_list;
	StringList checked_shadow_list;
	Shadow* tmp_shadow;
	shadows.Rewind();
	while( shadows.Next(tmp_shadow) ) {
		delete( tmp_shadow );
		shadows.DeleteCurrent();
	}

	char *tmp, *shadow_path;
	tmp = param( "SHADOW_LIST" );
	if( ! tmp ) {
			// Not defined, give them a default
		tmp = strdup( "SHADOW" );
		dprintf(D_ALWAYS,
				"WARNING: SHADOW_LIST not defined in config file, "
				"using default: %s\n", tmp);
	}
	shadow_list.initializeFromString( tmp );
	free( tmp );

	shadow_list.rewind();
	while( (tmp = shadow_list.next()) ) {
		shadow_path = param( tmp );
		if( ! shadow_path ) {
			dprintf( D_ALWAYS, "Shadow specified in SHADOW_LIST "
					 "\"%s\" not found in config file, ignoring.\n",
					 tmp ); 
			continue;
		}
		if( checked_shadow_list.contains(shadow_path) ) {
			dprintf( D_ALWAYS, "Shadow pointed to by \"%s\" (%s) is "
					 "in SHADOW_LIST more than once, ignoring.\n", 
					 tmp, shadow_path );
			free( shadow_path );
			continue;
		}
		
			// now, we've got a path to a binary we want to use as a
			// shadow.  try to run it with a -classad option and grab
			// the output (which should be a classad), and construct
			// the appropriate Shadow object for it.
		tmp_shadow = makeShadow( shadow_path );
		if( tmp_shadow ) {
			shadows.Append( tmp_shadow );
		}
			// record the fact that we've already considered this
			// shadow, even if it failed to give us a classad. 
		checked_shadow_list.append( shadow_path );
		free( shadow_path );
	}
}


Shadow*
ShadowMgr::findShadow( const char* ability )
{
	Shadow *new_shadow, *tmp_shadow;

	shadows.Rewind();
	while( shadows.Next(tmp_shadow) ) {
		if( tmp_shadow->provides(ability) ) {
			new_shadow = new Shadow( *tmp_shadow );
			return new_shadow;
		}
	}
	return NULL;
}


void
ShadowMgr::printShadowInfo( int debug_level )
{
	Shadow *tmp_shadow;
	shadows.Rewind();
	while( shadows.Next(tmp_shadow) ) {
		tmp_shadow->printInfo( debug_level );
	}
}


Shadow*
ShadowMgr::makeShadow( const char* path )
{
	Shadow* new_shadow;
	FILE* fp;
	const char *args[] = {path, "-classad", NULL};
	char buf[1024];

		// first, try to execute the given path with a "-classad"
		// option, and grab the output as a ClassAd
	fp = my_popenv( args, "r", FALSE );

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
					 "ignoring invalid shadow\n", buf );
			delete( ad );
			my_pclose( fp );
			return NULL;
		}
	}
	my_pclose( fp );
	if( ! read_something ) {
		dprintf( D_ALWAYS, "\"%s -classad\" did not produce any output, "
				 "ignoring\n", path ); 
		delete( ad );
		return NULL;
	}
	new_shadow = new Shadow( path, ad );
	return new_shadow;
}

