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
#include "condor_crontab.h"
#include "condor_classad.h"
#include "condor_debug.h"
#include "MyString.h"
#include "extArray.h"
//#include "regex.h"
#include "date_util.h"

//
// The list of attributes that can be defined for a cron schedule
//
const char* CronTab::attributes[] = {	ATTR_CRON_MINUTES,
										ATTR_CRON_HOURS,
										ATTR_CRON_DAYS_OF_MONTH,
										ATTR_CRON_MONTHS,
										ATTR_CRON_DAYS_OF_WEEK
};
	
//
// We use a single Regex object for all instances of CronTab
//
Regex CronTab::regex;
		
/**
 * Default Constructor
 **/
CronTab::CronTab()
{
	for (int i = 0; i < CRONTAB_FIELDS; i++) {
		parameters[i] = 0;
		ranges[i] = 0;
	}
	this->lastRunTime = CRONTAB_INVALID;
	this->valid = false;
}

/**
 * Constructor
 * This constuctor can be given a ClassAd from which the schedule
 * values can be plucked out. It is assumed that the ad will have
 * at least one CronTab attribute defined.
 * 
 * @param ad - the ClassAd to pull the CronTab attributes from.
 **/
CronTab::CronTab( ClassAd *ad )
{
		//
		// Pull out the different parameters from the ClassAd
		//
	for ( int ctr = 0; ctr < CRONTAB_FIELDS; ctr++ ) {
		std::string buffer;
			//
			// First get out the parameter value
			//
		if ( ad->LookupString( this->attributes[ctr], buffer ) ) {
			dprintf( D_FULLDEBUG, "CronTab: Pulled out '%s' for %s\n",
						buffer.c_str(), this->attributes[ctr] );
			this->parameters[ctr] = new MyString( buffer.c_str() );
			//
			// The parameter is empty, we'll use the wildcard
			//
		} else {
			dprintf( D_FULLDEBUG, "CronTab: No attribute for %s, using wildcard\n",
							this->attributes[ctr] );
			this->parameters[ctr] = new MyString( CRONTAB_WILDCARD );
		}
	} // FOR
	this->init();
}

/**
 * Constuctor
 * Provided to add backwards capabilities for cronos.c
 * Using integers really limits what can be done for scheduling
 * The STAR constant has been replaced with CRONTAB_CRONOS_STAR
 * Note that we are also not providing scheduling down to the second
 * These arguments can only be single values. If you want to use the more
 * complex syntax you'll have to use strings
 * 
 * @param minutes - the minutes attribute (0 - 59)
 * @param hours - the hours attribute (0 - 23)
 * @param days_of_month - a day in a month (1 - 31, depending on the month)
 * @param months - the months attribute (1 - 12)
 * @param days_of_week - a day in the week (0 - 7, Sunday is 0 or 7)
 **/
CronTab::CronTab(	int minutes,
					int hours,
					int days_of_month,
					int months,
					int days_of_week ) {
		//
		// Simply convert everything to strings
		// If the value is STAR, then use the wildcard
		//
	if ( minutes == CRONTAB_CRONOS_STAR ) {
		this->parameters[CRONTAB_MINUTES_IDX] = new MyString( CRONTAB_WILDCARD );
	} else {
		this->parameters[CRONTAB_MINUTES_IDX] = new MyString( std::to_string( minutes ) );
	}
	if ( hours == CRONTAB_CRONOS_STAR ) {
		this->parameters[CRONTAB_HOURS_IDX]	= new MyString( CRONTAB_WILDCARD );
	} else {
		this->parameters[CRONTAB_HOURS_IDX]	= new MyString( std::to_string( hours ) );
	}
	if ( days_of_month == CRONTAB_CRONOS_STAR ) {
		this->parameters[CRONTAB_DOM_IDX] = new MyString( CRONTAB_WILDCARD );
	} else {
		this->parameters[CRONTAB_DOM_IDX] = new MyString( std::to_string( days_of_month ) );
	}
	if ( months == CRONTAB_CRONOS_STAR ) {
		this->parameters[CRONTAB_MONTHS_IDX] = new MyString( CRONTAB_WILDCARD );
	} else {
		this->parameters[CRONTAB_MONTHS_IDX] = new MyString( std::to_string( months ) );
	}
	if ( days_of_week == CRONTAB_CRONOS_STAR ) {
		this->parameters[CRONTAB_DOW_IDX] = new MyString( CRONTAB_WILDCARD );
	} else {
		this->parameters[CRONTAB_DOW_IDX] = new MyString( std::to_string( days_of_week ) );
	}
	this->init();
}

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
CronTab::CronTab(	const char* minutes,
					const char* hours,
					const char* days_of_month,
					const char* months,
					const char* days_of_week ) {
		//
		// Just save into our object - how convienent!
		//
	this->parameters[CRONTAB_MINUTES_IDX]	= new MyString( minutes );
	this->parameters[CRONTAB_HOURS_IDX]		= new MyString( hours );
	this->parameters[CRONTAB_DOM_IDX]		= new MyString( days_of_month );
	this->parameters[CRONTAB_MONTHS_IDX]	= new MyString( months );
	this->parameters[CRONTAB_DOW_IDX]		= new MyString( days_of_week );

	this->init();
}

