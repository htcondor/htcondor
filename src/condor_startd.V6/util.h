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
#ifndef _UTIL_H
#define _UTIL_H

class ClassAd;
class Stream;
class Resource;

// Our utilities 
void	cleanup_execute_dir(int pid);
void	check_perms(void);
float	compute_rank( ClassAd*, ClassAd* );
int 	create_port( ReliSock* );
char*	command_to_string( int );
bool	reply( Stream*, int );
bool	refuse( Stream* );
bool	caInsert( ClassAd* target, ClassAd* source, const char* attr,
				  int verbose = 0 ); 
bool	configInsert( ClassAd* ad, const char* attr, bool is_fatal );
bool	configInsert( ClassAd* ad, const char* param_name, 
					  const char* attr, bool is_fatal );

		// Send given classads to the given sock.  If either pointer
		// is NULL, the class ad is not sent.  
int		send_classad_to_sock( int cmd, Sock* sock, ClassAd* pubCA,
							  ClassAd* privCA ); 
Resource* stream_to_rip( Stream* );

#endif /* _UTIL_H */
