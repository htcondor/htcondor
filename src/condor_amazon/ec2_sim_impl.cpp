/*
 * Copyright 2008 Red Hat, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "stdsoap2.h"

#include <iostream>
#include <vector>
#include <sstream>

#include "soapH.h"

using namespace std;

/*
 * The important functions to simulate are:
 *   CreateKeyPair
 *   DeleteKeyPair
 *   DescribeInstances
 *   DescribeKeyPairs
 *   RunInstances
 *   TerminateInstances
 */

/*
 * Shared state that makes up a simple EC2 model
 */

static vector<class ec2__DescribeKeyPairsResponseItemType *> key_pairs;
static vector<class ec2__ReservationInfoType *> reservations;
static int s_id;

int
PrintLeakSummary()
{
	int i;
	int rc = 0;
	for ( i = 0; i < key_pairs.size(); i++ ) {
		rc = 1;
		cout << "Leaked KeyPair " << key_pairs[i]->keyName << endl;
	}
	for ( i = 0; i < reservations.size(); i++ ) {
		if ( reservations[i]->instancesSet->item[0]->instanceState->code != 48 ) {
			rc = 1;
			cout << "Leaked Instance " << reservations[i]->instancesSet->item[0]->instanceId << endl;
		}
	}
	if ( rc == 0 ) {
		cout << "No leaked resources" << endl;
	}
	return rc;
}

/*
 * A dead simple way of advancing an instances through states
 */
void
RandomAdvance(ec2__InstanceStateType *state)
{
	if (rand() % 2) {
		switch (state->code) {
		case 0:
			state->code = 16;
			state->name = "running";
			break;
		case 16:
			state->code = 32;
			state->name = "shutting-down";
			break;
		case 32:
			state->code = 48;
			state->name = "terminated";
			break;
		case 48:
			break;
		}
	}
}

SOAP_FMAC5 int SOAP_FMAC6
ec2__CreateKeyPair(struct soap *soap,
					 ec2__CreateKeyPairType *request,
					 ec2__CreateKeyPairResponseType *response)
{
	cout << "CreateKeyPair(" << request->keyName << ")" << endl;

	for (int i = 0; i < key_pairs.size(); i++) { // XXX: use iterator
		if (request->keyName == key_pairs[i]->keyName) {
			cout << "Found '" << request->keyName << "'" << endl;
			cout << "Return Failure: key already exists" << endl;

			return soap_receiver_fault_subcode(soap,
											   "ec2:Client.InvalidKeyPair.Duplicate",
											   "Attempt to create a duplicate key pair.",
											   NULL);

		}
	}

	ec2__DescribeKeyPairsResponseItemType *keyPair = 
		new ec2__DescribeKeyPairsResponseItemType();
	keyPair->keyName = request->keyName;
	keyPair->keyFingerprint = "FAKE-FINGERPRINT";
	key_pairs.push_back(keyPair);

	response->keyName = keyPair->keyName;
	response->keyFingerprint = keyPair->keyFingerprint;
	response->keyMaterial = "FAKE-MATERIAL";

	cout << "Return Success" << endl;

	return SOAP_OK;
}

SOAP_FMAC5 int SOAP_FMAC6
ec2__DeleteKeyPair(struct soap *soap,
					 ec2__DeleteKeyPairType *request,
					 ec2__DeleteKeyPairResponseType *response)
{
	cout << "DeleteKeyPair(" << request->keyName << ")" << endl;

	response->return_ = false;

	for (vector<ec2__DescribeKeyPairsResponseItemType *>::iterator i = key_pairs.begin();
		 i != key_pairs.end();
		 i++) {
		if (request->keyName == (*i)->keyName) {
			key_pairs.erase(i);

			cout << "Return Success" << endl;

			return SOAP_OK;
		}
	}

		/*
		 * EC2 does not return an fault for trying to delete an
		 * already deleted keypair
		 */
//	return soap_receiver_fault_subcode(soap,
//									   "ec2:Client.InvalidKeyPair.NotFound",
//									   "Specified key pair name does not exist.",
//									   NULL);
	cout << "Return Success" << endl;
	response->return_ = true;
	return SOAP_OK;
}

