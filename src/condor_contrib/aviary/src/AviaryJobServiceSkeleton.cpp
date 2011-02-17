

    /**
     * AviaryJobServiceSkeleton.cpp
     *
     * This file was Auto-Generated from WSDL for "AviaryJobService|http://grid.redhat.com/aviary-job/" service
     * by the WSO2 WSF/CPP version:
     * AviaryJobServiceSkeleton WSF/CPP Skeleton For the Service
     */

     #include "AviaryJobServiceSkeleton.h"

    
     #include <RemoveJob.h>
    
     #include <RemoveJobResponse.h>
    
     #include <ReleaseJob.h>
    
     #include <ReleaseJobResponse.h>
    
     #include <SubmitJob.h>
    
     #include <SubmitJobResponse.h>
    
     #include <HoldJob.h>
    
     #include <HoldJobResponse.h>
    
     #include <SetJobAttribute.h>
    
     #include <SetJobAttributeResponse.h>

    using namespace std;
    using namespace com_redhat_grid_aviary_job;
    using namespace com_redhat_grid_aviary_common;


		 
        /**
         * Auto generated function definition signature
         * for "removeJob|http://grid.redhat.com/aviary-job/" operation.
         * 
         * @param _removeJob of the com_redhat_grid_aviary_common::RemoveJob
         *
         * @return com_redhat_grid_aviary_common::RemoveJobResponse*
         */
        com_redhat_grid_aviary_common::RemoveJobResponse* AviaryJobServiceSkeleton::removeJob(wso2wsf::MessageContext *outCtx ,com_redhat_grid_aviary_common::RemoveJob* _removeJob)

        {
          /* TODO fill this with the necessary business logic */
          return (com_redhat_grid_aviary_common::RemoveJobResponse*)NULL;
        }
     

		 
        /**
         * Auto generated function definition signature
         * for "releaseJob|http://grid.redhat.com/aviary-job/" operation.
         * 
         * @param _releaseJob of the com_redhat_grid_aviary_common::ReleaseJob
         *
         * @return com_redhat_grid_aviary_common::ReleaseJobResponse*
         */
        com_redhat_grid_aviary_common::ReleaseJobResponse* AviaryJobServiceSkeleton::releaseJob(wso2wsf::MessageContext *outCtx ,com_redhat_grid_aviary_common::ReleaseJob* _releaseJob)

        {
          /* TODO fill this with the necessary business logic */
          return (com_redhat_grid_aviary_common::ReleaseJobResponse*)NULL;
        }
     

		 
        /**
         * Auto generated function definition signature
         * for "submitJob|http://grid.redhat.com/aviary-job/" operation.
         * 
         * @param _submitJob of the com_redhat_grid_aviary_common::SubmitJob
         *
         * @return com_redhat_grid_aviary_common::SubmitJobResponse*
         */
        com_redhat_grid_aviary_common::SubmitJobResponse* AviaryJobServiceSkeleton::submitJob(wso2wsf::MessageContext *outCtx ,com_redhat_grid_aviary_common::SubmitJob* _submitJob)

        {
           cout << _submitJob->getCmd() << endl;
          JobID* arg_Id = new JobID("MyPool","MyScheduler","1.0");
          StatusCodeType* statusCode = new StatusCodeType();
          statusCode->setStatusCodeTypeEnum(StatusCodeType_SUCCESS);
          string status_str;
          Status* arg_Status = new Status(statusCode,status_str);
          SubmitJobResponse* response = new SubmitJobResponse(arg_Id,arg_Status);
          return response;
        }
     

		 
        /**
         * Auto generated function definition signature
         * for "holdJob|http://grid.redhat.com/aviary-job/" operation.
         * 
         * @param _holdJob of the com_redhat_grid_aviary_common::HoldJob
         *
         * @return com_redhat_grid_aviary_common::HoldJobResponse*
         */
        com_redhat_grid_aviary_common::HoldJobResponse* AviaryJobServiceSkeleton::holdJob(wso2wsf::MessageContext *outCtx ,com_redhat_grid_aviary_common::HoldJob* _holdJob)

        {
          /* TODO fill this with the necessary business logic */
          return (com_redhat_grid_aviary_common::HoldJobResponse*)NULL;
        }
     

		 
        /**
         * Auto generated function definition signature
         * for "setJobAttribute|http://grid.redhat.com/aviary-job/" operation.
         * 
         * @param _setJobAttribute of the com_redhat_grid_aviary_common::SetJobAttribute
         *
         * @return com_redhat_grid_aviary_common::SetJobAttributeResponse*
         */
        com_redhat_grid_aviary_common::SetJobAttributeResponse* AviaryJobServiceSkeleton::setJobAttribute(wso2wsf::MessageContext *outCtx ,com_redhat_grid_aviary_common::SetJobAttribute* _setJobAttribute)

        {
          /* TODO fill this with the necessary business logic */
          return (com_redhat_grid_aviary_common::SetJobAttributeResponse*)NULL;
        }
     