/**
 * Deconstructor
 * Remove our array lists and parameters that we have
 * dynamically allocated
 **/
CronTab::~CronTab() {
	int ctr;
	for ( ctr = 0; ctr < CRONTAB_FIELDS; ctr++ ) {
		if ( this->ranges[ctr] )		delete this->ranges[ctr];
		if ( this->parameters[ctr] )	delete this->parameters[ctr];
	} // FOR
}

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
bool
CronTab::needsCronTab( ClassAd *ad ) {
		//
		// Go through and look to see if we have at least
		// one of the attributes in the ClassAd
		//
	bool ret = false;
	int ctr;
	for ( ctr = 0; ctr < CRONTAB_FIELDS; ctr++ ) {
			//
			// As soon as we find one we can quit
			//
		if ( ad->LookupExpr( CronTab::attributes[ctr] ) ) {
			ret = true;
			break;
		}
	} // FOR
	return ( ret );
}

/**
 * Validate that all the CronTab parameters in a ClassAd are
 * valid syntax. If not, we'll report an error message in 
 * the string passed in
 *
 * @param ad - the ClassAd to check the syntax for
 * @param error - where the error message will be stored if there's a problem
 * @return true if ad had valid CronTab paramter syntax
 **/
bool
CronTab::validate( ClassAd *ad, MyString &error ) {
	bool ret = true;
		//
		// Loop through all the fields in the ClassAd
		// If one our attribute fields is defined, then check
		// to make sure that it has valid syntax
		//
	int ctr;
	for ( ctr = 0; ctr < CRONTAB_FIELDS; ctr++ ) {
		std::string buffer;
			//
			// If the validation fails, we keep going
			// so that they can see all the error messages about
			// their parameters at once
			//
		if ( ad->LookupString( CronTab::attributes[ctr], buffer ) ) {
			MyString curError;
			if ( !CronTab::validateParameter( buffer.c_str(), CronTab::attributes[ctr], curError ) ) {
				ret = false;
				error += curError;
			}
		}
	} // FOR
	return ( ret );
}

/**
 * Get back all the error messages that may have occured for
 * this instantiation of the object. This is different from
 * the error messages created by the static helper methods
 * 
 * @return the error message for this object
 **/
MyString
CronTab::getError() {
	return ( this->errorLog );
}

/**
 * Validates that a single parameter has valid syntax. We are passed
 * which attribute index we are testing so that we can have an 
 * informative error message. Our validation isn't very strict.
 * We use a regular expression to make sure that there are no
 * characeters other than the ones that we allow
 *
 * @param attribute_idx - the index for this parameter in CronTab::attributes
 * @param parameter - the parameter string to validate
 * @param error - where the error message will be stored if there's a problem
 * @return true if the parameter was a valid CronTab attribute
 * 
 * @TODO Write a more complicated regex that validates syntax and not
 * 		 just the characters
 **/
bool
CronTab::validateParameter(const char* parameter, const char * attr, MyString& error)
{
	bool ret = true;
		//
		// Make sure there are only valid characters 
		// in the parameter string
		//
	MyString temp(parameter);
	if ( CronTab::regex.match( temp ) ) {
		error  = "Invalid parameter value '";
		error += parameter;
		error += "' for ";
		error += attr;
		ret = false;
	}
	return ( ret );
}

