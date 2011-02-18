

    /**
     * AviaryQueryServiceSkeleton.h
     *
     * This file was auto-generated from WSDL for "AviaryQueryService|http://grid.redhat.com/aviary-query/" service
     * by the WSO2 WSF/CPP Version: 1.0
     * AviaryQueryServiceSkeleton WSO2 WSF/CPP Skeleton for the Service Header File
     */
#ifndef AVIARYQUERYSERVICESKELETON_H
#define AVIARYQUERYSERVICESKELETON_H

    #include <OMElement.h>
    #include <MessageContext.h>
   
     #include <FindJob.h>
    
     #include <FindJobResponse.h>
    
namespace com_redhat_grid_aviary_query{
    

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
         * for "findJob|http://grid.redhat.com/aviary-query/" operation.
         * 
         * @param _findJob of the com_redhat_grid_aviary_common::FindJob
         *
         * @return com_redhat_grid_aviary_common::FindJobResponse*
         */
        

         virtual 
        com_redhat_grid_aviary_common::FindJobResponse* findJob(wso2wsf::MessageContext *outCtx ,com_redhat_grid_aviary_common::FindJob* _findJob);


     



};


}



        
#endif // AVIARYQUERYSERVICESKELETON_H
    

