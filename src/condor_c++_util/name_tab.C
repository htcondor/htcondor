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
