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

        #include "AviaryQueryServiceSkeleton.h"
        #include "AviaryQueryService.h"  
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
        
        using namespace AviaryQuery;
        

        /** Load the service into axis2 engine */
        WSF_SERVICE_INIT(AviaryQueryService)

          
         /**
          * function to free any soap input headers
          */
         AviaryQueryService::AviaryQueryService()
	{
          skel = wsfGetAviaryQueryServiceSkeleton();
    }


	void WSF_CALL
	AviaryQueryService::init()
	{

      return;
	}


	AviaryQueryService::~AviaryQueryService()
	{
    }


     

     




	/*
	 * This method invokes the right service method
	 */
	OMElement* WSF_CALL
	AviaryQueryService::invoke(OMElement *omEle, MessageContext *msgCtx)
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

          
            AviaryQuery::GetJobDataResponse* ret_val1;
            AviaryQuery::GetJobData* input_val1;
            
            AviaryQuery::GetJobStatusResponse* ret_val2;
            AviaryQuery::GetJobStatus* input_val2;
            
            AviaryQuery::GetSubmissionSummaryResponse* ret_val3;
            AviaryQuery::GetSubmissionSummary* input_val3;
            
            AviaryQuery::GetJobDetailsResponse* ret_val4;
            AviaryQuery::GetJobDetails* input_val4;

            AviaryQuery::GetSubmissionIDResponse* ret_val5;
            AviaryQuery::GetSubmissionID* input_val5;
            
            AviaryQuery::GetJobSummaryResponse* ret_val6;
            AviaryQuery::GetJobSummary* input_val6;
            
       
          msg_ctx = msgCtx->getAxis2MessageContext();
          operation_ctx = axis2_msg_ctx_get_op_ctx(msg_ctx, Environment::getEnv());
          operation = axis2_op_ctx_get_op(operation_ctx, Environment::getEnv());
          op_qname = (axutil_qname_t *)axis2_op_get_qname(operation, Environment::getEnv());
          op_name = axutil_qname_get_localpart(op_qname, Environment::getEnv());

          if (op_name)
          {
               

                if ( axutil_strcmp(op_name, "getJobData") == 0 )
                {

                    
                    input_val1 =
                        
                        new AviaryQuery::GetJobData();
                        if( AXIS2_FAILURE ==  input_val1->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryQuery::GetJobData_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryQueryServiceSkeleton skel;
                        ret_val1 =  skel->getJobData(msgCtx ,input_val1);
                    
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
             

                if ( axutil_strcmp(op_name, "getJobStatus") == 0 )
                {

                    
                    input_val2 =
                        
                        new AviaryQuery::GetJobStatus();
                        if( AXIS2_FAILURE ==  input_val2->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryQuery::GetJobStatus_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryQueryServiceSkeleton skel;
                        ret_val2 =  skel->getJobStatus(msgCtx ,input_val2);
                    
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
             

                if ( axutil_strcmp(op_name, "getSubmissionSummary") == 0 )
                {

                    
                    input_val3 =
                        
                        new AviaryQuery::GetSubmissionSummary();
                        if( AXIS2_FAILURE ==  input_val3->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryQuery::GetSubmissionSummary_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryQueryServiceSkeleton skel;
                        ret_val3 =  skel->getSubmissionSummary(msgCtx ,input_val3);
                    
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
             

                if ( axutil_strcmp(op_name, "getJobDetails") == 0 )
                {

                    
                    input_val4 =
                        
                        new AviaryQuery::GetJobDetails();
                        if( AXIS2_FAILURE ==  input_val4->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryQuery::GetJobDetails_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryQueryServiceSkeleton skel;
                        ret_val4 =  skel->getJobDetails(msgCtx ,input_val4);
                    
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
             

                if ( axutil_strcmp(op_name, "getSubmissionID") == 0 )
                {

                    
                    input_val5 =
                        
                        new AviaryQuery::GetSubmissionID();
                        if( AXIS2_FAILURE ==  input_val5->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryQuery::GetSubmissionID_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryQueryServiceSkeleton skel;
                        ret_val5 =  skel->getSubmissionID(msgCtx ,input_val5);
                    
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
             

                if ( axutil_strcmp(op_name, "getJobSummary") == 0 )
                {

                    
                    input_val6 =
                        
                        new AviaryQuery::GetJobSummary();
                        if( AXIS2_FAILURE ==  input_val6->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryQuery::GetJobSummary_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryQueryServiceSkeleton skel;
                        ret_val6 =  skel->getJobSummary(msgCtx ,input_val6);
                    
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
             
             }
            
          AXIS2_LOG_ERROR(Environment::getEnv()->log, AXIS2_LOG_SI, "AviaryQueryService service ERROR: invalid OM parameters in request\n");
          return NULL;
    }

    OMElement* WSF_CALL
    AviaryQueryService::onFault(OMElement* omEle)
	{
		axiom_node_t *error_node = NULL;
		axiom_element_t *error_ele = NULL;
        axutil_error_codes_t error_code;
        axiom_node_t *node = NULL;
        if (omEle) {
            node = omEle->getAxiomNode();
        }
        error_code = (axutil_error_codes_t)Environment::getEnv()->error->error_number;

        if(error_code <= AVIARYQUERYSERVICESKELETON_ERROR_NONE ||
                error_code >= AVIARYQUERYSERVICESKELETON_ERROR_LAST )
        {
            error_ele = axiom_element_create(Environment::getEnv(), node, "fault", NULL,
                            &error_node);
            axiom_element_set_text(error_ele, Environment::getEnv(), "AviaryQueryService|http://grid.redhat.com/aviary-query/ failed",
                            error_node);
        }
        

		return new OMElement(NULL,error_node);
	}

    

