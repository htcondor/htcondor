#include "condor_common.h"
#include "condor_config.h"
#include "condor_updown.h"
#include "condor_disk.h"

extern "C" int SetSyscalls (int) { return 0;}

int main (int argc, char *argv[])
{
	UpDown	upDown;
	User 	user;
	char	fileName[_POSIX_PATH_MAX];
	char 	*spool;

	// check usage
	if (argc != 1 && argc != 3)
	{
		fprintf (stderr, "Usage:  %s  [user newprio]\n", argv[0]);
		exit (1);
	}

	// setup
	config (NULL);
	if (!(spool = param ("SPOOL")))
	{
		fprintf (stderr, "Error:  Could not find \"SPOOL\" in config file.\n");
		exit (1);
	}
	sprintf (fileName, "%s/UserPrio", spool);


	// read the up down file
	File	file(fileName);
	if (file >> upDown)
	{
		fprintf (stderr, "Error:  Could not read in file %s\n", fileName);
		exit (1);
	}


	// check if the user wanted to display upDown user prios or update it
	if (argc == 1)
	{
		// display
		upDown.StartUserInfo ();
		printf ("%-20s %10s %10s %20s\n", "User", "#Running", "#Pending", 
				"Priority");
		while (!upDown.GetNextUserInfo (user))
		{
			// print out user info
			printf ("%-20s %10d %10d %20d\n", user.name, user.jobRunning, 
				user.jobPending, user.priority);
			
			// the iterator will leak memory unless the name is deleted
			delete [] user.name;
		}

		// done
		exit (0);
	}

	// update
	int newUserPrio = atoi (argv[2]);
	int oldUserPrio;
	int failed;

	oldUserPrio = upDown.GetUserPriority (argv[1], &failed);
	if (failed)
	{
		fprintf (stderr, "Error:  No such user '%s'\n", argv[1]);
		exit (1);
	}
	
	failed = upDown.ChangeUserPriority (argv[1], newUserPrio);
	if (failed)
	{
		// should not happen
		fprintf (stderr, "Error:  No such user '%s'\n", argv[1]);
		exit (1);
	}

	// save the file
	if (file << upDown)
	{
		fprintf (stderr, "Error:  Could not write out file %s\n", fileName);
		exit (1);
	}

	// success
	printf ("Changed (user) priority of user %s from %d to %d\n",
			argv[1], oldUserPrio, newUserPrio);

	exit (0);
}
