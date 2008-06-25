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

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_string.h"
#include "string_list.h"
#include "MyString.h"
#include "util_lib_proto.h"
#include "stat_wrapper.h"
#include "amazongahp_common.h"
#include "amazonCommands.h"
#include "thread_control.h"

// For gsoap
#include <stdsoap2.h>
#include <smdevp.h> 
#include "soapH.h"
#include <wsseapi.h>
#include "AmazonEC2Binding.nsmap"

// For base64 encoding
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

// Caller need to free the returned pointer
static char* base64_encode(const unsigned char *input, int length)
{
	BIO *bmem, *b64;
	BUF_MEM *bptr;

	b64 = BIO_new(BIO_f_base64());
	bmem = BIO_new(BIO_s_mem());
	b64 = BIO_push(b64, bmem);
	BIO_write(b64, input, length);
	BIO_flush(b64);
	BIO_get_mem_ptr(b64, &bptr);

	char *buff = (char *)malloc(bptr->length);
	ASSERT(buff);
	memcpy(buff, bptr->data, bptr->length-1);
	buff[bptr->length-1] = 0;
	BIO_free_all(b64);

	return buff;
}

void
AmazonRequest::ParseSoapError(const char* callerstring) 
{
	if( !m_soap ) {
		return;
	}

		// In some error cases, the soap_fault*() functions will return
		// null strings if soap_set_fault() isn't called first.
	soap_set_fault(m_soap);

	const char** code = NULL;
	code = soap_faultcode(m_soap);

	if( *code ) {
		// For real error code, refer to 
		// http://docs.amazonwebservices.com/AWSEC2/2007-08-29/DeveloperGuide/api-error-codes.html
		//
		// Client error codes suggest that the error was caused by 
		// something the client did, such as an authentication failure or 
		// an invalid AMI identifier. 
		// In the SOAP API, These error codes are prefixed with Client. 
		// For example: Client.AuthFailure. 
		//
		// Server error codes suggest a server-side issue caused 
		// the error and should be reported. 
		// In the SOAP API, these error codes are prefixed with Server. 
		// For example: Server.Unavailable.
	

		// NOTE: The faultcode appears to have a qualifying 
		// namespace, which we need to strip
		char *s = strrchr(*code, ':');
		if( s ) {
			s++;
			if( !strncasecmp(s, "Client.", strlen("Client.")) ) {
				m_error_code = s + strlen("Client.");
			}else if( !strncasecmp(s, "Server.", strlen("Server.")) ) {
				m_error_code = s + strlen("Server.");
			}else {
				m_error_code = s;
			}
		}
	}

	const char** reason = NULL;
	reason = soap_faultstring(m_soap);
	if( *reason ) {
		m_error_msg = *reason;
	}

	char buffer[512];

	// We use the buffer as a string, so lets be safe and make
	// sure when you write to the front of it you always have a
	// NULL at the end
	memset((void *)buffer, 0, sizeof(buffer));

	soap_sprint_fault(m_soap, buffer, sizeof(buffer));
	if( strlen(buffer) ) { 
		dprintf(D_ALWAYS, "Call to %s failed: %s\n", 
				callerstring? callerstring:"", buffer);
	}
	return;
}

