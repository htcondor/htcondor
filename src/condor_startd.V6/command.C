#include "startd.h"
static char *_FileName_ = __FILE__;

int
command_handler( Service* serv, int cmd, Stream* stream )
{
	char* cap = NULL;
	Resource* rip;
	int rval;

	if( ! stream->code(cap) ) {
		dprintf( D_ALWAYS, "Can't read capability\n" );
		free( cap );
		return FALSE;
	}

	rip = resmgr->get_by_cur_cap( cap );

	if( !rip ) {
		dprintf( D_ALWAYS, 
				 "Error: can't find resource with capability (%s)\n",
				 cap );
		free( cap );
		return FALSE;
	}
	free( cap );

	State s = rip->state();
	if( s != claimed_state ) {
		log_ignore( cmd, s );
		return FALSE;
	}

	switch( cmd ) {
	case ALIVE:
		rval = alive( rip );
		break;
	case RELEASE_CLAIM:
		rval = release_claim( rip );
		break;
	case ACTIVATE_CLAIM:
		rval = activate_claim( rip, stream );
		break;
	case DEACTIVATE_CLAIM:
		rval = deactivate_claim( rip );
		break;
	case DEACTIVATE_CLAIM_FORCIBLY:
		rval = deactivate_claim_forcibly( rip );
		break;
	case PCKPT_FRGN_JOB:
		rval = periodic_checkpoint( rip );
		break;
	case REQ_NEW_PROC:
		rval = request_new_proc( rip );
		break;
	}
	return rval;
}


int
command_vacate( Service* serv, int cmd, Stream* stream ) 
{
	dprintf( D_ALWAYS, "command_vacate() called.\n" );
	resmgr->walk( release_claim );
	return TRUE;
}


int
command_pckpt_all( Service* serv, int cmd, Stream* stream ) 
{
	dprintf( D_ALWAYS, "command_pckpt_all() called.\n" );
	resmgr->walk( periodic_checkpoint );
	return TRUE;
}


int
command_x_event( Service* serv, int cmd, Stream* stream ) 
{
	dprintf( D_FULLDEBUG, "command_x_event() called.\n" );
	last_x_event = (int)time( NULL );
	return TRUE;
}


int
command_give_state( Service* serv, int cmd, Stream* stream ) 
{
	dprintf( D_FULLDEBUG, "command_give_state() called.\n" );
	stream->encode();
	stream->put( (int) resmgr->state() );
	stream->eom();
	return TRUE;
}


int
command_request_claim( Service* serv, int cmd, Stream* stream ) 
{
	char* cap = NULL;
	Resource* rip;
	int rval;

	if( ! stream->code(cap) < 0 ) {
		dprintf( D_ALWAYS, "Can't read capability\n" );
		free( cap );
		return FALSE;
	}

	rip = resmgr->get_by_any_cap( cap );
	if( !rip ) {
		dprintf( D_ALWAYS, 
				 "Error: can't find resource with capability (%s)\n",
				 cap );
		free( cap );
		return FALSE;
	}

	State s = rip->state();
	switch( s ) {
	case claimed_state:
	case matched_state:
	case unclaimed_state:
		rval = request_claim( rip, cap, stream );
		break;
	default:
		log_ignore( REQUEST_CLAIM, s );
		rval = FALSE;
		break;
	}
	free( cap );
	return rval;
}

int
command_match_info( Service* serv, int cmd, Stream* stream ) 
{
	char* cap = NULL;
	Resource* rip;
	int rval;
	char *str = NULL;

	if( ! stream->code(cap) < 0 ) {
		dprintf( D_ALWAYS, "Can't read capability\n" );
		free( cap );
		return FALSE;
	}

		// Find Resource object for this capability
#ifdef OLD_PROTOCOL
	rip = resmgr->rip();
#else 
	rip = resmgr->get_by_any_cap( cap );
	if( !rip ) {
		dprintf( D_ALWAYS, 
				 "Error: can't find resource with capability (%s)\n",
				 cap );
		free( cap );
		return FALSE;
	}
#endif

		// Check resource state.  If we're in claimed or unclaimed,
		// process the command.  Otherwise, ignore it.  
	State s = rip->state();
	if( s == claimed_state || s == unclaimed_state ) {

#ifdef OLD_PROTOCOL
			// Peel off the sequence number on the capability
		str = (char *)strchr(cap,'#');
		if( str ) {
			*str = '\0';
		}
		rip->r_cur->cap()->setcapab( cap );
#endif

		rval = match_info( rip, cap );
	} else {
		log_ignore( MATCH_INFO, s );
		rval = FALSE;
	}
	free( cap );
	return rval;
}
