#ifndef __UP_DOWN
#define __UP_DOWN


// status of job as recognised by the UpDown algo
const unsigned char	UpDownRunning   = 1;
const unsigned char	UpDownIdle      = 2;

// status returned by methods 
const int		Success	        = 0;
const int		Error	        = -1;

// operations supported in OppositeSigns method
const int		UpDownAdd       = 1;
const int		UpDownSubstract = 2;


// maximum length of user name
const unsigned int	MaxUserNameSize	=	64;

class User
{
public :
	char*	name;	    // name of user
	int	jobRunning; // no of jobs running for this user
	int	jobPending; // no of pending jobs by this user
	int	priority;   // priority of this user
};

class UpDown 
{
friend class File;         // File class is for storing this object in
			   // disk files
private :
	User*	table;	   // array of user entries
	int	maxUsers;  // max nomber of users possible
	int	activeUsers;// no of current users
	int	step;	   // step size for updown algo
	int	heavyUserPriority;// parameter for updown algo
	int   	OppositeSigns(const int a,const int b,const int operation);
	void	AllocateSpaceMoreUsers(int newsize);
			   // if there are more users than the MaxUsers 
			   // that was initially assumed, allocate more space
	int	f(const int  priority);
	int	g(const int  priority);
	int	h(const int  priority);
	int	l(const int  priority);
public  :
	UpDown(const int Maxusers=1024,const int step=1, 
		                 const int heavyUserPriority=120); 
	~UpDown();		  // destructor
	void SetParameters(const int step, const int heavyUserPriority);
				  // updates parameters used by algo
	void ClearUserInfo(void); // clears jobRunning and jobPending
				  // for all users
	void UpdateUserInfo(const char* userName, const int status);
				// updates jobRunning and jobPending
				// for this user in table
	int GetNoOfActiveUsers(void);         // no of active users
	void StartUserInfo(); // before GetNextUserInfo is called
	int GetNextUserInfo(User & data);
	int ChangeUserPriority(const char *name,int newPriority); 
	int GetUserPriority(const char *name,int*  status); 
				// get priority of user
				// status = Error if user is unknown
	void UpdatePriority(void);	      // updown algorithm

	// these are all for host  testing
	void Display(void);    // displays updown object

};

#endif /* __UP_DOWN */