bool 
AmazonRequest::SetupSoap(void)
{
	if( secretkeyfile.IsEmpty() ) {
		dprintf(D_ALWAYS, "There is no privatekey\n");
		return false;
	}
	if( accesskeyfile.IsEmpty() ) {
		dprintf(D_ALWAYS, "There is no accesskeyfile\n");
		return false;
	}

	if( m_soap ) {
		CleanupSoap();
	}

	// Must use canoicalization
	if( !(m_soap = soap_new1(SOAP_XML_CANONICAL))) {
		m_error_msg = "Failed to create SOAP context";
		dprintf(D_ALWAYS, "%s\n", m_error_msg.Value());
		return false;
	}

	const char* proxy_host_name;
	int proxy_port = 0;
	if( get_amazon_proxy_server(proxy_host_name, proxy_port) ) {
		m_soap->proxy_host = proxy_host_name;
		m_soap->proxy_port = proxy_port; 
	}

	if (soap_register_plugin(m_soap, soap_wsse)) {
		ParseSoapError("setup WS-Security plugin");
		return false;
	}

	FILE *file = NULL;
	if ((file = safe_fopen_wrapper(secretkeyfile.Value(), "r"))) {
		m_rsa_privk = PEM_read_PrivateKey(file, NULL, NULL, NULL);
		fclose(file);

		if( !m_rsa_privk ) {
			m_error_msg.sprintf("Could not read private RSA key from: %s",
					secretkeyfile.Value());
			dprintf(D_ALWAYS, "%s\n", m_error_msg.Value());
			return false;
		}
	} else {
		m_error_msg.sprintf("Could not read private key file: %s",
				secretkeyfile.Value());
		dprintf(D_ALWAYS, "%s\n", m_error_msg.Value());
		return false;
	}

	if ((file = safe_fopen_wrapper(accesskeyfile.Value(), "r"))) {
		m_cert = PEM_read_X509(file, NULL, NULL, NULL);
		fclose(file);

		if (!m_cert) {
			m_error_msg.sprintf("Could not read accesskeyfile from: %s",
					accesskeyfile.Value());
			dprintf(D_ALWAYS, "%s\n", m_error_msg.Value());
			return false;
		}
	} else {
		m_error_msg.sprintf("Could not read accesskeyfile file: %s",
				accesskeyfile.Value());
		dprintf(D_ALWAYS, "%s\n", m_error_msg.Value());
		return false;
	}

	// Timestamp must be signed, the "Timestamp" value just needs
	// to be non-NULL
	if( soap_wsse_add_Timestamp(m_soap, "Timestamp", 137)) { 
		m_error_msg = "Failed to sign timestamp";
		dprintf(D_ALWAYS, "%s\n", m_error_msg.Value());
		return false;
	}

	if( soap_wsse_add_BinarySecurityTokenX509(m_soap, "BinarySecurityToken", m_cert)) {
		m_error_msg.sprintf("Could not set BinarySecurityToken from: %s", 
				accesskeyfile.Value());
		dprintf(D_ALWAYS, "%s\n", m_error_msg.Value());
		return false;
	}

	// May be optional
	if( soap_wsse_add_KeyInfo_SecurityTokenReferenceX509(m_soap, "#X509Token") ) {
		m_error_msg = "Failed to setup SecurityTokenReference";
		dprintf(D_ALWAYS, "%s\n", m_error_msg.Value());
		return false;
	}

	// Body must be signed
	if( soap_wsse_sign_body(m_soap, SOAP_SMD_SIGN_RSA_SHA1, m_rsa_privk, 0)) {
		m_error_msg = "Failed to setup signing of SOAP body";
		dprintf(D_ALWAYS, "%s\n", m_error_msg.Value());
		return false;
	}

	return true;
}

void
AmazonRequest::CleanupSoap(void)
{
	if (m_rsa_privk) {
		EVP_PKEY_free(m_rsa_privk);
		m_rsa_privk = NULL;
	}

	if (m_cert) {
		X509_free(m_cert);
		m_cert = NULL;
	}

	if( m_soap ) {
		soap_wsse_delete_Security(m_soap);
		soap_end(m_soap);
		soap_done(m_soap);

		free(m_soap);
		m_soap = NULL;
	}
}

bool
AmazonVMKeypairNames::gsoapRequest(void)
{
	if( !SetupSoap() ) {
		dprintf(D_ALWAYS, "Failed to setup SOAP context\n");
		return false;
	}

	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(),
				secretkeyfile.GetCStr(), m_error_msg) ) {
		dprintf(D_ALWAYS, "AmazonVMKeypairNames Error: %s\n", m_error_msg.Value());
		return false;
	}

	ec2__DescribeKeyPairsType request;
	ec2__DescribeKeyPairsResponseType response;

	// Want info on all keys...
	request.keySet = NULL;

	int code = -1;		
	amazon_gahp_release_big_mutex();
	code = soap_call___ec2__DescribeKeyPairs(m_soap,
					get_ec2_url(),
					NULL,
					&request,
					&response);
	amazon_gahp_grab_big_mutex();
	if ( !code ) {
		if( response.keySet && response.keySet->item ) {
			int i = 0;
			for (i = 0; i < response.keySet->__sizeitem; i++) {
				keynames.append(response.keySet->item[i]->keyName);
			}
		}
		return true;

	}else {
		// Error
		ParseSoapError("DescribeKeyPairs");
	}

	return false;
}

