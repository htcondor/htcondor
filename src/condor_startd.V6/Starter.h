/*
  This file defines the Starter class, used by the startd to keep
  track of a resource's starter process.  

  Written 10/6/97 by Derek Wright <wright@cs.wisc.edu>
*/

#ifndef _CONDOR_STARTD_STARTER_H
#define _CONDOR_STARTD_STARTER_H

#if !defined( WIN32 )
    extern "C" void killkids( pid_t, int );
#endif

typedef struct jobstartinfo {
	char *ji_hname;
	int ji_sock1;
	int ji_sock2;
} start_info_t;

class Starter
{
public:
	Starter();
	~Starter();
	char*	name() {return s_name;};
	void	setname(char*);
	int		kill(int);
	int		killpg(int);
	void	killkids(int);
	int		spawn(start_info_t*);
	void	exited();
	int		pid() {return s_pid;};
	bool	active();
private:
	pid_t	s_pid;
	char*	s_name;
	int		reallykill(int, int);
};

int exec_starter(char*, char*, int, int);

#endif _CONDOR_STARTD_STARTER_H