SOAP_FMAC5 int SOAP_FMAC6
ec2__DescribeInstances(struct soap *soap,
						 ec2__DescribeInstancesType *request,
						 ec2__DescribeInstancesResponseType *response)
{
	std::string tmp;
	for ( int i = 0; i < request->instancesSet->item.size(); i++ ) {
		if ( i > 0 ) {
			tmp += ",";
		}
		tmp += request->instancesSet->item[i]->instanceId;
	}
	cout << "DescribeInstances(" << tmp << ")" << endl;

	response->reservationSet = soap_new_ec2__ReservationSetType(soap, -1);

	if (request->instancesSet->item.empty()) {
			// When an instance is observed it has a chance to change
		for (vector<ec2__ReservationInfoType *>::iterator j =
				 reservations.begin();
			 j != reservations.end();
			 j++) {
			RandomAdvance((*j)->instancesSet->item[0]->instanceState);
		}
		response->reservationSet->item = reservations;
	} else {
		for (vector<ec2__DescribeInstancesItemType *>::iterator i =
				 request->instancesSet->item.begin();
			 i != request->instancesSet->item.end();
			 i++) {
			int found;

			cout << "Looking for '" << (*i)->instanceId << "'" << endl;

			found = 0;
			for (vector<ec2__ReservationInfoType *>::iterator j =
					 reservations.begin();
				 j != reservations.end();
				 j++) {
				if ((*i)->instanceId == (*j)->instancesSet->item[0]->instanceId) {
					cout << "Found '" << (*j)->instancesSet->item[0]->instanceId << "'" << endl;
						// When an instance is observed it has a chance to change
					RandomAdvance((*j)->instancesSet->item[0]->instanceState);
					response->reservationSet->item.push_back(*j);
					found = 1;
					break;
				}
			}
			if (!found) {
				cout << "Return Failure: invalid instance id" << endl;
				return soap_receiver_fault_subcode(soap,
												   "ec2:Client.InvalidInstanceId.NotFound",
												   "Specified instanceId does not exist.",
												   NULL);
			}
		}
	}

	cout << "Return Success" << endl;
	return SOAP_OK;
}

SOAP_FMAC5 int SOAP_FMAC6
ec2__DescribeKeyPairs(struct soap *soap,
						ec2__DescribeKeyPairsType *request,
						ec2__DescribeKeyPairsResponseType *response)
{
	cout << "DescribeKeyPairs(...)" << endl;

	response->keySet = soap_new_ec2__DescribeKeyPairsResponseInfoType(soap, -1);

	if (request->keySet->item.empty()) {
		response->keySet->item = key_pairs;
	} else {
		for (vector<ec2__DescribeKeyPairsItemType *>::iterator i = request->keySet->item.begin();
			 i != request->keySet->item.end();
			 i++) {
			int found;

			cout << "Looking for '" << (*i)->keyName << "'" << endl;

			found = 0;
			for (vector<ec2__DescribeKeyPairsResponseItemType *>::iterator j = key_pairs.begin();
				 j != key_pairs.end();
				 j++) {
				if ((*i)->keyName == (*j)->keyName) {
					cout << "Found '" << (*j)->keyName << "'" << endl;
					response->keySet->item.push_back(*j);
					found = 1;
					break;
				}
			}
			if (!found) {
				cout << "Return Failure: invalid keypair name" << endl;
				return soap_receiver_fault_subcode(soap,
												   "ec2:Client.InvalidKeyPair.NotFound",
												   "Specified key pair name does not exist.",
												   NULL);
			}
		}
	}

	cout << "Return Success" << endl;

	return SOAP_OK;
}

