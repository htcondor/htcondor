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

 


/* MACROS for debugging */

#define INVALID_CONTEXT(c); \
	{ char *fname = __FILE__ ; \
		dprintf( D_ALWAYS, "Invalid context, file %s, line %d\n", fname , __LINE__ );  \
		/*display_context( c );*/ }
#define INVALID_EXPR(e); \
	{ char *fname = __FILE__ ; \
		dprintf( D_ALWAYS, "Invalid expr, file %s, line %d\n", fname, __LINE__ ); \
		/*display_expr( e );*/ }
#define INVALID_ELEM(e); \
	{ char *fname = __FILE__ ; \
		dprintf( D_ALWAYS, "Invalid elem, file %s, line %d\n", fname, __LINE__ ); \
		/*display_elem( e );*/ }

