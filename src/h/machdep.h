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

 


#include <machine/vmparam.h>

/*
**	Machine dependent things are defined here
**	These constants are derived from a.out.h.
**	We assume here that the program is loaded
**	with separate text and data (ZMAGIC).
*/

#if defined(vax)
#define PAGSIZ		1024
#define SEGSIZE		PAGSIZ
/*
**	When the stack is grown, the system allocates 4 pages more
**	than actually needed.  Hopefully, this value can be found
**	in an include file somewhere (when pokey comes back up).
*/
#define STACKGROW	(ctob(4))

#define TXTOFF		PAGSIZ		/* Offset in a.out */
#endif defined(vax)

#if defined(MC68020) && defined(BSD43)
#define PAGSIZ		NBPG
#define SEGSIZE		PAGSIZ
/*
**	When the stack is grown, the system allocates 4 pages more
**	than actually needed.  Hopefully, this value can be found
**	in an include file somewhere (when pokey comes back up).
*/
#define STACKGROW	0

#define TXTOFF		PAGSIZ		/* Offset in a.out */
#endif MC68020 && BSD43

#ifdef sun
#define STACKGROW	0
#define TXTOFF		0
#endif sun

#ifdef MIPS
#define STACKGROW	0
#endif MIPS

#ifdef sequent
#define PAGSIZ		NBPG
#define SEGSIZ		PAGSIZ
#define STACKGROW	0

#define TXTOFF		0			/* Offset in a.out */
#endif sequent

#if defined(ibm032)
/*
**	This is really only important if checkpointing has been implemented
**	for the IBM032.  As of now (April 1st, 1989) it has not been.  These
**	constants will have to be determined for real when/if it is implemented.
*/

#define PAGSIZ		1024
#define SEGSIZE		PAGSIZ
/*
**	When the stack is grown, the system allocates 4 pages more
**	than actually needed.  Hopefully, this value can be found
**	in an include file somewhere (when pokey comes back up).
*/
#define STACKGROW	0

#define TXTOFF		PAGSIZ		/* Offset in a.out */
#endif defined(ibm032)

#ifndef N_TROFF
#define N_TROFF(x) \
	(N_TXTOFF(x) + (x).a_text + (x).a_data)
#endif N_TROFF

#ifndef N_DROFF
#define N_DROFF(x) \
	(N_TXTOFF(x) + (x).a_text + (x).a_data + (x).a_trsize)
#endif N_DROFF


#ifdef notdef

#if !defined(vax) && !defined(ntohl) && !defined(lint)
#define ntohl(x)	(x)
#define ntohs(x)	(x)
#define htonl(x)	(x)
#define htons(x)	(x)
#endif !defined(vax) && !defined(ntohl) && !defined(lint)

#if !defined(ntohl) && (defined(vax) || defined(lint))
u_short ntohs(), htons();
u_long  ntohl(), htonl();
#endif !defined(ntohl) && (defined(vax) || defined(lint))

#endif notdef
