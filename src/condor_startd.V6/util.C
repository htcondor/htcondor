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


int
caInsert( ClassAd* target, ClassAd* source, const char* attr, int verbose )
{
	char buf[4096];
	buf[0] = '\0';
	ExprTree* tree;
	if( !attr ) {
		EXCEPT( "caInsert called with NULL attribute" );
	}
	if( !target || !source ) {
		EXCEPT( "caInsert called with NULL classad" );
	}

	tree = source->Lookup( attr );
	if( !tree ) {
		if( verbose ) {
			dprintf( D_ALWAYS, 
					 "caInsert: Can't find %s in source classad.\n", 
					 attr );
		}
		return FALSE;
	}
	tree->PrintToStr( buf );

	if( ! target->Insert( buf ) ) {
		dprintf( D_ALWAYS, "caInsert: Can't insert %s into target classad.\n",
				 attr );
		return FALSE;
	}		
	return TRUE;
}
