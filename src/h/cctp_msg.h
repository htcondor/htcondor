/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
//////////////////////////////////////////////////////////////////////////
//
//  cctp_msg.h -- Condor Configuration Transfer Protocol (CCTP) messages
//
//////////////////////////////////////////////////////////////////////////

#ifndef CCTP_MSG_H_
#define CCTP_MSG_H_

#include "stream.h"
//#include "condor_attrlist.h"
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
