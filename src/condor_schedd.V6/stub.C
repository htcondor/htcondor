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

#include "condor_common.h"
#include "debug.h"
#include "condor_updown.h"
#include "condor_disk.h"

// these are C callable functions
extern	UpDown	upDown;

extern "C" 
{
	void upDown_ClearUserInfo(void);
	void upDown_UpdateUserInfo(const char* , const int);
	int upDown_GetNoOfActiveUsers(void);
	int upDown_GetUserPriority(const char* , int* );
	void upDown_UpdatePriority(void);
	void upDown_SetParameters(const int , const int);
	void upDown_ReadFromFile(const char* );
	void upDown_WriteToFile(const char*);
	void upDown_Display(void);
	void dprintf(int, char*, char*, char*, char*, char*);
}


void upDown_ClearUserInfo(void)
{
	upDown.ClearUserInfo();
}
void upDown_UpdateUserInfo(const char* userName, const int status)
{
	upDown.UpdateUserInfo(userName, status) ;
}

int upDown_GetNoOfActiveUsers(void)
{
	return(upDown.GetNoOfActiveUsers());
}
int upDown_GetUserPriority(const char*  userName, int* status)
{
	int prio;
	prio = upDown.GetUserPriority(userName, status);
	return ( prio );
}
void upDown_UpdatePriority(void)	// updown algorithm
{
	upDown.UpdatePriority();	// updown algorithm
}
void upDown_SetParameters(const int stepSize, const int heavyUserPrio) 
{
	upDown.SetParameters(stepSize, heavyUserPrio);
}

void upDown_ReadFromFile(const char* filename)
{
	File	ff(filename);   // create a File object
	ff >> upDown;		// read from filename
}

void upDown_WriteToFile(const char* filename)
{
	File	ff(filename);   // create a File object
	ff << upDown;		// read from filename
}

void upDown_Display(void)
{
	upDown.Display();
}

void UpDown::Display(void)
{
	int i;
	dprintf(D_UPDOWN,"---------timeout-------------\n", 
						"","","","");
	dprintf(D_UPDOWN,"MaxUsers=%d ActiveUsers=%d Step=%d \n",
		(char *)maxUsers,(char *)activeUsers, (char *)step,"");
	for ( i=0; i < activeUsers; i++)
	{
		dprintf(D_UPDOWN,"Name:%s\tRun:%d Pend:%d Prio:%d\n",
			table[i].name,
			(char *)(table[i].jobRunning),
			(char *)(table[i].jobPending),
			(char *)(table[i].priority)
			);
	}
}
