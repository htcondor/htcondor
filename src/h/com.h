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

 


	/* struct for command and option tables */
struct table {
	char	*t_string;
	int 	t_code;
};
typedef struct table TABLE;

	/* range of values in a TABLE */
struct rnge {
	int	high;
	int low;
};
typedef struct rnge RNGE;

#define to_lower(a) (((a)>='A'&&(a)<='Z')?((a)|040):(a))


#define DELSTR	"\177\177\177\177\177\177\177\177\177\177"
#define NOCOM	 		998
#define AMBIG			999

/* Commands */
#define		C_COMMAND		1
#define		C_ARGUMENTS		2
#define		C_ERROR			3
#define		C_INPUT			4
#define		C_OUTPUT		5

#define		C_PREFERENCES	7
#define		C_REQUIREMENTS	8
#define		C_PRIO			9
#define		C_ROOTDIR		10
#define		C_QUEUE			11
#define		C_NOTIFICATION	12
