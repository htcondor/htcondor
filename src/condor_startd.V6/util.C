#include "startd.h"
static char *_FileName_ = __FILE__;

void
check_perms()
{
	struct stat st;
	mode_t mode;

	if (stat(exec_path, &st) < 0) {
		EXCEPT("stat exec path");
	}

	if ((st.st_mode & 0777) != 0777) {
		dprintf(D_FULLDEBUG, "Changing permission on %s\n", exec_path);
		mode = st.st_mode | 0777;
		if (chmod(exec_path, mode) < 0) {
			EXCEPT("chmod exec path");
		}
	}
}


void
cleanup_execute_dir()
{
	char buf[2048];

#if defined(WIN32)
	sprintf(buf, "del /f /s /q %.256s\\condor_exec*",
			exec_path);
	system(buf);
	sprintf(buf, "rmdir /s /q %.256s\\dir_1",
			exec_path);
	system(buf);
#else
	sprintf(buf, "/bin/rm -rf %.256s/condor_exec* %.256s/dir_*",
			exec_path, exec_path);
	system(buf);
#endif
}


float
compute_rank( ClassAd* mach_classad, ClassAd* req_classad ) 
{
	float rank;

	if( mach_classad->EvalFloat( ATTR_RANK, req_classad, rank ) == 0 ) {
		dprintf( D_ALWAYS, "Error evaluating rank.\n" );
		rank = 0;
	}
	return rank;
}


int
wants_vacate( Resource* rip )
{
	int want_vacate = 0;
	if( rip->r_cur->universe() == VANILLA ) {
		if( (rip->r_classad)->EvalBool( "WANT_VACATE_VANILLA",
										rip->r_cur->ad(),
										want_vacate ) == 0) { 
			want_vacate = 1;
		}
	} else {
		if( (rip->r_classad)->EvalBool( "WANT_VACATE",
										rip->r_cur->ad(),
										want_vacate ) == 0) { 
			want_vacate = 1;
		}
	}
	return want_vacate;
}


int 
wants_suspend( Resource* rip )
{
	int want_suspend;
	if( rip->r_cur->universe() == VANILLA ) {
		if( (rip->r_classad)->EvalBool( "WANT_SUSPEND_VANILLA",
										rip->r_cur->ad(),
										want_suspend ) == 0) {  
			want_suspend = 1;
		}
	} else {
		if( (rip->r_classad)->EvalBool( "WANT_SUSPEND",
										rip->r_cur->ad(),
										want_suspend ) == 0) { 
			want_suspend = 1;
		}
	}
	return want_suspend;
}


int
eval_kill( Resource* rip )
{
	int tmp;
	if( rip->r_cur->universe() == VANILLA ) {
		if( ((rip->r_classad)->EvalBool("KILL_VANILLA",
										rip->r_cur->ad(), tmp) ) == 0 ) {  
			if( ((rip->r_classad)->EvalBool("KILL",
											rip->r_classad,
											tmp) ) == 0 ) { 
				EXCEPT("Can't evaluate KILL");
			}
		}
	} else {
		if( ((rip->r_classad)->EvalBool("KILL",
										rip->r_cur->ad(), 
										tmp)) == 0 )	{ 
			EXCEPT("Can't evaluate KILL");
		}	
	}
	return tmp;
}


int
eval_vacate( Resource* rip )
{
	int tmp;
	if( rip->r_cur->universe() == VANILLA ) {
		if( ((rip->r_classad)->EvalBool("VACATE_VANILLA",
										rip->r_cur->ad(), 
										tmp)) == 0 ) {
			if( ((rip->r_classad)->EvalBool("VACATE",
											rip->r_cur->ad(), 
											tmp)) == 0 ) {
				EXCEPT("Can't evaluate VACATE");
			}
		}
	} else {
		if( ((rip->r_classad)->EvalBool("VACATE",
										rip->r_cur->ad(), 
										tmp)) == 0 ) {
			EXCEPT("Can't evaluate VACATE");
		}
	}
	return tmp;
}


