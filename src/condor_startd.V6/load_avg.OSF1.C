/* 
** Copyright 1994 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Mike Litzkow
**
*/ 

#include "condor_common.h"
#include "condor_debug.h"
#include <sys/table.h>

static char *_FileName_ = __FILE__;


class LoadVector {
public:
	LoadVector();
	void	Update();
	float	Short()		{ return short_avg; }
	float	Medium()	{ return medium_avg; }
	float	Long()		{ return long_avg; }
private:
	float	short_avg;
	float	medium_avg;
	float	long_avg;
};

extern "C" {
	float calc_load_avg();
	int table(int id, int idx, char *addr, int nel, u_int lel );
	void get_k_vars();
}


#define TESTING 0
#if TESTING
int
main()
{
	LoadVector	vect;

	for(;;) {
		printf( "load avg: %f\n", calc_load_avg() );
		sleep( 1 );
	}
	return 0;
}

#define dprintf fprintf
#define D_ALWAYS stdout
#define D_FULLDEBUG stdout

#endif /* TESTING */


static LoadVector MyLoad;

extern "C" {

float
calc_load_avg()
{
	MyLoad.Update();
	dprintf( D_FULLDEBUG, "Load avg: %f %f %f\n", 
			 MyLoad.Short(), MyLoad.Medium(), MyLoad.Long() );
	return MyLoad.Short();
}

void
get_k_vars() {}
}


LoadVector::LoadVector()
{
	Update();
}

void
LoadVector::Update()
{
	struct tbl_loadavg	avg;

	if( table(TBL_LOADAVG,0,(char *)&avg,1,sizeof(avg)) < 0 ) {
		dprintf( D_ALWAYS, "Can't get load average info from kernel\n" );
		exit( 1 );
	}

	if( avg.tl_lscale ) {
		short_avg = avg.tl_avenrun.l[0] / (float)avg.tl_lscale;
		medium_avg = avg.tl_avenrun.l[1] / (float)avg.tl_lscale;
		long_avg = avg.tl_avenrun.l[2] / (float)avg.tl_lscale;
	} else {
		short_avg = avg.tl_avenrun.d[0];
		medium_avg = avg.tl_avenrun.d[1];
		long_avg = avg.tl_avenrun.d[2];
	}
}
