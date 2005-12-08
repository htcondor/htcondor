/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2005, Condor Team, Computer Sciences Department,
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

#include "condor_crontab.h"
#include "condor_common.h"
#include "condor_classad.h"
#include "condor_debug.h"
#include "MyString.h"
#include "extArray.h"
//#include "RegExer.h"
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
// Setup the regex we will use to validate the cron
// parameters. Since C++ does not have static initialization
// blocks, we can't check here to make sure the object was 
// initialized properly. We will have to check in CronTab::init()
// Since the pattern is hardcoded, we better have a good reason
// for failing!
// 
//RegExer CronTab::regex( "[^\\/0-9"
//						CRONTAB_DELIMITER
//						CRONTAB_RANGE
//						CRONTAB_STEP
//						CRONTAB_WILDCARD
//						"\\/*]" );
						
//
// Default Constructor
// 
CronTab::CronTab()
{
	//
	// Do nothing!
	//
}

//
// Constructor
// This constuctor can be given a ClassAd from which the schedule
// values can be plucked out.
//
CronTab::CronTab( ClassAd *ad )
{
		//
		// Pull out the different parameters from the ClassAd
		//
	char buffer[512];
	for ( int ctr = 0; ctr < CRONTAB_FIELDS; ctr++ ) {
			//
			// First get out the parameter value
			//
		if ( ad->LookupString( this->attributes[ctr], buffer, 512 ) ) {
			dprintf( D_FULLDEBUG, "CronTab: Pulled out '%s' for %s\n",
							buffer, this->attributes[ctr] );
			this->parameters[ctr] = new MyString( buffer );
			//
			// The parameter is empty, we'll use the wildcard
			//
		} else {
			dprintf( D_FULLDEBUG, "CronTab: No attribute for %s, using wildcard\n",
							this->attributes[ctr] );
			this->parameters[ctr] = new MyString( CRONTAB_WILDCARD );
		}
	} // FOR
		//
		// Initialize
		//
	this->init();
}

//
// Constuctor
// Provided to add backwards capabilities for cronos.c
// Using integers really limits what can be done for scheduling
// The STAR constant has been replaced with CRONTAB_CRONOS_STAR
// Note that we are also not providing scheduling down to the second
//
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
		this->parameters[CRONTAB_MINUTES_IDX] = new MyString( minutes );
	}
	if ( hours == CRONTAB_CRONOS_STAR ) {
		this->parameters[CRONTAB_HOURS_IDX]	= new MyString( CRONTAB_WILDCARD );
	} else {
		this->parameters[CRONTAB_HOURS_IDX]	= new MyString( hours );
	}
	if ( days_of_month == CRONTAB_CRONOS_STAR ) {
		this->parameters[CRONTAB_DOM_IDX] = new MyString( CRONTAB_WILDCARD );
	} else {
		this->parameters[CRONTAB_DOM_IDX] = new MyString( days_of_month );
	}
	if ( months == CRONTAB_CRONOS_STAR ) {
		this->parameters[CRONTAB_MONTHS_IDX] = new MyString( CRONTAB_WILDCARD );
	} else {
		this->parameters[CRONTAB_MONTHS_IDX] = new MyString( months );
	}
	if ( days_of_week == CRONTAB_CRONOS_STAR ) {
		this->parameters[CRONTAB_DOW_IDX] = new MyString( CRONTAB_WILDCARD );
	} else {
		this->parameters[CRONTAB_DOW_IDX] = new MyString( days_of_week );
	}
		//
		// Initialize
		//
	this->init();
}
								
//
// Constructor
// Instead of being given a ClassAd, we can be given string values
// following the same format to create a cron schedule
//
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
	
		//
		// Initialize
		//
	this->init();
}

//
// Deconstructor
// Remove our array lists and parameters that we have
// dynamically allocated
//
CronTab::~CronTab() {
		//
		// 'delete []' didn't work so I'm doing this
		//
	int ctr;
	for ( ctr = 0; ctr < CRONTAB_FIELDS; ctr++ ) {
		if ( this->ranges[ctr] )		delete this->ranges[ctr];
		if ( this->parameters[ctr] )	delete this->parameters[ctr];
	} // FOR
}

