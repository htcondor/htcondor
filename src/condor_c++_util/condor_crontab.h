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
  
#ifndef CONDOR_CRONTAB_H
#define CONDOR_CRONTAB_H

#include "condor_common.h"
#include "condor_classad.h"
#include "dc_service.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "MyString.h"
#include "extArray.h"
//#include "RegExer.h"

//
// Attributes for a parameter will be separated by the this character
// 
#define CRONTAB_DELIMITER ","
//
// Wildcard Character
//
#define CRONTAB_WILDCARD "*"
//
// Range Delimiter
//
#define CRONTAB_RANGE "-"
//
// Step Delimiter
//
#define CRONTAB_STEP "/"
//
// cronos.h defined a STAR value to mean the wildcard
//
#define CRONTAB_CRONOS_STAR -1

//
// Time Ranges
//
#define CRONTAB_MINUTE_MIN			0
#define CRONTAB_MINUTE_MAX			59
#define CRONTAB_HOUR_MIN			0
#define CRONTAB_HOUR_MAX			23
#define CRONTAB_DAY_OF_MONTH_MIN	1
#define CRONTAB_DAY_OF_MONTH_MAX	31
#define CRONTAB_MONTH_MIN			1
#define CRONTAB_MONTH_MAX			12
#define CRONTAB_DAY_OF_WEEK_MIN		0	// Note that Sunday is either 0 or 7
#define CRONTAB_DAY_OF_WEEK_MAX		7

//
// Data Field Indices
//
#define CRONTAB_MINUTES_IDX		0
#define CRONTAB_HOURS_IDX		1
#define CRONTAB_DOM_IDX			2
#define CRONTAB_MONTHS_IDX		3
#define CRONTAB_DOW_IDX			4
#define CRONTAB_YEARS_IDX		5 // NOT USED IN CRON CALCULATIONS!!
//
// Number of crontab elements - cleaner loop code!
// Note that we don't care about the year. But we will
// stuff the current year in the current time for convience
//
#define CRONTAB_FIELDS 			5

//
// Invalid Runtime Identifier
//
#define CRONTAB_INVALID			-1

//
// CronTab
// Andy Pavlo - pavlo@cs.wisc.edu
// October 20th, 205
//
// The object can be given parameters just like Unix crond and will generate
// a set of ranges for schedules. After the object is initialized
// you can call nextRunTime() to be given back a UTC timestamp of what
// the next run time according to the schedule should be.
//
// Parameter Format (From crontab manpage):
//              field          allowed values
//              -----          --------------
//              minute         0-59
//              hour           0-23
//              day of month   1-31
//              month          1-12
//              day of week    0-7 (0 or 7 is Sun)
//
// NOTE:
// We do not allow for name representations of Months of Days of the Week
// like unix cron does. If we are given a value that is outside of the
// the allowed ranges, we will simply cap it to the bounded range. 
//
class CronTab {
public:
		//
		// Constructor
		// This constuctor can be given a ClassAd from which the schedule
		// values can be plucked out.
		//
	CronTab( ClassAd* );
		//
		// Constuctor
		// Provided to add backwards capabilities for cronos.c
		// Using integers really limits what can be done for scheduling
		// The STAR constant has been replaced with CRONTAB_CRONOS_STAR
		// Note that we are also not providing scheduling down to the second
		//
	CronTab( int, int, int, int, int );
		//
		// Constructor
		// Instead of being given a ClassAd, we can be given string values
		// following the same format to create a cron schedule
		//
	CronTab( const char*, const char*, const char*, const char*, const char* );
		//
		// Deconstructor
		// Remove our array lists and parameters that we have
		// dynamically allocated
		//
	~CronTab();
		//
		// needsCronTab()
		// This is a static helper method that is given a ClassAd object
		// and will return true a CronTab scheduler object should be 
		// created for it. This is for convience so that we are not
		// instantiating a CronTab object for every ClassAd and then
		// checking if the initialization failed
		//
	static bool needsCronTab( ClassAd* );
		//
		// validate()
		// Checks to see if the parameters specified for a cron schedule
		// are valid. This is made static so that they do not have
		// to create the object to check whether the schedule is valid
		// You must pass in a string object to get any error messages 
		// accumulated by calling this function
		//
	static bool validate( ClassAd*, MyString& );	
		//
		// getError()
		// This method will return the error message for the calling object
		// This is what you'll want to use if you are trying to figure
		// out why a specific schedule is returning invalid runtimes
		//
	MyString getError();
		//
		// nextRunTime()
		// Returns the next execution time for our cron schedule from
		// the current time.
		// The times are the number of seconds elapsed since 
		// 00:00:00 on January 1, 1970, Coordinated Universal Time (UTC)
		//
	long nextRunTime();
		//
		// nextRunTime()
		// Returns the next execution time for our cron schedule from the
		// provided timestamp. We will only return an execution time if we
		// have been initialized properly and the valid flag is true. The 
		// timestamp that we calculate here will be stored and can be accessed
		// again with lastRun().
		// The times are the number of seconds elapsed since 
		// 00:00:00 on January 1, 1970, Coordinated Universal Time (UTC)
		//
	long nextRunTime( long );
		//
		// If we were able to parse the parameters correctly
		// then we will let them query us for runtimes
		// 
	bool isValid() { return ( this->valid ); }
		//
		// Returns the last calculated timestamp
		//
	bool lastRun() { return (this->lastRunTime); }
	
protected:
		//
		// Default Constructor
		//
	CronTab();
		//
		// validateParameter()
		// Internal static helper method that can check to see 
		// if a cron tab parameter has the proper syntax. The reason
		// why it is not public is because we don't want to expose
		// how we represent the attributes internally
		//
	static bool validateParameter( int, const char*, MyString& );
		//
		// init()
		// Initializes our object to prepare it for being asked 
		// what the next run time should be. If a parameter is invalid
		// we will not allow them to query us for runtimes
		//
	void init();
		//
		// matchFields()
		// Important helper function that actually does the grunt work of
		// find the next runtime. See function implementation to see 
		// how it actually works
		//
	bool matchFields( int*, int*, int, bool useFirst = false );
		//
		// expandParameter()
		// This is the logic that is used to take a parameter string
		// given for a specific field in the cron schedule and expand it
		// out into a range of int's that can be looked up quickly later on
		//
	bool expandParameter( int, int, int );
		//
		// contains()
		// Just checks to see if a value is in an array
		//
	bool contains( ExtArray<int>&, const int&  );
		//
		// sort()
		// Ye' Olde Insertion Sort!
		// This is here so I can sort ExtArray<int>'s
		//
	void sort( ExtArray<int>& );

protected:
		//
		// Static Error Log
		//
	static MyString staticErrorLog;
		//
		// Instantiated Error Log
		//
	MyString errorLog;
		//
		// We need to know whether our fields are valid
		// and we can proceed with looking for a runtime
		//
	bool valid;
		//
		// The last runtime we calculated
		// We don't use this internally for anything, but we'll 
		// allow them to look it up
		//
	long lastRunTime;
		//
		// The various scheduling properties of the cron definition
		// These will be in pulled by the various Constructors
		//
	MyString *parameters[CRONTAB_FIELDS];
		//
		// After we parse the cron schedule we will have ranges
		// for the different properties.
		//
	ExtArray<int> *ranges[CRONTAB_FIELDS];
		//
		// Attribute names
		// Merely here for convienence
		//
	static const char* attributes[];
		//
		// The regular expresion object we will use to make sure 
		// our parameters are in the proper format.
		//
	//static RegExer regex;

}; // END CLASS

#endif // CONDOR_CRONTAB_H
