/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
		dprintf( D_ALWAYS, "%d  %s\n", tab[i].value, tab[i].name );
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
