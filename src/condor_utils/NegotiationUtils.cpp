/*
 * Copyright 2008 Red Hat, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "condor_common.h"

#include "NegotiationUtils.h"
#include "condor_classad.h"

bool
ParseConcurrencyLimit(char *input, double &increment)
{
	char *sep = NULL;
	char *dot = NULL;
	bool valid = true;

	increment = 1;

	if (NULL != (sep = strchr(input, ':'))) {
		*sep = '\0';
		sep++;

		increment = strtod(sep, NULL);
	}

	if (increment <= 0) {
		increment = 1;
	}

	if ( NULL != (dot = strchr( input, '.' )) ) {
		*dot = '\0';
		if ( !IsValidAttrName( dot + 1 ) ) {
			valid = false;
		}
	}
	if ( !IsValidAttrName( input ) ) {
		valid = false;
	}
	if ( dot ) {
		*dot = '.';
	}

	return valid;
}


/* Submitter names cannot contain any whitespace, because the name is
 * used as part of a key for the Accountant ClassAd log.
 * Additional restrictions may be introduced later.
 */
bool IsValidSubmitterName( const char *name )
{
	while ( *name != '\0' ) {
		if ( isspace( name[0] ) ) {
			return false;
		}
		name++;
	}
	return true;
}
