/***************************************************************
 *
 * Copyright (C) 1990-2010, Condor Team, Computer Sciences Department,
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
#include <ctype.h>
#include "stl_string_utils.h"
#include "startd.h"
#include "classad_cron_job.h"
#include "startd_cron_job.h"
#include <list>

// Override the lookup methods so that we can stuff in default values
StartdCronJobParams::StartdCronJobParams(
	const char			*job_name,
	const CronJobMgr	&mgr )
		: ClassAdCronJobParams( job_name, mgr )
{
}

bool
StartdCronJobParams::Initialize( void )
{
	if ( !ClassAdCronJobParams::Initialize() ) {
		return false;
	}

	std::string slots_str;
	Lookup( "SLOTS", slots_str );

	m_slots.clear();
	for (const auto& slot: StringTokenIterator(slots_str)) {
		if( !isdigit(*slot.c_str())) {
			dprintf( D_ALWAYS,
					 "Cron Job '%s': INVALID slot # '%s': ignoring slot list",
					 GetName(), slot.c_str() );
			m_slots.clear();
			break;
		}
		unsigned	slotno = atoi( slot.c_str() );
		m_slots.push_back( slotno );
	}

	// Print out the slots for D_FULLDEBUG
	if ( IsFulldebug(D_FULLDEBUG) ) {
		std::string	s;
		formatstr( s, "CronJob '%s' slots: ", GetName() );
		if ( m_slots.empty() ) {
			s += "ALL";
		}
		else {
			std::list<unsigned>::iterator iter;
			for( iter = m_slots.begin(); iter != m_slots.end(); iter++ ) {
				formatstr_cat( s, "%u ", *iter );
			}
		}
		dprintf( D_ALWAYS, "%s\n", s.c_str() );
	}


	// Set up my own metrics.
	const char * jobName = this->GetName();
	char * metricString = this->Lookup( "METRICS" );
	if( metricString != NULL && metricString[0] != '\0' ) {
		for (const auto& pair: StringTokenIterator(metricString)) {
			std::vector<std::string> tn = split(pair, ":");
			if (tn.size() < 2) continue;
			const char * metricType = tn[0].c_str();

			const char * attributeName = tn[1].c_str();
			if(! this->addMetric( metricType, attributeName )) {
				dprintf(    D_ALWAYS, "Unknown metric type '%s' for attribute "
				            "'%s' in monitor '%s', ignoring.\n", metricType,
				            attributeName, jobName );
			} else {
				dprintf(    D_FULLDEBUG, "Added %s as %s metric for %s job\n",
				            attributeName, metricType, jobName );
			}
		}
	}
	if (metricString) free( metricString );


	return true;
};

bool
StartdCronJobParams::InSlotList( unsigned slot ) const
{
	if ( m_slots.empty() ) {
		return true;
	}

	std::list<unsigned>::const_iterator iter;
	for( iter = m_slots.begin(); iter != m_slots.end(); iter++ ) {
		if ( slot == *iter ) {
			return true;
		}
	}
	return false;
}

double sumOfTwoDoubles( double a, double b ) { return a + b; }
double maxOfTwoDoubles( double a, double b ) { return a > b ? a : b; }

bool
StartdCronJobParams::addMetric( const char * metricType, const char * resourceName ) {
	std::string attributeName;

	if( strncasecmp( "SUM", metricType, 3 ) == 0 ) {
		formatstr( attributeName, "Uptime%sSeconds", resourceName );
		metrics.insert( make_pair( attributeName, Metric( sumOfTwoDoubles, "+" ) ) );
	} else if( strncasecmp( "PEAK", metricType, 4 ) == 0 ) {
		formatstr( attributeName, "Uptime%sPeakUsage", resourceName );
		metrics.insert( make_pair( attributeName, Metric( maxOfTwoDoubles, ">" ) ) );
	} else {
		return false;
	}
	return true;
}

bool
StartdCronJobParams::getMetric( const std::string & attributeName, Metric & m ) const {
	auto i = metrics.find( attributeName );
	if( i == metrics.end() ) { return false; }
	m = i->second;
	return true;
}

bool
StartdCronJobParams::isMetric( const std::string & attributeName ) const {
	return metrics.find( attributeName ) != metrics.end();
}

bool
splitAttributeName( const std::string & attributeName,
					      std::string & resourceName,
					      std::string & suffix ) {
	if( attributeName.find( "Uptime" ) != 0 ) { return false; }
	std::string rName = attributeName.substr( 6 );

	size_t eORN;
	std::string suffixCandidate;

	suffixCandidate = "Seconds";
	eORN = rName.rfind( suffixCandidate );
	if( eORN != std::string::npos ) {
		if( eORN + suffixCandidate.length() != rName.length() ) { return false; }
		resourceName = rName.substr( 0, eORN );
		suffix = suffixCandidate;
		return true;
	}

	suffixCandidate = "PeakUsage";
	eORN = rName.rfind( "PeakUsage" );

	if( eORN != std::string::npos ) {
		if( eORN + suffixCandidate.length() != rName.length() ) { return false; }
		resourceName = rName.substr( 0, eORN );
		suffix = suffixCandidate;
		return true;
	}

	return false;
}

bool
StartdCronJobParams::getResourceNameFromAttributeName( const std::string & attributeName, std::string & resourceName ) {
	std::string suffix;
	if( splitAttributeName( attributeName, resourceName, suffix ) ) {
		return true;
	}
	return false;
}

bool
StartdCronJobParams::attributeIsSumMetric( const std::string & attributeName ) {
	std::string resourceName, suffix;
	if( splitAttributeName( attributeName, resourceName, suffix ) ) {
		if( suffix == "Seconds" ) { return true; }
	}
	return false;
}

bool
StartdCronJobParams::attributeIsPeakMetric( const std::string & attributeName ) {
	std::string resourceName, suffix;
	if( splitAttributeName( attributeName, resourceName, suffix ) ) {
		if( suffix == "PeakUsage" ) { return true; }
	}
	return false;
}