bool
AmazonVMCreateKeypair::gsoapRequest(void)
{
	if( !SetupSoap() ) {
		dprintf(D_ALWAYS, "Failed to setup SOAP context\n");
		return false;
	}

	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(), 
				secretkeyfile.GetCStr(), m_error_msg) ) {
		dprintf(D_ALWAYS, "AmazonVMCreateKeyPair Error: %s\n", m_error_msg.Value());
		return false;
	}

	if( keyname.IsEmpty() ) {
		m_error_msg = "Empty_Keyname";
		dprintf(D_ALWAYS, "AmazonVMCreateKeyPair Error: %s\n", m_error_msg.Value());
		return false;
	}

	if( strcmp(outputfile.Value(), NULL_FILE) ) { 
		has_outputfile = true;
	}

	// check if output file could be created
	if( has_outputfile ) { 
		if( check_create_file(outputfile.GetCStr()) == false ) {
			m_error_msg = "No_permission_for_keypair_outputfile";
			dprintf(D_ALWAYS, "AmazonVMCreateKeypair Error: %s\n", m_error_msg.Value());
			return false;
		}
	}

	ec2__CreateKeyPairType request;
	ec2__CreateKeyPairResponseType response;

	request.keyName = (char *)keyname.GetCStr();

	int code = -1;
	amazon_gahp_release_big_mutex();
	code = soap_call___ec2__CreateKeyPair(m_soap,
					get_ec2_url(),
					NULL,
					&request,
					&response);
	amazon_gahp_grab_big_mutex();
	if ( !code ) {
		if( has_outputfile && response.keyMaterial ) {

			FILE *fp = NULL;
			fp = safe_fopen_wrapper(outputfile.Value(), "w");
			if( !fp ) {
				m_error_msg.sprintf("failed to safe_fopen_wrapper %s in write mode: "
						"safe_fopen_wrapper returns %s", 
						outputfile.Value(), strerror(errno));
				dprintf(D_ALWAYS, "%s\n", m_error_msg.Value());
				return false;
			}

			fprintf(fp,"%s", response.keyMaterial);
			fclose(fp);
		}
		return true;
	}else {
		// Error
		ParseSoapError("CreateKeyPair");
	}
	return false;
}

bool
AmazonVMDestroyKeypair::gsoapRequest(void)
{
	if( !SetupSoap() ) {
		dprintf(D_ALWAYS, "Failed to setup SOAP context\n");
		return false;
	}

	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(),
				secretkeyfile.GetCStr(), m_error_msg) ) {
		dprintf(D_ALWAYS, "AmazonVMDestroyKeypair Error: %s\n", 
				m_error_msg.Value());
		return false;
	}

	if( keyname.IsEmpty() ) {
		m_error_msg = "Empty_Keyname";
		dprintf(D_ALWAYS, "AmazonVMDestroyKeypair Error: %s\n", 
				m_error_msg.Value());
		return false;
	}

	ec2__DeleteKeyPairType request;
	ec2__DeleteKeyPairResponseType response;

	request.keyName = (char *) keyname.GetCStr();

	int code = -1;
	amazon_gahp_release_big_mutex();
	code = soap_call___ec2__DeleteKeyPair(m_soap,
					get_ec2_url(),
					NULL,
					&request,
					&response);
	amazon_gahp_grab_big_mutex();
	if ( !code ) {
		return true;
	}else {
		// Error
		ParseSoapError("DeleteKeyPair");
	}
	return false;
}

bool
AmazonVMRunningKeypair::gsoapRequest(void)
{
	return AmazonVMStatusAll::gsoapRequest();
}

