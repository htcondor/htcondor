#ifndef _DAP_CLIENT_INTERFACE_H
#define _DAP_CLIENT_INTERFACE_H

#define WANT_NAMESPACES
#include "classad_distribution.h"

/** dap_submit()
	Submits a specified request to Stork server

	@param request - the ClassAd to submit
	@param stork_server_sin - the sinful string of Stork server
	@param cred - the buffer containing the credential
	@param cred_size - the size of the credential in bytes
	@param id - an out-parameter specifying the id of new request
	   (only valid if return code is TRUE)
	   -must be deleted with free() by caller
	@param error_reason - an out-parameter specifying error reason
	   (only valid if return code is FALSE)
	   -must be deleted with free() by caller
	@return - TRUE if success, FALSE if failure
*/

int stork_submit (const classad::ClassAd * request,
				const char * stork_server_sin, 
				const char * cred, 
				const int cred_size, 
				char *& id,
				char *& error_reason);


/** dap_rm()
	Removes a request with specified id from server

	@param id - id of the request
	@param stork_server_sin - the sinful string of Stork server
	@param result - result of the request - currently empty
	   (only valid if return code is TRUE)
	   -must be deleted with free() by caller
	@param error_reason - an out-parameter specifying error reason
	   (only valid if return code is FALSE)
	   -must be deleted with free() by caller
	@return - TRUE if success, FALSE if failure
*/

int stork_rm (const char * id, 
			const char * stork_server_sin,
			char *& result,
			char *& error_reason);

/** dap_status()
	Queries for the status (ClassAd) of a given request

	@param id - id of the request
	@param stork_server_sin - the sinful string of Stork server
	@param result - an out-parameter ClassAd result of the request
	   (only valid if return code is TRUE)
	   -must be deleted with free() by caller
	@param error_reason - an out-parameter specifying error reason
	   (only valid if return code is FALSE)
	   -must be deleted with free() by caller
	@return - TRUE if success, FALSE if failure
*/

int stork_status (const char * id, 
				const char * stork_server_sin, 
				classad::ClassAd * & result,
				char *& error_reason);

/** dap_list()
	List all active requests for specified user

	@param id - id of the request
	@param stork_server_sin - the sinful string of Stork server
	@param result - an out-parameter, an array of  ClassAds
	   corresponding to each active request
	   (only valid if return code is TRUE)
	   -each element and the array must be deleted with free()/delete
	   respectively
	@param error_reason - an out-parameter specifying error reason
	   (only valid if return code is FALSE)
	   -must be deleted with free() by caller
	@return - TRUE if success, FALSE if failure
*/

int stork_list (const char * stork_server_sin,
			  int & size, 
			  classad::ClassAd ** & result,
			  char *& error_reason);



/** get_stork_sinful_string()
	Returns a sinful string (<xxx.xxx.xxx.xxx:1234>) for the Stork server,
given a host name. This string is what needs to be passed to dap_* commands.
    @param hostname - the hostname of the server. 
	Can be null, may or may not contain port
*/

char * 
get_stork_sinful_string (const char * hostname);

#endif