//
// needsCronTab()
// This is a static helper method that is given a ClassAd object
// and will return true a CronTab scheduler object should be 
// created for it. This is for convience so that we are not
// instantiating a CronTab object for every ClassAd and then
// checking if the initialization failed
//
bool
CronTab::needsCronTab( ClassAd *ad ) {
		//
		// Go through and look to see if we have at least
		// one of the attributes in the ClassAd
		//
	bool ret = false;
	char buffer[512];
	int ctr;
	for ( ctr = 0; ctr < CRONTAB_FIELDS; ctr++ ) {
			//
			// As soon as we find one we can quit
			//
		if ( ad->LookupString( CronTab::attributes[ctr], buffer ) ) {
			ret = true;
			break;
		}
	} // FOR
	return ( ret );
}

//
// validate()
//
bool
CronTab::validate( ClassAd *ad, MyString &error ) {
	bool ret = true;
		//
		// Loop through all the fields in the ClassAd
		// If one our attribute fields is defined, then check
		// to make sure that it has valid syntax
		//
	char *buffer = NULL;
	int ctr;
	for ( ctr = 0; ctr < CRONTAB_FIELDS; ctr++ ) {
		MyString curError;
			//
			// If the validation fails, we keep going
			// so that they can see all the error messages about
			// their parameters at once
			//
		if ( ad->LookupString( CronTab::attributes[ctr], buffer ) ) {
			if ( !CronTab::validateParameter( ctr, buffer, curError ) ) {
				ret = false;
					//
					// Add this error message to our list
					//
				error += curError;
				error += "\n";
			}
			free( buffer );
		}
	} // FOR
	return ( ret );
}


//
// getError()
// Get back all the error messages that may have occured for
// this instantiation of the object
//
MyString
CronTab::getError() {
	return ( this->errorLog );
}

//
// validateParameter()
//
bool
CronTab::validateParameter( int attribute_idx, const char *parameter,
							MyString &error ) {
	bool ret = true;
		//
		// Make sure there are only valid characters 
		// in the parameter string
		//
	if ( false ) { //CronTab::regex.match( (char*)parameter ) ) {
		error  = "Invalid parameter value '";
		error += parameter;
		error += "' for ";
		error += CronTab::attributes[attribute_idx];
		ret = false;
	}
	return ( ret );
}