bool
AmazonVMStart::gsoapRequest(void)
{
	if( !SetupSoap() ) {
		dprintf(D_ALWAYS, "Failed to setup SOAP context\n");
		return false;
	}

	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(),
				secretkeyfile.GetCStr(), m_error_msg) ) {
		dprintf(D_ALWAYS, "AmazonVMStart Error: %s\n", m_error_msg.Value());
		return false;
	}

	if( user_data_file.IsEmpty() == false ) {
		if( !check_read_access_file( user_data_file.GetCStr()) ) {
			m_error_msg.sprintf("Cannot read the file for user data(%s)",
					user_data_file.Value());
			return false;
		}
	}

	if( ami_id.IsEmpty() ) {
		m_error_msg = "Empty_AMI_ID";
		dprintf(D_ALWAYS, "AmazonVMStart Error: %s\n", m_error_msg.Value());
		return false;
	}

	// userData
	if( user_data_file.IsEmpty() == false ) {
		// Need to read file
		int fd = -1;
		fd = safe_open_wrapper(user_data_file.Value(), O_RDONLY);
		if( fd < 0 ) {
			m_error_msg.sprintf("failed to safe_open_wrapper file(%s) : "
					"safe_open_wrapper returns %s", user_data_file.Value(), 
					strerror(errno));
			dprintf(D_ALWAYS, "%s\n", m_error_msg.Value());
			return false;
		}

		int file_size = 0;
		StatWrapper swrap(fd);
		file_size = swrap.GetStatBuf()->st_size;

		char *readbuffer = (char*)malloc(file_size);
		ASSERT(readbuffer);

		int ret = read(fd, readbuffer, file_size);
		close(fd);

		if( ret != file_size ) {
			m_error_msg.sprintf("failed to read(need %d but real read %d) "
					"in file(%s)", file_size, ret, user_data_file.Value());
			dprintf(D_ALWAYS, "%s\n", m_error_msg.Value());

			free(readbuffer);
			readbuffer = NULL;
			return false;
		}
	
		base64_userdata = base64_encode((unsigned char*)readbuffer, file_size);
		free(readbuffer);
	}else {
		if( user_data.IsEmpty() == false ) { 
			base64_userdata = base64_encode((unsigned char*)user_data.GetCStr(), user_data.Length());
		}
	}

	ec2__RunInstancesType request;
	ec2__ReservationInfoType response;

	// image id
	request.imageId = (char *)ami_id.GetCStr();
	// min Count
	request.minCount = 1;
	// max Count
	request.maxCount = 1;

	// Keypair
	if( keypair.IsEmpty() == false ) {
		request.keyName = (char *)keypair.GetCStr();
	}else {
		request.keyName = NULL;
	}

	// groupSet
	ec2__GroupSetType groupSet;
	ec2__GroupItemType **groupItems = NULL;

	if( groupnames.isEmpty() == false ) {
		int group_nums = groupnames.number();

		groupItems = (ec2__GroupItemType **)soap_malloc(m_soap, 
				sizeof(ec2__GroupItemType *)*group_nums);
		ASSERT(groupItems);

		int i=0;
		char *one_group = NULL;
		groupnames.rewind();
	
		ec2__GroupItemType *one_group_item = NULL;
		while((one_group = groupnames.next()) != NULL ) {
			one_group_item = soap_new_ec2__GroupItemType(m_soap, -1);
			ASSERT(one_group_item);

			one_group_item->groupId = one_group; 
			groupItems[i++] = one_group_item;
		}

		groupSet.__sizeitem = group_nums;
		groupSet.item = groupItems;
	}else {
		groupSet.__sizeitem = 0;
		groupSet.item = NULL;
	}
	request.groupSet = &groupSet;

	// additionalInfo
	request.additionalInfo = NULL;

	// user data
	ec2__UserDataType userdata_type;
	if( base64_userdata ) {
		// TODO 
		// Need to check
		userdata_type.data = base64_userdata;
		userdata_type.version = "1.0";
		userdata_type.encoding = "base64";
		userdata_type.__mixed = NULL;

		request.userData = &userdata_type;
	}else {
		request.userData = NULL;
	}

	// addressingType
	request.addressingType = NULL;

	// instanceType
	if( instance_type.IsEmpty() == false ) {
		request.instanceType = (char *) instance_type.GetCStr();
	}else {
		request.instanceType = "m1.small";
	}

	int code = -1;
	amazon_gahp_release_big_mutex();
	code = soap_call___ec2__RunInstances(m_soap,
					get_ec2_url(),
					NULL,
					&request,
					&response);
	amazon_gahp_grab_big_mutex();
	if ( !code ) {
		if( response.instancesSet && response.instancesSet->item ) {
			instance_id = response.instancesSet->item[0]->instanceId;
			return true;
		}
	}else {
		// Error
		ParseSoapError("RunInstances");
	}
	return false;
}

