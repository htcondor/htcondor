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
#include "condor_debug.h"
#include "name_tab.h"


NameTable::NameTable( NAME_VALUE t[] )
{
	tab = t;
	for( n_entries=0; tab[n_entries].value != -1; n_entries++ )
		;
}

void
NameTable::display()
{
	int		i;

	for( i=0; i<n_entries; i++ ) {
		dprintf( D_ALWAYS, "%ld  %s\n", tab[i].value, tab[i].name );
	}
}


char *
NameTable::get_name( long value )
{
	int		i;

	for( i=0; i<n_entries; i++ ) {
		if( tab[i].value == value ) {
			return tab[i].name;
		}
	}
	return tab[i].name;
}

long
NameTable::get_value( int i )
{
	if( i >= 0 && i < n_entries ) {
		return tab[i].value;
	}
	return -1;
}

NameTableIterator::NameTableIterator( NameTable &table )
{
	tab = &table;
	cur = 0;
}

long
NameTableIterator::operator()()
{
	return tab->get_value( cur++ );
}