SOAP_FMAC5 int SOAP_FMAC6
ec2__RunInstances(struct soap *soap,
					ec2__RunInstancesType *request,
					ec2__ReservationInfoType *response)
{
	cout << "RunInstances(" << request->imageId << ")" << endl;

	ostringstream tmp;

	ec2__ReservationInfoType *reservation = new ec2__ReservationInfoType();

	tmp << "r-" << s_id++; reservation->reservationId = tmp.str(); tmp.str("");
	tmp << "o-" << s_id++; reservation->ownerId = tmp.str(); tmp.str("");
	reservation->groupSet = new ec2__GroupSetType();
	reservation->groupSet->item = request->groupSet->item;
	reservation->instancesSet = new ec2__RunningInstancesSetType();
	ec2__RunningInstancesItemType *item = new ec2__RunningInstancesItemType();
	tmp << "i-" << s_id++; item->instanceId = tmp.str(); tmp.str("");
	item->imageId = request->imageId;
	item->instanceState = new ec2__InstanceStateType();
	item->instanceState->code = 0;
	item->instanceState->name = "pending";
	item->privateDnsName = "PRIVATE-DNS-NAME";
	item->dnsName = "DNS-NAME";
	item->reason = NULL;
	if (request->keyName) {
		item->keyName = new string(request->keyName->c_str());
	}
	item->amiLaunchIndex = NULL;
	item->productCodes = NULL;
	item->instanceType = request->instanceType;
	item->launchTime = 0;

	reservation->instancesSet->item.push_back(item);

	response->reservationId = reservation->reservationId;
	response->ownerId = reservation->ownerId;
	response->groupSet = reservation->groupSet;
	response->instancesSet = reservation->instancesSet;

	reservations.push_back(reservation);

	cout << "Creating instance " << item->instanceId << endl;
	cout << "Return Success" << endl;

	return SOAP_OK;
}

SOAP_FMAC5 int SOAP_FMAC6
ec2__TerminateInstances(struct soap *soap,
						  ec2__TerminateInstancesType *request,
						  ec2__TerminateInstancesResponseType *response)
{
	std::string tmp;
	for ( int i = 0; i < request->instancesSet->item.size(); i++ ) {
		if ( i > 0 ) {
			tmp += ",";
		}
		tmp += request->instancesSet->item[i]->instanceId;
	}
	cout << "TerminateInstances(" << tmp << ")" << endl;

	response->instancesSet = soap_new_ec2__TerminateInstancesResponseInfoType(soap, -1);

	if (request->instancesSet->item.empty()) {
			// XXX
	} else {
		for (vector<ec2__TerminateInstancesItemType *>::iterator i =
				 request->instancesSet->item.begin();
			 i != request->instancesSet->item.end();
			 i++) {
			int found;

			cout << "Looking for '" << (*i)->instanceId << "'" << endl;

			found = 0;
			for (vector<ec2__ReservationInfoType *>::iterator j =
					 reservations.begin();
				 j != reservations.end();
				 j++) {
				if ((*i)->instanceId == (*j)->instancesSet->item[0]->instanceId) {
					cout << "Found '" << (*i)->instanceId << "'" << endl;
					ec2__TerminateInstancesResponseItemType *item =
						soap_new_ec2__TerminateInstancesResponseItemType(soap, -1);
					item->instanceId = (*j)->instancesSet->item[0]->instanceId;
					item->previousState = (*j)->instancesSet->item[0]->instanceState;
					(*j)->instancesSet->item[0]->instanceState =
						new ec2__InstanceStateType(); // XXX: leak
					(*j)->instancesSet->item[0]->instanceState->code = 48;
					(*j)->instancesSet->item[0]->instanceState->name = "terminated";;
					item->shutdownState = (*j)->instancesSet->item[0]->instanceState;
					response->instancesSet->item.push_back(item);
						//reservations.erase(j);
					found = 1;
					break;
				}
			}
			if (!found) { // XXX: Is this what EC2 does?
				cout << "Return Failure: invalid instance id" << endl;
				return soap_receiver_fault_subcode(soap,
												   "ec2:Client.InvalidInstanceId.NotFound",
												   "Specified instanceId does not exist.",
												   NULL);
			}
		}
	}

	cout << "Return Success" << endl;

	return SOAP_OK;
}


/*
 * The functions below are not implemented...
 */
