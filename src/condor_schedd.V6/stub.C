#include  <stdio.h>
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
