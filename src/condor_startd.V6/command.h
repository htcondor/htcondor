#ifndef _STARTD_COMMAND_H
#define _STARTD_COMMAND_H

class Service;

// command_handler reads a capability off the socket, tries to find
// the resource that the capability belongs to, does a switch on the
// command, and calls the proper action with the pointer to the
// resource the capability matched. 
int command_handler( Service*, int, Stream* );

// These commands are a special case, since the capability they gets might
// be for the preempting match.  Therefore, it must be handled 
// differently.
int command_request_claim( Service*, int, Stream* );
int command_match_info( Service*, int, Stream* );

// These commands all act startd-wide, and therefore, should be handled
// seperately. 
int command_vacate( Service*, int, Stream* );
int command_pckpt_all( Service*, int, Stream* );
int command_x_event( Service*, int, Stream* );
int	command_give_state( Service*, int, Stream* );

#endif _STARTD_COMMAND_H
