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
