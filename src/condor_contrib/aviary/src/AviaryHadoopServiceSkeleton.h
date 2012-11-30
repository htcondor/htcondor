
    /**
     * AviaryHadoopServiceSkeleton.h
     *
     * This file was auto-generated from WSDL for "AviaryHadoopService|http://grid.redhat.com/aviary-hadoop/" service
     * by the WSO2 WSF/CPP Version: 1.0
     * AviaryHadoopServiceSkeleton WSO2 WSF/CPP Skeleton for the Service Header File
     */
#ifndef AVIARYHADOOPSERVICESKELETON_H
#define AVIARYHADOOPSERVICESKELETON_H

    #include <OMElement.h>
    #include <MessageContext.h>
   
     #include <AviaryHadoop_StartTaskTracker.h>
    
     #include <AviaryHadoop_StartTaskTrackerResponse.h>
    
     #include <AviaryHadoop_StartDataNode.h>
    
     #include <AviaryHadoop_StartDataNodeResponse.h>
    
     #include <AviaryHadoop_GetTaskTracker.h>
    
     #include <AviaryHadoop_GetTaskTrackerResponse.h>
    
     #include <AviaryHadoop_StopJobTracker.h>
    
     #include <AviaryHadoop_StopJobTrackerResponse.h>
    
     #include <AviaryHadoop_GetJobTracker.h>
    
     #include <AviaryHadoop_GetJobTrackerResponse.h>
    
     #include <AviaryHadoop_StopTaskTracker.h>
    
     #include <AviaryHadoop_StopTaskTrackerResponse.h>
    
     #include <AviaryHadoop_StartNameNode.h>
    
     #include <AviaryHadoop_StartNameNodeResponse.h>
    
     #include <AviaryHadoop_GetDataNode.h>
    
     #include <AviaryHadoop_GetDataNodeResponse.h>
    
     #include <AviaryHadoop_StopNameNode.h>
    
     #include <AviaryHadoop_StopNameNodeResponse.h>
    
     #include <AviaryHadoop_GetNameNode.h>
    
     #include <AviaryHadoop_GetNameNodeResponse.h>
    
     #include <AviaryHadoop_StartJobTracker.h>
    
     #include <AviaryHadoop_StartJobTrackerResponse.h>
    
     #include <AviaryHadoop_StopDataNode.h>
    
     #include <AviaryHadoop_StopDataNodeResponse.h>
    
namespace AviaryHadoop{
    

   /** we have to reserve some error codes for adb and for custom messages */
    #define AVIARYHADOOPSERVICESKELETON_ERROR_CODES_START (AXIS2_ERROR_LAST + 2500)

    typedef enum
    {
        AVIARYHADOOPSERVICESKELETON_ERROR_NONE = AVIARYHADOOPSERVICESKELETON_ERROR_CODES_START,

        AVIARYHADOOPSERVICESKELETON_ERROR_LAST
    } AviaryHadoopServiceSkeleton_error_codes;

    


class AviaryHadoopServiceSkeleton
{
        public:
            AviaryHadoopServiceSkeleton(){}


     




		 


        /**
         * Auto generated method declaration
         * for "startTaskTracker|http://grid.redhat.com/aviary-hadoop/" operation.
         * 
         * @param _startTaskTracker of the AviaryHadoop::StartTaskTracker
         *
         * @return AviaryHadoop::StartTaskTrackerResponse*
         */
        

         virtual 
        AviaryHadoop::StartTaskTrackerResponse* startTaskTracker(wso2wsf::MessageContext *outCtx ,AviaryHadoop::StartTaskTracker* _startTaskTracker);


     




		 


        /**
         * Auto generated method declaration
         * for "startDataNode|http://grid.redhat.com/aviary-hadoop/" operation.
         * 
         * @param _startDataNode of the AviaryHadoop::StartDataNode
         *
         * @return AviaryHadoop::StartDataNodeResponse*
         */
        

         virtual 
        AviaryHadoop::StartDataNodeResponse* startDataNode(wso2wsf::MessageContext *outCtx ,AviaryHadoop::StartDataNode* _startDataNode);


     




		 


        /**
         * Auto generated method declaration
         * for "getTaskTracker|http://grid.redhat.com/aviary-hadoop/" operation.
         * 
         * @param _getTaskTracker of the AviaryHadoop::GetTaskTracker
         *
         * @return AviaryHadoop::GetTaskTrackerResponse*
         */
        

         virtual 
        AviaryHadoop::GetTaskTrackerResponse* getTaskTracker(wso2wsf::MessageContext *outCtx ,AviaryHadoop::GetTaskTracker* _getTaskTracker);


     




		 