/*
SOAP_FMAC5 int SOAP_FMAC6
__ec2__RegisterImage(struct soap *soap,
					struct ec2__RegisterImageType *ec2__RegisterImage,
					struct ec2__RegisterImageResponseType *ec2__RegisterImageResponse)
{
	return SOAP_FAULT;
}

SOAP_FMAC5 int SOAP_FMAC6
__ec2__DeregisterImage(struct soap *soap,
					  struct ec2__DeregisterImageType *ec2__DeregisterImage,
					  struct ec2__DeregisterImageResponseType *ec2__DeregisterImageResponse)
{
	return SOAP_FAULT;
}

SOAP_FMAC5 int SOAP_FMAC6
__ec2__GetConsoleOutput(struct soap *soap,
					   struct ec2__GetConsoleOutputType *ec2__GetConsoleOutput,
					   struct ec2__GetConsoleOutputResponseType *ec2__GetConsoleOutputResponse)
{
	return SOAP_FAULT;
}

SOAP_FMAC5 int SOAP_FMAC6
__ec2__RebootInstances(struct soap *soap,
					  struct ec2__RebootInstancesType *ec2__RebootInstances,
					  struct ec2__RebootInstancesResponseType *ec2__RebootInstancesResponse)
{
	return SOAP_FAULT;
}

SOAP_FMAC5 int SOAP_FMAC6
__ec2__DescribeImages(struct soap *soap,
					 struct ec2__DescribeImagesType *ec2__DescribeImages,
					 struct ec2__DescribeImagesResponseType *ec2__DescribeImagesResponse)
{
	return SOAP_FAULT;
}

SOAP_FMAC5 int SOAP_FMAC6
__ec2__CreateSecurityGroup(struct soap *soap,
						  struct ec2__CreateSecurityGroupType *ec2__CreateSecurityGroup,
						  struct ec2__CreateSecurityGroupResponseType *ec2__CreateSecurityGroupResponse)
{
	return SOAP_FAULT;
}

SOAP_FMAC5 int SOAP_FMAC6
__ec2__DeleteSecurityGroup(struct soap *soap,
						  struct ec2__DeleteSecurityGroupType *ec2__DeleteSecurityGroup,
						  struct ec2__DeleteSecurityGroupResponseType *ec2__DeleteSecurityGroupResponse)
{
	return SOAP_FAULT;
}

SOAP_FMAC5 int SOAP_FMAC6
__ec2__DescribeSecurityGroups(struct soap *soap,
							 struct ec2__DescribeSecurityGroupsType *ec2__DescribeSecurityGroups,
							 struct ec2__DescribeSecurityGroupsResponseType *ec2__DescribeSecurityGroupsResponse)
{
	return SOAP_FAULT;
}

SOAP_FMAC5 int SOAP_FMAC6
__ec2__AuthorizeSecurityGroupIngress(struct soap *soap,
									struct ec2__AuthorizeSecurityGroupIngressType *ec2__AuthorizeSecurityGroupIngress,
									struct ec2__AuthorizeSecurityGroupIngressResponseType *ec2__AuthorizeSecurityGroupIngressResponse)
{
	return SOAP_FAULT;
}

SOAP_FMAC5 int SOAP_FMAC6
__ec2__RevokeSecurityGroupIngress(struct soap *soap,
								 struct ec2__RevokeSecurityGroupIngressType *ec2__RevokeSecurityGroupIngress,
								 struct ec2__RevokeSecurityGroupIngressResponseType *ec2__RevokeSecurityGroupIngressResponse)
{
	return SOAP_FAULT;
}

SOAP_FMAC5 int SOAP_FMAC6
__ec2__ModifyImageAttribute(struct soap *soap,
						   struct ec2__ModifyImageAttributeType *ec2__ModifyImageAttribute,
						   struct ec2__ModifyImageAttributeResponseType *ec2__ModifyImageAttributeResponse)
{
	return SOAP_FAULT;
}

SOAP_FMAC5 int SOAP_FMAC6
__ec2__ResetImageAttribute(struct soap *soap,
						  struct ec2__ResetImageAttributeType *ec2__ResetImageAttribute,
						  struct ec2__ResetImageAttributeResponseType *ec2__ResetImageAttributeResponse)
{
	return SOAP_FAULT;
}

SOAP_FMAC5 int SOAP_FMAC6
__ec2__DescribeImageAttribute(struct soap *soap,
							 struct ec2__DescribeImageAttributeType *ec2__DescribeImageAttribute,
							 struct ec2__DescribeImageAttributeResponseType *ec2__DescribeImageAttributeResponse)
{
	return SOAP_FAULT;
}

SOAP_FMAC5 int SOAP_FMAC6
__ec2__ConfirmProductInstance(struct soap *soap,
							 struct ec2__ConfirmProductInstanceType *ec2__ConfirmProductInstance,
							 struct ec2__ConfirmProductInstanceResponseType *ec2__ConfirmProductInstanceResponse)
{
	return SOAP_FAULT;
}
*/
