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

#include <sys/types.h>


#if defined(AIX31) || defined(AIX32) || defined(OSF1)
#include <string.h>
#endif

#if defined(IRIX331)
void	*calloc();
char	*ctime();
void	*malloc();
char	*param();
void	*realloc();
char	*strcat();
char	*strcpy();
char	*strncpy();
int		sprintf();
#endif

#if 0
#if !defined(AIX31) && !defined(AIX32)  && !defined(IRIX331) && !defined(HPUX8) && !defined(OSF1)
char	*calloc();
char	*ctime();
char	*getwd();
char	*malloc();
char	*param();
char	*realloc();
char	*strcat();
char	*strcpy();
char	*strncpy();
char	*sprintf();
#endif
#endif

#if !defined( OSF1 ) && !defined(WIN32)
#ifndef htons
u_short	htons();
#endif htons

#ifndef ntohs
u_short	ntohs();
#endif ntohs

#ifndef htonl
u_long	htonl();
#endif htonl

#ifndef ntohl
u_long	ntohl();
#endif ntohl

#ifndef time
time_t	time();
#endif time
#endif	/* !OSF1 && !WIN32 */
