#include "startd.h"
static char *_FileName_ = __FILE__;

Resource::Resource( Sock* coll_sock, Sock* alt_sock )
{
	r_classad = NULL;
	r_state = new ResState( this );
	r_starter = new Starter;
	r_cur = new Match;
	r_pre = NULL;
	r_reqexp = new Reqexp( &r_classad );
	r_attr = new ResAttributes( this );
	r_name = strdup( my_hostname() );

	this->coll_sock = coll_sock;
	this->alt_sock = alt_sock;
	up_tid = -1;
	poll_tid = -1;
}


Resource::~Resource()
{
	daemonCore->Cancel_Timer( up_tid );
	this->cancel_poll_timer();

	delete r_state;
	delete r_classad;
	delete r_starter;
	delete r_cur;		
	delete r_pre;		
	delete r_reqexp;   
	delete r_attr;		
	free( r_name );
}


int
Resource::release_claim()
{
	switch( state() ) {
	case claimed_state:
		return change_state( preempting_state, vacating_act );
		break;
	case matched_state:
		return change_state( owner_state );
		break;
	default:
		return FALSE;
	}
}


int
Resource::kill_claim()
{
	switch( state() ) {
	case claimed_state:
		return change_state( preempting_state, killing_act );
		break;
	case matched_state:
		return change_state( owner_state );
		break;
	default:
		return FALSE;
	}
}


int
Resource::got_alive()
{
	assert( state() == claimed_state );
	if( !r_cur ) {
		dprintf( D_ALWAYS, "Got keep alive with no current match object.\n" );
		return FALSE;
	}
	if( !r_cur->client() ) {
		dprintf( D_ALWAYS, "Got keep alive with no current client object.\n" );
		return FALSE;
	}
	r_cur->alive();
	return TRUE;
}


int
Resource::periodic_checkpoint()
{
	char tmp[80];
	if( state() != claimed_state ) {
		return FALSE;
	}
	if( r_starter->active() ) {
		if( r_starter->kill( DC_SIGPCKPT ) < 0 ) {
			return FALSE;
		}
	}
	sprintf( tmp, "%s=%d", ATTR_LAST_PERIODIC_CHECKPOINT, (int)time(NULL) );
	r_classad->Insert(tmp);
	return TRUE;
}


int
Resource::request_new_proc()
{
	if( state() != claimed_state ) {
		return FALSE;
	}
	if( r_starter->active() ) {
		if( r_starter->kill( DC_SIGHUP ) < 0 ) {
			return FALSE;
		} else {
			return TRUE;
		}
	}
	return FALSE;
}	


int
Resource::deactivate_claim()
{
	dprintf(D_ALWAYS, "Called deactivate_claim()\n");
	if( (state() == claimed_state) &&
		(r_starter->active()) ) {
		return r_starter->kill( DC_SIGSOFTKILL );
	} else {
		return FALSE;
	}
}


int
Resource::deactivate_claim_forcibly()
{
	dprintf(D_ALWAYS, "Called deactivate_claim_forcibly()\n");
	if( (state() == claimed_state) &&
		(r_starter->active()) ) {
		return r_starter->kill( DC_SIGHARDKILL );
	} else {
		return FALSE;
	}
}


int
Resource::change_state( State newstate )
{
	return r_state->change( newstate );
}


int
Resource::change_state( Activity newact )
{
	return r_state->change( newact );
}


int
Resource::change_state( State newstate, Activity newact )
{
	return r_state->change( newstate, newact );
}


bool
Resource::in_use()
{
	State s = state();
	if( s == owner_state || s == unclaimed_state ) {
		return false;
	} 
	return true;
}


void
Resource::starter_exited()
{
	dprintf( D_ALWAYS, "Starter pid %d has exited.\n",
			 r_starter->pid() );
	r_starter->exited();
	change_state( idle_act );
}


