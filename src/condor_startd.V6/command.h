#ifndef _STARTD_COMMAND_H
#define _STARTD_COMMAND_H

class Service;

/*
  command_handler reads a capability off the socket, tries to find
  the resource that the capability belongs to, does a switch on the
  command, and calls the proper action with the pointer to the
  resource the capability matched.  None of the commands send
  anything other than the command and the capability.  
*/
int command_handler( Service*, int, Stream* );

/*
  ACTIVATE_CLAIM is a special case of the above case, in that more
  stuff is sent over the wire than just the command and the
  capability, and if anything goes wrong, the shadow wants a reply. 
*/
int command_activate_claim( Service*, int, Stream* );

/*
  These commands are a special case, since the capability they get might
  be for the preempting match.  Therefore, they must be handled 
  differently.
*/
int command_request_claim( Service*, int, Stream* );
int command_match_info( Service*, int, Stream* );

/* 
   These commands all act startd-wide, and therefore, should be handled
   seperately. 
*/
int command_vacate( Service*, int, Stream* );
int command_pckpt_all( Service*, int, Stream* );
int command_x_event( Service*, int, Stream* );
int	command_give_state( Service*, int, Stream* );
int	command_give_classad( Service*, int, Stream* );
int command_give_request_ad( Service*, int, Stream* );

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

#endif _STARTD_COMMAND_H
