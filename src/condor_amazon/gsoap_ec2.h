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

//
// We don't care about all the symbols defined in 2007-08-29.ec2.wsdl,
// only the ones we use.
//
// A start to figuring out what symbols we need:
//    $ grep ec2__ gsoap_commands.cpp | sed 's/.*ec2__\([^ ,\*(]*\).*/\1/' | sort -u
//
// Useful regular expressions:
//    <xs:complexType name="\(.*\)"> -> class ec2_\1
//    </xs:complexType> ->
//    <xs:sequence> -> { public:
//    </xs:sequence> -> };
//     /> -> />
//    <xs:element name="\(.*\)" type="xs:\(.*\)" minOccurs="0"/> -> \2 *\1 0;
//    <xs:element name="\(.*\)" type="xs:\(.*\)" minOccurs="0" maxOccurs="\(.*\)"/> -> \2 *\1 \3:\4;
//    <xs:element name="\(.*\)" type="xs:\(.*\)" minOccurs="\(.*\)" maxOccurs="\(.*\)"/> -> \2 \1 \3:\4;
//    <xs:element name="\(.*\)" type="xs:\(.*\)"/> -> \2 \1 1;
//    <xs:element name="\(.*\)" type="tns:\(.*\)" minOccurs="\(.*\)" maxOccurs="unbounded"/> -> vector<ec2__\2*> \1 \3;
//    <xs:element name="\(.*\)" type="tns:\(.*\)" minOccurs="\(.*\)" maxOccurs="\(.*\)"/> -> ec2__\2 *\1 \3:\4;
//    <xs:element name="\(.*\)" type="tns:\(.*\)"/> -> ec2__\2 *\1 1;
//    string -> std::string
//    vector -> std::vector
//

//
// These are important configuration settings scraped from the top and
// bottom of the WSDL file.
//
//gsoap ec2 service name: AmazonEC2
//gsoap ec2 service location: https://ec2.amazonaws.com/
//gsoap ec2 service style: document
//gsoap ec2 service encoding: literal
//gsoap ec2 schema form: qualified
//gsoap ec2 schema attributeForm: unqualified
//gsoap ec2 schema namespace: http://ec2.amazonaws.com/doc/2007-08-29/

#import "stlvector.h"


// EC2 uses WS-Security, this is how you enable it with gSOAP

#import "wsse.h"
struct SOAP_ENV__Header
{
	_wsse__Security *wsse__Security;
};

//
// Type symbols used in gsoap_commands.cpp
//

class ec2__CreateKeyPairType
{ public:
	std::string keyName 1;
};


class ec2__CreateKeyPairResponseType
{ public:
	std::string keyName 1;
	std::string keyFingerprint 1;
	std::string keyMaterial 1;
};


class ec2__DeleteKeyPairType
{ public:
	std::string keyName 1;
};

  
class ec2__DeleteKeyPairResponseType
{ public:
	bool return_ 1;
};


class ec2__DescribeKeyPairsItemType
{ public:
	std::string keyName 1;
};


class ec2__DescribeKeyPairsInfoType
{ public:
	std::vector<ec2__DescribeKeyPairsItemType*> item 0;
};


class ec2__DescribeKeyPairsType
{ public:
	ec2__DescribeKeyPairsInfoType *keySet 1;
};


class ec2__DescribeKeyPairsResponseItemType
{ public:
	std::string keyName 1;
	std::string keyFingerprint 1;
};


class ec2__DescribeKeyPairsResponseInfoType
{ public:
	std::vector<ec2__DescribeKeyPairsResponseItemType*> item 0;
};

  
class ec2__DescribeKeyPairsResponseType
{ public:
	ec2__DescribeKeyPairsResponseInfoType *keySet 1;
};


class ec2__GroupItemType
{ public:
	std::string groupId 1;
};


class ec2__GroupSetType
{ public:
	std::vector<ec2__GroupItemType*> item 0;
};


class ec2__UserDataType // mixed="true" ?
{ public:
	std::string data 1;
	@std::string version 1; // fixed="1.0" ?
	@std::string encoding 1; // fixed="base64" ?
};


class ec2__RunInstancesType
{ public:
	std::string imageId 1;
	int minCount 1;
	int maxCount 1;
	std::string *keyName 0;
	ec2__GroupSetType *groupSet 1;
	std::string *additionalInfo 0;
	ec2__UserDataType *userData 0:1;
	std::string *addressingType 0:1;
	std::string instanceType 1;
};


class  ec2__InstanceStateType
{ public:
	int code 1;
	std::string name 1;
};


class ec2__ProductCodesSetItemType
{ public:
	std::string productCode 1;
};


class ec2__ProductCodesSetType
{ public:
	std::vector<ec2__ProductCodesSetItemType*> item 0;
};


