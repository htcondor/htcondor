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
	JICLocalFile( const char* classad_filename, const char* keyword,
				  int cluster, int proc, int subproc );

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

	char* fileName( void );

protected:

		/** This version first checks the ClassAd we got from the file
			before looking in the config file
		*/
	virtual bool getUniverse( void );

		/** Private helper to actually read the file and try to insert
			it into our job ClassAd.
		*/
	bool readClassAdFromFile( void );

		/** Private helper to initialize our filename data member.
			we use this since we have two constructors, both of which
			need to do the same thing with this info.
		*/
	void initFilename( const char* path );

		/** The full path to the file we get our ClassAd from (or "-"
			if we're reading it from STDIN).
		*/
	char* filename;
};


#endif /* _CONDOR_JIC_LOCAL_FILE_H */
