/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
/*#ifndef _PROC_OBJ_TMPL_H
#define _PROC_OBJ_TMPL_H */

/*
  Copy an array of some scalar type object.  We're given a pointer by
  reference here (dst) so that we can allocate space for the array and
  make the pointer point to that space.  Once that's done, we just copy
  over the n objects in the array.

  We use this for integers, rusage structures, and pipe descriptors.
*/
template <class Type>
static void
copy_array( Type * &dst, Type *src, int n )
{
	int		i;

	dst = new Type [ n ];
	for( i=0; i<n; i++ ) {
		dst[i] = src[i];
	}
}


/*
  Display those elements which are common to both the V2_PROC and V3_PROC
  structures.
*/
template <class Type>
static void
display_common_elements( Type *p )
{
	d_int( "version_num", p->version_num );
	d_int( "prio", p->prio );
	d_string( "owner", p->owner );
	d_date( "q_date", p->q_date );

	if( p->status == COMPLETED ) {
		if( p->completion_date == 0 ) {
			d_msg( "completion_date", "not recorded" );
		} else {
			d_date( "completion_date", p->completion_date );
		}
	} else {
		d_msg( "completion_date", "not completed" );
	}

	d_enum( Status, "status", p->status );
	d_int( "prio", p->prio );
	d_enum( Notifications, "notification", p->notification );

	if( p->image_size ) {
		d_int( "image_size", p->image_size );
	} else {
		d_msg( "image_size", "not recorded" );
	}

	d_string( "env", p->env );
	d_string( "rootdir", p->rootdir );
	d_string( "iwd", p->iwd );
	d_string( "requirements", p->requirements );
	d_string( "preferences", p->preferences );
	d_rusage( "local_usage", p->local_usage );
}
#if !defined(AIX32)
template static void display_common_elements(V2_PROC *);
#endif
/*#endif */