/**
 * Initializes our object to prepare it for being asked 
 * what the next run time should be. If a parameter is invalid
 * we will not allow them to query us for runtimes
 **/
void
CronTab::init() {
	initRegexObject();
		//
		// Set the last runtime as empty
		//
	this->lastRunTime = CRONTAB_INVALID;
		//
		// We're invalid until we parse all the parameters
		//
	this->valid = false;
	
		//
		// Now run through all the parameters and create the cron schedule
		//
	int mins[]				= { CRONTAB_MINUTE_MIN,
								CRONTAB_HOUR_MIN,
								CRONTAB_DAY_OF_MONTH_MIN,
								CRONTAB_MONTH_MIN,
								CRONTAB_DAY_OF_WEEK_MIN
	};
	int maxs[]				= { CRONTAB_MINUTE_MAX,
								CRONTAB_HOUR_MAX,
								CRONTAB_DAY_OF_MONTH_MAX,
								CRONTAB_MONTH_MAX,
								CRONTAB_DAY_OF_WEEK_MAX
	};
		//
		// For each attribute field, expand out the crontab parameter
		//
	bool failed = false;
	int ctr;
	for ( ctr = 0; ctr < CRONTAB_FIELDS; ctr++ ) {
		this->ranges[ctr] = new ExtArray<int>();			
			//
			// Call to expand the parameter
			// The function will modify the queue for us
			// If we fail we will keep running so that they can 
			// get errors for other parameters that might be wrong
			// Plus it makes it easier on us when it comes time
			// to deallocate memory for the range arrays
			//
		if ( !this->expandParameter( ctr, mins[ctr], maxs[ctr] )) {
			failed = true;
		}
	} // FOR
		//
		// If the parameter expansion didn't fail, then we can
		// set ourselves as being valid and ready to calculate runtimes
		//
	if ( !failed ) {
		this->valid = true;
	}
	
	return;
}

/**
 * Initializes CronTab::regex
 **/  
void
CronTab::initRegexObject() {
		//
		// There should be only one Regex object shared for all instances
		// of our object since the pattern that it needs to match is the same
		// So we only need to compile the pattern once
		//
	if ( ! CronTab::regex.isInitialized() ) {
		int errcode, erroffset;
		MyString pattern( CRONTAB_PARAMETER_PATTERN ) ;
			//
			// It's a big problem if we can't compile the pattern, so
			// we'll want to dump out right now
			//
		if ( ! CronTab::regex.compile( pattern, &errcode, &erroffset )) {
			MyString error = "CronTab: Failed to compile Regex - ";
			error += pattern;
			EXCEPT( "%s", error.c_str() );
		}
	}
}
	
/**
 * Returns the next execution time for our cron schedule from
 * the current time. The times are the number of seconds
 * elapsed since 00:00:00 on January 1, 1970, Coordinated Universal Time (UTC)
 * If no valid run time could be calculated, this method will
 * return CRONTAB_INVALID
 * 
 * @return the next run time for the object's schedule starting at the current time
 **/
long
CronTab::nextRunTime( ) {
		//
		// Call our own method but using the current time
		//
	return ( this->nextRunTime( (long)time( NULL ) ) );
}

/**
 * Returns the next execution time for our cron schedule from the
 * provided timestamp. We will only return an execution time if we
 * have been initialized properly and the valid flag is true. The 
 * timestamp that we calculate here will be stored and can be accessed
 * again with lastRun().
 * The times are the number of seconds elapsed since 00:00:00 on
 * January 1, 1970, Coordinated Universal Time (UTC)
 * If no valid run time could be calculated, this method will
 * return CRONTAB_INVALID
 * 
 * This may be useful if somebody wants to figure out the next 5 runs
 * for a job if they keep feeding us the times we return
 * 
 * @param timestamp - the starting time to get the next run time for
 * @return the next run time for the object's schedule
 **/
