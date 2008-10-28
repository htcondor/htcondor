/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

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

#if !defined(WIN32)
/*
  DELEGATE_GSI_CRED_STARTD is called by the shadow before the claim is
  activated if it suspects the startd may try to use glexec to spawn
  the starter (i.e. if GLEXEC_STARTER is set).  This is required since
  glexec needs the user proxy to determine what user to run the starter
  as.
*/
int command_delegate_gsi_cred( Service*, int, Stream* );
#endif

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
int command_vm_universe( Service*, int, Stream* );

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
int request_claim( Resource*, Claim *, char*, Stream* ); 

// Accept claim from schedd agent
bool accept_request_claim( Resource* ); 

// Activate a claim with a given starter
int activate_claim( Resource*, Stream* ); 

#endif /* _STARTD_COMMAND_H */
