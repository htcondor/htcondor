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

#include <stdio.h>
#define ITEMS_TO_READ 1
#define BYTES_PER_ITEM 8
int main()
{
	FILE* f_in;
	int itemsRead;
	char buf[BUFSIZ];
	f_in = fopen("x_data.in", "r"); 
	if (!f_in) {
		printf("cannot open file x_data.in\n");
	} else {
			/* The line below must be uncommented for program to work
			   under condor */
		/* setbuf(f_in, buf); */
		itemsRead = fread( buf, BYTES_PER_ITEM, ITEMS_TO_READ,
						   f_in );
		if( itemsRead < 1 ) {
				/* This block gets executed under condor if setbuf not called */
			printf( "Cannot read from x_data.in\n" );
			printf( "return from fread = %d, error = %d, eof = %d\n",
					itemsRead, ferror(f_in), feof(f_in) );
		} else {
				/* This line is printed under conventional execution */
			printf( "read %d items from x_data.in\n", itemsRead );
		}
	}
	return 0;
}