long
CronTab::nextRunTime( long timestamp ) {
	long runtime = CRONTAB_INVALID;
	struct tm *tm;

		//
		// We can only look for the next runtime if we 
		// are valid
		//
	if ( ! this->valid ) {
		this->lastRunTime = CRONTAB_INVALID;
		return ( this->lastRunTime );
	}
		
		//
		// IMPORTANT:
		// We have have to round up to the next minute because
		// the run times will always be at set at 0 seconds, meaning
		// it suppose to start exactly when the minute starts. 
		// Currently we do not allow them to specify the seconds
		// in which jobs should start. Without rounding to the next minute
		// then if a job is suppose to run this at the current minute, the
		// runtime would come back as being in the past, which is incorrect
		//
	timestamp += ( 60 - ( timestamp % 60 ) );
	const time_t _timestamp = (time_t)timestamp;
		
		//
		// Load up the time information about the timestamp we
		// were given. The logic for matching the next job is fairly 
		// simple. We just go through the fields backwards and find the next
		// match. This assumes that the ranges are sorted, which they 
		// should be
		//
	tm = localtime( &_timestamp );
	int fields[CRONTAB_FIELDS];
	fields[CRONTAB_MINUTES_IDX]	= tm->tm_min;
	fields[CRONTAB_HOURS_IDX]	= tm->tm_hour;
	fields[CRONTAB_DOM_IDX]		= tm->tm_mday;
	fields[CRONTAB_MONTHS_IDX]	= tm->tm_mon + 1;
	fields[CRONTAB_DOW_IDX]		= tm->tm_wday;
	
		//
		// This array will contain the matched values 
		// for the different fields for the next job run time
		// Unlike the crontab specifiers, we actually need to know
		// the year of the next runtime so we'll set the year here
		//
	int match[CRONTAB_FIELDS + 1];
	match[CRONTAB_YEARS_IDX] = tm->tm_year + 1900;
	match[CRONTAB_DOW_IDX] = -1;
	
		//
		// If we get a match then create the timestamp for it
		// Notice that we have to set explicitly set the the seconds,
		// offset the month by 1, and change the year to be based
		// off of 1900.
		//
	if ( this->matchFields( fields, match, CRONTAB_FIELDS - 2 ) ) {
		struct tm matchTime;
		matchTime.tm_sec	= 0;
		matchTime.tm_min	= match[CRONTAB_MINUTES_IDX];
		matchTime.tm_hour	= match[CRONTAB_HOURS_IDX];
		matchTime.tm_mday	= match[CRONTAB_DOM_IDX];
		matchTime.tm_mon	= match[CRONTAB_MONTHS_IDX] - 1;
		matchTime.tm_year	= match[CRONTAB_YEARS_IDX] - 1900;
		matchTime.tm_isdst  = -1; // auto-calculate whether daylight savings time applies
		runtime = (long)mktime( &matchTime );
		
			//
			// Make sure that our next runtime is in the future
			// and never in the past. The runtime can be equal
			// to the current time because the current time 
			// may be rounded up to the next minute
			//
		if ( runtime < timestamp ) {
			dprintf( D_ALWAYS, "CronTab: Generated a runtime that is in the past (%d < %d), scheduling now\n" , (int)runtime, (int)timestamp );
			runtime = time(0) + 120;
		}
		
		//
		// As long as we are configured properly, we should always
		// be able to find a match.
		//
	} else {
		EXCEPT( "CronTab: Failed to find a match for timestamp %d", 
			(int)timestamp );
	}
	
	this->lastRunTime = runtime;
	return ( runtime );
}

/**
 * Important helper function that actually does the grunt work of
 * find the next runtime. We need to be given an array of the different
 * time fields so that we can match the current time with different 
 * parameters. The function will recursively call itself until
 * it can find a match at the MINUTES level. If one recursive call
 * cannot find a match, it will return false and the previous level
 * will increment its range index by one and try to find a match. On the 
 * the subsequent call useFirst will be set to true meaning that the level
 * does not need to look for a value in the range that is greater than or
 * equal to the current time value. If the failing returns to the MONTHS level, 
 * that means we've exhausted all our possiblities from the current time to the
 * the end of the year. So we'll increase the year by 1 and try the same logic again
 * using January 1st as the starting point.
 * The array match will contain the timestamp information of the job's
 * next run time
 * 
 * @param curTime - an array of time attributes for where we start our calculations
 * @param match - an array of time attributes that is the next run time
 * @param attribute_idx - the index for the current parameter in CronTab::attributes
 * @param useFirst - whether we should base ourselves off of Jan 1st
 * @return true if we able to find a new run time
 **/
