/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef _STARTD_COMMAND_H
#define _STARTD_COMMAND_H

class Service;

/*
  command_handler reads a ClaimId off the socket, tries to find
  the resource that the ClaimId belongs to, does a switch on the
  command, and calls the proper action with the pointer to the
  resource the ClaimId matched.  None of the commands send
  anything other than the command and the ClaimId.  
*/
int command_handler( Service*, int, Stream* );

/*
  ACTIVATE_CLAIM is a special case of the above case, in that more
  stuff is sent over the wire than just the command and the
  ClaimId, and if anything goes wrong, the shadow wants a reply. 
*/
int command_activate_claim( Service*, int, Stream* );

/*
  These commands are a special case, since the ClaimId they get might
  be for the preempting match.  Therefore, they must be handled 
  differently.
*/
int command_request_claim( Service*, int, Stream* );
int command_match_info( Service*, int, Stream* );
int command_release_claim( Service*, int, Stream* );

/* 
   These commands all act startd-wide, and therefore, should be handled
   seperately. 
*/
int command_vacate_all( Service*, int, Stream* );
int command_pckpt_all( Service*, int, Stream* );
int command_x_event( Service*, int, Stream* );
int	command_give_state( Service*, int, Stream* );
int	command_give_totals_classad( Service*, int, Stream* );
int command_give_request_ad( Service*, int, Stream* );
int command_query_ads( Service*, int, Stream* );
int command_vm_register( Service*, int, Stream* );

/*
   This command handler deals with commands that send a name as part
   of the protocol to specify which resource they should effect,
   instead of a ClaimId.
*/
int command_name_handler( Service*, int, Stream* );

/*
   Command handler for dealing w/ ClassAd-only protocol commands
*/
int command_classad_handler( Service*, int, Stream* );

/*
  Since the protocol is complex and what we have to do so
  complicated, these functions are here that actually do the work of 
  some of the commands. 
*/
// Negotiator sent match information to us
int match_info( Resource*, char* );

// Schedd Agent requests a claim
int request_claim( Resource*, char*, Stream* ); 

// Accept claim from schedd agent
int	accept_request_claim( Resource* ); 

// Activate a claim with a given starter
int activate_claim( Resource*, Stream* ); 

#endif /* _STARTD_COMMAND_H */
