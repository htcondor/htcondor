

#ifndef AVIARYHADOOPSERVICESTUB_H
#define AVIARYHADOOPSERVICESTUB_H
/**
* AviaryHadoopServiceStub.h
*
* This file was auto-generated from WSDL for "AviaryHadoopService|http://grid.redhat.com/aviary-hadoop/" service
* by the Apache Axis2/Java version: 1.0  Built on : Jan 28, 2013 (02:29:53 CST)
*/

#include <stdio.h>
#include <OMElement.h>
#include <Stub.h>
#include <ServiceClient.h>


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


namespace com_redhat_grid_aviary_hadoop
{

#define AVIARYHADOOPSERVICESTUB_ERROR_CODES_START (AXIS2_ERROR_LAST + 2000)

typedef enum
{
     AVIARYHADOOPSERVICESTUB_ERROR_NONE = AVIARYHADOOPSERVICESTUB_ERROR_CODES_START,

    AVIARYHADOOPSERVICESTUB_ERROR_LAST
} AviaryHadoopServiceStub_error_codes;

 class IAviaryHadoopServiceCallback;

 

class AviaryHadoopServiceStub : public wso2wsf::Stub
{

        public:
        /**
         *  Constructor of AviaryHadoopService class
         *  @param client_home WSF/C home directory
         *  
         */
        AviaryHadoopServiceStub(std::string& client_home);

        /**
         *  Constructor of AviaryHadoopService class
         *  @param client_home WSF/C home directory
         *  @param endpoint_uri The to endpoint uri,
         */

        AviaryHadoopServiceStub(std::string& client_home, std::string& endpoint_uri);

        /**
         *  Destructor of AviaryHadoopService class
         */

        ~AviaryHadoopServiceStub();

        /**
         * Populate Services for AviaryHadoopServiceStub
         */
        void WSF_CALL
        populateServicesForAviaryHadoopService();

        /**
         * Get the endpoint uri of the AviaryHadoopServiceStub
         */

        std::string WSF_CALL
        getEndpointUriOfAviaryHadoopService();

        

            /**
             * Auto generated function declaration
             * for "startTaskTracker|http://grid.redhat.com/aviary-hadoop/" operation.
             * 
             * @param _startTaskTracker72 of the AviaryHadoop::StartTaskTracker
             *
             * @return AviaryHadoop::StartTaskTrackerResponse*
             */

            AviaryHadoop::StartTaskTrackerResponse* WSF_CALL startTaskTracker( AviaryHadoop::StartTaskTracker*  _startTaskTracker72);
          

            /**
             * Auto generated function declaration
             * for "startDataNode|http://grid.redhat.com/aviary-hadoop/" operation.
             * 
             * @param _startDataNode74 of the AviaryHadoop::StartDataNode
             *
             * @return AviaryHadoop::StartDataNodeResponse*
             */

            AviaryHadoop::StartDataNodeResponse* WSF_CALL startDataNode( AviaryHadoop::StartDataNode*  _startDataNode74);
          

            /**
             * Auto generated function declaration
             * for "getTaskTracker|http://grid.redhat.com/aviary-hadoop/" operation.
             * 
             * @param _getTaskTracker76 of the AviaryHadoop::GetTaskTracker
             *
             * @return AviaryHadoop::GetTaskTrackerResponse*
             */

            AviaryHadoop::GetTaskTrackerResponse* WSF_CALL getTaskTracker( AviaryHadoop::GetTaskTracker*  _getTaskTracker76);
          

            /**
             * Auto generated function declaration
             * for "stopJobTracker|http://grid.redhat.com/aviary-hadoop/" operation.
             * 
             * @param _stopJobTracker78 of the AviaryHadoop::StopJobTracker
             *
             * @return AviaryHadoop::StopJobTrackerResponse*
             */

            AviaryHadoop::StopJobTrackerResponse* WSF_CALL stopJobTracker( AviaryHadoop::StopJobTracker*  _stopJobTracker78);
          

            /**
             * Auto generated function declaration
             * for "getJobTracker|http://grid.redhat.com/aviary-hadoop/" operation.
             * 
             * @param _getJobTracker80 of the AviaryHadoop::GetJobTracker
             *
             * @return AviaryHadoop::GetJobTrackerResponse*
             */

            AviaryHadoop::GetJobTrackerResponse* WSF_CALL getJobTracker( AviaryHadoop::GetJobTracker*  _getJobTracker80);
          