bool
CronTab::matchFields( int *curTime, int *match, int attribute_idx, bool useFirst )
{
		//
		// Whether we need to tell the next level above that they
		// should use the first element in their range. If we were told
		// to then certainly the next level will as well
		//
	bool nextUseFirst = useFirst;
		//
		// First initialize ourself to know that we don't have a match
		//
	match[attribute_idx] = -1;
	
		//
		// Special Day of Week Handling
		// In order to get the day of week stuff to work, we have 
		// to insert all the days of the month for the matching days of the
		// week in the range.
		//
	ExtArray<int> *curRange = NULL;
	if ( attribute_idx == CRONTAB_DOM_IDX ) {
			//
			// We have to take the current month & year
			// and convert the days of the week range into
			// days of the month
			//
			
			//Issue here is that range for dom will be 1-31
			//for * and if one doesn't specify day_of_month in a job file
		if (this->ranges[attribute_idx]->length()==CRONTAB_DAY_OF_MONTH_MAX){	
			if ((this->ranges[CRONTAB_DOW_IDX]->length()==CRONTAB_DAY_OF_WEEK_MAX)||
			   (this->ranges[CRONTAB_DOW_IDX]->length()==0)){ 
				//if both wildcards, use month range
				//if DOW range empty use DOM range
				curRange = new ExtArray<int>( *this->ranges[attribute_idx] );
			} else {
				//only wildcard in month, so use day of week range
				//this isn't quite right
				curRange = new ExtArray<int>( CRONTAB_DAY_OF_MONTH_MAX );
			}
		}else{
		      // get to here means DOM was specified
		      curRange = new ExtArray<int>( *this->ranges[attribute_idx] );
		}
		
		int firstDay = dayOfWeek( match[CRONTAB_MONTHS_IDX],
								  1,
								  match[CRONTAB_YEARS_IDX] );
		int ctr, cnt;
		for ( ctr = 0, cnt = this->ranges[CRONTAB_DOW_IDX]->getlast();
			  ctr <= cnt;
			  ctr++ ) {
				//
				// Now figure out all the days for this specific day of the week
				//
			int day = (this->ranges[CRONTAB_DOW_IDX]->getElementAt(ctr) - firstDay) + 1;
			while ( day <= CRONTAB_DAY_OF_MONTH_MAX ) {
				if (curRange && day > 0 && !this->contains( *curRange, day ) ) {
					curRange->add( day );
				}
				day += 7;
			} // WHILE
		} // FOR
			//
			// We have sort the list since we've added new elements
			//
		this->sort( *curRange );
		
		//
		// Otherwise we'll just use the unmodified range
		//
	} else {
		curRange = this->ranges[attribute_idx];
	}

		//
		// Find the first match for this field
		// If our value isn't in the list, then we'll take the next one
		//
	bool ret = false;
	int range_idx, cnt;
	for ( range_idx = 0, cnt = curRange->getlast();
		  range_idx <= cnt;
		  range_idx++ ) {
			//
			// Two ways to make a match:
			//
			//	1) The level below told us that we need to just 
			//	   the first value in our range if we can. 
			//	2) We need to select the first value in our range
			//	   that is greater than or equal to the current
			//	   time value for this field. This is usually
			//	   what we will do on the very first call to
			//	   us. If we fail and return back to the previous
			//	   level, when they call us again they'll ask
			//	   us to just use the first value that we can
			//
		int value = curRange->getElementAt( range_idx );
		if ( useFirst || value >= curTime[attribute_idx] ) {
				//
				// If this value is greater than the current time value,
				// ask the next level to useFirst
				//
			if ( value > curTime[attribute_idx] ) nextUseFirst = true;
				//
				// Day of Month Check!
				// If this day doesn't exist in this month for
				// the current year in our search, we have to skip it
				//
			if ( attribute_idx == CRONTAB_DOM_IDX ) {
				int maxDOM = daysInMonth( 	match[CRONTAB_MONTHS_IDX],
											match[CRONTAB_YEARS_IDX] );
				if ( value > maxDOM ) {
					continue;
				}
			}
			match[attribute_idx] = value;
				//
				// If we're the last field for the crontab (i.e. minutes),
				// then we should have a valid time now!
				//
			if ( attribute_idx == CRONTAB_MINUTES_IDX ) {
				ret = true;
				break;
				
				//
				// Now that we have a current value for this field, call to the
				// next field level and see if they can find a match using our
				// If we roll back then we'll want the next time around for the
				// next level to just use the first parameter in their range
				// if they can
				//
			} else {
				ret = this->matchFields( curTime, match, attribute_idx - 1, nextUseFirst );
				nextUseFirst = true;
				if ( ret ) break;
			}
		}
	} // FOR
		//
		// If the next level up failed, we need to have
		// special handling for months so we can roll over the year
		// While this may seem trivial it's needed so that we 
		// can change the year in the matching for leap years
		// We will call ourself to make a nice little loop!
		//
	if ( !ret && attribute_idx == CRONTAB_MONTHS_IDX ) {
		match[CRONTAB_YEARS_IDX]++;	
		ret = this->matchFields( curTime, match, attribute_idx, true );
	}
	
		//
		// We only need to delete curRange if we had
		// instantiated a new object for it
		//
	if ( attribute_idx == CRONTAB_DOM_IDX) delete curRange;
	
	return ( ret );
}