bool
AmazonVMStop::gsoapRequest(void)
{
	if( !SetupSoap() ) {
		dprintf(D_ALWAYS, "Failed to setup SOAP context\n");
		return false;
	}

	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(),
				secretkeyfile.GetCStr(), m_error_msg) ) {
		dprintf(D_ALWAYS, "AmazonVMStop Error: %s\n", m_error_msg.Value());
		return false;
	}

	if( instance_id.IsEmpty() ) {
		m_error_msg = "Empty_Instance_ID";
		dprintf(D_ALWAYS, "AmazonVMStop Error: %s\n", m_error_msg.Value());
		return false;
	}

	ec2__TerminateInstancesType request;
	ec2__TerminateInstancesResponseType response;

	ec2__TerminateInstancesInfoType instanceSet;

	ec2__TerminateInstancesItemType item;
	item.instanceId = (char *) instance_id.GetCStr();

	ec2__TerminateInstancesItemType *itemptr = &item;

	instanceSet.__sizeitem = 1;
	instanceSet.item = &itemptr;

	request.instancesSet = &instanceSet;

	int code = -1;
	amazon_gahp_release_big_mutex();
	code = soap_call___ec2__TerminateInstances(m_soap,
					get_ec2_url(),
					NULL,
					&request,
					&response);
	amazon_gahp_grab_big_mutex();
	if ( !code ) {
		return true;
	}else {
		// Error
		ParseSoapError("TerminateInstances");
	}
	return false;
}

bool
AmazonVMStatus::gsoapRequest(void)
{
	if( !SetupSoap() ) {
		dprintf(D_ALWAYS, "Failed to setup SOAP context\n");
		return false;
	}

	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(),
				secretkeyfile.GetCStr(), m_error_msg) ) {
		dprintf(D_ALWAYS, "AmazonVMStatus Error: %s\n", m_error_msg.Value());
		return false;
	}

	if( instance_id.IsEmpty() ) {
		m_error_msg = "Empty_Instance_ID";
		dprintf(D_ALWAYS, "AmazonVMStatus Error: %s\n", m_error_msg.Value());
		return false;
	}

	ec2__DescribeInstancesType request;
	ec2__DescribeInstancesResponseType response;

	ec2__DescribeInstancesInfoType instancesSet;
	ec2__DescribeInstancesItemType item;

	item.instanceId = (char *) instance_id.GetCStr();

	ec2__DescribeInstancesItemType *itemptr = &item;

	instancesSet.__sizeitem = 1;
	instancesSet.item = &itemptr;

	// Show a specific running instance
	request.instancesSet = &instancesSet;

	int code = -1;
	amazon_gahp_release_big_mutex();
	code = soap_call___ec2__DescribeInstances(m_soap,
					get_ec2_url(),
					NULL,
					&request,
					&response);
	amazon_gahp_grab_big_mutex();
	if ( !code ) {
		if( response.reservationSet && response.reservationSet->item ) {
			int total_nums = response.reservationSet->__sizeitem;

			int i = 0;
			for (i = 0; i < total_nums; i++) {
				ec2__RunningInstancesSetType* _instancesSet = 
					response.reservationSet->item[i]->instancesSet;

				if( !_instancesSet ) {
					continue;
				}

				ec2__RunningInstancesItemType **instance = 
					_instancesSet->item;

				if( !instance || !(*instance)->instanceId ) {
					continue;
				}

				if( !(*instance)->instanceState || 
						!(*instance)->instanceState->name) {
					dprintf(D_ALWAYS, "Failed to get valid status\n");
					continue;
				}

				if( !strcmp((*instance)->instanceId, instance_id.Value()) ) {

					status_result.status = (*instance)->instanceState->name;
					status_result.instance_id = (*instance)->instanceId;
					status_result.ami_id = (*instance)->imageId;
					status_result.public_dns = (*instance)->dnsName;
					status_result.private_dns = (*instance)->privateDnsName;
					status_result.keyname = (*instance)->keyName;
					status_result.instancetype = (*instance)->instanceType;

					// Set group names
					ec2__GroupSetType* groupSet =
						response.reservationSet->item[i]->groupSet;

					if( groupSet && groupSet->item ) {
						int j = 0;
						for( j = 0; j < groupSet->__sizeitem; j++ ) {
							status_result.groupnames.append(groupSet->item[j]->groupId);
						}
					}

					break;
				}
			}
		}
		return true;
	}else {
		// Error
		ParseSoapError("DescribeInstance");
	}
	return false;
}

