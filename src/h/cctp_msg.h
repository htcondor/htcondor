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
//////////////////////////////////////////////////////////////////////////
//
//  cctp_msg.h -- Condor Configuration Transfer Protocol (CCTP) messages
//
//////////////////////////////////////////////////////////////////////////

#ifndef CCTP_MSG_H_
#define CCTP_MSG_H_

#include "stream.h"
#include "condor_attrlist.h"
#include "condor_classad.h"
#include "cctp.h"

class CCTP_Request
{

public:

    CCTP_Request() 
      { strcpy(requesterID, "");
	action = ACTION_NONE;
	strcpy(constraint, "");
	seqNum++; 
	mySeqNum = seqNum; }
    CCTP_Request(char id[], CCTP_Action act, char constr[])
      { strcpy(requesterID, id);
	action = act;
	strcpy(constraint, constr);
        seqNum++; 
	mySeqNum = seqNum; }
    ~CCTP_Request() { }

    // get/set functions
    CCTP_Client    GetClientType() { return clientType; }
    void           GetRequesterID(char* id) { strcpy(id, requesterID); }
    int            GetMySeqNum()  { return mySeqNum; }
    CCTP_Action    GetAction()  { return action; }
    void           GetClassAd(char* ad) { strcpy(ad, myAd); }
    void           GetConstraint(char* constr) { strcpy(constr, constraint); }
    char           GetSeperator() { return seperator; }

    void           SetClientType(CCTP_Client client) { clientType = client; }
    void           SetRequesterID(char* id) { strcpy(requesterID, id); }
//    void           SetMySeqNum(int num) { mySeqNum = num; }
    void           SetAction(CCTP_Action act) { action = act; }
    void           SetClassAd(char* ad) { strcpy(myAd, ad); }
    void           SetConstraint(char* constr) { strcpy(constraint, constr); }
    void           SetSeperator(char sp) { seperator = sp; }

    // shipping functions
    int            get(Stream& s);
    int            put(Stream& s);
    int            code(Stream& s);

private:

    enum { strLen = 256 };

    static long    seqNum;              // sequence number of the request

    CCTP_Client    clientType;
    char           requesterID[strLen]; // requester's id 
    int            mySeqNum;            // sequence number of this
    CCTP_Action    action;              // action to be performed on server
    char           myAd[strLen];        // requester's classified ad
    char           seperator;
    char           constraint[strLen];  // the attr name list
};

class CCTP_Response
{

public:

    CCTP_Response() 
                      { requestFor = 0;
                        status = STATUS_NOMATCH;
			strcpy(requesterID, "");
		        strcpy(reason, ""); }
    CCTP_Response(char id[], int num, CCTP_Status st, char why[])
                    { strcpy(requesterID, id);
		      requestFor = num;
		      status = st;
		      strcpy(reason, why); }
    ~CCTP_Response() {  }

    // get/set functions
    void          GetRequesterID(char* id) { strcpy(id, requesterID); }
    int           GetRequestFor() { return requestFor; }
    CCTP_Status   GetStatus() { return status; }
    void          GetReason(char* why) { strcpy(why, reason); }
//    void          GetAttrList(AttrList& list) { list = attrList; }

    void          SetRequesterID(char* id) { strcpy(requesterID, id); }
    void          SetRequestFor(int num) { requestFor = num; }
    void          SetStatus(CCTP_Status st)  { status = st; }
    void          SetReason(char* why) { strcpy(reason, why); }
//    void          SetAttrList(const AttrList& list) {attrList = list; }
/*                  { char* name = NULL;
		    attrList.ResetName();
		    name = attrList.NextName();
		    while(name) {
		      attrList.Delete(name);
		      name = attrList.NextName();
		    }

		    attrList = list; }*/

    // shipping functions
    int           get(Stream& s);
    int           put(Stream& s);
    int           code(Stream& s);

private:

    enum { strLen = 64 };

    char           requesterID[strLen];  // requester's id
    int            requestFor;           // the request seq. num. this response is for
    CCTP_Status    status;               // the status of the request
    char           reason[strLen];       // reason for any failure
//    AttrList       attrList;             // attribute list requested
};

#endif /* CCTP_MSG_H_ */