int
Resource::init_classad()
{
	char 	tmp[1024];
	char*	ptr;
	int		needs_free = 0;

	if( r_classad )	delete(r_classad);
	r_classad 		= new ClassAd();

		// Initialize classad types.
	r_classad->SetMyTypeName( STARTD_ADTYPE );
	r_classad->SetTargetTypeName( JOB_ADTYPE );

		// Read in config files and fill up local ad with all attributes 
	config( r_classad, 0 );

		// Name of this resource
	sprintf( tmp, "%s = \"%s\"", ATTR_NAME, r_name );
	r_classad->Insert( tmp );

		// Grab the hostname of this machine
	sprintf( tmp, "%s = \"%s\"", ATTR_MACHINE, my_hostname() );
	r_classad->Insert( tmp );

		// STARTD_IP_ADDR (needs to be in all ads)
	sprintf( tmp, "%s = \"%s\"", ATTR_STARTD_IP_ADDR, 
			 daemonCore->InfoCommandSinfulString() );
	r_classad->Insert( tmp );

		// Arch and OpSys.  Note: these will always return something,
		// since config() will insert values for these from uname() if
		// we don't have them in the config file.  
	ptr = param( "ARCH" );
	sprintf( tmp, "%s = \"%s\"", ATTR_ARCH, ptr );
	r_classad->Insert( tmp );
	free( ptr );

	ptr = param( "OPSYS" );
	sprintf( tmp, "%s = \"%s\"", ATTR_OPSYS, ptr );
	r_classad->Insert( tmp );
	free( ptr );

		// Insert state and activity attributes.
	r_state->update( r_classad );

		// Insert all resource attribute info.
	r_attr->init( r_classad );

		// If the UID domain is not set, use our hostname as the
		// default.   
	if( (ptr = param("UID_DOMAIN")) == NULL ) {
		ptr = my_full_hostname();
	} else {
			// If the UID domain is defined as "*", accept uids from
			// anyone.  
		if( ptr[0] == '*' ) {
			free( ptr );
			ptr = "";
		} else {
			needs_free = 1;
		}
	}
	sprintf( tmp, "%s = \"%s\"", ATTR_UID_DOMAIN, ptr );
	dprintf( D_ALWAYS, "%s\n", tmp );
	r_classad->Insert( tmp );
	if( needs_free ) {
		free( ptr );
		needs_free = 0;
	}

		// If the file system domain is not set, use our hostname as
		// the default. 
	if( (ptr = param("FILESYSTEM_DOMAIN")) == NULL ) {
		ptr = my_full_hostname();
	} else {
			// If the file system domain is defined as "*", assume we
			// share files with everyone.
		if( ptr[0] == '*' ) {
			free( ptr );
			ptr = "";
		} else {
			needs_free = 1;
		}
	}
	sprintf( tmp, "%s = \"%s\"", ATTR_FILE_SYSTEM_DOMAIN, ptr );
	dprintf( D_ALWAYS, "%s\n", tmp );
	r_classad->Insert( tmp );
	if( needs_free ) {
		free( ptr );
		needs_free = 0;
	}

		// Number of CPUs.  
	sprintf( tmp, "%s = %d", ATTR_CPUS, calc_ncpus() );
	r_classad->Insert( tmp );

	return TRUE;
}


void
Resource::update_classad()
{
	char line[100];

		// Recompute update only stats and fill in classad.
	r_attr->compute( UPDATE );
	r_attr->refresh( r_classad, UPDATE );
	
		// Put in state info
	r_state->update( r_classad );

		// Put in requirement expression info
	r_reqexp->update( r_classad );

		// Add currently useful capability.  If r_pre exists, we
		// need to advertise it's capability.  Otherwise, we should
		// get the capability from r_cur.
	if( r_pre ) {
		sprintf( line, "%s = \"%s\"", ATTR_CAPABILITY, r_pre->capab() );
	} else {
		sprintf( line, "%s = \"%s\"", ATTR_CAPABILITY, r_cur->capab() );
	}		
	r_classad->Insert( line );

		// Update current rank expression in local and public ads.
	sprintf( line, "%s = %f", ATTR_CURRENT_RANK, r_cur->rank() );
	r_classad->Insert( line );
}


void
Resource::timeout_classad()
{
		// Recompute statistics needed at every timeout and fill in classad
	r_attr->compute( TIMEOUT );
	r_attr->refresh( r_classad, TIMEOUT );
}


int
Resource::update()
{
	int rval1 = TRUE, rval2 = TRUE;
	ClassAd private_ad;
	ClassAd public_ad;

		// Recompute stats needed for updates and refresh classad. 
	this->update_classad();

	this->make_public_ad( &public_ad );
	this->make_private_ad( &private_ad );


		// Send class ads to collector
	if( (rval1 = 
		 send_classad_to_sock( coll_sock, &public_ad, &private_ad )) ) {
		dprintf( D_ALWAYS, "Sent update to the collector (%s)\n", 
				 collector_host );
	}  

		// If we have an alternate collector, send public CA there.
	if( alt_sock ) {
		if( (rval2 = send_classad_to_sock( alt_sock, &public_ad, NULL )) ) {
			dprintf( D_FULLDEBUG, 
					 "Sent update to the condor_view host (%s)\n",
					 condor_view_host );
		}
	}

		// Set a flag to indicate that we've done an update.
	did_update = TRUE;

		// Reset our timer so we update again after update_interval.
	daemonCore->Reset_Timer( up_tid, update_interval, 0 );

	return( rval1 && rval2 );
}