            /**
             * Auto generated function declaration
             * for "stopTaskTracker|http://grid.redhat.com/aviary-hadoop/" operation.
             * 
             * @param _stopTaskTracker82 of the AviaryHadoop::StopTaskTracker
             *
             * @return AviaryHadoop::StopTaskTrackerResponse*
             */

            AviaryHadoop::StopTaskTrackerResponse* WSF_CALL stopTaskTracker( AviaryHadoop::StopTaskTracker*  _stopTaskTracker82);
          

            /**
             * Auto generated function declaration
             * for "startNameNode|http://grid.redhat.com/aviary-hadoop/" operation.
             * 
             * @param _startNameNode84 of the AviaryHadoop::StartNameNode
             *
             * @return AviaryHadoop::StartNameNodeResponse*
             */

            AviaryHadoop::StartNameNodeResponse* WSF_CALL startNameNode( AviaryHadoop::StartNameNode*  _startNameNode84);
          

            /**
             * Auto generated function declaration
             * for "getDataNode|http://grid.redhat.com/aviary-hadoop/" operation.
             * 
             * @param _getDataNode86 of the AviaryHadoop::GetDataNode
             *
             * @return AviaryHadoop::GetDataNodeResponse*
             */

            AviaryHadoop::GetDataNodeResponse* WSF_CALL getDataNode( AviaryHadoop::GetDataNode*  _getDataNode86);
          

            /**
             * Auto generated function declaration
             * for "stopNameNode|http://grid.redhat.com/aviary-hadoop/" operation.
             * 
             * @param _stopNameNode88 of the AviaryHadoop::StopNameNode
             *
             * @return AviaryHadoop::StopNameNodeResponse*
             */

            AviaryHadoop::StopNameNodeResponse* WSF_CALL stopNameNode( AviaryHadoop::StopNameNode*  _stopNameNode88);
          

            /**
             * Auto generated function declaration
             * for "getNameNode|http://grid.redhat.com/aviary-hadoop/" operation.
             * 
             * @param _getNameNode90 of the AviaryHadoop::GetNameNode
             *
             * @return AviaryHadoop::GetNameNodeResponse*
             */

            AviaryHadoop::GetNameNodeResponse* WSF_CALL getNameNode( AviaryHadoop::GetNameNode*  _getNameNode90);
          

            /**
             * Auto generated function declaration
             * for "startJobTracker|http://grid.redhat.com/aviary-hadoop/" operation.
             * 
             * @param _startJobTracker92 of the AviaryHadoop::StartJobTracker
             *
             * @return AviaryHadoop::StartJobTrackerResponse*
             */

            AviaryHadoop::StartJobTrackerResponse* WSF_CALL startJobTracker( AviaryHadoop::StartJobTracker*  _startJobTracker92);
          

            /**
             * Auto generated function declaration
             * for "stopDataNode|http://grid.redhat.com/aviary-hadoop/" operation.
             * 
             * @param _stopDataNode94 of the AviaryHadoop::StopDataNode
             *
             * @return AviaryHadoop::StopDataNodeResponse*
             */

            AviaryHadoop::StopDataNodeResponse* WSF_CALL stopDataNode( AviaryHadoop::StopDataNode*  _stopDataNode94);
          

        /**
         * Auto generated function for asynchronous invocations
         * for "startTaskTracker|http://grid.redhat.com/aviary-hadoop/" operation.
         * @param stub The stub
         * 
         * @param _startTaskTracker72 of the AviaryHadoop::StartTaskTracker
         * @param ICallback callback handler
         */


        void WSF_CALL
        start_startTaskTracker(AviaryHadoop::StartTaskTracker*  _startTaskTracker72,IAviaryHadoopServiceCallback* callback);

        

        /**
         * Auto generated function for asynchronous invocations
         * for "startDataNode|http://grid.redhat.com/aviary-hadoop/" operation.
         * @param stub The stub
         * 
         * @param _startDataNode74 of the AviaryHadoop::StartDataNode
         * @param ICallback callback handler
         */


        void WSF_CALL
        start_startDataNode(AviaryHadoop::StartDataNode*  _startDataNode74,IAviaryHadoopServiceCallback* callback);

        

        /**
         * Auto generated function for asynchronous invocations
         * for "getTaskTracker|http://grid.redhat.com/aviary-hadoop/" operation.
         * @param stub The stub
         * 
         * @param _getTaskTracker76 of the AviaryHadoop::GetTaskTracker
         * @param ICallback callback handler
         */


