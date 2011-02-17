

    /**
     * AviaryJobServiceSkeleton.h
     *
     * This file was auto-generated from WSDL for "AviaryJobService|http://grid.redhat.com/aviary-job/" service
     * by the WSO2 WSF/CPP Version: 1.0
     * AviaryJobServiceSkeleton WSO2 WSF/CPP Skeleton for the Service Header File
     */
#ifndef AVIARYJOBSERVICESKELETON_H
#define AVIARYJOBSERVICESKELETON_H

    #include <OMElement.h>
    #include <MessageContext.h>
   
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
    
namespace com_redhat_grid_aviary_job{
    

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
         * @param _removeJob of the com_redhat_grid_aviary_common::RemoveJob
         *
         * @return com_redhat_grid_aviary_common::RemoveJobResponse*
         */
        

         virtual 
        com_redhat_grid_aviary_common::RemoveJobResponse* removeJob(wso2wsf::MessageContext *outCtx ,com_redhat_grid_aviary_common::RemoveJob* _removeJob);


     




		 


        /**
         * Auto generated method declaration
         * for "releaseJob|http://grid.redhat.com/aviary-job/" operation.
         * 
         * @param _releaseJob of the com_redhat_grid_aviary_common::ReleaseJob
         *
         * @return com_redhat_grid_aviary_common::ReleaseJobResponse*
         */
        

         virtual 
        com_redhat_grid_aviary_common::ReleaseJobResponse* releaseJob(wso2wsf::MessageContext *outCtx ,com_redhat_grid_aviary_common::ReleaseJob* _releaseJob);


     




		 


        /**
         * Auto generated method declaration
         * for "submitJob|http://grid.redhat.com/aviary-job/" operation.
         * 
         * @param _submitJob of the com_redhat_grid_aviary_common::SubmitJob
         *
         * @return com_redhat_grid_aviary_common::SubmitJobResponse*
         */
        

         virtual 
        com_redhat_grid_aviary_common::SubmitJobResponse* submitJob(wso2wsf::MessageContext *outCtx ,com_redhat_grid_aviary_common::SubmitJob* _submitJob);


     




		 


        /**
         * Auto generated method declaration
         * for "holdJob|http://grid.redhat.com/aviary-job/" operation.
         * 
         * @param _holdJob of the com_redhat_grid_aviary_common::HoldJob
         *
         * @return com_redhat_grid_aviary_common::HoldJobResponse*
         */
        

         virtual 
        com_redhat_grid_aviary_common::HoldJobResponse* holdJob(wso2wsf::MessageContext *outCtx ,com_redhat_grid_aviary_common::HoldJob* _holdJob);


     




		 


        /**
         * Auto generated method declaration
         * for "setJobAttribute|http://grid.redhat.com/aviary-job/" operation.
         * 
         * @param _setJobAttribute of the com_redhat_grid_aviary_common::SetJobAttribute
         *
         * @return com_redhat_grid_aviary_common::SetJobAttributeResponse*
         */
        

         virtual 
        com_redhat_grid_aviary_common::SetJobAttributeResponse* setJobAttribute(wso2wsf::MessageContext *outCtx ,com_redhat_grid_aviary_common::SetJobAttribute* _setJobAttribute);


     



};


}



        
#endif // AVIARYJOBSERVICESKELETON_H
    

