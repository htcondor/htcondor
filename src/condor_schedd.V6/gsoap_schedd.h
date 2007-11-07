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

//gsoap condor service name: condorSchedd

#import "gsoap_schedd_types.h"

#import "gsoap_daemon_core.h"

int condor__beginTransaction(xsd__int duration,
							 struct condor__beginTransactionResponse {
								 struct condor__TransactionAndStatus response;
							 } & result);

int condor__commitTransaction(struct condor__Transaction transaction,
							  struct condor__commitTransactionResponse {
								  struct condor__Status response;
							  } & result);

int condor__abortTransaction(struct condor__Transaction transaction,
							  struct condor__abortTransactionResponse {
								  struct condor__Status response;
							  } & result);

int condor__extendTransaction(struct condor__Transaction transaction,
							  xsd__int duration,
							  struct condor__extendTransactionResponse {
								  struct condor__TransactionAndStatus response;
							  } & result);

int condor__newCluster(struct condor__Transaction transaction,
					   struct condor__newClusterResponse {
					       struct condor__IntAndStatus response;
					   } & result);

int condor__removeCluster(struct condor__Transaction *transaction,
						  xsd__int clusterId,
						  xsd__string reason,
						  struct condor__removeClusterResponse {
							  struct condor__Status response;
						  } & result);

int condor__newJob(struct condor__Transaction transaction,
				   xsd__int clusterId,
				   struct condor__newJobResponse {
				       struct condor__IntAndStatus response;
				   } & result);

int condor__removeJob(struct condor__Transaction *transaction,
					  xsd__int clusterId,
					  xsd__int jobId,
					  xsd__string reason,
					  xsd__boolean force_removal,
					  struct condor__removeJobResponse {
						  struct condor__Status response;
					  } & result);

int condor__holdJob(struct condor__Transaction *transaction,
					xsd__int clusterId,
					xsd__int jobId,
					xsd__string reason,
					xsd__boolean email_user,
					xsd__boolean email_admin,
					xsd__boolean system_hold,
					struct condor__holdJobResponse {
					    struct condor__Status response;
					} & result);

int condor__releaseJob(struct condor__Transaction *transaction,
					   xsd__int clusterId,
					   xsd__int jobId,
					   xsd__string reason,
					   xsd__boolean email_user,
					   xsd__boolean email_admin,
					   struct condor__releaseJobResponse {
					       struct condor__Status response;
					   } & result);

int condor__submit(struct condor__Transaction transaction,
				   xsd__int clusterId,
				   xsd__int jobId,
				   struct condor__ClassAdStruct * jobAd,
				   struct condor__submitResponse {
				       struct condor__RequirementsAndStatus response;
                   } & result);

int condor__getJobAds(struct condor__Transaction *transaction,
					  xsd__string constraint,
					  struct condor__getJobAdsResponse {
					      struct condor__ClassAdStructArrayAndStatus response;
					  } & result);

int condor__getJobAd(struct condor__Transaction *transaction,
					 xsd__int clusterId,
					 xsd__int jobId,
					 struct condor__getJobAdResponse {
					     struct condor__ClassAdStructAndStatus response;
					 } & result);

int condor__declareFile(struct condor__Transaction transaction,
						xsd__int clusterId,
						xsd__int jobId,
						xsd__string name,
						xsd__int size,
						enum condor__HashType hashType,
						xsd__string hash,
						struct condor__declareFileResponse {
						    struct condor__Status response;
						} & result);

int condor__sendFile(struct condor__Transaction transaction,
					 xsd__int clusterId,
					 xsd__int jobId,
					 xsd__string name,
					 xsd__int offset,
					 struct xsd__base64Binary * data,
					 struct condor__sendFileResponse {
					     struct condor__Status response;
					 } & result);

int condor__getFile(struct condor__Transaction *transaction,
					xsd__int clusterId,
					xsd__int jobId,
					xsd__string name,
					xsd__int offset,
					xsd__int length,
					struct condor__getFileResponse {
					    struct condor__Base64DataAndStatus response;
					} & result);

int condor__closeSpool(struct condor__Transaction *transaction,
					   xsd__int clusterId,
					   xsd__int jobId,
					   struct condor__closeSpoolResponse {
					       struct condor__Status response;
					   } & result);

int condor__listSpool(struct condor__Transaction *transaction,
					  xsd__int clusterId,
					  xsd__int jobId,
					  struct condor__listSpoolResponse {
					      struct condor__FileInfoArrayAndStatus response;
					  } & result);

int condor__requestReschedule(void *,
							  struct condor__requestRescheduleResponse {
								  struct condor__Status response;
							  } & result);

int condor__discoverJobRequirements(struct condor__ClassAdStruct * jobAd,
									struct condor__discoverJobRequirementsResponse {
									    struct condor__RequirementsAndStatus response;
									} & result);

int condor__createJobTemplate(xsd__int clusterId,
							  xsd__int jobId,
							  xsd__string owner,
							  enum condor__UniverseType type,
							  xsd__string cmd,
							  xsd__string args,
							  xsd__string requirements,
							  struct condor__createJobTemplateResponse {
							      struct condor__ClassAdStructAndStatus response;
							  } & result);
