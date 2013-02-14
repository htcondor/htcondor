

#ifndef IAVIARYHADOOPSERVICECALLBACK_H
#define IAVIARYHADOOPSERVICECALLBACK_H
/**
 * IAviaryHadoopServiceCallback.h
 *
 * This file was auto-generated from WSDL
 * by the Apache Axis2 version: 1.0  Built on : Jan 28, 2013 (02:29:53 CST)
 */
       
#include "AviaryHadoop_StartTaskTrackerResponse.h"
        
#include "AviaryHadoop_StartDataNodeResponse.h"
        
#include "AviaryHadoop_GetTaskTrackerResponse.h"
        
#include "AviaryHadoop_StopJobTrackerResponse.h"
        
#include "AviaryHadoop_GetJobTrackerResponse.h"
        
#include "AviaryHadoop_StopTaskTrackerResponse.h"
        
#include "AviaryHadoop_StartNameNodeResponse.h"
        
#include "AviaryHadoop_GetDataNodeResponse.h"
        
#include "AviaryHadoop_StopNameNodeResponse.h"
        
#include "AviaryHadoop_GetNameNodeResponse.h"
        
#include "AviaryHadoop_StartJobTrackerResponse.h"
        
#include "AviaryHadoop_StopDataNodeResponse.h"
        

#include "AviaryHadoopServiceStub.h" 

 namespace com_redhat_grid_aviary_hadoop
   {

    /**
     *  IAviaryHadoopServiceCallback Callback class, Users can extend this class and implement
     *  their own receiveResult and receiveError methods.
     */

    class IAviaryHadoopServiceCallback
{


    protected:

    void* clientData;

    public:

    /**
    * User can pass in any object that needs to be accessed once the NonBlocking
    * Web service call is finished and appropriate method of this CallBack is called.
    * @param clientData Object mechanism by which the user can pass in user data
    * that will be available at the time this callback is called.
    */
    IAviaryHadoopServiceCallback(void* clientData);

    /**
    * Please use this constructor if you don't want to set any clientData
    */
    IAviaryHadoopServiceCallback();


    /**
     * Get the client data
     */

     void* getClientData();

    
           /**
            * auto generated WSF/C++ call back method for startTaskTracker method
            * override this method for handling normal response from startTaskTracker operation
            */
          virtual void receiveResult_startTaskTracker(
              
                    AviaryHadoop::StartTaskTrackerResponse* result
                    
                    ){}


          /**
           * auto generated WSF/C++ Error handler
           * override this method for handling error response from startTaskTracker operation
           */
           virtual void receiveError_startTaskTracker(int exception){}
            
           /**
            * auto generated WSF/C++ call back method for startDataNode method
            * override this method for handling normal response from startDataNode operation
            */
          virtual void receiveResult_startDataNode(
              
                    AviaryHadoop::StartDataNodeResponse* result
                    
                    ){}


          /**
           * auto generated WSF/C++ Error handler
           * override this method for handling error response from startDataNode operation
           */
           virtual void receiveError_startDataNode(int exception){}
            
           /**
            * auto generated WSF/C++ call back method for getTaskTracker method
            * override this method for handling normal response from getTaskTracker operation
            */
          virtual void receiveResult_getTaskTracker(
              
                    AviaryHadoop::GetTaskTrackerResponse* result
                    
                    ){}


          /**
           * auto generated WSF/C++ Error handler
           * override this method for handling error response from getTaskTracker operation
           */
           virtual void receiveError_getTaskTracker(int exception){}
            
           /**
            * auto generated WSF/C++ call back method for stopJobTracker method
            * override this method for handling normal response from stopJobTracker operation
            */
          virtual void receiveResult_stopJobTracker(
              
                    AviaryHadoop::StopJobTrackerResponse* result
                    
                    ){}


          /**
           * auto generated WSF/C++ Error handler
           * override this method for handling error response from stopJobTracker operation
           */
           virtual void receiveError_stopJobTracker(int exception){}
            
           /**
            * auto generated WSF/C++ call back method for getJobTracker method
            * override this method for handling normal response from getJobTracker operation
            */
          virtual void receiveResult_getJobTracker(
              
                    AviaryHadoop::GetJobTrackerResponse* result
                    
                    ){}


          /**
           * auto generated WSF/C++ Error handler
           * override this method for handling error response from getJobTracker operation
           */
           virtual void receiveError_getJobTracker(int exception){}
            
           /**
            * auto generated WSF/C++ call back method for stopTaskTracker method
            * override this method for handling normal response from stopTaskTracker operation
            */
          virtual void receiveResult_stopTaskTracker(
              
                    AviaryHadoop::StopTaskTrackerResponse* result
                    
                    ){}


          /**
           * auto generated WSF/C++ Error handler
           * override this method for handling error response from stopTaskTracker operation
           */
           virtual void receiveError_stopTaskTracker(int exception){}
            
           /**
            * auto generated WSF/C++ call back method for startNameNode method
            * override this method for handling normal response from startNameNode operation
            */
          virtual void receiveResult_startNameNode(
              
                    AviaryHadoop::StartNameNodeResponse* result
                    
                    ){}


          /**
           * auto generated WSF/C++ Error handler
           * override this method for handling error response from startNameNode operation
           */
           virtual void receiveError_startNameNode(int exception){}
            
           /**
            * auto generated WSF/C++ call back method for getDataNode method
            * override this method for handling normal response from getDataNode operation
            */
          virtual void receiveResult_getDataNode(
              
                    AviaryHadoop::GetDataNodeResponse* result
                    
                    ){}


          /**
           * auto generated WSF/C++ Error handler
           * override this method for handling error response from getDataNode operation
           */
           virtual void receiveError_getDataNode(int exception){}
            
           /**
            * auto generated WSF/C++ call back method for stopNameNode method
            * override this method for handling normal response from stopNameNode operation
            */
          virtual void receiveResult_stopNameNode(
              
                    AviaryHadoop::StopNameNodeResponse* result
                    
                    ){}


          /**
           * auto generated WSF/C++ Error handler
           * override this method for handling error response from stopNameNode operation
           */
           virtual void receiveError_stopNameNode(int exception){}
            
           /**
            * auto generated WSF/C++ call back method for getNameNode method
            * override this method for handling normal response from getNameNode operation
            */
          virtual void receiveResult_getNameNode(
              
                    AviaryHadoop::GetNameNodeResponse* result
                    
                    ){}


          /**
           * auto generated WSF/C++ Error handler
           * override this method for handling error response from getNameNode operation
           */
           virtual void receiveError_getNameNode(int exception){}
            
           /**
            * auto generated WSF/C++ call back method for startJobTracker method
            * override this method for handling normal response from startJobTracker operation
            */
          virtual void receiveResult_startJobTracker(
              
                    AviaryHadoop::StartJobTrackerResponse* result
                    
                    ){}


          /**
           * auto generated WSF/C++ Error handler
           * override this method for handling error response from startJobTracker operation
           */
           virtual void receiveError_startJobTracker(int exception){}
            
           /**
            * auto generated WSF/C++ call back method for stopDataNode method
            * override this method for handling normal response from stopDataNode operation
            */
          virtual void receiveResult_stopDataNode(
              
                    AviaryHadoop::StopDataNodeResponse* result
                    
                    ){}


          /**
           * auto generated WSF/C++ Error handler
           * override this method for handling error response from stopDataNode operation
           */
           virtual void receiveError_stopDataNode(int exception){}
            


    };
}

#endif  //IAVIARYHADOOPSERVICECALLBACK_H
    

