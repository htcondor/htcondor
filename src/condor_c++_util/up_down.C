#include <iostream.h>
#include <fcntl.h>

#if 0
#	include <strings.h>
#else
#	include <string.h>
#endif

#include "condor_updown.h"

UpDown		upDown;

UpDown::UpDown(const int users,const int stepSize, 
		const int heavyUserPrio) // constructor
{
	table      	  = new User[users];
	maxUsers   	  = users;
	step	   	  = stepSize;
	heavyUserPriority = heavyUserPrio;
	activeUsers	  = 0;
}

UpDown::~UpDown()
{
	for ( int i =0; i < activeUsers; i++)
		delete table[i].name;
	delete table;
}

void UpDown::ClearUserInfo(void)
{
	for ( int i=0; i < activeUsers; i++)
	{
		table[i].jobRunning = 0;
		table[i].jobPending = 0;
	}
}

void UpDown::UpdateUserInfo(const char* userName, const int status)
				// updates jobRunning and jobPending
				// for this user in table
{
	char flag = 0;
	int i=0; 

	// search if this user is already existing
	for (i=0; i < activeUsers; i++)
	  if ( strncmp(table[i].name,userName,MaxUserNameSize-1) == 0 )
	  {
		flag = 1;
		break;
	  }
	// if a new user is to be added
	if ( flag  == 0 )
	{
		// check that space is available
		if ( activeUsers == maxUsers ) 
				AllocateSpaceMoreUsers(2*maxUsers+1);

		table[activeUsers].name = new char[strlen(userName)+1];
		strcpy(table[activeUsers].name, userName);
		table[activeUsers].jobRunning = 0;
		table[activeUsers].jobPending = 0;
		table[activeUsers].priority = 0;
		activeUsers++;
	}

	if ( status == UpDownRunning )
		table[i].jobRunning++;
	else if ( status == UpDownIdle )
		table[i].jobPending++;
}

int UpDown::GetNoOfActiveUsers(void)
{
	return ( activeUsers );
}

int UpDown::GetUserPriority(const char*  userName,int*  status)
{
	*status = Success;
	for ( int i =0; i < activeUsers; i++)
	  if ( strncmp(table[i].name, userName,MaxUserNameSize-1) == 0 )
		return table[i].priority;
	
	*status = Error; // User not found
	return Error;
}

int UpDown::ChangeUserPriority(const char*  userName,int  newPriority)
{
	for ( int i =0; i < activeUsers; i++)
	  if ( strncmp(table[i].name, userName,MaxUserNameSize-1) == 0 )
		{
			table[i].priority = newPriority;
			return Success;
		}
	
	return Error;
}

	
void UpDown::UpdatePriority(void)	// updown algorithm
{
   int temp;
   for ( int i = 0; i < activeUsers; i++ )
	if ( (table[i].jobRunning > 0) || ( table[i].jobPending > 0) )
	{
	  if ( table[i].jobRunning > 0 )
	     table[i].priority += table[i].jobRunning * f(table[i].priority );
	  else
	     table[i].priority -=  g(table[i].priority);
	}
	else if ( table[i].priority > 0 )
	{
		// this user has no running or pending jobs. hence his 
		// priority shud converge to zero. The Opposite signs test
		// is required because the Step value is not constant and
		// may change anytime.
		if ( OppositeSigns(table[i].priority, h(table[i].priority), UpDownSubstract))
			table[i].priority = 0;
	     	else table[i].priority -= h(table[i].priority);
	}
	else if (table[i].priority < 0 )
	{
		if ( OppositeSigns(table[i].priority, l(table[i].priority), UpDownAdd))
			table[i].priority = 0;
	     	else table[i].priority += l(table[i].priority);
	}
}

void UpDown::SetParameters(const int stepSize, const int heavyUserPrio) 
{
	step	   	  = stepSize;
	heavyUserPriority = heavyUserPrio;
}
int UpDown::OppositeSigns(const int a,const int b,const int operation)
{
	int result;
	switch (operation)
	{
	case UpDownAdd: 	
		result = a + b;
		if ( a>0  &&   result <0) return  1;
		if ( a<0  &&   result >0) return  1;
		return 0;

	case UpDownSubstract:
		result = a - b;
		if ( a>0  &&   result <0) return  1;
		if ( a<0  &&   result >0) return  1;
		return 0;

	}
	cerr << "wrong type in Opposite Signs";
	return ( Error);   // this is actually interpreted as non-zero
}

// allocates space for new users : similar to ralloc()
void UpDown::AllocateSpaceMoreUsers(int newsize)
{
	if ( newsize > maxUsers )
	{
		// allocate more space for storing data for users
		User* ptr = new User[newsize];

		// copy original data into new space
		for ( int j=0; j < activeUsers; j++)
			ptr[j] = table[j];
		delete table;  // free original space
		maxUsers = newsize;
		table = ptr;   // store pointer to new data
	}
}	

int UpDown::f(const int priority)
{
	return step;
}
int UpDown::g(const int priority)
{
	return step;
}
int UpDown::l(const int priority)
{
	return step;
}
int UpDown::h(const int priority)
{
	if (priority > heavyUserPriority) 
		return (priority - heavyUserPriority);
	return step;
}

static int xx_user ;	// this is for GetFirstUser& GetNextUser	

int UpDown::GetNextUserInfo(User & data)
{
	if ( xx_user < 0 ) return  Error ;
	if ( xx_user < activeUsers )
	{
		data.name = new char[strlen(table[xx_user].name)+1];
		strcpy(data.name, table[xx_user].name);
		data.jobRunning = table[xx_user].jobRunning;
		data.jobPending = table[xx_user].jobPending;
		data.priority   = table[xx_user].priority;
		xx_user++;
		return Success;
	}
	return Error;
}

void UpDown::StartUserInfo(void)
{
	xx_user = 0;
}


