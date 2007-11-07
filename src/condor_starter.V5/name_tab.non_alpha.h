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


 

#ifndef NAME_TAB_H
#define NAME_TAB_H

class NameTable;
class NameTableIterator;

class NAME_VALUE {
friend NameTable;
friend NameTableIterator;
private:
	int		value;
	char	*name;
};

class NameTable {
friend NameTableIterator;
public:
	NameTable( NAME_VALUE tab[] );
	char	*get_name( int value );
	int		get_value( int index );
	void 	display();
private:
	NAME_VALUE	*tab;
	int			n_entries;
};

class NameTableIterator {
public:
	NameTableIterator( NameTable & );
	int	operator() ();
private:
	int			cur;
	NameTable	*tab;
};
#endif