//
// init()
// Initializes our object to prepare it for being asked 
// what the next run time should be. If a parameter is invalid
// we will not allow them to query us for runtimes
//
void
CronTab::init() {
		//
		// Set the last runtime as empty
		//
	this->lastRunTime = CRONTAB_INVALID;
		//
		// We're invalid until we parse all the parameters
		//
	this->valid = false;
	
		//
		// Check to see if we were able to instantiate our
		// regex object statically
		//
//	if ( &CronTab::regex == NULL || CronTab::regex.getErrno() != 0 ) {
//		MyString errorMessage("CronTab: Failed to instantiate Regex - ");
//			//
//			// Pluck out the error message
//			//
//		if ( &CronTab::regex == NULL ) {
//			errorMessage += CronTab::regex.getStrerror();
//		} else {
//			errorMessage += "Unable to allocate memory";
//		}
//		EXCEPT( (char*)errorMessage.Value() );
//	}
	
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
			//
			// Instantiate our queue
			//
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

//
// nextRunTime()
// Returns the next execution time for our cron schedule from
// the current time
// The times are the number of seconds elapsed since 00:00:00 on
// January 1, 1970, Coordinated Universal Time (UTC)
//
long
CronTab::nextRunTime( ) {
		//
		// Call our own method but using the current time
		//
	return ( this->nextRunTime( (long)time( NULL ) ) );
}

//
// nextRunTime()
// Returns the next execution time for our cron schedule from the
// provided timestamp. We will only return an execution time if we
// have been initialized properly and the valid flag is true. The 
// timestamp that we calculate here will be stored and can be accessed
// again with lastRun().
// The times are the number of seconds elapsed since 00:00:00 on
// January 1, 1970, Coordinated Universal Time (UTC)
//
// This may be useful if somebody wants to figure out the next 5 runs
// for a job if they keep feeding us the times we return
//
long
CronTab::nextRunTime( long timestamp ) {
	long runtime = CRONTAB_INVALID;
	struct tm	*tm;

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
		//
	if ( this->matchFields( fields, match, CRONTAB_FIELDS - 2 ) ) {
		struct tm matchTime;
		matchTime.tm_sec	= 0;
		matchTime.tm_min	= match[CRONTAB_MINUTES_IDX];
		matchTime.tm_hour	= match[CRONTAB_HOURS_IDX];
		matchTime.tm_mday	= match[CRONTAB_DOM_IDX];
		matchTime.tm_mon	= match[CRONTAB_MONTHS_IDX] - 1;
		matchTime.tm_year	= match[CRONTAB_YEARS_IDX] - 1900;
		matchTime.tm_isdst  = tm->tm_isdst;
		runtime = (long)mktime( &matchTime );
		
			//
			// Make sure that our next runtime is in the future
			// and never in the past. The runtime can be equal
			// to the current time because the current time 
			// may be rounded up to the next minute
			//
		if ( runtime < timestamp ) {
			dprintf( D_FULLDEBUG, "CronTab: Generated a runtime that is in "
								  "the past (%d < %d)\n",	(int)runtime,
							   								(int)timestamp );
			runtime = CRONTAB_INVALID;
		}
		
		//
		// As long as we are configured properly, we should always
		// be able to find a match.
		//
	} else {
		dprintf( D_FULLDEBUG, "CronTab: Failed to find a match for timestamp %d\n",
														(int)timestamp );
	}
	
	this->lastRunTime = runtime;
	return ( runtime );
}

//
// matchFields()
// Important helper function that actually does the grunt work of
// find the next runtime. We need to be given an array of the different
// time fields so that we can match the current time with different 
// parameters. The function will recursively call itself until
// it can find a match at the MINUTES level. If one recursive call
// cannot find a match, it will return false and the previous level
// will increment its range index by one and try to find a match. On the 
// the subsequent call useFirst will be set to true meaning that the level
// does not need to look for a value in the range that is greater than or
// equal to the current time value. If the failing returns to the MONTHS level, 
// that means we've exhausted all our possiblities from the current time to the
// the end of the year. So we'll increase the year by 1 and try the same logic again
// using January 1st as the starting point.
// The array match will contain the timestamp information of the job's
// next run time
//
bool
CronTab::matchFields( int *curTime, int *match, int ctr, bool useFirst )
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
	match[ctr] = -1;
	
		//
		// Special Day of Week Handling
		// In order to get the day of week stuff to work, we have 
		// to insert all the days of the month for the matching days of the
		// week in the range.
		//
	ExtArray<int> *curRange = NULL;
	if ( ctr == CRONTAB_DOM_IDX ) {
			//
			// We have to take the current month & year
			// and convert the days of the week range into
			// days of the month
			//
		curRange = new ExtArray<int>( *this->ranges[ctr] );
		int firstDay = dayOfWeek(	match[CRONTAB_MONTHS_IDX],
									1,
									match[CRONTAB_YEARS_IDX] );
		int ctr2, cnt;
		for ( ctr2 = 0, cnt = this->ranges[CRONTAB_DOW_IDX]->getlast();
				ctr2 <= cnt;
				ctr2++ ) {
				//
				// Now figure out all the days for this specific day of the week
				//
			int day = (this->ranges[CRONTAB_DOW_IDX]->getElementAt(ctr2) - firstDay) + 1;
			while ( day <= CRONTAB_DAY_OF_MONTH_MAX ) {
				if ( day > 0 && !this->contains( *curRange, day ) ) {
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
		curRange = this->ranges[ctr];
	}

		//
		// Find the first match for this field
		// If our value isn't in the list, then we'll take the next one
		//
	bool ret = false;
	int rangeIdx, cnt;
	for ( rangeIdx = 0, cnt = this->ranges[ctr]->getlast();
		  rangeIdx <= cnt;
		  rangeIdx++ ) {
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
		int value = this->ranges[ctr]->getElementAt( rangeIdx );
		if ( useFirst || value >= curTime[ctr] ) {
				//
				// If this value is greater than the current time value,
				// ask the next level to useFirst
				//
			if ( value > curTime[ctr] ) nextUseFirst = true;
				//
				// Day of Month Check!
				// If this day doesn't exist in this month for
				// the current year in our search, we have to skip it
				//
			if ( ctr == CRONTAB_DOM_IDX ) {
				int maxDOM = daysInMonth( 	match[CRONTAB_MONTHS_IDX],
											match[CRONTAB_YEARS_IDX] );
				if ( value > maxDOM ) {
					continue;
				}
			}
			match[ctr] = value;
				//
				// If we're the last field for the crontab (i.e. minutes),
				// then we should have a valid time now!
				//
			if ( ctr == CRONTAB_MINUTES_IDX ) {
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
				ret = this->matchFields( curTime, match, ctr - 1, nextUseFirst );
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
	if ( !ret && ctr == CRONTAB_MONTHS_IDX ) {
		match[CRONTAB_YEARS_IDX]++;	
		ret = this->matchFields( curTime, match, ctr, true );
	}
	
		//
		// We only need to delete curRange if we had
		// instantiated a new object for it
		//
	if ( ctr == CRONTAB_DOM_IDX && curRange ) delete curRange;
	
	return ( ret );
}

//
// expandParameter()
// This is the logic that is used to take a parameter string
// given for a specific field in the cron schedule and expand it
// out into a range of int's that can be looked up quickly later on
// We must be given the index number of the field we're going to parse
// and a min/max for the range of values allowed for the attribute.
// If the parameter is invalid, we will report an error and return false
// This will prevent them from querying nextRunTime() for runtimes
//
bool
CronTab::expandParameter( int attribute_idx, int min, int max )
{
	MyString *param 		= this->parameters[attribute_idx];
	ExtArray<int> *list 	= this->ranges[attribute_idx];
	
		//
		// Make sure the parameter is valid
		// The validation method will have already printed out
		// the error message to the log
		//
	MyString error;
	if ( ! CronTab::validateParameter(	attribute_idx,
										(char*)param->Value(),
										error ) ) {
		dprintf( D_ALWAYS, "%s", error.Value() );
			//
			// Store the error in case they want to email
			// the user to tell them that they goofed
			//
		CronTab::errorLog += error;
		CronTab::errorLog += "\n";
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
	param->Tokenize();
	const char *_token;
	while ( ( _token = param->GetNextToken( CRONTAB_DELIMITER, true ) ) != NULL ) {
		MyString token( _token );
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
				cur_step = atoi( stepStr.Value() );
			}
				//
				// Now that we have the denominator, put the numerator back
				// as the token. This makes it easier to parse later on
				//
			token = *new MyString( _numerator );
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
			value = atoi( _temp->Value() );
			cur_min = ( value >= min ? value : min );
			delete _temp;
				//
				// Max
				//
			_temp = new MyString( token.GetNextToken( CRONTAB_RANGE, true ) );
			_temp->trim();
			value = atoi( _temp->Value() );
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
			int value = atoi(token.Value());
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
			if ( ( ( temp % cur_step ) == 0 ) && !this->contains( *list, temp ) ) {
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

//
// contains()
// Just checks to see if a value is in an array
//
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

//
// sort()
// Ye' Olde Insertion Sort!
// This is here so I can sort ExtArray<int>'s
//
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

