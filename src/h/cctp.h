/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
/**********************************************************************

  cctp.h -- Condor Configuration Transfer Protocol (CCTP) routines

**********************************************************************/

#ifndef CCTP_H_
#define CCTP_H_

typedef enum {
    ACTION_NONE,
    ACTION_GET,
    ACTION_ADD,
    ACTION_DELETE,
    ACTION_ADDMACROS,
    ACTION_DELETEMACROS,
    ACTION_UPDATE,
    ACTION_SAVE,
    ACTION_ADDUSER,
    ACTION_DELUSER
} CCTP_Action;

typedef enum {
    STATUS_SUCCESS,
    STATUS_PARSINGERR,
    STATUS_NOMATCH,
    STATUS_DUPLICATE,
    STATUS_NOTFOUND,
    STATUS_UNAUTHORIZED,
    STATUS_ERRSAVE
} CCTP_Status;

typedef enum {
    SIGNAL_RECONFIG
} CCTP_Signal;

typedef enum {
    CONDOR_ON,
    CONDOR_OFF,
    CONDOR_MASTER,
    CONDOR_NEGOTIATOR,
    CONDOR_COLLECTOR,
    CONDOR_KBDD,
    CONDOR_SCHEDD,
    CONDOR_STARTD,
    CONDOR_DAEMON,
    CONDOR_CONFIG_EDITOR
} CCTP_Client;

#if defined(__cplusplus)
extern "C" {
#endif

/* Make connection through socket to the configuration server
 ----------------------------------------------------------

     PARAM:  
              
              

     RETURN: socket id, if connection is successful
             0,         if connection is failed
*/

int config_server_connect(char* serverHost, int serverPort);

/* Send request through socket to the configuration server
 -------------------------------------------------------

    PARAM:  procType    -- either "Editor" or "Daemon"
            action      -- type of action to be performed on server
            ad          -- a string for the construction of classad
            sp          -- seperator of the ad
            constraint  -- the name list of the attribute client wants
            more        -- more requests to be send?

     RETURN: a request seq. num., if request is sent successfully
             0,                   if fail to send request
*/

int send_request(CCTP_Client client, CCTP_Action action, char* ad, char sp, char* constraint, int more);

/* Get response from the configuration server
 ------------------------------------------

     PARAM: 
            status   -- response status
            attrList -- string of attribute list
            sp       -- the seperator of the attribute list string
            reason   -- reason for the failure
			timestamp-- timestamp on this attrlist

     RETURN: the seq. num. of the request this response is for, on success
             0,         if fail to retrieve response
*/

int get_response(char* id, CCTP_Status* status, char* attrList, char sp, char* reason, time_t* timestamp);

/* Close the connection to the configuration server
 ------------------------------------------------

     PARAM: 

     RETURN: 1, if connection is closed successfully
             0, if failure
*/

int config_server_disconnect();

/* Signal the client by the configuration server when config changes
 -----------------------------------------------------------------

     PARAM: clientID  -- client id
            signal    -- signal type
            change    -- the change to the config

     RETURN: 1, if signal is sent successfully
             0, if fail to send signal
*/

/*int signal_client(char* clientID, CCTP_Signal signal, char* change);*/

#if defined(__cplusplus)
}
#endif

#endif /* CCTP_H_ */