int
Resource::eval_and_update()
{
	did_update = FALSE;

		// Evaluate the state of this resource.
	eval_state();

		// If we didn't update b/c of the eval_state, we need to
		// actually do the update now.
	if( ! did_update ) {
		update();
	}
	return TRUE;
}


int
Resource::start_update_timer()
{
	up_tid = 
		daemonCore->Register_Timer( update_interval, 0,
									(TimerHandlercpp)eval_and_update,
									"eval_and_update", this );
	if( up_tid < 0 ) {
		EXCEPT( "Can't register DaemonCore timer" );
	}
	return TRUE;
}


int
Resource::start_poll_timer()
{
	if( poll_tid >= 0 ) {
			// Timer already started.
		return TRUE;
	}
	poll_tid = 
		daemonCore->Register_Timer( polling_interval,
									polling_interval, 
									(TimerHandlercpp)eval_state,
									"poll_resource", this );
	if( poll_tid < 0 ) {
		EXCEPT( "Can't register DaemonCore timer" );
	}
	return TRUE;
}


void
Resource::cancel_poll_timer()
{
	if( poll_tid != -1 ) {
		daemonCore->Cancel_Timer( poll_tid );
		poll_tid = -1;
		dprintf( D_FULLDEBUG, "Canceled polling timer.\n" );
	}
}


int
Resource::wants_vacate()
{
	int want_vacate = 0;
	if( r_cur->universe() == VANILLA ) {
		if( r_classad->EvalBool( "WANT_VACATE_VANILLA",
								   r_cur->ad(),
								   want_vacate ) == 0) { 
			want_vacate = 1;
		}
	} else {
		if( r_classad->EvalBool( "WANT_VACATE",
								   r_cur->ad(),
								   want_vacate ) == 0) { 
			want_vacate = 1;
		}
	}
	return want_vacate;
}


int 
Resource::wants_suspend()
{
	int want_suspend;
	if( r_cur->universe() == VANILLA ) {
		if( r_classad->EvalBool( "WANT_SUSPEND_VANILLA",
								   r_cur->ad(),
								   want_suspend ) == 0) {  
			want_suspend = 1;
		}
	} else {
		if( r_classad->EvalBool( "WANT_SUSPEND",
								   r_cur->ad(),
								   want_suspend ) == 0) { 
			want_suspend = 1;
		}
	}
	return want_suspend;
}


int
Resource::eval_kill()
{
	int tmp;
	if( r_cur->universe() == VANILLA ) {
		if( (r_classad->EvalBool( "KILL_VANILLA",
									r_cur->ad(), tmp) ) == 0 ) {  
			if( (r_classad->EvalBool( "KILL",
										r_classad,
										tmp) ) == 0 ) { 
				EXCEPT("Can't evaluate KILL");
			}
		}
	} else {
		if( (r_classad->EvalBool( "KILL",
									r_cur->ad(), 
									tmp)) == 0 ) { 
			EXCEPT("Can't evaluate KILL");
		}	
	}
	return tmp;
}


int
Resource::eval_vacate()
{
	int tmp;
	if( r_cur->universe() == VANILLA ) {
		if( (r_classad->EvalBool( "VACATE_VANILLA",
								   r_cur->ad(), 
								   tmp)) == 0 ) {
			if( (r_classad->EvalBool( "VACATE",
									   r_cur->ad(), 
									   tmp)) == 0 ) {
				EXCEPT("Can't evaluate VACATE");
			}
		}
	} else {
		if( (r_classad->EvalBool( "VACATE",
								   r_cur->ad(), 
								   tmp)) == 0 ) {
			EXCEPT("Can't evaluate VACATE");
		}
	}
	return tmp;
}


int
Resource::eval_suspend()
{
	int tmp;
	if( r_cur->universe() == VANILLA ) {
		if( (r_classad->EvalBool( "SUSPEND_VANILLA",
								   r_cur->ad(),
								   tmp)) == 0 ) {
			if( (r_classad->EvalBool( "SUSPEND",
									   r_cur->ad(),
									   tmp)) == 0 ) {
				EXCEPT("Can't evaluate SUSPEND");
			}
		}
	} else {
		if( (r_classad->EvalBool( "SUSPEND",
								   r_cur->ad(),
								   tmp)) == 0 ) {
			EXCEPT("Can't evaluate SUSPEND");
		}
	}
	return tmp;
}


