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