class ec2__RunningInstancesItemType
{ public:
	std::string instanceId 1;
	std::string imageId 1;
	ec2__InstanceStateType *instanceState 0;
	std::string privateDnsName 1;
	std::string dnsName 1;
	std::string *reason 0;
	std::string *keyName 0;
	std::string *amiLaunchIndex 0:1;
	ec2__ProductCodesSetType *productCodes 0:1;
	std::string instanceType 1;
	time_t launchTime 1;
};


class ec2__RunningInstancesSetType
{ public:
	std::vector<ec2__RunningInstancesItemType*> item 1;
};


class ec2__TerminateInstancesItemType
{ public:
	std::string instanceId 1;
};


class ec2__TerminateInstancesInfoType
{ public:
	std::vector<ec2__TerminateInstancesItemType*> item 1;
};


class ec2__TerminateInstancesType
{ public:
	ec2__TerminateInstancesInfoType *instancesSet 1;
};


class ec2__TerminateInstancesResponseItemType
{ public:
	std::string instanceId 1;
	ec2__InstanceStateType *shutdownState 1;
	ec2__InstanceStateType *previousState 1;
};


class ec2__TerminateInstancesResponseInfoType
{ public:
	std::vector<ec2__TerminateInstancesResponseItemType*> item 0;
};


class ec2__TerminateInstancesResponseType
{ public:
	ec2__TerminateInstancesResponseInfoType *instancesSet 1;
};


class ec2__DescribeInstancesItemType
{ public:
	std::string instanceId 1;
};


class ec2__DescribeInstancesInfoType
{ public:
	std::vector<ec2__DescribeInstancesItemType*> item 0;
};


class ec2__DescribeInstancesType
{
	ec2__DescribeInstancesInfoType *instancesSet 1;
};


class ec2__ReservationInfoType
{ public:
	std::string reservationId 1;
	std::string ownerId 1;
	ec2__GroupSetType *groupSet 1;
	ec2__RunningInstancesSetType *instancesSet 1;
};


class ec2__ReservationSetType
{ public:
	std::vector<ec2__ReservationInfoType*> item 0;
};

  
class ec2__DescribeInstancesResponseType
{ public:
	ec2__ReservationSetType *reservationSet 0;
};


//
// Function symbols used in gsoap_commands.cpp
//
// Example: CreateKeyPair
//
//    <operation name="CreateKeyPair">
//       <input message="tns:CreateKeyPairRequestMsg"/>
//       <output message="tns:CreateKeyPairResponseMsg"/>
//    </operation>
//
//    <message name="CreateKeyPairRequestMsg">
//       <part name="CreateKeyPairRequestMsgReq" element="tns:CreateKeyPair"/>
//    </message>
//    <message name="CreateKeyPairResponseMsg">
//       <part name="CreateKeyPairResponseMsgResp" element="tns:CreateKeyPairResponse"/>
//    </message>
//
//    <xs:element name="CreateKeyPair" type="tns:CreateKeyPairType"/>
//
//    <xs:element name="CreateKeyPairResponse" type="tns:CreateKeyPairResponseType"/>
//
// Important Note:
//
//   EC2 needs anonymout params for input. gSOAP 2.7.12's
//   documentation say this can be achieved by omitting the param name
//   or calling it _param (underscore does the trick). These
//   apparently do not work. Instead two underscores does, thus the
//   __anon params.
//
//   Also, for gSOAP to properly parse the output from EC2 the param
//   name must match exactly the tag name returned by EC2. Thus all
//   the ec2__*Response argument names. When this is broken you'll see
//   errors such as:
//      Validation constraint violation: tag name or namespace
//      mismatch in element <DescribeInstancesResponse>
//

int ec2__CreateKeyPair(ec2__CreateKeyPairType *__anon,
					   ec2__CreateKeyPairResponseType *ec2__CreateKeyPairResponse);

int ec2__DeleteKeyPair(ec2__DeleteKeyPairType *__anon,
					   ec2__DeleteKeyPairResponseType *ec2__DeleteKeyPairResponse);

int ec2__DescribeInstances(ec2__DescribeInstancesType *__anon,
							 ec2__DescribeInstancesResponseType *ec2__DescribeInstancesResponse);

int ec2__DescribeKeyPairs(ec2__DescribeKeyPairsType *__anon,
						  ec2__DescribeKeyPairsResponseType *ec2__DescribeKeyPairsResponse);

int ec2__RunInstances(ec2__RunInstancesType *__anon,
					  ec2__ReservationInfoType *ec2__RunInstancesResponse);

int ec2__TerminateInstances(ec2__TerminateInstancesType *__anon,
							ec2__TerminateInstancesResponseType *ec2__TerminateInstancesResponse);