bool
AmazonVMStatusAll::gsoapRequest(void)
{
	if( !SetupSoap() ) {
		dprintf(D_ALWAYS, "Failed to setup SOAP context\n");
		return false;
	}

	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(),
				secretkeyfile.GetCStr(), m_error_msg) ) {
		dprintf(D_ALWAYS, "AmazonVMStatusAll Error: %s\n", m_error_msg.Value());
		return false;
	}

	ec2__DescribeInstancesType request;
	ec2__DescribeInstancesResponseType response;

	ec2__DescribeInstancesInfoType instancesSet;

	instancesSet.__sizeitem = 0;
	instancesSet.item = NULL;

	// Show a specific running instance
	request.instancesSet = &instancesSet;

	int code = -1;
	amazon_gahp_release_big_mutex();
	code = soap_call___ec2__DescribeInstances(m_soap,
					get_ec2_url(),
					NULL,
					&request,
					&response);
	amazon_gahp_grab_big_mutex();
	if ( !code ) {
		if( response.reservationSet && response.reservationSet->item ) {
			int total_nums = response.reservationSet->__sizeitem;

			status_num = 0;
			status_results = new AmazonStatusResult[total_nums];
			ASSERT(status_results);

			int i = 0;
			for (i = 0; i < total_nums; i++) {
				ec2__RunningInstancesSetType* _instancesSet = 
					response.reservationSet->item[i]->instancesSet;

				if( !_instancesSet ) {
					continue;
				}

				ec2__RunningInstancesItemType **instance = 
					_instancesSet->item;

				if( !instance || !(*instance)->instanceId ) {
					continue;
				}

				if( !(*instance)->instanceState || 
						!(*instance)->instanceState->name) {
					dprintf(D_ALWAYS, "Failed to get valid status\n");
					continue;
				}

				status_results[status_num].status = 
					(*instance)->instanceState->name;

				status_results[status_num].instance_id =
				   	(*instance)->instanceId;
				status_results[status_num].ami_id = 
					(*instance)->imageId;
				status_results[status_num].public_dns =
					(*instance)->dnsName;
				status_results[status_num].private_dns =
					(*instance)->privateDnsName;
				status_results[status_num].keyname =
					(*instance)->keyName;
				status_results[status_num].instancetype =
					(*instance)->instanceType;

				// Set group names
				ec2__GroupSetType* groupSet =
					response.reservationSet->item[i]->groupSet;

				if( groupSet && groupSet->item ) {
					int j = 0;
					for( j = 0; j < groupSet->__sizeitem; j++ ) {
						status_results[status_num].groupnames.append(
								groupSet->item[j]->groupId);
					}
				}

				status_num++;
			}
		}
		return true;
	}else {
		// Error
		ParseSoapError("DescribeInstances");

	}
	return false;
}


