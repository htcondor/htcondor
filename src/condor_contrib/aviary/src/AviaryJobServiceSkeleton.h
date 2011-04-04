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

#ifndef AVIARYJOBSERVICESKELETON_H
#define AVIARYJOBSERVICESKELETON_H

    #include <OMElement.h>
    #include <MessageContext.h>
   
     #include <AviaryJob_RemoveJob.h>
    
     #include <AviaryJob_RemoveJobResponse.h>
    
     #include <AviaryJob_ReleaseJob.h>
    
     #include <AviaryJob_ReleaseJobResponse.h>
    
     #include <AviaryJob_SubmitJob.h>
    
     #include <AviaryJob_SubmitJobResponse.h>
    
     #include <AviaryJob_HoldJob.h>
    
     #include <AviaryJob_HoldJobResponse.h>
    
     #include <AviaryJob_SetJobAttribute.h>
    
     #include <AviaryJob_SetJobAttributeResponse.h>

typedef std::vector<AviaryCommon::ResourceConstraint*> ResourceConstraintVectorType;
    
namespace AviaryJob {
    

   /** we have to reserve some error codes for adb and for custom messages */
    #define AVIARYJOBSERVICESKELETON_ERROR_CODES_START (AXIS2_ERROR_LAST + 2500)

    typedef enum
    {
        AVIARYJOBSERVICESKELETON_ERROR_NONE = AVIARYJOBSERVICESKELETON_ERROR_CODES_START,

        AVIARYJOBSERVICESKELETON_ERROR_LAST
    } AviaryJobServiceSkeleton_error_codes;

    


class AviaryJobServiceSkeleton
{

        public:
            AviaryJobServiceSkeleton(){}


     




		 


        /**
         * Auto generated method declaration
         * for "removeJob|http://grid.redhat.com/aviary-job/" operation.
         * 
         * @param _removeJob of the AviaryJob::RemoveJob
         *
         * @return AviaryJob::RemoveJobResponse*
         */
        

         virtual 
        AviaryJob::RemoveJobResponse* removeJob(wso2wsf::MessageContext *outCtx ,AviaryJob::RemoveJob* _removeJob);


     




		 


        /**
         * Auto generated method declaration
         * for "releaseJob|http://grid.redhat.com/aviary-job/" operation.
         * 
         * @param _releaseJob of the AviaryJob::ReleaseJob
         *
         * @return AviaryJob::ReleaseJobResponse*
         */
        

         virtual 
        AviaryJob::ReleaseJobResponse* releaseJob(wso2wsf::MessageContext *outCtx ,AviaryJob::ReleaseJob* _releaseJob);


     




		 


        /**
         * Auto generated method declaration
         * for "submitJob|http://grid.redhat.com/aviary-job/" operation.
         * 
         * @param _submitJob of the AviaryJob::SubmitJob
         *
         * @return AviaryJob::SubmitJobResponse*
         */
        

         virtual 
        AviaryJob::SubmitJobResponse* submitJob(wso2wsf::MessageContext *outCtx ,AviaryJob::SubmitJob* _submitJob);


     




		 


        /**
         * Auto generated method declaration
         * for "holdJob|http://grid.redhat.com/aviary-job/" operation.
         * 
         * @param _holdJob of the AviaryJob::HoldJob
         *
         * @return AviaryJob::HoldJobResponse*
         */
        

         virtual 
        AviaryJob::HoldJobResponse* holdJob(wso2wsf::MessageContext *outCtx ,AviaryJob::HoldJob* _holdJob);


     




		 


        /**
         * Auto generated method declaration
         * for "setJobAttribute|http://grid.redhat.com/aviary-job/" operation.
         * 
         * @param _setJobAttribute of the AviaryJob::SetJobAttribute
         *
         * @return AviaryJob::SetJobAttributeResponse*
         */
        

         virtual 
        AviaryJob::SetJobAttributeResponse* setJobAttribute(wso2wsf::MessageContext *outCtx ,AviaryJob::SetJobAttribute* _setJobAttribute);


     



};


}



        
#endif // AVIARYJOBSERVICESKELETON_H
    

