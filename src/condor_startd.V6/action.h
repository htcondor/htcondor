#ifndef _STARTD_ACTION_H
#define _STARTD_ACTION_H

// Timer handler for when a given resource's update timer goes off.
int timeout_and_update();		

// Timer handler to poll a resource and evaluate it's state.
int poll_resource();

int alive( Resource* );			// Keep alive command

// Schedd Agent requests a claim
int request_claim( Resource*, char*, Stream* ); 

// Accept claim from schedd agent
int	accept_request_claim( Resource* ); 

// Activate a claim with a given starter
int activate_claim( Resource*, Stream* ); 

// Release the given claim
int release_claim( Resource* );			

// Gracefully shutdown the starter, and don't relea	se the claim.
int deactivate_claim( Resource* );		

// Forcibly shutdown the starter, and don't release the claim.
int deactivate_claim_forcibly( Resource* );

int match_info( Resource*, char* );		// You got matched by CM
int periodic_checkpoint( Resource* );	// Do a periodic checkpoint

int vacate_claim( Resource* );			// Gracefully shutdown starter on a claim
int kill_claim( Resource* );			// Quickly shutdown starter on a claim
int	suspend_claim( Resource* );			// Suspend the starter on this claim
int continue_claim( Resource* ); 		// Continue the starter on this claim

// Multi-shadow wants to run more processes.  Send a SIGHUP to the starter.
int request_new_proc( Resource* );

// This resource was matched, but not claimed in time
int	match_timed_out( Service* );

// This resource didn't get a keep alive in time from the schedd
int claim_timed_out( Service* );

#endif _STARTD_ACTION_H
