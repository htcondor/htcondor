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