        void WSF_CALL
        start_getTaskTracker(AviaryHadoop::GetTaskTracker*  _getTaskTracker76,IAviaryHadoopServiceCallback* callback);

        

        /**
         * Auto generated function for asynchronous invocations
         * for "stopJobTracker|http://grid.redhat.com/aviary-hadoop/" operation.
         * @param stub The stub
         * 
         * @param _stopJobTracker78 of the AviaryHadoop::StopJobTracker
         * @param ICallback callback handler
         */


        void WSF_CALL
        start_stopJobTracker(AviaryHadoop::StopJobTracker*  _stopJobTracker78,IAviaryHadoopServiceCallback* callback);

        

        /**
         * Auto generated function for asynchronous invocations
         * for "getJobTracker|http://grid.redhat.com/aviary-hadoop/" operation.
         * @param stub The stub
         * 
         * @param _getJobTracker80 of the AviaryHadoop::GetJobTracker
         * @param ICallback callback handler
         */


        void WSF_CALL
        start_getJobTracker(AviaryHadoop::GetJobTracker*  _getJobTracker80,IAviaryHadoopServiceCallback* callback);

        

        /**
         * Auto generated function for asynchronous invocations
         * for "stopTaskTracker|http://grid.redhat.com/aviary-hadoop/" operation.
         * @param stub The stub
         * 
         * @param _stopTaskTracker82 of the AviaryHadoop::StopTaskTracker
         * @param ICallback callback handler
         */


        void WSF_CALL
        start_stopTaskTracker(AviaryHadoop::StopTaskTracker*  _stopTaskTracker82,IAviaryHadoopServiceCallback* callback);

        

        /**
         * Auto generated function for asynchronous invocations
         * for "startNameNode|http://grid.redhat.com/aviary-hadoop/" operation.
         * @param stub The stub
         * 
         * @param _startNameNode84 of the AviaryHadoop::StartNameNode
         * @param ICallback callback handler
         */


        void WSF_CALL
        start_startNameNode(AviaryHadoop::StartNameNode*  _startNameNode84,IAviaryHadoopServiceCallback* callback);

        

        /**
         * Auto generated function for asynchronous invocations
         * for "getDataNode|http://grid.redhat.com/aviary-hadoop/" operation.
         * @param stub The stub
         * 
         * @param _getDataNode86 of the AviaryHadoop::GetDataNode
         * @param ICallback callback handler
         */


        void WSF_CALL
        start_getDataNode(AviaryHadoop::GetDataNode*  _getDataNode86,IAviaryHadoopServiceCallback* callback);

        

        /**
         * Auto generated function for asynchronous invocations
         * for "stopNameNode|http://grid.redhat.com/aviary-hadoop/" operation.
         * @param stub The stub
         * 
         * @param _stopNameNode88 of the AviaryHadoop::StopNameNode
         * @param ICallback callback handler
         */


        void WSF_CALL
        start_stopNameNode(AviaryHadoop::StopNameNode*  _stopNameNode88,IAviaryHadoopServiceCallback* callback);

        

        /**
         * Auto generated function for asynchronous invocations
         * for "getNameNode|http://grid.redhat.com/aviary-hadoop/" operation.
         * @param stub The stub
         * 
         * @param _getNameNode90 of the AviaryHadoop::GetNameNode
         * @param ICallback callback handler
         */


        void WSF_CALL
        start_getNameNode(AviaryHadoop::GetNameNode*  _getNameNode90,IAviaryHadoopServiceCallback* callback);

        

        /**
         * Auto generated function for asynchronous invocations
         * for "startJobTracker|http://grid.redhat.com/aviary-hadoop/" operation.
         * @param stub The stub
         * 
         * @param _startJobTracker92 of the AviaryHadoop::StartJobTracker
         * @param ICallback callback handler
         */


        void WSF_CALL
        start_startJobTracker(AviaryHadoop::StartJobTracker*  _startJobTracker92,IAviaryHadoopServiceCallback* callback);

        

        /**
         * Auto generated function for asynchronous invocations
         * for "stopDataNode|http://grid.redhat.com/aviary-hadoop/" operation.
         * @param stub The stub
         * 
         * @param _stopDataNode94 of the AviaryHadoop::StopDataNode
         * @param ICallback callback handler
         */


        void WSF_CALL
        start_stopDataNode(AviaryHadoop::StopDataNode*  _stopDataNode94,IAviaryHadoopServiceCallback* callback);

          


};

/** we have to reserve some error codes for adb and for custom messages */



}


        
#endif        
   