int
eval_suspend( Resource* rip )
{
	int tmp;
	if( rip->r_cur->universe() == VANILLA ) {
		if( ((rip->r_classad)->EvalBool("SUSPEND_VANILLA",
										rip->r_cur->ad(),
										tmp)) == 0 ) {
			if( ((rip->r_classad)->EvalBool("SUSPEND",
											rip->r_cur->ad(),
											tmp)) == 0 ) {
				EXCEPT("Can't evaluate SUSPEND");
			}
		}
	} else {
		if( ((rip->r_classad)->EvalBool("SUSPEND",
										rip->r_cur->ad(),
										tmp)) == 0 ) {
			EXCEPT("Can't evaluate SUSPEND");
		}
	}
	return tmp;
}


int
eval_continue( Resource* rip )
{
	int tmp;
	if( rip->r_cur->universe() == VANILLA ) {
		if( ((rip->r_classad)->EvalBool("CONTINUE_VANILLA",
										rip->r_cur->ad(),
										tmp)) == 0 ) {
			if( ((rip->r_classad)->EvalBool("CONTINUE",
											rip->r_cur->ad(),
											tmp)) == 0 ) {
				EXCEPT("Can't evaluate CONTINUE");
			}
		}
	} else {	
		if( ((rip->r_classad)->EvalBool("CONTINUE",
										rip->r_cur->ad(),
										tmp)) == 0 ) {
			EXCEPT("Can't evaluate CONTINUE");
		}
	}
	return tmp;
}


int
create_port(int* sock)
{
	struct sockaddr_in sin;
	int len = sizeof sin;

	memset((char *)&sin,0,sizeof sin);
	sin.sin_family = AF_INET;
	
#if 0
	char address[100];
	if( IP ) {
		sprintf(address, "<%s:%d>", IP, 0);
		string_to_sin(address, &sin);
	} else {
	}
#endif 0
		
	sin.sin_port = 0;

	if( (*sock=socket(AF_INET,SOCK_STREAM,0)) < 0 ) {
		EXCEPT( "socket" );
	}

	if( bind(*sock,(struct sockaddr *)&sin, sizeof sin) < 0 ) {
		EXCEPT( "bind" );
	}

	if( listen(*sock,1) < 0 ) {
		EXCEPT( "listen" );
	}

	if( getsockname(*sock,(struct sockaddr *)&sin, &len) < 0 ) {
		EXCEPT("getsockname");
	}

	return (int)ntohs((u_short)sin.sin_port);
}


int
config_classad( Resource* rip )
{
	config( rip->r_classad, "STARTD" );
	rip->init_classad();
	return TRUE;
}


void
log_ignore( int cmd, State s ) 
{
	dprintf( D_ALWAYS, "Got %s while in %s state, ignoring.\n", 
			 command_to_string(cmd), state_to_string(s) );
}


char*
command_to_string( int cmd )
{
	switch( cmd ) {
	case CONTINUE_CLAIM:
		return "continue_claim";
	case SUSPEND_CLAIM:
		return "suspend_claim";
	case DEACTIVATE_CLAIM:
		return "deactivate_claim";
	case DEACTIVATE_CLAIM_FORCIBLY:
		return "deactivate_claim_forcibly";
	case MATCH_INFO:
		return "match_info";
	case ALIVE:
		return "alive";
	case REQUEST_CLAIM:
		return "request_claim";
	case RELEASE_CLAIM:
		return "release_claim";
	case ACTIVATE_CLAIM:
		return "activate_claim";
	case GIVE_STATE:
		return "give_state";
	case PCKPT_ALL_JOBS:
		return "pckpt_all_jobs";
	case VACATE_ALL_CLAIMS:
		return "vacate_all_claims";
	default:
		return "unknown";
	}
	return "error";
}


int
reply( Stream* s, int cmd )
{
	s->encode();
	if( !s->code( cmd ) || !s->end_of_message() ) {
		return FALSE;
	} else {
		return TRUE;
	}
}