int
Resource::eval_continue()
{
	int tmp;
	if( r_cur->universe() == VANILLA ) {
		if( (r_classad->EvalBool( "CONTINUE_VANILLA",
								   r_cur->ad(),
								   tmp)) == 0 ) {
			if( (r_classad->EvalBool( "CONTINUE",
									   r_cur->ad(),
									   tmp)) == 0 ) {
				EXCEPT("Can't evaluate CONTINUE");
			}
		}
	} else {	
		if( (r_classad->EvalBool( "CONTINUE",
								   r_cur->ad(),
								   tmp)) == 0 ) {
			EXCEPT("Can't evaluate CONTINUE");
		}
	}
	return tmp;
}


void
Resource::make_public_ad(ClassAd* pubCA)
{
	char*	expr;
	char*	ptr;
	char	tmp[1024];

	StringList	config_file_exprs;

	pubCA->SetMyTypeName( STARTD_ADTYPE );
	pubCA->SetTargetTypeName( JOB_ADTYPE );

	caInsert( pubCA, r_classad, ATTR_NAME );
	caInsert( pubCA, r_classad, ATTR_MACHINE );
	caInsert( pubCA, r_classad, ATTR_STARTD_IP_ADDR );
	caInsert( pubCA, r_classad, ATTR_ARCH );
	caInsert( pubCA, r_classad, ATTR_OPSYS );
	caInsert( pubCA, r_classad, ATTR_UID_DOMAIN );
	caInsert( pubCA, r_classad, ATTR_FILE_SYSTEM_DOMAIN );

	r_state->update( pubCA );
	r_attr->refresh( pubCA, PUBLIC );

	caInsert( pubCA, r_classad, ATTR_CPUS );
	caInsert( pubCA, r_classad, ATTR_MEMORY );
	caInsert( pubCA, r_classad, ATTR_AFS_CELL );

		// Put everything in the public classad from STARTD_EXPRS. 
	ptr = param("STARTD_EXPRS");
	if( ptr ) {
		config_file_exprs.initializeFromString( ptr );   
		free( ptr );
		config_file_exprs.rewind();   
		while( (ptr = config_file_exprs.next()) ) {
			expr = param( ptr );
			if( expr == NULL ) continue;
			sprintf( tmp, "%s = %s", ptr, expr );
			pubCA->Insert( tmp );
			free( expr );
		}
	}
		// Insert the currently active requirements expression.  If
		// it's just START, we need to insert that too.
	if( (r_reqexp->update( pubCA ) == ORIG) ) {
		caInsert( pubCA, r_classad, ATTR_START );
	}

	caInsert( pubCA, r_classad, ATTR_RANK );
	caInsert( pubCA, r_classad, ATTR_CURRENT_RANK );

	if( this->state() == claimed_state ) {
		caInsert( pubCA, r_classad, ATTR_CLIENT_MACHINE );
		caInsert( pubCA, r_classad, ATTR_REMOTE_USER );
		caInsert( pubCA, r_classad, ATTR_JOB_START );
		caInsert( pubCA, r_classad, ATTR_JOB_ID );
		caInsert( pubCA, r_classad, ATTR_JOB_UNIVERSE );
		caInsert( pubCA, r_classad, ATTR_LAST_PERIODIC_CHECKPOINT );
	}		

}


void
Resource::make_private_ad(ClassAd* privCA)
{
	privCA->SetMyTypeName( STARTD_ADTYPE );
	privCA->SetTargetTypeName( JOB_ADTYPE );

	caInsert( privCA, r_classad, ATTR_NAME );
	caInsert( privCA, r_classad, ATTR_STARTD_IP_ADDR );
	caInsert( privCA, r_classad, ATTR_CAPABILITY );
}


int
Resource::send_classad_to_sock( Sock* sock, ClassAd* pubCA, ClassAd* privCA ) 
{
	sock->encode();
	if( ! sock->put( UPDATE_STARTD_AD ) ) {
		dprintf( D_ALWAYS, "Can't send command\n");
		sock->end_of_message();
		return FALSE;
	}
	if( pubCA ) {
		if( ! pubCA->put( *sock ) ) {
			dprintf( D_ALWAYS, "Can't send public classad\n");
			sock->end_of_message();
			return FALSE;
		}
	}
	if( privCA ) {
		if( ! privCA->put( *sock ) ) {
			dprintf( D_ALWAYS, "Can't send private classad\n");
			sock->end_of_message();
			return FALSE;
		}
	}
	if( ! sock->end_of_message() ) {
		dprintf( D_ALWAYS, "Can't send end_of_message\n");
		return FALSE;
	}
	return TRUE;
}


/*
int
Resource::
*/
