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

        #include "AviaryHadoopServiceSkeleton.h"
        #include "AviaryHadoopService.h"  
        #include <ServiceSkeleton.h>
        #include <stdio.h>
        #include <axis2_svc.h>
        #include <Environment.h>
        #include <axiom_soap.h>

       #ifdef __GNUC__
       #pragma GCC diagnostic ignored "-Wunused-variable"
       #pragma GCC diagnostic ignored "-Wunused-value"
       #pragma GCC diagnostic ignored "-Wunused-but-set-variable"
       #pragma GCC diagnostic ignored "-Wunused-parameter"
       #pragma GCC diagnostic ignored "-Wcast-qual"
       #pragma GCC diagnostic ignored "-Wshadow"
       #pragma GCC diagnostic ignored "-Wwrite-strings"
       #pragma GCC diagnostic ignored "-Wenum-compare"
       #endif

        using namespace wso2wsf;
        
        using namespace AviaryHadoop;
        

        /** Load the service into axis2 engine */
        WSF_SERVICE_INIT(AviaryHadoopService)

          
         /**
          * function to free any soap input headers
          */
         AviaryHadoopService::AviaryHadoopService()
	{
          skel = wsfGetAviaryHadoopServiceSkeleton();
    }


	void WSF_CALL
	AviaryHadoopService::init()
	{

      return;
	}


	AviaryHadoopService::~AviaryHadoopService()
	{
    }


     

     




	/*
	 * This method invokes the right service method
	 */
	OMElement* WSF_CALL
	AviaryHadoopService::invoke(OMElement *omEle, MessageContext *msgCtx)
	{
         /* Using the function name, invoke the corresponding method
          */

          axis2_op_ctx_t *operation_ctx = NULL;
          axis2_op_t *operation = NULL;
          axutil_qname_t *op_qname = NULL;
          axis2_char_t *op_name = NULL;
          axis2_msg_ctx_t *in_msg_ctx = NULL;
          
          axiom_soap_envelope_t *req_soap_env = NULL;
          axiom_soap_header_t *req_soap_header = NULL;
          axiom_soap_envelope_t *res_soap_env = NULL;
          axiom_soap_header_t *res_soap_header = NULL;

          axiom_node_t *ret_node = NULL;
          axiom_node_t *input_header = NULL;
          axiom_node_t *output_header = NULL;
          axiom_node_t *header_base_node = NULL;
          axis2_msg_ctx_t *msg_ctx = NULL;
          axiom_node_t* content_node = NULL;
          if (omEle) {
              content_node = omEle->getAxiomNode();
          }
          else {
              return NULL;
          }

          
            AviaryHadoop::StartTaskTrackerResponse* ret_val1;
            AviaryHadoop::StartTaskTracker* input_val1;
            
            AviaryHadoop::StartDataNodeResponse* ret_val2;
            AviaryHadoop::StartDataNode* input_val2;
            
            AviaryHadoop::GetTaskTrackerResponse* ret_val3;
            AviaryHadoop::GetTaskTracker* input_val3;
            
            AviaryHadoop::StopJobTrackerResponse* ret_val4;
            AviaryHadoop::StopJobTracker* input_val4;
            
            AviaryHadoop::GetJobTrackerResponse* ret_val5;
            AviaryHadoop::GetJobTracker* input_val5;
            
            AviaryHadoop::StopTaskTrackerResponse* ret_val6;
            AviaryHadoop::StopTaskTracker* input_val6;
            
            AviaryHadoop::StartNameNodeResponse* ret_val7;
            AviaryHadoop::StartNameNode* input_val7;
            
            AviaryHadoop::GetDataNodeResponse* ret_val8;
            AviaryHadoop::GetDataNode* input_val8;
            
            AviaryHadoop::StopNameNodeResponse* ret_val9;
            AviaryHadoop::StopNameNode* input_val9;
            
            AviaryHadoop::GetNameNodeResponse* ret_val10;
            AviaryHadoop::GetNameNode* input_val10;
            
            AviaryHadoop::StartJobTrackerResponse* ret_val11;
            AviaryHadoop::StartJobTracker* input_val11;
            
            AviaryHadoop::StopDataNodeResponse* ret_val12;
            AviaryHadoop::StopDataNode* input_val12;
            
       
          msg_ctx = msgCtx->getAxis2MessageContext();
          operation_ctx = axis2_msg_ctx_get_op_ctx(msg_ctx, Environment::getEnv());
          operation = axis2_op_ctx_get_op(operation_ctx, Environment::getEnv());
          op_qname = (axutil_qname_t *)axis2_op_get_qname(operation, Environment::getEnv());
          op_name = axutil_qname_get_localpart(op_qname, Environment::getEnv());

          if (op_name)
          {
               

                if ( axutil_strcmp(op_name, "startTaskTracker") == 0 )
                {

                    
                    input_val1 =
                        
                        new AviaryHadoop::StartTaskTracker();
                        if( AXIS2_FAILURE ==  input_val1->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryHadoop::StartTaskTracker_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryHadoopServiceSkeleton skel;
                        ret_val1 =  skel->startTaskTracker(msgCtx ,input_val1);
                    
                        if ( NULL == ret_val1 )
                        {
                            
                                delete input_val1;
                            
                            return NULL; 
                        }
                        ret_node = 
                                            ret_val1->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                                            delete ret_val1;
                                        
                                            delete input_val1;
                                        

                        return new OMElement(NULL,ret_node);
                    

                    /* since this has no output params it just returns NULL */                    
                    

                }
             

                if ( axutil_strcmp(op_name, "startDataNode") == 0 )
                {

                    
                    input_val2 =
                        
                        new AviaryHadoop::StartDataNode();
                        if( AXIS2_FAILURE ==  input_val2->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryHadoop::StartDataNode_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryHadoopServiceSkeleton skel;
                        ret_val2 =  skel->startDataNode(msgCtx ,input_val2);
                    
                        if ( NULL == ret_val2 )
                        {
                            
                                delete input_val2;
                            
                            return NULL; 
                        }
                        ret_node = 
                                            ret_val2->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                                            delete ret_val2;
                                        
                                            delete input_val2;
                                        

                        return new OMElement(NULL,ret_node);
                    

                    /* since this has no output params it just returns NULL */                    
                    

                }
             

                if ( axutil_strcmp(op_name, "getTaskTracker") == 0 )
                {

                    
                    input_val3 =
                        
                        new AviaryHadoop::GetTaskTracker();
                        if( AXIS2_FAILURE ==  input_val3->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryHadoop::GetTaskTracker_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryHadoopServiceSkeleton skel;
                        ret_val3 =  skel->getTaskTracker(msgCtx ,input_val3);
                    
                        if ( NULL == ret_val3 )
                        {
                            
                                delete input_val3;
                            
                            return NULL; 
                        }
                        ret_node = 
                                            ret_val3->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                                            delete ret_val3;
                                        
                                            delete input_val3;
                                        

                        return new OMElement(NULL,ret_node);
                    

                    /* since this has no output params it just returns NULL */                    
                    

                }
             

                if ( axutil_strcmp(op_name, "stopJobTracker") == 0 )
                {

                    
                    input_val4 =
                        
                        new AviaryHadoop::StopJobTracker();
                        if( AXIS2_FAILURE ==  input_val4->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryHadoop::StopJobTracker_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryHadoopServiceSkeleton skel;
                        ret_val4 =  skel->stopJobTracker(msgCtx ,input_val4);
                    
                        if ( NULL == ret_val4 )
                        {
                            
                                delete input_val4;
                            
                            return NULL; 
                        }
                        ret_node = 
                                            ret_val4->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                                            delete ret_val4;
                                        
                                            delete input_val4;
                                        

                        return new OMElement(NULL,ret_node);
                    

                    /* since this has no output params it just returns NULL */                    
                    

                }
             

                if ( axutil_strcmp(op_name, "getJobTracker") == 0 )
                {

                    
                    input_val5 =
                        
                        new AviaryHadoop::GetJobTracker();
                        if( AXIS2_FAILURE ==  input_val5->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryHadoop::GetJobTracker_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryHadoopServiceSkeleton skel;
                        ret_val5 =  skel->getJobTracker(msgCtx ,input_val5);
                    
                        if ( NULL == ret_val5 )
                        {
                            
                                delete input_val5;
                            
                            return NULL; 
                        }
                        ret_node = 
                                            ret_val5->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                                            delete ret_val5;
                                        
                                            delete input_val5;
                                        

                        return new OMElement(NULL,ret_node);
                    

                    /* since this has no output params it just returns NULL */                    
                    

                }
             

                if ( axutil_strcmp(op_name, "stopTaskTracker") == 0 )
                {

                    
                    input_val6 =
                        
                        new AviaryHadoop::StopTaskTracker();
                        if( AXIS2_FAILURE ==  input_val6->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryHadoop::StopTaskTracker_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryHadoopServiceSkeleton skel;
                        ret_val6 =  skel->stopTaskTracker(msgCtx ,input_val6);
                    
                        if ( NULL == ret_val6 )
                        {
                            
                                delete input_val6;
                            
                            return NULL; 
                        }
                        ret_node = 
                                            ret_val6->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                                            delete ret_val6;
                                        
                                            delete input_val6;
                                        

                        return new OMElement(NULL,ret_node);
                    

                    /* since this has no output params it just returns NULL */                    
                    

                }
             

                if ( axutil_strcmp(op_name, "startNameNode") == 0 )
                {

                    
                    input_val7 =
                        
                        new AviaryHadoop::StartNameNode();
                        if( AXIS2_FAILURE ==  input_val7->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryHadoop::StartNameNode_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryHadoopServiceSkeleton skel;
                        ret_val7 =  skel->startNameNode(msgCtx ,input_val7);
                    
                        if ( NULL == ret_val7 )
                        {
                            
                                delete input_val7;
                            
                            return NULL; 
                        }
                        ret_node = 
                                            ret_val7->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                                            delete ret_val7;
                                        
                                            delete input_val7;
                                        

                        return new OMElement(NULL,ret_node);
                    

                    /* since this has no output params it just returns NULL */                    
                    

                }
             

                if ( axutil_strcmp(op_name, "getDataNode") == 0 )
                {

                    
                    input_val8 =
                        
                        new AviaryHadoop::GetDataNode();
                        if( AXIS2_FAILURE ==  input_val8->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryHadoop::GetDataNode_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryHadoopServiceSkeleton skel;
                        ret_val8 =  skel->getDataNode(msgCtx ,input_val8);
                    
                        if ( NULL == ret_val8 )
                        {
                            
                                delete input_val8;
                            
                            return NULL; 
                        }
                        ret_node = 
                                            ret_val8->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                                            delete ret_val8;
                                        
                                            delete input_val8;
                                        

                        return new OMElement(NULL,ret_node);
                    

                    /* since this has no output params it just returns NULL */                    
                    

                }
             

                if ( axutil_strcmp(op_name, "stopNameNode") == 0 )
                {

                    
                    input_val9 =
                        
                        new AviaryHadoop::StopNameNode();
                        if( AXIS2_FAILURE ==  input_val9->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryHadoop::StopNameNode_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryHadoopServiceSkeleton skel;
                        ret_val9 =  skel->stopNameNode(msgCtx ,input_val9);
                    
                        if ( NULL == ret_val9 )
                        {
                            
                                delete input_val9;
                            
                            return NULL; 
                        }
                        ret_node = 
                                            ret_val9->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                                            delete ret_val9;
                                        
                                            delete input_val9;
                                        

                        return new OMElement(NULL,ret_node);
                    

                    /* since this has no output params it just returns NULL */                    
                    

                }
             

                if ( axutil_strcmp(op_name, "getNameNode") == 0 )
                {

                    
                    input_val10 =
                        
                        new AviaryHadoop::GetNameNode();
                        if( AXIS2_FAILURE ==  input_val10->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryHadoop::GetNameNode_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryHadoopServiceSkeleton skel;
                        ret_val10 =  skel->getNameNode(msgCtx ,input_val10);
                    
                        if ( NULL == ret_val10 )
                        {
                            
                                delete input_val10;
                            
                            return NULL; 
                        }
                        ret_node = 
                                            ret_val10->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                                            delete ret_val10;
                                        
                                            delete input_val10;
                                        

                        return new OMElement(NULL,ret_node);
                    

                    /* since this has no output params it just returns NULL */                    
                    

                }
             

                if ( axutil_strcmp(op_name, "startJobTracker") == 0 )
                {

                    
                    input_val11 =
                        
                        new AviaryHadoop::StartJobTracker();
                        if( AXIS2_FAILURE ==  input_val11->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryHadoop::StartJobTracker_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryHadoopServiceSkeleton skel;
                        ret_val11 =  skel->startJobTracker(msgCtx ,input_val11);
                    
                        if ( NULL == ret_val11 )
                        {
                            
                                delete input_val11;
                            
                            return NULL; 
                        }
                        ret_node = 
                                            ret_val11->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                                            delete ret_val11;
                                        
                                            delete input_val11;
                                        

                        return new OMElement(NULL,ret_node);
                    

                    /* since this has no output params it just returns NULL */                    
                    

                }
             

                if ( axutil_strcmp(op_name, "stopDataNode") == 0 )
                {

                    
                    input_val12 =
                        
                        new AviaryHadoop::StopDataNode();
                        if( AXIS2_FAILURE ==  input_val12->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryHadoop::StopDataNode_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryHadoopServiceSkeleton skel;
                        ret_val12 =  skel->stopDataNode(msgCtx ,input_val12);
                    
                        if ( NULL == ret_val12 )
                        {
                            
                                delete input_val12;
                            
                            return NULL; 
                        }
                        ret_node = 
                                            ret_val12->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                                            delete ret_val12;
                                        
                                            delete input_val12;
                                        

                        return new OMElement(NULL,ret_node);
                    

                    /* since this has no output params it just returns NULL */                    
                    

                }
             
             }
            
          AXIS2_LOG_ERROR(Environment::getEnv()->log, AXIS2_LOG_SI, "AviaryHadoopService service ERROR: invalid OM parameters in request\n");
          return NULL;
    }

    OMElement* WSF_CALL
    AviaryHadoopService::onFault(OMElement* omEle)
	{
		axiom_node_t *error_node = NULL;
		axiom_element_t *error_ele = NULL;
        axutil_error_codes_t error_code;
        axiom_node_t *node = NULL;
        if (omEle) {
            node = omEle->getAxiomNode();
        }
        error_code = (axutil_error_codes_t)Environment::getEnv()->error->error_number;

        if(error_code <= AVIARYHADOOPSERVICESKELETON_ERROR_NONE ||
                error_code >= AVIARYHADOOPSERVICESKELETON_ERROR_LAST )
        {
            error_ele = axiom_element_create(Environment::getEnv(), node, "fault", NULL,
                            &error_node);
            axiom_element_set_text(error_ele, Environment::getEnv(), "AviaryHadoopService|http://grid.redhat.com/aviary-hadoop/ failed",
                            error_node);
        }
        

		return new OMElement(NULL,error_node);
	}

    

