/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Jim Basney
** 	        University of Wisconsin, Computer Sciences Dept.
** 
*/ 

#include <time.h>
#include <values.h>
#include <stdlib.h>

/* srand48, lrand48, and drand48 seem to be available on all Condor
   platforms.  -Jim B. */

static char initialized = 0;

/* (re)sets the seed for the random number generator -- may be useful
   for generating less predictable random numbers */
int set_seed(int seed)
{
	if (seed == 0) {
		seed = time(0);
	}

	srand48(seed);
	initialized = 1;

	return seed;
}

/* returns a random positive integer, trying to use best random number
   generator available on each platform */
int get_random_int()
{
	if (!initialized) {
		set_seed(0);
	}

	return (int) (lrand48() & MAXINT);
}

/* returns a random floating point number between 0.0 and 1.0, trying
   to use best random number generator available on each platform */
float get_random_float()
{
	if (!initialized) {
		set_seed(0);
	}

	return (float) drand48();
}
