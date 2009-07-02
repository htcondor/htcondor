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


#if !defined(_CONDOR_JIC_LOCAL_FILE_H)
#define _CONDOR_JIC_LOCAL_FILE_H

#include "jic_local_config.h"

/** 
	This is the child class of JICLocal (and therefore
	JobInfoCommunicator) that deals with running "local" jobs and
	getting the job ClassAd info from both a file and out of the
	config file.  There are two constructors, one where we take
	another string, one where we don't.  The string is the optional
	keyword we got passed on the command line to prepend to all the
	attributes we want to look up which aren't in the classad that's
	written to the file.  If there's no keyword, either the job
	ClassAd in the file should have all the attributes we need, or it
	should define the keyword itself.
*/

class JICLocalFile : public JICLocalConfig {
public:

		/** Constructor with a keyword on the command-line.
			@param classad_filename Full path to the ClassAd, "-" if STDIN
			@param keyword Config file keyword to find other attributes
			@cluster Cluster ID number (if any)
			@proc Proc ID number (if any)
			@subproc Subproc ID number (if any)
		*/
	JICLocalFile( const char* classad_filename,
				  const char* keyword, int cluster, int proc, int subproc );

		/// Constructor without a keyword on the command-line
	JICLocalFile( const char* classad_filename,
				  int cluster, int proc, int subproc );

		/// Destructor
	virtual ~JICLocalFile();

		/** This is the version of getLocalJobAd that knows to try to
			read from the file to get the job ClassAd.  If there's a
			config keyword (either on the command-line or in the
			ClassAd in the file, we also try calling
			JICLocalConfig::getLocalJobAd() to read in any attributes
			we might need from the config file.  This allows users to
			define most of the job in the config file, and only
			specify some of the attributes in the ClassAd which is
			passed dynamically.  The attributes in the dynamic ClassAd
			always take precedence over ones in the config file.
		*/
	bool getLocalJobAd( void );

	char* jobAdFileName( void );

protected:

		/** This version first checks the ClassAd we got from the file
			before looking in the config file
		*/
	virtual bool getUniverse( void );

		/** Private helper to actually read the file and try to insert
			it into the passed ClassAd.
		*/
	bool readClassAdFromFile( char* filename, ClassAd* ad );


		/** Private helper to initialize our job_filename
			data member.  We use this since we have two
			constructors, both of which need to do the same thing
			with this info.
		*/
	void initFilenames( const char* jobad_path );

		/** The full path to the file we get our ClassAd from (or "-"
			if we're reading it from STDIN).
		*/
	char* job_filename;

};


#endif /* _CONDOR_JIC_LOCAL_FILE_H */
