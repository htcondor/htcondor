#include "startd.h"
static char *_FileName_ = __FILE__;


Resource::Resource( Sock* coll_sock, Sock* alt_sock )
{
	char tmp[100];

	r_classad = new ClassAd;
	r_private_classad = new ClassAd;
	r_state = new ResState( this );
	r_starter = new Starter;
	r_cur = new Match;
	r_pre = NULL;
	r_reqexp = new Reqexp( r_classad );
	r_attr = new ResAttributes( this );

	if( gethostname( tmp, 99 ) < 0 ) {
		EXCEPT( "gethostname" );
	}
	r_name = strdup( tmp );

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
	delete r_private_classad;
	delete r_starter;
	delete r_cur;		
	delete r_pre;		
	delete r_reqexp;   
	delete r_attr;		
	free( r_name );
}


int
Resource::init_classad()
{
	char 	tmp[80];
	int 	phys_memory = -1;
	char*	host_cell;
	char*	ptr;
	int		needs_free = 0;

		// Insert state and activity attributes into public ad
	r_state->init_classad( r_classad );

		// Name of this resource (needs to be in public and private ads)
	sprintf( tmp, "%s=\"%s\"", ATTR_NAME, r_name );
	r_classad->Insert( tmp );
	r_private_classad->Insert( tmp );

		// STARTD_IP_ADDR (needs to be in public and private ads)
	sprintf( tmp, "%s=\"%s\"", ATTR_STARTD_IP_ADDR, 
			 daemonCore->InfoCommandSinfulString() );
	r_classad->Insert( tmp );
	r_private_classad->Insert( tmp );

		// AFS cell
	host_cell = get_host_cell();
	if( host_cell ) {
		sprintf( tmp, "%s=\"%s\"", ATTR_AFS_CELL, host_cell );
		r_classad->Insert( tmp );
		dprintf( D_ALWAYS, "AFS_Cell = \"%s\"\n", host_cell );
		delete [] host_cell;
	} else {
		dprintf( D_ALWAYS, "AFS_Cell not set\n" );
	}

		// If the UID domain is not set, use our hostname as the
		// default.   
	if( (ptr = param("UID_DOMAIN")) == NULL ) {
		ptr = get_full_hostname();
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
	dprintf( D_ALWAYS, "%s = \"%s\"\n", ATTR_UID_DOMAIN, ptr );
	sprintf( tmp, "%s=\"%s\"", ATTR_UID_DOMAIN, ptr );
	r_classad->Insert( tmp );
	if( needs_free ) {
		free( ptr );
		needs_free = 0;
	}

		// If the file system domain is not set, use our hostname as
		// the default. 
	if( (ptr = param("FILESYSTEM_DOMAIN")) == NULL ) {
		ptr = get_full_hostname();
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
	dprintf( D_ALWAYS, "%s = \"%s\"\n", ATTR_FILE_SYSTEM_DOMAIN, ptr );
	sprintf( tmp, "%s=\"%s\"", ATTR_FILE_SYSTEM_DOMAIN, ptr );
	r_classad->Insert( tmp );
	if( needs_free ) {
		free( ptr );
		needs_free = 0;
	}

		// Number of CPUs.  
	sprintf( tmp, "%s=%d", ATTR_CPUS, calc_ncpus() );
	r_classad->Insert( tmp );

		// Physical memory
	phys_memory = calc_phys_memory();
	if( phys_memory > 0 ) {
		sprintf( tmp, "%s=%d", ATTR_MEMORY, phys_memory );
		r_classad->Insert( tmp );
	}

		// Nest
	if( (ptr = param("NEST")) != NULL ) {
		sprintf( tmp, "%s=\"%s\"", ATTR_NEST, ptr );
		r_classad->Insert( tmp );
		free( ptr );
	}

		// Current rank
	sprintf( tmp, "%s=0", ATTR_CURRENT_RANK );
	r_classad->Insert( tmp );

	return TRUE;
}


int
Resource::update_classad()
{
	ClassAd* cp = r_classad;
	char line[80];

		// Recompute update-only statistics
	r_attr->update();
	
		// Refresh the classad with statistics that are only needed on updates
	sprintf( line, "%s=%lu", ATTR_VIRTUAL_MEMORY, r_attr->virtmem() );
 	cp->Insert( line ); 

	sprintf( line, "%s=%lu", ATTR_DISK, r_attr->disk() );
	cp->Insert( line ); 
  
	// KFLOPS and MIPS are only conditionally computed; thus, only
	// advertise them if we computed them.
	if ( r_attr->kflops() > 0 ) {
		sprintf( line, "%s=%d", ATTR_KFLOPS, r_attr->kflops() );
		cp->Insert( line );
	}
	if ( r_attr->mips() > 0 ) {
		sprintf( line, "%s=%d", ATTR_MIPS, r_attr->mips() );
		cp->Insert( line );
	}

		// Add capability to private classad.  If r_pre exists, we
		// need to advertise it's capability.  Otherwise, we should
		// get the capability from r_cur.
	if( r_pre ) {
		sprintf( line, "%s=\"%s\"", ATTR_CAPABILITY, r_pre->capab() );
	} else {
		sprintf( line, "%s=\"%s\"", ATTR_CAPABILITY, r_cur->capab() );
	}		
	r_private_classad->Insert( line );

		// Update current rank expression in classad
	sprintf( line, "%s=%f", ATTR_CURRENT_RANK, r_cur->rank() );
	cp->Insert( line );

	return TRUE;
}


int
Resource::timeout_classad()
{
	ClassAd* cp = r_classad;
	char line[80];
  
		// Recompute statistics needed at every timeout
	r_attr->timeout();

	sprintf( line, "%s=%f", ATTR_LOAD_AVG, r_attr->load() );
	cp->Insert(line);

	sprintf( line, "%s=%f", ATTR_CONDOR_LOAD_AVG, r_attr->condor_load() );
	cp->Insert(line);

	sprintf(line, "%s=%d", ATTR_KEYBOARD_IDLE, r_attr->idle() );
	cp->Insert(line); 
  
	// ConsoleIdle cannot be determined on all platforms; thus, only
	// advertise if it is not -1.
	if( r_attr->console_idle() != -1 ) {
		sprintf( line, "%s=%d", ATTR_CONSOLE_IDLE, 
				 r_attr->console_idle() );
		cp->Insert(line); 
	}

	return TRUE;
}


int
Resource::update()
{
	int rval1 = TRUE, rval2 = TRUE;

		// Recompute stats needed only for updates and refresh classad. 
	this->update_classad();

		// Send class ad to collector
	rval1 = send_classad_to_sock( coll_sock, TRUE );
	if( rval1 ) {
		dprintf( D_ALWAYS, "Sent update to the collector (%s)\n", 
				 collector_host );
	}

		// If we have an alternate collector, send CA there.
	if( alt_sock ) {
		if( (rval2 = send_classad_to_sock( alt_sock, FALSE )) ) {
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
									(TimerHandler)timeout_and_update,
									"timeout_and_update" );
	if( up_tid < 0 ) {
		EXCEPT( "Can't register DaemonCore timer" );
	}

	if( ! daemonCore->Register_DataPtr( (void*) this ) ) {
		EXCEPT( "Can't register update timer data pointer" );
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
									(TimerHandler)poll_resource,
									"poll_resource" );
	if( poll_tid < 0 ) {
		EXCEPT( "Can't register DaemonCore timer" );
	}

	if( ! daemonCore->Register_DataPtr( (void*) this ) ) {
		EXCEPT( "Can't register update timer data pointer" );
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
Resource::change_state( State newstate )
{
	return r_state->change( newstate );
}


int
Resource::change_state( Activity newact )
{
	return r_state->change( newact );
}


bool
Resource::in_use()
{
	State s = state();
	if( s == owner_state || s == unclaimed_state ) {
		return false;
	} else {
		return true;
	}
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
Resource::send_classad_to_sock( Sock* sock, int send_private = FALSE ) 
{
	sock->encode();

	r_classad->SetMyTypeName( STARTD_ADTYPE );
	r_classad->SetTargetTypeName( JOB_ADTYPE );

	if( send_private ) {
		r_private_classad->SetMyTypeName( STARTD_ADTYPE );
		r_private_classad->SetTargetTypeName( JOB_ADTYPE );
	}

	if( ! sock->put( UPDATE_STARTD_AD ) ) {
		dprintf( D_ALWAYS, "Can't send command\n");
		sock->end_of_message();
		return FALSE;
	}

	if( ! r_classad->put( *sock ) ) {
		dprintf( D_ALWAYS, "Can't send public classad\n");
		sock->end_of_message();
		return FALSE;
	}

	if( send_private ) {
		if( ! r_private_classad->put( *sock ) ) {
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


