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
#include <setjmp.h>

#ifdef VAX
#include <frame.h>
#define _CJBLEN	4
typedef long condor_jmp_buf[_CJBLEN];
#else VAX
#if defined(MC68020) && defined(BSD43)
#define condor_jmp_buf			jmp_buf
#define condor_setjmp(cenv)		_setjmp(cenv)
#define condor_longjmp(cenv)	_longjmp((cenv), 1)
#else defined(MC68020) && defined(BSD43)
#define condor_jmp_buf			jmp_buf
#define condor_setjmp(cenv)		setjmp(cenv)
#define condor_longjmp(cenv)	longjmp((cenv), 1)
#endif defined(MC68020) && defined(BSD43)
#endif
