/* $Xorg: ccimake.c,v 1.4 2001/02/09 02:03:15 xorgcvs Exp $ */
/*

Copyright (c) 1993, 1994, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group .

*/

/* 
 * Warning:  This file must be kept as simple as posible so that it can 
 * compile without any special flags on all systems.  Do not touch it unless
 * you *really* know what you're doing.  Make changes in imakemdep.h, not here.
 */

#define CCIMAKE			/* only get imake_ccflags definitions */
#include "imakemdep.h"		/* things to set when porting imake */

#ifndef imake_ccflags
#define imake_ccflags "-O"
#endif

main()
{
	write(1, imake_ccflags, sizeof(imake_ccflags) - 1);
	exit(0);
}

