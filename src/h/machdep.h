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
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 


#ifdef IRIX331
#include <sys/sysmacros.h>
#else
#include <machine/vmparam.h>
#endif

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

#if defined(IRIX331)
#define STACKGROW 0
#endif

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