        /**
         * Auto generated method declaration
         * for "stopJobTracker|http://grid.redhat.com/aviary-hadoop/" operation.
         * 
         * @param _stopJobTracker of the AviaryHadoop::StopJobTracker
         *
         * @return AviaryHadoop::StopJobTrackerResponse*
         */
        

         virtual 
        AviaryHadoop::StopJobTrackerResponse* stopJobTracker(wso2wsf::MessageContext *outCtx ,AviaryHadoop::StopJobTracker* _stopJobTracker);


     




		 


        /**
         * Auto generated method declaration
         * for "getJobTracker|http://grid.redhat.com/aviary-hadoop/" operation.
         * 
         * @param _getJobTracker of the AviaryHadoop::GetJobTracker
         *
         * @return AviaryHadoop::GetJobTrackerResponse*
         */
        

         virtual 
        AviaryHadoop::GetJobTrackerResponse* getJobTracker(wso2wsf::MessageContext *outCtx ,AviaryHadoop::GetJobTracker* _getJobTracker);


     




		 


        /**
         * Auto generated method declaration
         * for "stopTaskTracker|http://grid.redhat.com/aviary-hadoop/" operation.
         * 
         * @param _stopTaskTracker of the AviaryHadoop::StopTaskTracker
         *
         * @return AviaryHadoop::StopTaskTrackerResponse*
         */
        

         virtual 
        AviaryHadoop::StopTaskTrackerResponse* stopTaskTracker(wso2wsf::MessageContext *outCtx ,AviaryHadoop::StopTaskTracker* _stopTaskTracker);


     




		 


        /**
         * Auto generated method declaration
         * for "startNameNode|http://grid.redhat.com/aviary-hadoop/" operation.
         * 
         * @param _startNameNode of the AviaryHadoop::StartNameNode
         *
         * @return AviaryHadoop::StartNameNodeResponse*
         */
        

         virtual 
        AviaryHadoop::StartNameNodeResponse* startNameNode(wso2wsf::MessageContext *outCtx ,AviaryHadoop::StartNameNode* _startNameNode);


     




		 


        /**
         * Auto generated method declaration
         * for "getDataNode|http://grid.redhat.com/aviary-hadoop/" operation.
         * 
         * @param _getDataNode of the AviaryHadoop::GetDataNode
         *
         * @return AviaryHadoop::GetDataNodeResponse*
         */
        

         virtual 
        AviaryHadoop::GetDataNodeResponse* getDataNode(wso2wsf::MessageContext *outCtx ,AviaryHadoop::GetDataNode* _getDataNode);


     




		 


        /**
         * Auto generated method declaration
         * for "stopNameNode|http://grid.redhat.com/aviary-hadoop/" operation.
         * 
         * @param _stopNameNode of the AviaryHadoop::StopNameNode
         *
         * @return AviaryHadoop::StopNameNodeResponse*
         */
        

         virtual 
        AviaryHadoop::StopNameNodeResponse* stopNameNode(wso2wsf::MessageContext *outCtx ,AviaryHadoop::StopNameNode* _stopNameNode);


     




		 


        /**
         * Auto generated method declaration
         * for "getNameNode|http://grid.redhat.com/aviary-hadoop/" operation.
         * 
         * @param _getNameNode of the AviaryHadoop::GetNameNode
         *
         * @return AviaryHadoop::GetNameNodeResponse*
         */
        

         virtual 
        AviaryHadoop::GetNameNodeResponse* getNameNode(wso2wsf::MessageContext *outCtx ,AviaryHadoop::GetNameNode* _getNameNode);


     




		 


        /**
         * Auto generated method declaration
         * for "startJobTracker|http://grid.redhat.com/aviary-hadoop/" operation.
         * 
         * @param _startJobTracker of the AviaryHadoop::StartJobTracker
         *
         * @return AviaryHadoop::StartJobTrackerResponse*
         */
        

         virtual 
        AviaryHadoop::StartJobTrackerResponse* startJobTracker(wso2wsf::MessageContext *outCtx ,AviaryHadoop::StartJobTracker* _startJobTracker);


     




		 


        /**
         * Auto generated method declaration
         * for "stopDataNode|http://grid.redhat.com/aviary-hadoop/" operation.
         * 
         * @param _stopDataNode of the AviaryHadoop::StopDataNode
         *
         * @return AviaryHadoop::StopDataNodeResponse*
         */
        

         virtual 
        AviaryHadoop::StopDataNodeResponse* stopDataNode(wso2wsf::MessageContext *outCtx ,AviaryHadoop::StopDataNode* _stopDataNode);


     



};


}



        
#endif // AVIARYHADOOPSERVICESKELETON_H
    

