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

