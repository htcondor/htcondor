/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


 


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
