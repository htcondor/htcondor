/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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

  
#ifndef CONDOR_CRONTAB_H
#define CONDOR_CRONTAB_H

#include "condor_common.h"
#include "condor_classad.h"
#include "dc_service.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "MyString.h"
#include "extArray.h"
#include "condor_regex.h"

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
// The PCRE pattern used to validate the user's parameters
//
#define CRONTAB_PARAMETER_PATTERN "[^\\/0-9" \
									CRONTAB_DELIMITER \
									CRONTAB_RANGE \
									CRONTAB_STEP \
									CRONTAB_WILDCARD \
									"\\ " \
									"\\/*]"

/**
 * CronTab
 * The object can be given parameters just like Unix crond and will generate
 * a set of ranges for schedules. After the object is initialized
 * you can call nextRunTime() to be given back a UTC timestamp of what
 * the next run time according to the schedule should be.
 * 
 * Parameter Format (From crontab manpage):
 *              field          allowed values
 *              -----          --------------
 *              minute         0-59
 *              hour           0-23
 *              day of month   1-31
 *              month          1-12
 *              day of week    0-7 (0 or 7 is Sun)
 * 
 * NOTE:
 * We do not allow for name representations of Months of Days of the Week
 * like unix cron does. If we are given a value that is outside of the
 * the allowed ranges, we will simply cap it to the bounded range. 
 * 
 * @author Andy Pavlo - pavlo@cs.wisc.edu
 * @date October 20th, 2005
 **/
