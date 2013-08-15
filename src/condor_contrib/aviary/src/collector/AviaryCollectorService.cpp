/*
 * Copyright 2009-2012 Red Hat, Inc.
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

        #include "AviaryCollectorServiceSkeleton.h"
        #include "AviaryCollectorService.h"  
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
        
        using namespace AviaryCollector;
        

        /** Load the service into axis2 engine */
        WSF_SERVICE_INIT(AviaryCollectorService)

          
         /**
          * function to free any soap input headers
          */
         AviaryCollectorService::AviaryCollectorService()
	{
          skel = wsfGetAviaryCollectorServiceSkeleton();
    }


	void WSF_CALL
	AviaryCollectorService::init()
	{

      return;
	}


	AviaryCollectorService::~AviaryCollectorService()
	{
    }


     

     




	/*
	 * This method invokes the right service method
	 */
	OMElement* WSF_CALL
	AviaryCollectorService::invoke(OMElement *omEle, MessageContext *msgCtx)
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

          
            AviaryCollector::GetSlotIDResponse* ret_val1;
            AviaryCollector::GetSlotID* input_val1;
            
            AviaryCollector::GetNegotiatorResponse* ret_val2;
            AviaryCollector::GetNegotiator* input_val2;
            
            AviaryCollector::GetSubmitterResponse* ret_val3;
            AviaryCollector::GetSubmitter* input_val3;
            
            AviaryCollector::GetSlotResponse* ret_val4;
            AviaryCollector::GetSlot* input_val4;
            
            AviaryCollector::GetAttributesResponse* ret_val5;
            AviaryCollector::GetAttributes* input_val5;
            
            AviaryCollector::GetSchedulerResponse* ret_val6;
            AviaryCollector::GetScheduler* input_val6;
            
            AviaryCollector::GetCollectorResponse* ret_val7;
            AviaryCollector::GetCollector* input_val7;
            
            AviaryCollector::GetMasterIDResponse* ret_val8;
            AviaryCollector::GetMasterID* input_val8;
            
            AviaryCollector::GetMasterResponse* ret_val9;
            AviaryCollector::GetMaster* input_val9;
            
       
          msg_ctx = msgCtx->getAxis2MessageContext();
          operation_ctx = axis2_msg_ctx_get_op_ctx(msg_ctx, Environment::getEnv());
          operation = axis2_op_ctx_get_op(operation_ctx, Environment::getEnv());
          op_qname = (axutil_qname_t *)axis2_op_get_qname(operation, Environment::getEnv());
          op_name = axutil_qname_get_localpart(op_qname, Environment::getEnv());

          if (op_name)
          {
               

                if ( axutil_strcmp(op_name, "getSlotID") == 0 )
                {

                    
                    input_val1 =
                        
                        new AviaryCollector::GetSlotID();
                        if( AXIS2_FAILURE ==  input_val1->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryCollector::GetSlotID_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryCollectorServiceSkeleton skel;
                        ret_val1 =  skel->getSlotID(msgCtx ,input_val1);
                    
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
             

                if ( axutil_strcmp(op_name, "getNegotiator") == 0 )
                {

                    
                    input_val2 =
                        
                        new AviaryCollector::GetNegotiator();
                        if( AXIS2_FAILURE ==  input_val2->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryCollector::GetNegotiator_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryCollectorServiceSkeleton skel;
                        ret_val2 =  skel->getNegotiator(msgCtx ,input_val2);
                    
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
             

                if ( axutil_strcmp(op_name, "getSubmitter") == 0 )
                {

                    
                    input_val3 =
                        
                        new AviaryCollector::GetSubmitter();
                        if( AXIS2_FAILURE ==  input_val3->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryCollector::GetSubmitter_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryCollectorServiceSkeleton skel;
                        ret_val3 =  skel->getSubmitter(msgCtx ,input_val3);
                    
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
             

                if ( axutil_strcmp(op_name, "getSlot") == 0 )
                {

                    
                    input_val4 =
                        
                        new AviaryCollector::GetSlot();
                        if( AXIS2_FAILURE ==  input_val4->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryCollector::GetSlot_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryCollectorServiceSkeleton skel;
                        ret_val4 =  skel->getSlot(msgCtx ,input_val4);
                    
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
             

                if ( axutil_strcmp(op_name, "getAttributes") == 0 )
                {

                    
                    input_val5 =
                        
                        new AviaryCollector::GetAttributes();
                        if( AXIS2_FAILURE ==  input_val5->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryCollector::GetAttributes_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryCollectorServiceSkeleton skel;
                        ret_val5 =  skel->getAttributes(msgCtx ,input_val5);
                    
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
             

                if ( axutil_strcmp(op_name, "getScheduler") == 0 )
                {

                    
                    input_val6 =
                        
                        new AviaryCollector::GetScheduler();
                        if( AXIS2_FAILURE ==  input_val6->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryCollector::GetScheduler_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryCollectorServiceSkeleton skel;
                        ret_val6 =  skel->getScheduler(msgCtx ,input_val6);
                    
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
             

                if ( axutil_strcmp(op_name, "getCollector") == 0 )
                {

                    
                    input_val7 =
                        
                        new AviaryCollector::GetCollector();
                        if( AXIS2_FAILURE ==  input_val7->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryCollector::GetCollector_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryCollectorServiceSkeleton skel;
                        ret_val7 =  skel->getCollector(msgCtx ,input_val7);
                    
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
             

                if ( axutil_strcmp(op_name, "getMasterID") == 0 )
                {

                    
                    input_val8 =
                        
                        new AviaryCollector::GetMasterID();
                        if( AXIS2_FAILURE ==  input_val8->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryCollector::GetMasterID_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryCollectorServiceSkeleton skel;
                        ret_val8 =  skel->getMasterID(msgCtx ,input_val8);
                    
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
             

                if ( axutil_strcmp(op_name, "getMaster") == 0 )
                {

                    
                    input_val9 =
                        
                        new AviaryCollector::GetMaster();
                        if( AXIS2_FAILURE ==  input_val9->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryCollector::GetMaster_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryCollectorServiceSkeleton skel;
                        ret_val9 =  skel->getMaster(msgCtx ,input_val9);
                    
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
             
             }
            
          AXIS2_LOG_ERROR(Environment::getEnv()->log, AXIS2_LOG_SI, "AviaryCollectorService service ERROR: invalid OM parameters in request\n");
          return NULL;
    }

    OMElement* WSF_CALL
    AviaryCollectorService::onFault(OMElement* omEle)
	{
		axiom_node_t *error_node = NULL;
		axiom_element_t *error_ele = NULL;
        axutil_error_codes_t error_code;
        axiom_node_t *node = NULL;
        if (omEle) {
            node = omEle->getAxiomNode();
        }
        error_code = (axutil_error_codes_t)Environment::getEnv()->error->error_number;

        if(error_code <= AVIARYCOLLECTORSERVICESKELETON_ERROR_NONE ||
                error_code >= AVIARYCOLLECTORSERVICESKELETON_ERROR_LAST )
        {
            error_ele = axiom_element_create(Environment::getEnv(), node, "fault", NULL,
                            &error_node);
            axiom_element_set_text(error_ele, Environment::getEnv(), "AviaryCollectorService|http://grid.redhat.com/aviary-collector/ failed",
                            error_node);
        }
        

		return new OMElement(NULL,error_node);
	}

    