/**
 * This is the logic that is used to take a parameter string
 * given for a specific field in the cron schedule and expand it
 * out into a range of int's that can be looked up quickly later on
 * We must be given the index number of the field we're going to parse
 * and a min/max for the range of values allowed for the attribute.
 * If the parameter is invalid, we will report an error and return false
 * This will prevent them from querying nextRunTime() for runtimes
 * 
 * @param attribute_idx - the index for the parameter in CronTab::attributes
 * @param min - the mininum value in the range for this parameter
 * @param max - the maximum value in the range for this parameter
 * @return true if we were able to create the range of values
 **/
bool
CronTab::expandParameter( int attribute_idx, int min, int max )
{
	MyString *param = this->parameters[attribute_idx];
	ExtArray<int> *list	= this->ranges[attribute_idx];
	
		//
		// Make sure the parameter is valid
		// The validation method will have already printed out
		// the error message to the log
		//
	MyString error;
	if ( ! CronTab::validateParameter(	param->c_str(),
										CronTab::attributes[attribute_idx],
										error ) ) {
		dprintf( D_ALWAYS, "%s", error.c_str() );
			//
			// Store the error in case they want to email
			// the user to tell them that they goofed
			//
		CronTab::errorLog += error;
		return ( false );
	}
		//
		// Remove any spaces
		//
	param->replaceString(" ", "");
	
		//
		// Now here's the tricky part! We need to expand their parameter
		// out into a range that can be put in array of integers
		// First start by spliting the string by commas
		//
	MyStringTokener tok;
	tok.Tokenize(param->c_str());
	const char *_token;
	while ( ( _token = tok.GetNextToken( CRONTAB_DELIMITER, true ) ) != NULL ) {
		MyStringWithTokener token( _token );
		int cur_min = min, cur_max = max, cur_step = 1;
		
			// -------------------------------------------------
			// STEP VALUES
			// The step value is independent of whether we have
			// a range, the wildcard, or a single number.
			// -------------------------------------------------
		if ( token.find( CRONTAB_STEP ) > 0 ) {
				//
				// Just look for the step value to replace 
				// the current step value. The other code will
				// handle the rest
				//
			token.Tokenize();
			const char *_temp;
				//
				// Take out the numerator, keep it for later
				//
			const char *_numerator = token.GetNextToken( CRONTAB_STEP, true );
			if ( ( _temp = token.GetNextToken( CRONTAB_STEP, true ) ) != NULL ) {
				MyString stepStr( _temp );
				stepStr.trim();
				cur_step = atoi( stepStr.c_str() );
				if (cur_step == 0) {
					return false;
				}
			}
				//
				// Now that we have the denominator, put the numerator back
				// as the token. This makes it easier to parse later on
				//
			token = _numerator;
		} // STEP
		
			// -------------------------------------------------
			// RANGE
			// If it's a range, expand the range but make sure we 
			// don't go above/below our limits
			// Note that the find will ignore the token if the
			// range delimiter is in the first character position
			// -------------------------------------------------
		if ( token.find( CRONTAB_RANGE ) > 0 ) {
				//
				// Split out the integers
				//
			token.Tokenize();
			MyString *_temp;
			int value;
			
				//
				// Min
				//
			_temp = new MyString( token.GetNextToken( CRONTAB_RANGE, true ) );
			_temp->trim();
			value = atoi( _temp->c_str() );
			cur_min = ( value >= min ? value : min );
			delete _temp;
				//
				// Max
				//
			_temp = new MyString( token.GetNextToken( CRONTAB_RANGE, true ) );
			_temp->trim();
			value = atoi( _temp->c_str() );
			cur_max = ( value <= max ? value : max );
			delete _temp;
			
			// -------------------------------------------------
			// WILDCARD
			// This will select all values for the given range
			// -------------------------------------------------
		} else if ( token.find( CRONTAB_WILDCARD ) >= 0 ) {
				//
				// For this we do nothing since it will just 
				// be the min-max range
				//
				// Day of Week Special Case
				// The day of week specifier is kind of weird
				// If it's the wildcard, then it doesn't mean
				// include all like the other fields. The reason
				// why we don't want to do that is because later 
				// on we are going to expand the day of the week 
				// field to be actual dates in a month in order
				// to figure out when to run next, so we don't 
				// want to expand out the wildcard for all days
				// if the day of month field is restricted
				// Read the cron manpage!
				//
				if ( attribute_idx  == CRONTAB_DOW_IDX ) {
					continue;
				}
			// -------------------------------------------------
			// SINGLE VALUE
			// They just want a single value to be added
			// Note that a single value with a step like "2/3" will
			// work in this code but its meaningless unless its whole number
			// -------------------------------------------------
		} else {
				//
				// Replace the range to be just this value only if it
				// fits in the min/max range we were given
				//
			int value = atoi(token.c_str());
			if ( value >= min && value <= max ) {
				cur_min = cur_max = value;
			}
		}
			//
			// Fill out the numbers based on the range using
			// the step value
			//
		int ctr;
		for ( ctr = cur_min; ctr <= cur_max; ctr++ ) {
				//
				// Day of Week Special Case
				// The crontab specifications lets Sunday be
				// represented with either a 0 or a 7. Our 
				// dayOfWeek() method in date_util.h uses 0-6
				// So if this the day of the week attribute and 
				// we are given a 7 for Sunday, just convert it
				// to a zero
				//
			int temp = ctr;
			if ( attribute_idx == CRONTAB_DOW_IDX && 
				 temp == CRONTAB_DAY_OF_WEEK_MAX ) {
				temp = CRONTAB_DAY_OF_WEEK_MIN;
			}
			
				//
		 		// Make sure this value isn't alreay added and 
		 		// that it falls in our step listing for the range
		 		//
			if (((temp % cur_step) == 0) && !this->contains(*list, temp)) {
				list->add( temp );
		 	}
		} // FOR
	} // WHILE
		//
		// Sort! Makes life easier later on
		//
	this->sort( *list );	
	return ( true );
}

/**
 * Just checks to see if a value is in an array
 * 
 * @param list - the array to search for the value
 * @param elt - the value to search for in the array
 * @return true if the element exists in the list
 **/
bool
CronTab::contains( ExtArray<int> &list, const int &elt ) 
{
		//
		// Just run through our array and look for the 
		// the element
		//
	bool ret = false;
	int ctr;
	for ( ctr = 0; ctr <= list.getlast(); ctr++ ) {
			//
			// All we can really do is do a simple comparison
			//
		if ( elt == list[ctr] ) {
			ret = true;
			break;	
		}
	} // FOR
	return ( ret );
}

/**
 * Ascending Insertion Sort
 * This is here so I can sort ExtArray<int>'s
 * 
 * @param list - the array to sort
 **/
void
CronTab::sort( ExtArray<int> &list )
{
	int ctr, ctr2, value;
	for ( ctr = 1; ctr <= list.getlast(); ctr++ ) {
		value = list[ctr];
    	ctr2 = ctr;
		while ( ( ctr2 > 0 ) && ( list[ctr2 - 1] > value ) ) {
			list[ctr2] = list[ctr2 - 1];
			ctr2--;
		} // WHILE
		list[ctr2] = value;
	} // FOR
	return;
}