class CronTab {
public:
		/**
		 * Constructor
		 * This constuctor can be given a ClassAd from which the schedule
		 * values can be plucked out.
		 *
		 * @param ad - the ClassAd to pull the CronTab attributes from.
		 **/
	CronTab( ClassAd* );
		/**
		 * Constuctor
		 * Provided to add backwards capabilities for cronos.c
		 * Using integers really limits what can be done for scheduling
		 * The STAR constant has been replaced with CRONTAB_CRONOS_STAR
		 * Note that we are also not providing scheduling down to the second
		 * 
		 * @param minutes - the minutes attribute (0 - 59)
		 * @param hours - the hours attribute (0 - 23)
		 * @param days_of_month - a day in a month (1 - 31, depending on the month)
		 * @param months - the months attribute (1 - 12)
		 * @param days_of_week - a day in the week (0 - 7, Sunday is 0 or 7)
		 **/
	CronTab( int, int, int, int, int );
		/**
		 * Constructor
		 * Instead of being given a ClassAd, we can be given string values
		 * following the same format to create a cron schedule
		 * 
		 * @param minutes
		 * @param hours
		 * @param days_of_month
		 * @param months
		 * @param days_of_week
		 **/
	CronTab( const char*, const char*, const char*, const char*, const char* );
		/**
		 * Deconstructor
		 * Remove our array lists and parameters that we have
		 * dynamically allocated
		 **/
	~CronTab();
		/**
		 * This is a static helper method that is given a ClassAd object
		 * and will return true a CronTab scheduler object should be 
		 * created for it. This is for convience so that we are not
		 * instantiating a CronTab object for every ClassAd and then
		 * checking if the initialization failed
		 * 
		 * @param ad - the ClassAd to check whether it has CronTab attributes
		 * @return true if this ad has defined a CronTab schedule
		 **/
	static bool needsCronTab( ClassAd* );
		/**
		 * Checks to see if the parameters specified for a cron schedule
		 * are valid. This is made static so that they do not have
		 * to create the object to check whether the schedule is valid
		 * You must pass in a string object to get any error messages 
		 * accumulated by calling this function
		 * 
		 * @param ad - the ClassAd to check the syntax for
		 * @param error - where the error message will be stored if there's a problem
		 * @return true if ad had valid CronTab paramter syntax
		 **/
	static bool validate( ClassAd*, MyString& );
		/**
		 * This method will return the error message for the calling object
		 * This is what you'll want to use if you are trying to figure
		 * out why a specific schedule is returning invalid runtimes
		 * 
		 * @return the error message for this object
		 **/
	MyString getError();
		/**
		 * Returns the next execution time for our cron schedule from
		 * the current time.
		 * The times are the number of seconds elapsed since 
		 * 00:00:00 on January 1, 1970, Coordinated Universal Time (UTC)
		 * 
		 * @return the next run time for the object's schedule starting at the current time
		 **/
	long nextRunTime();
		/**
		 * Returns the next execution time for our cron schedule from the
		 * provided timestamp. We will only return an execution time if we
		 * have been initialized properly and the valid flag is true. The 
		 * timestamp that we calculate here will be stored and can be accessed
		 * again with lastRun().
		 * The times are the number of seconds elapsed since 
		 * 00:00:00 on January 1, 1970, Coordinated Universal Time (UTC)
		 * 
		 * @param timestamp - the starting time to get the next run time for
		 * @return the next run time for the object's schedule
		 **/
	long nextRunTime( long );
		/**
		 * If we were able to parse the parameters correctly
		 * then we will let them query us for runtimes
		 * 
		 * @return true if this object has been initialized properly
		 **/
	bool isValid() { return ( this->valid ); }
		/**
		 * Returns the last calculated timestamp
		 * If no runtime has been calculated, this method will
		 * return CRONTAB_INVALID
		 * 
		 * @return the last run time calculated by this object
		 **/
	long lastRun() { return (this->lastRunTime); }
		/**
		 * Initializes CronTab::regex
		 */
	static void initRegexObject();
		/**
		 * Static helper method that can check to see if a 
		 * parameter has the proper syntax.
		 * 
		 * @param attribute_idx - the index for this parameter in CronTab::attributes
		 * @param parameter - the parameter string to validate
		 * @param error - where the error message will be stored if there's a problem
		 * @return true if the parameter was a valid CronTab attribute
		 **/
	static bool validateParameter(const char* param, const char * attr, MyString& error);
		//
		// Attribute names
		// A nice list that we can iterate through easily
		//
	static const char* attributes[];
	
protected:
		/**
		 * Default Constructor
		 * This should never be called
		 **/
	CronTab();
		/**
		 * Initializes our object to prepare it for being asked 
		 * what the next run time should be. If a parameter is invalid
		 * we will not allow them to query us for runtimes
		 **/
	void init();
		/**
		 * Important helper function that actually does the grunt work of
		 * find the next runtime. See function implementation to see 
		 * how it actually works and for further documentation
		 * 
		 * @param curTime - an array of time attributes for where we start our calculations
		 * @param match - an array of time attributes that is the next run time
		 * @param attribute_idx - the index for the current parameter in CronTab::attributes
		 * @param useFirst - whether we should base ourselves off of Jan 1st
		 * @return true if we able to find a new run time
		 **/
	bool matchFields( int*, int*, int, bool useFirst = false );
		/**
		 * This is the logic that is used to take a parameter string
		 * given for a specific field in the cron schedule and expand it
		 * out into a range of int's that can be looked up quickly later on
		 * 
		 * @param attribute_idx - the index for the parameter in CronTab::attributes
		 * @param min - the mininum value in the range for this parameter
		 * @param max - the maximum value in the range for this parameter
		 * @return true if we were able to create the range of values
		 **/
	bool expandParameter( int, int, int );
		/**
		 * Just checks to see if a value is in an array
		 * 
		 * @param list - the array to search for the value\
		 * @param elt - the value to search for in the array
		 * @return true if the element exists in the list
		 **/
	bool contains( ExtArray<int>&, const int&  );
		/**
		 * Ascending Insertion Sort
		 * 
		 * @param list - the array to sort
		 **/
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
		// The regular expresion object we will use to make sure 
		// our parameters are in the proper format.
		//
	static Regex regex;

}; // END CLASS

#endif // CONDOR_CRONTAB_H
