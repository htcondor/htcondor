/*
 * Copyright 2009-2011 Red Hat, Inc.
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

#ifndef AVIARYQUERYSERVICESKELETON_H
#define AVIARYQUERYSERVICESKELETON_H

    #include <OMElement.h>
    #include <MessageContext.h>
   
     #include <AviaryQuery_GetJobData.h>
    
     #include <AviaryQuery_GetJobDataResponse.h>
    
     #include <AviaryQuery_GetJobStatus.h>
    
     #include <AviaryQuery_GetJobStatusResponse.h>
    
     #include <AviaryQuery_GetSubmissionSummary.h>
    
     #include <AviaryQuery_GetSubmissionSummaryResponse.h>
    
     #include <AviaryQuery_GetJobDetails.h>
    
     #include <AviaryQuery_GetJobDetailsResponse.h>

     #include <AviaryQuery_GetJobSummary.h>

     #include <AviaryQuery_GetJobSummaryResponse.h>
    
    
namespace AviaryQuery {
    

   /** we have to reserve some error codes for adb and for custom messages */
    #define AVIARYQUERYSERVICESKELETON_ERROR_CODES_START (AXIS2_ERROR_LAST + 2500)

    typedef enum
    {
        AVIARYQUERYSERVICESKELETON_ERROR_NONE = AVIARYQUERYSERVICESKELETON_ERROR_CODES_START,

        AVIARYQUERYSERVICESKELETON_ERROR_LAST
    } AviaryQueryServiceSkeleton_error_codes;

    


class AviaryQueryServiceSkeleton
{
        public:
            AviaryQueryServiceSkeleton(){}


     




		 


        /**
         * Auto generated method declaration
         * for "getJobData|http://grid.redhat.com/aviary-query/" operation.
         * 
         * @param _getJobData of the AviaryQuery::GetJobData
         *
         * @return AviaryQuery::GetJobDataResponse*
         */
        

         virtual 
        AviaryQuery::GetJobDataResponse* getJobData(wso2wsf::MessageContext *outCtx ,AviaryQuery::GetJobData* _getJobData);


     




		 


        /**
         * Auto generated method declaration
         * for "getJobStatus|http://grid.redhat.com/aviary-query/" operation.
         * 
         * @param _getJobStatus of the AviaryQuery::GetJobStatus
         *
         * @return AviaryQuery::GetJobStatusResponse*
         */
        

         virtual 
        AviaryQuery::GetJobStatusResponse* getJobStatus(wso2wsf::MessageContext *outCtx ,AviaryQuery::GetJobStatus* _getJobStatus);


     




		 


        /**
         * Auto generated method declaration
         * for "getSubmissionSummary|http://grid.redhat.com/aviary-query/" operation.
         * 
         * @param _getSubmissionSummary of the AviaryQuery::GetSubmissionSummary
         *
         * @return AviaryQuery::GetSubmissionSummaryResponse*
         */
        

         virtual 
        AviaryQuery::GetSubmissionSummaryResponse* getSubmissionSummary(wso2wsf::MessageContext *outCtx ,AviaryQuery::GetSubmissionSummary* _getSubmissionSummary);


     




		 

        /**
         * Auto generated method declaration
         * for "getJobDetails|http://grid.redhat.com/aviary-query/" operation.
         * 
         * @param _getJobDetails of the AviaryQuery::GetJobDetails
         *
         * @return AviaryQuery::GetJobDetailsResponse*
         */
        

         virtual 
        AviaryQuery::GetJobDetailsResponse* getJobDetails(wso2wsf::MessageContext *outCtx ,AviaryQuery::GetJobDetails* _getJobDetails);


         /**
         * Auto generated method declaration
         * for "getJobSummary|http://grid.redhat.com/aviary-query/" operation.
         *
         * @param _getJobSummary of the AviaryQuery::GetJobSummary
         *
         * @return AviaryQuery::GetJobSummaryResponse*
         */


         virtual
        AviaryQuery::GetJobSummaryResponse* getJobSummary(wso2wsf::MessageContext *outCtx ,AviaryQuery::GetJobSummary* _getJobSummary);


};


}



        
#endif // AVIARYQUERYSERVICESKELETON_H
    

