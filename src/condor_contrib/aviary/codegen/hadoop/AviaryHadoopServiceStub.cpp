
      /**
       * AviaryHadoopServiceStub.cpp
       *
       * This file was auto-generated from WSDL for "AviaryHadoopService|http://grid.redhat.com/aviary-hadoop/" service
       * by the Apache Axis2/Java version: 1.0  Built on : Jan 28, 2013 (02:29:53 CST)
       */

      #include "AviaryHadoopServiceStub.h"
      #include "IAviaryHadoopServiceCallback.h"
      #include <axis2_msg.h>
      #include <axis2_policy_include.h>
      #include <neethi_engine.h>
      #include <Stub.h>
      #include <Environment.h>
      #include <WSFError.h>
      
      using namespace std;
      using namespace wso2wsf;
        
      using namespace com_redhat_grid_aviary_hadoop;
        
      /**
       * AviaryHadoopServiceStub CPP implementation
       */
       AviaryHadoopServiceStub::AviaryHadoopServiceStub(std::string& clientHome)
        {
                if(clientHome.empty())
                {
                   cout<<"Please specify the client home";
                }
                std::string endpointUri= getEndpointUriOfAviaryHadoopService();

                init(clientHome,endpointUri);

                populateServicesForAviaryHadoopService();


        }


      AviaryHadoopServiceStub::AviaryHadoopServiceStub(std::string& clientHome,std::string& endpointURI)
      {
         std::string endpointUri;

         if(clientHome.empty())
         {
            cout<<"Please specify the client home";
         }
         endpointUri = endpointURI;

         if (endpointUri.empty())
         {
            endpointUri = getEndpointUriOfAviaryHadoopService();
         }


         init(clientHome,endpointUri);

         populateServicesForAviaryHadoopService();

      }
      
      AviaryHadoopServiceStub::~AviaryHadoopServiceStub()
      {
      }
	

      void WSF_CALL
      AviaryHadoopServiceStub::populateServicesForAviaryHadoopService()
      {
         axis2_svc_client_t *svc_client = NULL;
         axutil_qname_t *svc_qname =  NULL;
         axutil_qname_t *op_qname =  NULL;
         axis2_svc_t *svc = NULL;
         axis2_op_t *op = NULL;
         axis2_op_t *annon_op = NULL;
         axis2_msg_t *msg_out = NULL;
         axis2_msg_t *msg_in = NULL;
         axis2_msg_t *msg_out_fault = NULL;
         axis2_msg_t *msg_in_fault = NULL;
         axis2_policy_include_t *policy_include = NULL;

         axis2_desc_t *desc = NULL;
         axiom_node_t *policy_node = NULL;
         axiom_element_t *policy_root_ele = NULL;
         neethi_policy_t *neethi_policy = NULL;


         /* Modifying the Service */
	 svc_client = serviceClient->getAxis2SvcClient();
         svc = (axis2_svc_t*)axis2_svc_client_get_svc( svc_client, Environment::getEnv() );

         annon_op = axis2_svc_get_op_with_name(svc, Environment::getEnv(), AXIS2_ANON_OUT_IN_OP);
         msg_out = axis2_op_get_msg(annon_op, Environment::getEnv(), AXIS2_MSG_OUT);
         msg_in = axis2_op_get_msg(annon_op, Environment::getEnv(), AXIS2_MSG_IN);
         msg_out_fault = axis2_op_get_msg(annon_op, Environment::getEnv(), AXIS2_MSG_OUT_FAULT);
         msg_in_fault = axis2_op_get_msg(annon_op, Environment::getEnv(), AXIS2_MSG_IN_FAULT);

         svc_qname = axutil_qname_create(Environment::getEnv(),"AviaryHadoopService" ,NULL, NULL);
         axis2_svc_set_qname (svc, Environment::getEnv(), svc_qname);
		 axutil_qname_free(svc_qname,Environment::getEnv());

         /* creating the operations*/

         
           op_qname = axutil_qname_create(Environment::getEnv(),
                                         "startTaskTracker" ,
                                         "http://grid.redhat.com/aviary-hadoop/",
                                         NULL);
           op = axis2_op_create_with_qname(Environment::getEnv(), op_qname);
           axutil_qname_free(op_qname,Environment::getEnv());

           
           axis2_op_set_msg_exchange_pattern(op, Environment::getEnv(), AXIS2_MEP_URI_OUT_IN);
             
           axis2_msg_increment_ref(msg_out, Environment::getEnv());
           axis2_msg_increment_ref(msg_in, Environment::getEnv());
           axis2_msg_increment_ref(msg_out_fault, Environment::getEnv());
           axis2_msg_increment_ref(msg_in_fault, Environment::getEnv());
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT, msg_out);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN, msg_in);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT_FAULT, msg_out_fault);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN_FAULT, msg_in_fault);
       
           
           axis2_svc_add_op(svc, Environment::getEnv(), op);
         
           op_qname = axutil_qname_create(Environment::getEnv(),
                                         "startDataNode" ,
                                         "http://grid.redhat.com/aviary-hadoop/",
                                         NULL);
           op = axis2_op_create_with_qname(Environment::getEnv(), op_qname);
           axutil_qname_free(op_qname,Environment::getEnv());

           
           axis2_op_set_msg_exchange_pattern(op, Environment::getEnv(), AXIS2_MEP_URI_OUT_IN);
             
           axis2_msg_increment_ref(msg_out, Environment::getEnv());
           axis2_msg_increment_ref(msg_in, Environment::getEnv());
           axis2_msg_increment_ref(msg_out_fault, Environment::getEnv());
           axis2_msg_increment_ref(msg_in_fault, Environment::getEnv());
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT, msg_out);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN, msg_in);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT_FAULT, msg_out_fault);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN_FAULT, msg_in_fault);
       
           
           axis2_svc_add_op(svc, Environment::getEnv(), op);
         
           op_qname = axutil_qname_create(Environment::getEnv(),
                                         "getTaskTracker" ,
                                         "http://grid.redhat.com/aviary-hadoop/",
                                         NULL);
           op = axis2_op_create_with_qname(Environment::getEnv(), op_qname);
           axutil_qname_free(op_qname,Environment::getEnv());

           
           axis2_op_set_msg_exchange_pattern(op, Environment::getEnv(), AXIS2_MEP_URI_OUT_IN);
             
           axis2_msg_increment_ref(msg_out, Environment::getEnv());
           axis2_msg_increment_ref(msg_in, Environment::getEnv());
           axis2_msg_increment_ref(msg_out_fault, Environment::getEnv());
           axis2_msg_increment_ref(msg_in_fault, Environment::getEnv());
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT, msg_out);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN, msg_in);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT_FAULT, msg_out_fault);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN_FAULT, msg_in_fault);
       
           
           axis2_svc_add_op(svc, Environment::getEnv(), op);
         
           op_qname = axutil_qname_create(Environment::getEnv(),
                                         "stopJobTracker" ,
                                         "http://grid.redhat.com/aviary-hadoop/",
                                         NULL);
           op = axis2_op_create_with_qname(Environment::getEnv(), op_qname);
           axutil_qname_free(op_qname,Environment::getEnv());

           
           axis2_op_set_msg_exchange_pattern(op, Environment::getEnv(), AXIS2_MEP_URI_OUT_IN);
             
           axis2_msg_increment_ref(msg_out, Environment::getEnv());
           axis2_msg_increment_ref(msg_in, Environment::getEnv());
           axis2_msg_increment_ref(msg_out_fault, Environment::getEnv());
           axis2_msg_increment_ref(msg_in_fault, Environment::getEnv());
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT, msg_out);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN, msg_in);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT_FAULT, msg_out_fault);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN_FAULT, msg_in_fault);
       
           
           axis2_svc_add_op(svc, Environment::getEnv(), op);
         
           op_qname = axutil_qname_create(Environment::getEnv(),
                                         "getJobTracker" ,
                                         "http://grid.redhat.com/aviary-hadoop/",
                                         NULL);
           op = axis2_op_create_with_qname(Environment::getEnv(), op_qname);
           axutil_qname_free(op_qname,Environment::getEnv());

           
           axis2_op_set_msg_exchange_pattern(op, Environment::getEnv(), AXIS2_MEP_URI_OUT_IN);
             
           axis2_msg_increment_ref(msg_out, Environment::getEnv());
           axis2_msg_increment_ref(msg_in, Environment::getEnv());
           axis2_msg_increment_ref(msg_out_fault, Environment::getEnv());
           axis2_msg_increment_ref(msg_in_fault, Environment::getEnv());
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT, msg_out);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN, msg_in);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT_FAULT, msg_out_fault);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN_FAULT, msg_in_fault);
       
           
           axis2_svc_add_op(svc, Environment::getEnv(), op);
         
           op_qname = axutil_qname_create(Environment::getEnv(),
                                         "stopTaskTracker" ,
                                         "http://grid.redhat.com/aviary-hadoop/",
                                         NULL);
           op = axis2_op_create_with_qname(Environment::getEnv(), op_qname);
           axutil_qname_free(op_qname,Environment::getEnv());

           
           axis2_op_set_msg_exchange_pattern(op, Environment::getEnv(), AXIS2_MEP_URI_OUT_IN);
             
           axis2_msg_increment_ref(msg_out, Environment::getEnv());
           axis2_msg_increment_ref(msg_in, Environment::getEnv());
           axis2_msg_increment_ref(msg_out_fault, Environment::getEnv());
           axis2_msg_increment_ref(msg_in_fault, Environment::getEnv());
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT, msg_out);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN, msg_in);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT_FAULT, msg_out_fault);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN_FAULT, msg_in_fault);
       
           
           axis2_svc_add_op(svc, Environment::getEnv(), op);
         
           op_qname = axutil_qname_create(Environment::getEnv(),
                                         "startNameNode" ,
                                         "http://grid.redhat.com/aviary-hadoop/",
                                         NULL);
           op = axis2_op_create_with_qname(Environment::getEnv(), op_qname);
           axutil_qname_free(op_qname,Environment::getEnv());

           
           axis2_op_set_msg_exchange_pattern(op, Environment::getEnv(), AXIS2_MEP_URI_OUT_IN);
             
           axis2_msg_increment_ref(msg_out, Environment::getEnv());
           axis2_msg_increment_ref(msg_in, Environment::getEnv());
           axis2_msg_increment_ref(msg_out_fault, Environment::getEnv());
           axis2_msg_increment_ref(msg_in_fault, Environment::getEnv());
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT, msg_out);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN, msg_in);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT_FAULT, msg_out_fault);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN_FAULT, msg_in_fault);
       
           
           axis2_svc_add_op(svc, Environment::getEnv(), op);
         
           op_qname = axutil_qname_create(Environment::getEnv(),
                                         "getDataNode" ,
                                         "http://grid.redhat.com/aviary-hadoop/",
                                         NULL);
           op = axis2_op_create_with_qname(Environment::getEnv(), op_qname);
           axutil_qname_free(op_qname,Environment::getEnv());

           
           axis2_op_set_msg_exchange_pattern(op, Environment::getEnv(), AXIS2_MEP_URI_OUT_IN);
             
           axis2_msg_increment_ref(msg_out, Environment::getEnv());
           axis2_msg_increment_ref(msg_in, Environment::getEnv());
           axis2_msg_increment_ref(msg_out_fault, Environment::getEnv());
           axis2_msg_increment_ref(msg_in_fault, Environment::getEnv());
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT, msg_out);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN, msg_in);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT_FAULT, msg_out_fault);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN_FAULT, msg_in_fault);
       
           
           axis2_svc_add_op(svc, Environment::getEnv(), op);
         
           op_qname = axutil_qname_create(Environment::getEnv(),
                                         "stopNameNode" ,
                                         "http://grid.redhat.com/aviary-hadoop/",
                                         NULL);
           op = axis2_op_create_with_qname(Environment::getEnv(), op_qname);
           axutil_qname_free(op_qname,Environment::getEnv());

           
           axis2_op_set_msg_exchange_pattern(op, Environment::getEnv(), AXIS2_MEP_URI_OUT_IN);
             
           axis2_msg_increment_ref(msg_out, Environment::getEnv());
           axis2_msg_increment_ref(msg_in, Environment::getEnv());
           axis2_msg_increment_ref(msg_out_fault, Environment::getEnv());
           axis2_msg_increment_ref(msg_in_fault, Environment::getEnv());
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT, msg_out);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN, msg_in);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT_FAULT, msg_out_fault);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN_FAULT, msg_in_fault);
       
           
           axis2_svc_add_op(svc, Environment::getEnv(), op);
         
           op_qname = axutil_qname_create(Environment::getEnv(),
                                         "getNameNode" ,
                                         "http://grid.redhat.com/aviary-hadoop/",
                                         NULL);
           op = axis2_op_create_with_qname(Environment::getEnv(), op_qname);
           axutil_qname_free(op_qname,Environment::getEnv());

           
           axis2_op_set_msg_exchange_pattern(op, Environment::getEnv(), AXIS2_MEP_URI_OUT_IN);
             
           axis2_msg_increment_ref(msg_out, Environment::getEnv());
           axis2_msg_increment_ref(msg_in, Environment::getEnv());
           axis2_msg_increment_ref(msg_out_fault, Environment::getEnv());
           axis2_msg_increment_ref(msg_in_fault, Environment::getEnv());
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT, msg_out);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN, msg_in);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT_FAULT, msg_out_fault);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN_FAULT, msg_in_fault);
       
           
           axis2_svc_add_op(svc, Environment::getEnv(), op);
         
           op_qname = axutil_qname_create(Environment::getEnv(),
                                         "startJobTracker" ,
                                         "http://grid.redhat.com/aviary-hadoop/",
                                         NULL);
           op = axis2_op_create_with_qname(Environment::getEnv(), op_qname);
           axutil_qname_free(op_qname,Environment::getEnv());

           
           axis2_op_set_msg_exchange_pattern(op, Environment::getEnv(), AXIS2_MEP_URI_OUT_IN);
             
           axis2_msg_increment_ref(msg_out, Environment::getEnv());
           axis2_msg_increment_ref(msg_in, Environment::getEnv());
           axis2_msg_increment_ref(msg_out_fault, Environment::getEnv());
           axis2_msg_increment_ref(msg_in_fault, Environment::getEnv());
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT, msg_out);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN, msg_in);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT_FAULT, msg_out_fault);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN_FAULT, msg_in_fault);
       
           
           axis2_svc_add_op(svc, Environment::getEnv(), op);
         
           op_qname = axutil_qname_create(Environment::getEnv(),
                                         "stopDataNode" ,
                                         "http://grid.redhat.com/aviary-hadoop/",
                                         NULL);
           op = axis2_op_create_with_qname(Environment::getEnv(), op_qname);
           axutil_qname_free(op_qname,Environment::getEnv());

           
           axis2_op_set_msg_exchange_pattern(op, Environment::getEnv(), AXIS2_MEP_URI_OUT_IN);
             
           axis2_msg_increment_ref(msg_out, Environment::getEnv());
           axis2_msg_increment_ref(msg_in, Environment::getEnv());
           axis2_msg_increment_ref(msg_out_fault, Environment::getEnv());
           axis2_msg_increment_ref(msg_in_fault, Environment::getEnv());
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT, msg_out);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN, msg_in);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT_FAULT, msg_out_fault);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN_FAULT, msg_in_fault);
       
           
           axis2_svc_add_op(svc, Environment::getEnv(), op);
         
      }

      /**
       *return end point picked from wsdl
       */
      std::string WSF_CALL
      AviaryHadoopServiceStub::getEndpointUriOfAviaryHadoopService()
      {
        std::string endpoint_uri;
        /* set the address from here */
        
        endpoint_uri = string("http://localhost");
            
        return endpoint_uri;
      }


  
         /**
          * Auto generated method signature
          * For "startTaskTracker|http://grid.redhat.com/aviary-hadoop/" operation.
          *
          * @param _startTaskTracker72 of the AviaryHadoop::StartTaskTracker
          *
          * @return AviaryHadoop::StartTaskTrackerResponse*
          */

         AviaryHadoop::StartTaskTrackerResponse* WSF_CALL AviaryHadoopServiceStub::startTaskTracker(AviaryHadoop::StartTaskTracker*  _startTaskTracker72)
         {
            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;
            axiom_node_t *ret_node = NULL;

            const axis2_char_t *soap_action = NULL;
            axutil_qname_t *op_qname =  NULL;
            axiom_node_t *payload = NULL;
            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            AviaryHadoop::StartTaskTrackerResponse* ret_val;
            
                                payload = _startTaskTracker72->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           
	    svc_client = serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
                AXIS2_LOG_ERROR(Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
                return (AviaryHadoop::StartTaskTrackerResponse*)NULL;
            }
            soap_act = axis2_options_get_soap_action( options, Environment::getEnv() );
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-hadoop/tasktracker/start";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-hadoop/tasktracker/start");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);    
            }

            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             
            ret_node =  axis2_svc_client_send_receive_with_op_qname( svc_client, Environment::getEnv(), op_qname, payload);
 
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);    
              
              axis2_options_set_action( options, Environment::getEnv(), NULL);
            }
            if(soap_act)
            {
              axutil_string_free(soap_act, Environment::getEnv());
            }

            
                    if ( NULL == ret_node )
                    {
                        return (AviaryHadoop::StartTaskTrackerResponse*)NULL;
                    }
                    ret_val = new AviaryHadoop::StartTaskTrackerResponse();

                    if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                    {
                        if(ret_val != NULL)
                        {
                           delete ret_val;
                        }

                        AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the _deserialize: "
                                                                "This should be due to an invalid XML");
                        return (AviaryHadoop::StartTaskTrackerResponse*)NULL;
                    }

                   
                            return ret_val;
                       
        }
        
         /**
          * Auto generated method signature
          * For "startDataNode|http://grid.redhat.com/aviary-hadoop/" operation.
          *
          * @param _startDataNode74 of the AviaryHadoop::StartDataNode
          *
          * @return AviaryHadoop::StartDataNodeResponse*
          */

         AviaryHadoop::StartDataNodeResponse* WSF_CALL AviaryHadoopServiceStub::startDataNode(AviaryHadoop::StartDataNode*  _startDataNode74)
         {
            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;
            axiom_node_t *ret_node = NULL;

            const axis2_char_t *soap_action = NULL;
            axutil_qname_t *op_qname =  NULL;
            axiom_node_t *payload = NULL;
            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            AviaryHadoop::StartDataNodeResponse* ret_val;
            
                                payload = _startDataNode74->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           
	    svc_client = serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
                AXIS2_LOG_ERROR(Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
                return (AviaryHadoop::StartDataNodeResponse*)NULL;
            }
            soap_act = axis2_options_get_soap_action( options, Environment::getEnv() );
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-hadoop/datanode/start";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-hadoop/datanode/start");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);    
            }

            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             
            ret_node =  axis2_svc_client_send_receive_with_op_qname( svc_client, Environment::getEnv(), op_qname, payload);
 
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);    
              
              axis2_options_set_action( options, Environment::getEnv(), NULL);
            }
            if(soap_act)
            {
              axutil_string_free(soap_act, Environment::getEnv());
            }

            
                    if ( NULL == ret_node )
                    {
                        return (AviaryHadoop::StartDataNodeResponse*)NULL;
                    }
                    ret_val = new AviaryHadoop::StartDataNodeResponse();

                    if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                    {
                        if(ret_val != NULL)
                        {
                           delete ret_val;
                        }

                        AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the _deserialize: "
                                                                "This should be due to an invalid XML");
                        return (AviaryHadoop::StartDataNodeResponse*)NULL;
                    }

                   
                            return ret_val;
                       
        }
        
         /**
          * Auto generated method signature
          * For "getTaskTracker|http://grid.redhat.com/aviary-hadoop/" operation.
          *
          * @param _getTaskTracker76 of the AviaryHadoop::GetTaskTracker
          *
          * @return AviaryHadoop::GetTaskTrackerResponse*
          */

         AviaryHadoop::GetTaskTrackerResponse* WSF_CALL AviaryHadoopServiceStub::getTaskTracker(AviaryHadoop::GetTaskTracker*  _getTaskTracker76)
         {
            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;
            axiom_node_t *ret_node = NULL;

            const axis2_char_t *soap_action = NULL;
            axutil_qname_t *op_qname =  NULL;
            axiom_node_t *payload = NULL;
            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            AviaryHadoop::GetTaskTrackerResponse* ret_val;
            
                                payload = _getTaskTracker76->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           
	    svc_client = serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
                AXIS2_LOG_ERROR(Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
                return (AviaryHadoop::GetTaskTrackerResponse*)NULL;
            }
            soap_act = axis2_options_get_soap_action( options, Environment::getEnv() );
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-hadoop/tasktracker/get";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-hadoop/tasktracker/get");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);    
            }

            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             
            ret_node =  axis2_svc_client_send_receive_with_op_qname( svc_client, Environment::getEnv(), op_qname, payload);
 
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);    
              
              axis2_options_set_action( options, Environment::getEnv(), NULL);
            }
            if(soap_act)
            {
              axutil_string_free(soap_act, Environment::getEnv());
            }

            
                    if ( NULL == ret_node )
                    {
                        return (AviaryHadoop::GetTaskTrackerResponse*)NULL;
                    }
                    ret_val = new AviaryHadoop::GetTaskTrackerResponse();

                    if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                    {
                        if(ret_val != NULL)
                        {
                           delete ret_val;
                        }

                        AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the _deserialize: "
                                                                "This should be due to an invalid XML");
                        return (AviaryHadoop::GetTaskTrackerResponse*)NULL;
                    }

                   
                            return ret_val;
                       
        }
        
         /**
          * Auto generated method signature
          * For "stopJobTracker|http://grid.redhat.com/aviary-hadoop/" operation.
          *
          * @param _stopJobTracker78 of the AviaryHadoop::StopJobTracker
          *
          * @return AviaryHadoop::StopJobTrackerResponse*
          */

         AviaryHadoop::StopJobTrackerResponse* WSF_CALL AviaryHadoopServiceStub::stopJobTracker(AviaryHadoop::StopJobTracker*  _stopJobTracker78)
         {
            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;
            axiom_node_t *ret_node = NULL;

            const axis2_char_t *soap_action = NULL;
            axutil_qname_t *op_qname =  NULL;
            axiom_node_t *payload = NULL;
            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            AviaryHadoop::StopJobTrackerResponse* ret_val;
            
                                payload = _stopJobTracker78->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           
	    svc_client = serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
                AXIS2_LOG_ERROR(Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
                return (AviaryHadoop::StopJobTrackerResponse*)NULL;
            }
            soap_act = axis2_options_get_soap_action( options, Environment::getEnv() );
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-hadoop/jobtracker/stop";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-hadoop/jobtracker/stop");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);    
            }

            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             
            ret_node =  axis2_svc_client_send_receive_with_op_qname( svc_client, Environment::getEnv(), op_qname, payload);
 
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);    
              
              axis2_options_set_action( options, Environment::getEnv(), NULL);
            }
            if(soap_act)
            {
              axutil_string_free(soap_act, Environment::getEnv());
            }

            
                    if ( NULL == ret_node )
                    {
                        return (AviaryHadoop::StopJobTrackerResponse*)NULL;
                    }
                    ret_val = new AviaryHadoop::StopJobTrackerResponse();

                    if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                    {
                        if(ret_val != NULL)
                        {
                           delete ret_val;
                        }

                        AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the _deserialize: "
                                                                "This should be due to an invalid XML");
                        return (AviaryHadoop::StopJobTrackerResponse*)NULL;
                    }

                   
                            return ret_val;
                       
        }
        
         /**
          * Auto generated method signature
          * For "getJobTracker|http://grid.redhat.com/aviary-hadoop/" operation.
          *
          * @param _getJobTracker80 of the AviaryHadoop::GetJobTracker
          *
          * @return AviaryHadoop::GetJobTrackerResponse*
          */

         AviaryHadoop::GetJobTrackerResponse* WSF_CALL AviaryHadoopServiceStub::getJobTracker(AviaryHadoop::GetJobTracker*  _getJobTracker80)
         {
            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;
            axiom_node_t *ret_node = NULL;

            const axis2_char_t *soap_action = NULL;
            axutil_qname_t *op_qname =  NULL;
            axiom_node_t *payload = NULL;
            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            AviaryHadoop::GetJobTrackerResponse* ret_val;
            
                                payload = _getJobTracker80->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           
	    svc_client = serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
                AXIS2_LOG_ERROR(Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
                return (AviaryHadoop::GetJobTrackerResponse*)NULL;
            }
            soap_act = axis2_options_get_soap_action( options, Environment::getEnv() );
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-hadoop/jobtracker/get";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-hadoop/jobtracker/get");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);    
            }

            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             
            ret_node =  axis2_svc_client_send_receive_with_op_qname( svc_client, Environment::getEnv(), op_qname, payload);
 
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);    
              
              axis2_options_set_action( options, Environment::getEnv(), NULL);
            }
            if(soap_act)
            {
              axutil_string_free(soap_act, Environment::getEnv());
            }

            
                    if ( NULL == ret_node )
                    {
                        return (AviaryHadoop::GetJobTrackerResponse*)NULL;
                    }
                    ret_val = new AviaryHadoop::GetJobTrackerResponse();

                    if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                    {
                        if(ret_val != NULL)
                        {
                           delete ret_val;
                        }

                        AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the _deserialize: "
                                                                "This should be due to an invalid XML");
                        return (AviaryHadoop::GetJobTrackerResponse*)NULL;
                    }

                   
                            return ret_val;
                       
        }
        
         /**
          * Auto generated method signature
          * For "stopTaskTracker|http://grid.redhat.com/aviary-hadoop/" operation.
          *
          * @param _stopTaskTracker82 of the AviaryHadoop::StopTaskTracker
          *
          * @return AviaryHadoop::StopTaskTrackerResponse*
          */

         AviaryHadoop::StopTaskTrackerResponse* WSF_CALL AviaryHadoopServiceStub::stopTaskTracker(AviaryHadoop::StopTaskTracker*  _stopTaskTracker82)
         {
            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;
            axiom_node_t *ret_node = NULL;

            const axis2_char_t *soap_action = NULL;
            axutil_qname_t *op_qname =  NULL;
            axiom_node_t *payload = NULL;
            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            AviaryHadoop::StopTaskTrackerResponse* ret_val;
            
                                payload = _stopTaskTracker82->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           
	    svc_client = serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
                AXIS2_LOG_ERROR(Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
                return (AviaryHadoop::StopTaskTrackerResponse*)NULL;
            }
            soap_act = axis2_options_get_soap_action( options, Environment::getEnv() );
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-hadoop/tasktracker/stop";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-hadoop/tasktracker/stop");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);    
            }

            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             
            ret_node =  axis2_svc_client_send_receive_with_op_qname( svc_client, Environment::getEnv(), op_qname, payload);
 
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);    
              
              axis2_options_set_action( options, Environment::getEnv(), NULL);
            }
            if(soap_act)
            {
              axutil_string_free(soap_act, Environment::getEnv());
            }

            
                    if ( NULL == ret_node )
                    {
                        return (AviaryHadoop::StopTaskTrackerResponse*)NULL;
                    }
                    ret_val = new AviaryHadoop::StopTaskTrackerResponse();

                    if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                    {
                        if(ret_val != NULL)
                        {
                           delete ret_val;
                        }

                        AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the _deserialize: "
                                                                "This should be due to an invalid XML");
                        return (AviaryHadoop::StopTaskTrackerResponse*)NULL;
                    }

                   
                            return ret_val;
                       
        }
        
         /**
          * Auto generated method signature
          * For "startNameNode|http://grid.redhat.com/aviary-hadoop/" operation.
          *
          * @param _startNameNode84 of the AviaryHadoop::StartNameNode
          *
          * @return AviaryHadoop::StartNameNodeResponse*
          */

         AviaryHadoop::StartNameNodeResponse* WSF_CALL AviaryHadoopServiceStub::startNameNode(AviaryHadoop::StartNameNode*  _startNameNode84)
         {
            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;
            axiom_node_t *ret_node = NULL;

            const axis2_char_t *soap_action = NULL;
            axutil_qname_t *op_qname =  NULL;
            axiom_node_t *payload = NULL;
            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            AviaryHadoop::StartNameNodeResponse* ret_val;
            
                                payload = _startNameNode84->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           
	    svc_client = serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
                AXIS2_LOG_ERROR(Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
                return (AviaryHadoop::StartNameNodeResponse*)NULL;
            }
            soap_act = axis2_options_get_soap_action( options, Environment::getEnv() );
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-hadoop/namenode/start";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-hadoop/namenode/start");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);    
            }

            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             
            ret_node =  axis2_svc_client_send_receive_with_op_qname( svc_client, Environment::getEnv(), op_qname, payload);
 
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);    
              
              axis2_options_set_action( options, Environment::getEnv(), NULL);
            }
            if(soap_act)
            {
              axutil_string_free(soap_act, Environment::getEnv());
            }

            
                    if ( NULL == ret_node )
                    {
                        return (AviaryHadoop::StartNameNodeResponse*)NULL;
                    }
                    ret_val = new AviaryHadoop::StartNameNodeResponse();

                    if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                    {
                        if(ret_val != NULL)
                        {
                           delete ret_val;
                        }

                        AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the _deserialize: "
                                                                "This should be due to an invalid XML");
                        return (AviaryHadoop::StartNameNodeResponse*)NULL;
                    }

                   
                            return ret_val;
                       
        }
        
         /**
          * Auto generated method signature
          * For "getDataNode|http://grid.redhat.com/aviary-hadoop/" operation.
          *
          * @param _getDataNode86 of the AviaryHadoop::GetDataNode
          *
          * @return AviaryHadoop::GetDataNodeResponse*
          */

         AviaryHadoop::GetDataNodeResponse* WSF_CALL AviaryHadoopServiceStub::getDataNode(AviaryHadoop::GetDataNode*  _getDataNode86)
         {
            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;
            axiom_node_t *ret_node = NULL;

            const axis2_char_t *soap_action = NULL;
            axutil_qname_t *op_qname =  NULL;
            axiom_node_t *payload = NULL;
            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            AviaryHadoop::GetDataNodeResponse* ret_val;
            
                                payload = _getDataNode86->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           
	    svc_client = serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
                AXIS2_LOG_ERROR(Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
                return (AviaryHadoop::GetDataNodeResponse*)NULL;
            }
            soap_act = axis2_options_get_soap_action( options, Environment::getEnv() );
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-hadoop/datanode/get";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-hadoop/datanode/get");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);    
            }

            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             
            ret_node =  axis2_svc_client_send_receive_with_op_qname( svc_client, Environment::getEnv(), op_qname, payload);
 
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);    
              
              axis2_options_set_action( options, Environment::getEnv(), NULL);
            }
            if(soap_act)
            {
              axutil_string_free(soap_act, Environment::getEnv());
            }

            
                    if ( NULL == ret_node )
                    {
                        return (AviaryHadoop::GetDataNodeResponse*)NULL;
                    }
                    ret_val = new AviaryHadoop::GetDataNodeResponse();

                    if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                    {
                        if(ret_val != NULL)
                        {
                           delete ret_val;
                        }

                        AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the _deserialize: "
                                                                "This should be due to an invalid XML");
                        return (AviaryHadoop::GetDataNodeResponse*)NULL;
                    }

                   
                            return ret_val;
                       
        }
        
         /**
          * Auto generated method signature
          * For "stopNameNode|http://grid.redhat.com/aviary-hadoop/" operation.
          *
          * @param _stopNameNode88 of the AviaryHadoop::StopNameNode
          *
          * @return AviaryHadoop::StopNameNodeResponse*
          */

         AviaryHadoop::StopNameNodeResponse* WSF_CALL AviaryHadoopServiceStub::stopNameNode(AviaryHadoop::StopNameNode*  _stopNameNode88)
         {
            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;
            axiom_node_t *ret_node = NULL;

            const axis2_char_t *soap_action = NULL;
            axutil_qname_t *op_qname =  NULL;
            axiom_node_t *payload = NULL;
            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            AviaryHadoop::StopNameNodeResponse* ret_val;
            
                                payload = _stopNameNode88->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           
	    svc_client = serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
                AXIS2_LOG_ERROR(Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
                return (AviaryHadoop::StopNameNodeResponse*)NULL;
            }
            soap_act = axis2_options_get_soap_action( options, Environment::getEnv() );
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-hadoop/namenode/stop";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-hadoop/namenode/stop");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);    
            }

            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             
            ret_node =  axis2_svc_client_send_receive_with_op_qname( svc_client, Environment::getEnv(), op_qname, payload);
 
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);    
              
              axis2_options_set_action( options, Environment::getEnv(), NULL);
            }
            if(soap_act)
            {
              axutil_string_free(soap_act, Environment::getEnv());
            }

            
                    if ( NULL == ret_node )
                    {
                        return (AviaryHadoop::StopNameNodeResponse*)NULL;
                    }
                    ret_val = new AviaryHadoop::StopNameNodeResponse();

                    if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                    {
                        if(ret_val != NULL)
                        {
                           delete ret_val;
                        }

                        AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the _deserialize: "
                                                                "This should be due to an invalid XML");
                        return (AviaryHadoop::StopNameNodeResponse*)NULL;
                    }

                   
                            return ret_val;
                       
        }
        
         /**
          * Auto generated method signature
          * For "getNameNode|http://grid.redhat.com/aviary-hadoop/" operation.
          *
          * @param _getNameNode90 of the AviaryHadoop::GetNameNode
          *
          * @return AviaryHadoop::GetNameNodeResponse*
          */

         AviaryHadoop::GetNameNodeResponse* WSF_CALL AviaryHadoopServiceStub::getNameNode(AviaryHadoop::GetNameNode*  _getNameNode90)
         {
            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;
            axiom_node_t *ret_node = NULL;

            const axis2_char_t *soap_action = NULL;
            axutil_qname_t *op_qname =  NULL;
            axiom_node_t *payload = NULL;
            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            AviaryHadoop::GetNameNodeResponse* ret_val;
            
                                payload = _getNameNode90->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           
	    svc_client = serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
                AXIS2_LOG_ERROR(Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
                return (AviaryHadoop::GetNameNodeResponse*)NULL;
            }
            soap_act = axis2_options_get_soap_action( options, Environment::getEnv() );
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-hadoop/namenode/get";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-hadoop/namenode/get");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);    
            }

            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             
            ret_node =  axis2_svc_client_send_receive_with_op_qname( svc_client, Environment::getEnv(), op_qname, payload);
 
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);    
              
              axis2_options_set_action( options, Environment::getEnv(), NULL);
            }
            if(soap_act)
            {
              axutil_string_free(soap_act, Environment::getEnv());
            }

            
                    if ( NULL == ret_node )
                    {
                        return (AviaryHadoop::GetNameNodeResponse*)NULL;
                    }
                    ret_val = new AviaryHadoop::GetNameNodeResponse();

                    if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                    {
                        if(ret_val != NULL)
                        {
                           delete ret_val;
                        }

                        AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the _deserialize: "
                                                                "This should be due to an invalid XML");
                        return (AviaryHadoop::GetNameNodeResponse*)NULL;
                    }

                   
                            return ret_val;
                       
        }
        
         /**
          * Auto generated method signature
          * For "startJobTracker|http://grid.redhat.com/aviary-hadoop/" operation.
          *
          * @param _startJobTracker92 of the AviaryHadoop::StartJobTracker
          *
          * @return AviaryHadoop::StartJobTrackerResponse*
          */

         AviaryHadoop::StartJobTrackerResponse* WSF_CALL AviaryHadoopServiceStub::startJobTracker(AviaryHadoop::StartJobTracker*  _startJobTracker92)
         {
            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;
            axiom_node_t *ret_node = NULL;

            const axis2_char_t *soap_action = NULL;
            axutil_qname_t *op_qname =  NULL;
            axiom_node_t *payload = NULL;
            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            AviaryHadoop::StartJobTrackerResponse* ret_val;
            
                                payload = _startJobTracker92->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           
	    svc_client = serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
                AXIS2_LOG_ERROR(Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
                return (AviaryHadoop::StartJobTrackerResponse*)NULL;
            }
            soap_act = axis2_options_get_soap_action( options, Environment::getEnv() );
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-hadoop/jobtracker/start";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-hadoop/jobtracker/start");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);    
            }

            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             
            ret_node =  axis2_svc_client_send_receive_with_op_qname( svc_client, Environment::getEnv(), op_qname, payload);
 
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);    
              
              axis2_options_set_action( options, Environment::getEnv(), NULL);
            }
            if(soap_act)
            {
              axutil_string_free(soap_act, Environment::getEnv());
            }

            
                    if ( NULL == ret_node )
                    {
                        return (AviaryHadoop::StartJobTrackerResponse*)NULL;
                    }
                    ret_val = new AviaryHadoop::StartJobTrackerResponse();

                    if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                    {
                        if(ret_val != NULL)
                        {
                           delete ret_val;
                        }

                        AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the _deserialize: "
                                                                "This should be due to an invalid XML");
                        return (AviaryHadoop::StartJobTrackerResponse*)NULL;
                    }

                   
                            return ret_val;
                       
        }
        
         /**
          * Auto generated method signature
          * For "stopDataNode|http://grid.redhat.com/aviary-hadoop/" operation.
          *
          * @param _stopDataNode94 of the AviaryHadoop::StopDataNode
          *
          * @return AviaryHadoop::StopDataNodeResponse*
          */

         AviaryHadoop::StopDataNodeResponse* WSF_CALL AviaryHadoopServiceStub::stopDataNode(AviaryHadoop::StopDataNode*  _stopDataNode94)
         {
            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;
            axiom_node_t *ret_node = NULL;

            const axis2_char_t *soap_action = NULL;
            axutil_qname_t *op_qname =  NULL;
            axiom_node_t *payload = NULL;
            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            AviaryHadoop::StopDataNodeResponse* ret_val;
            
                                payload = _stopDataNode94->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           
	    svc_client = serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
                AXIS2_LOG_ERROR(Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
                return (AviaryHadoop::StopDataNodeResponse*)NULL;
            }
            soap_act = axis2_options_get_soap_action( options, Environment::getEnv() );
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-hadoop/datanode/stop";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-hadoop/datanode/stop");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);    
            }

            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             
            ret_node =  axis2_svc_client_send_receive_with_op_qname( svc_client, Environment::getEnv(), op_qname, payload);
 
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);    
              
              axis2_options_set_action( options, Environment::getEnv(), NULL);
            }
            if(soap_act)
            {
              axutil_string_free(soap_act, Environment::getEnv());
            }

            
                    if ( NULL == ret_node )
                    {
                        return (AviaryHadoop::StopDataNodeResponse*)NULL;
                    }
                    ret_val = new AviaryHadoop::StopDataNodeResponse();

                    if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                    {
                        if(ret_val != NULL)
                        {
                           delete ret_val;
                        }

                        AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the _deserialize: "
                                                                "This should be due to an invalid XML");
                        return (AviaryHadoop::StopDataNodeResponse*)NULL;
                    }

                   
                            return ret_val;
                       
        }
        

        struct axis2_stub_AviaryHadoopService_startTaskTracker_callback_data
        {   
            IAviaryHadoopServiceCallback *callback;
          
        };

        static axis2_status_t WSF_CALL axis2_stub_on_error_AviaryHadoopService_startTaskTracker(axis2_callback_t *axis_callback, const axutil_env_t *env, int exception)
        {
            struct axis2_stub_AviaryHadoopService_startTaskTracker_callback_data* callback_data = NULL;
            callback_data = (struct axis2_stub_AviaryHadoopService_startTaskTracker_callback_data*)axis2_callback_get_data(axis_callback);
        
            IAviaryHadoopServiceCallback* callback = NULL;
            callback = callback_data->callback;
            callback->receiveError_startTaskTracker(exception);
            return AXIS2_SUCCESS;
        } 

        axis2_status_t  AXIS2_CALL axis2_stub_on_complete_AviaryHadoopService_startTaskTracker(axis2_callback_t *axis_callback, const axutil_env_t *env)
        {
            struct axis2_stub_AviaryHadoopService_startTaskTracker_callback_data* callback_data = NULL;
            axis2_status_t status = AXIS2_SUCCESS;
            AviaryHadoop::StartTaskTrackerResponse* ret_val;
            

            axiom_node_t *ret_node = NULL;
            axiom_soap_envelope_t *soap_envelope = NULL;

            

            IAviaryHadoopServiceCallback *callback = NULL;

            callback_data = (struct axis2_stub_AviaryHadoopService_startTaskTracker_callback_data*)axis2_callback_get_data(axis_callback);

            callback = callback_data->callback;

            soap_envelope = axis2_callback_get_envelope(axis_callback, Environment::getEnv());
            if(soap_envelope)
            {
                axiom_soap_body_t *soap_body;
                soap_body = axiom_soap_envelope_get_body(soap_envelope, Environment::getEnv());
                if(soap_body)
                {
                    axiom_soap_fault_t *soap_fault = NULL;
                    axiom_node_t *body_node = axiom_soap_body_get_base_node(soap_body, Environment::getEnv());

                      if(body_node)
                    {
                        ret_node = axiom_node_get_first_child(body_node, Environment::getEnv());
                    }
                }
                
                
            }


            
                    if(ret_node != NULL)
                    {
                        ret_val = new AviaryHadoop::StartTaskTrackerResponse();
     
                        if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                        {
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the LendResponse_deserialize: "
                                                                    "This should be due to an invalid XML");
                            delete ret_val;
                            ret_val = NULL;
                        }
                     }
                     else
                     {
                         ret_val = NULL; 
                     }

                     
                     callback->receiveResult_startTaskTracker(ret_val);
                         
 
            if(callback_data)
            {
                AXIS2_FREE(Environment::getEnv()->allocator, callback_data);
            }
            return AXIS2_SUCCESS;
        }

        /**
          * auto generated method signature for asynchronous invocations
          * for "startTaskTracker|http://grid.redhat.com/aviary-hadoop/" operation.
          * @param stub The stub
          * @param env environment ( mandatory)
          * @param _startTaskTracker72 of the AviaryHadoop::StartTaskTracker
          * @param user_data user data to be accessed by the callbacks
          * @param on_complete callback to handle on complete
          * @param on_error callback to handle on error
          */

         void WSF_CALL
        AviaryHadoopServiceStub::start_startTaskTracker(AviaryHadoop::StartTaskTracker*  _startTaskTracker72,
                                IAviaryHadoopServiceCallback* cb)
         {

            axis2_callback_t *callback = NULL;

            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;

            const axis2_char_t *soap_action = NULL;
            axiom_node_t *payload = NULL;

            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            
            
            struct axis2_stub_AviaryHadoopService_startTaskTracker_callback_data *callback_data;

            callback_data = (struct axis2_stub_AviaryHadoopService_startTaskTracker_callback_data*) AXIS2_MALLOC(Environment::getEnv()->allocator, 
                                    sizeof(struct axis2_stub_AviaryHadoopService_startTaskTracker_callback_data));
            if(NULL == callback_data)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_NO_MEMORY, AXIS2_FAILURE);
                AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "Can not allocate memory for the callback data structures");
                return;
            }
            

            
                                payload = _startTaskTracker72->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           

	    svc_client =   serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
              AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
              AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
              return;
            }

            soap_act =axis2_options_get_soap_action (options, Environment::getEnv());
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-hadoop/tasktracker/start";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-hadoop/tasktracker/start");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);
            }
            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             

            callback = axis2_callback_create(Environment::getEnv());
            /* Set our on_complete function pointer to the callback object */
            axis2_callback_set_on_complete(callback, axis2_stub_on_complete_AviaryHadoopService_startTaskTracker);
            /* Set our on_error function pointer to the callback object */
            axis2_callback_set_on_error(callback, axis2_stub_on_error_AviaryHadoopService_startTaskTracker);

            callback_data->callback = cb;
            axis2_callback_set_data(callback, (void*)callback_data);

            /* Send request */
            axis2_svc_client_send_receive_non_blocking(svc_client, Environment::getEnv(), payload, callback);
            
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);
              
              axis2_options_set_action(options, Environment::getEnv(), NULL);
            }
         }

         

        struct axis2_stub_AviaryHadoopService_startDataNode_callback_data
        {   
            IAviaryHadoopServiceCallback *callback;
          
        };

        static axis2_status_t WSF_CALL axis2_stub_on_error_AviaryHadoopService_startDataNode(axis2_callback_t *axis_callback, const axutil_env_t *env, int exception)
        {
            struct axis2_stub_AviaryHadoopService_startDataNode_callback_data* callback_data = NULL;
            callback_data = (struct axis2_stub_AviaryHadoopService_startDataNode_callback_data*)axis2_callback_get_data(axis_callback);
        
            IAviaryHadoopServiceCallback* callback = NULL;
            callback = callback_data->callback;
            callback->receiveError_startDataNode(exception);
            return AXIS2_SUCCESS;
        } 

        axis2_status_t  AXIS2_CALL axis2_stub_on_complete_AviaryHadoopService_startDataNode(axis2_callback_t *axis_callback, const axutil_env_t *env)
        {
            struct axis2_stub_AviaryHadoopService_startDataNode_callback_data* callback_data = NULL;
            axis2_status_t status = AXIS2_SUCCESS;
            AviaryHadoop::StartDataNodeResponse* ret_val;
            

            axiom_node_t *ret_node = NULL;
            axiom_soap_envelope_t *soap_envelope = NULL;

            

            IAviaryHadoopServiceCallback *callback = NULL;

            callback_data = (struct axis2_stub_AviaryHadoopService_startDataNode_callback_data*)axis2_callback_get_data(axis_callback);

            callback = callback_data->callback;

            soap_envelope = axis2_callback_get_envelope(axis_callback, Environment::getEnv());
            if(soap_envelope)
            {
                axiom_soap_body_t *soap_body;
                soap_body = axiom_soap_envelope_get_body(soap_envelope, Environment::getEnv());
                if(soap_body)
                {
                    axiom_soap_fault_t *soap_fault = NULL;
                    axiom_node_t *body_node = axiom_soap_body_get_base_node(soap_body, Environment::getEnv());

                      if(body_node)
                    {
                        ret_node = axiom_node_get_first_child(body_node, Environment::getEnv());
                    }
                }
                
                
            }


            
                    if(ret_node != NULL)
                    {
                        ret_val = new AviaryHadoop::StartDataNodeResponse();
     
                        if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                        {
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the LendResponse_deserialize: "
                                                                    "This should be due to an invalid XML");
                            delete ret_val;
                            ret_val = NULL;
                        }
                     }
                     else
                     {
                         ret_val = NULL; 
                     }

                     
                     callback->receiveResult_startDataNode(ret_val);
                         
 
            if(callback_data)
            {
                AXIS2_FREE(Environment::getEnv()->allocator, callback_data);
            }
            return AXIS2_SUCCESS;
        }

        /**
          * auto generated method signature for asynchronous invocations
          * for "startDataNode|http://grid.redhat.com/aviary-hadoop/" operation.
          * @param stub The stub
          * @param env environment ( mandatory)
          * @param _startDataNode74 of the AviaryHadoop::StartDataNode
          * @param user_data user data to be accessed by the callbacks
          * @param on_complete callback to handle on complete
          * @param on_error callback to handle on error
          */

         void WSF_CALL
        AviaryHadoopServiceStub::start_startDataNode(AviaryHadoop::StartDataNode*  _startDataNode74,
                                IAviaryHadoopServiceCallback* cb)
         {

            axis2_callback_t *callback = NULL;

            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;

            const axis2_char_t *soap_action = NULL;
            axiom_node_t *payload = NULL;

            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            
            
            struct axis2_stub_AviaryHadoopService_startDataNode_callback_data *callback_data;

            callback_data = (struct axis2_stub_AviaryHadoopService_startDataNode_callback_data*) AXIS2_MALLOC(Environment::getEnv()->allocator, 
                                    sizeof(struct axis2_stub_AviaryHadoopService_startDataNode_callback_data));
            if(NULL == callback_data)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_NO_MEMORY, AXIS2_FAILURE);
                AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "Can not allocate memory for the callback data structures");
                return;
            }
            

            
                                payload = _startDataNode74->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           

	    svc_client =   serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
              AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
              AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
              return;
            }

            soap_act =axis2_options_get_soap_action (options, Environment::getEnv());
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-hadoop/datanode/start";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-hadoop/datanode/start");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);
            }
            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             

            callback = axis2_callback_create(Environment::getEnv());
            /* Set our on_complete function pointer to the callback object */
            axis2_callback_set_on_complete(callback, axis2_stub_on_complete_AviaryHadoopService_startDataNode);
            /* Set our on_error function pointer to the callback object */
            axis2_callback_set_on_error(callback, axis2_stub_on_error_AviaryHadoopService_startDataNode);

            callback_data->callback = cb;
            axis2_callback_set_data(callback, (void*)callback_data);

            /* Send request */
            axis2_svc_client_send_receive_non_blocking(svc_client, Environment::getEnv(), payload, callback);
            
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);
              
              axis2_options_set_action(options, Environment::getEnv(), NULL);
            }
         }

         

        struct axis2_stub_AviaryHadoopService_getTaskTracker_callback_data
        {   
            IAviaryHadoopServiceCallback *callback;
          
        };

        static axis2_status_t WSF_CALL axis2_stub_on_error_AviaryHadoopService_getTaskTracker(axis2_callback_t *axis_callback, const axutil_env_t *env, int exception)
        {
            struct axis2_stub_AviaryHadoopService_getTaskTracker_callback_data* callback_data = NULL;
            callback_data = (struct axis2_stub_AviaryHadoopService_getTaskTracker_callback_data*)axis2_callback_get_data(axis_callback);
        
            IAviaryHadoopServiceCallback* callback = NULL;
            callback = callback_data->callback;
            callback->receiveError_getTaskTracker(exception);
            return AXIS2_SUCCESS;
        } 

        axis2_status_t  AXIS2_CALL axis2_stub_on_complete_AviaryHadoopService_getTaskTracker(axis2_callback_t *axis_callback, const axutil_env_t *env)
        {
            struct axis2_stub_AviaryHadoopService_getTaskTracker_callback_data* callback_data = NULL;
            axis2_status_t status = AXIS2_SUCCESS;
            AviaryHadoop::GetTaskTrackerResponse* ret_val;
            

            axiom_node_t *ret_node = NULL;
            axiom_soap_envelope_t *soap_envelope = NULL;

            

            IAviaryHadoopServiceCallback *callback = NULL;

            callback_data = (struct axis2_stub_AviaryHadoopService_getTaskTracker_callback_data*)axis2_callback_get_data(axis_callback);

            callback = callback_data->callback;

            soap_envelope = axis2_callback_get_envelope(axis_callback, Environment::getEnv());
            if(soap_envelope)
            {
                axiom_soap_body_t *soap_body;
                soap_body = axiom_soap_envelope_get_body(soap_envelope, Environment::getEnv());
                if(soap_body)
                {
                    axiom_soap_fault_t *soap_fault = NULL;
                    axiom_node_t *body_node = axiom_soap_body_get_base_node(soap_body, Environment::getEnv());

                      if(body_node)
                    {
                        ret_node = axiom_node_get_first_child(body_node, Environment::getEnv());
                    }
                }
                
                
            }


            
                    if(ret_node != NULL)
                    {
                        ret_val = new AviaryHadoop::GetTaskTrackerResponse();
     
                        if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                        {
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the LendResponse_deserialize: "
                                                                    "This should be due to an invalid XML");
                            delete ret_val;
                            ret_val = NULL;
                        }
                     }
                     else
                     {
                         ret_val = NULL; 
                     }

                     
                     callback->receiveResult_getTaskTracker(ret_val);
                         
 
            if(callback_data)
            {
                AXIS2_FREE(Environment::getEnv()->allocator, callback_data);
            }
            return AXIS2_SUCCESS;
        }

        /**
          * auto generated method signature for asynchronous invocations
          * for "getTaskTracker|http://grid.redhat.com/aviary-hadoop/" operation.
          * @param stub The stub
          * @param env environment ( mandatory)
          * @param _getTaskTracker76 of the AviaryHadoop::GetTaskTracker
          * @param user_data user data to be accessed by the callbacks
          * @param on_complete callback to handle on complete
          * @param on_error callback to handle on error
          */

         void WSF_CALL
        AviaryHadoopServiceStub::start_getTaskTracker(AviaryHadoop::GetTaskTracker*  _getTaskTracker76,
                                IAviaryHadoopServiceCallback* cb)
         {

            axis2_callback_t *callback = NULL;

            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;

            const axis2_char_t *soap_action = NULL;
            axiom_node_t *payload = NULL;

            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            
            
            struct axis2_stub_AviaryHadoopService_getTaskTracker_callback_data *callback_data;

            callback_data = (struct axis2_stub_AviaryHadoopService_getTaskTracker_callback_data*) AXIS2_MALLOC(Environment::getEnv()->allocator, 
                                    sizeof(struct axis2_stub_AviaryHadoopService_getTaskTracker_callback_data));
            if(NULL == callback_data)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_NO_MEMORY, AXIS2_FAILURE);
                AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "Can not allocate memory for the callback data structures");
                return;
            }
            

            
                                payload = _getTaskTracker76->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           

	    svc_client =   serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
              AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
              AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
              return;
            }

            soap_act =axis2_options_get_soap_action (options, Environment::getEnv());
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-hadoop/tasktracker/get";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-hadoop/tasktracker/get");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);
            }
            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             

            callback = axis2_callback_create(Environment::getEnv());
            /* Set our on_complete function pointer to the callback object */
            axis2_callback_set_on_complete(callback, axis2_stub_on_complete_AviaryHadoopService_getTaskTracker);
            /* Set our on_error function pointer to the callback object */
            axis2_callback_set_on_error(callback, axis2_stub_on_error_AviaryHadoopService_getTaskTracker);

            callback_data->callback = cb;
            axis2_callback_set_data(callback, (void*)callback_data);

            /* Send request */
            axis2_svc_client_send_receive_non_blocking(svc_client, Environment::getEnv(), payload, callback);
            
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);
              
              axis2_options_set_action(options, Environment::getEnv(), NULL);
            }
         }

         

        struct axis2_stub_AviaryHadoopService_stopJobTracker_callback_data
        {   
            IAviaryHadoopServiceCallback *callback;
          
        };

        static axis2_status_t WSF_CALL axis2_stub_on_error_AviaryHadoopService_stopJobTracker(axis2_callback_t *axis_callback, const axutil_env_t *env, int exception)
        {
            struct axis2_stub_AviaryHadoopService_stopJobTracker_callback_data* callback_data = NULL;
            callback_data = (struct axis2_stub_AviaryHadoopService_stopJobTracker_callback_data*)axis2_callback_get_data(axis_callback);
        
            IAviaryHadoopServiceCallback* callback = NULL;
            callback = callback_data->callback;
            callback->receiveError_stopJobTracker(exception);
            return AXIS2_SUCCESS;
        } 

        axis2_status_t  AXIS2_CALL axis2_stub_on_complete_AviaryHadoopService_stopJobTracker(axis2_callback_t *axis_callback, const axutil_env_t *env)
        {
            struct axis2_stub_AviaryHadoopService_stopJobTracker_callback_data* callback_data = NULL;
            axis2_status_t status = AXIS2_SUCCESS;
            AviaryHadoop::StopJobTrackerResponse* ret_val;
            

            axiom_node_t *ret_node = NULL;
            axiom_soap_envelope_t *soap_envelope = NULL;

            

            IAviaryHadoopServiceCallback *callback = NULL;

            callback_data = (struct axis2_stub_AviaryHadoopService_stopJobTracker_callback_data*)axis2_callback_get_data(axis_callback);

            callback = callback_data->callback;

            soap_envelope = axis2_callback_get_envelope(axis_callback, Environment::getEnv());
            if(soap_envelope)
            {
                axiom_soap_body_t *soap_body;
                soap_body = axiom_soap_envelope_get_body(soap_envelope, Environment::getEnv());
                if(soap_body)
                {
                    axiom_soap_fault_t *soap_fault = NULL;
                    axiom_node_t *body_node = axiom_soap_body_get_base_node(soap_body, Environment::getEnv());

                      if(body_node)
                    {
                        ret_node = axiom_node_get_first_child(body_node, Environment::getEnv());
                    }
                }
                
                
            }


            
                    if(ret_node != NULL)
                    {
                        ret_val = new AviaryHadoop::StopJobTrackerResponse();
     
                        if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                        {
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the LendResponse_deserialize: "
                                                                    "This should be due to an invalid XML");
                            delete ret_val;
                            ret_val = NULL;
                        }
                     }
                     else
                     {
                         ret_val = NULL; 
                     }

                     
                     callback->receiveResult_stopJobTracker(ret_val);
                         
 
            if(callback_data)
            {
                AXIS2_FREE(Environment::getEnv()->allocator, callback_data);
            }
            return AXIS2_SUCCESS;
        }

        /**
          * auto generated method signature for asynchronous invocations
          * for "stopJobTracker|http://grid.redhat.com/aviary-hadoop/" operation.
          * @param stub The stub
          * @param env environment ( mandatory)
          * @param _stopJobTracker78 of the AviaryHadoop::StopJobTracker
          * @param user_data user data to be accessed by the callbacks
          * @param on_complete callback to handle on complete
          * @param on_error callback to handle on error
          */

         void WSF_CALL
        AviaryHadoopServiceStub::start_stopJobTracker(AviaryHadoop::StopJobTracker*  _stopJobTracker78,
                                IAviaryHadoopServiceCallback* cb)
         {

            axis2_callback_t *callback = NULL;

            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;

            const axis2_char_t *soap_action = NULL;
            axiom_node_t *payload = NULL;

            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            
            
            struct axis2_stub_AviaryHadoopService_stopJobTracker_callback_data *callback_data;

            callback_data = (struct axis2_stub_AviaryHadoopService_stopJobTracker_callback_data*) AXIS2_MALLOC(Environment::getEnv()->allocator, 
                                    sizeof(struct axis2_stub_AviaryHadoopService_stopJobTracker_callback_data));
            if(NULL == callback_data)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_NO_MEMORY, AXIS2_FAILURE);
                AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "Can not allocate memory for the callback data structures");
                return;
            }
            

            
                                payload = _stopJobTracker78->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           

	    svc_client =   serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
              AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
              AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
              return;
            }

            soap_act =axis2_options_get_soap_action (options, Environment::getEnv());
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-hadoop/jobtracker/stop";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-hadoop/jobtracker/stop");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);
            }
            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             

            callback = axis2_callback_create(Environment::getEnv());
            /* Set our on_complete function pointer to the callback object */
            axis2_callback_set_on_complete(callback, axis2_stub_on_complete_AviaryHadoopService_stopJobTracker);
            /* Set our on_error function pointer to the callback object */
            axis2_callback_set_on_error(callback, axis2_stub_on_error_AviaryHadoopService_stopJobTracker);

            callback_data->callback = cb;
            axis2_callback_set_data(callback, (void*)callback_data);

            /* Send request */
            axis2_svc_client_send_receive_non_blocking(svc_client, Environment::getEnv(), payload, callback);
            
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);
              
              axis2_options_set_action(options, Environment::getEnv(), NULL);
            }
         }

         

        struct axis2_stub_AviaryHadoopService_getJobTracker_callback_data
        {   
            IAviaryHadoopServiceCallback *callback;
          
        };

        static axis2_status_t WSF_CALL axis2_stub_on_error_AviaryHadoopService_getJobTracker(axis2_callback_t *axis_callback, const axutil_env_t *env, int exception)
        {
            struct axis2_stub_AviaryHadoopService_getJobTracker_callback_data* callback_data = NULL;
            callback_data = (struct axis2_stub_AviaryHadoopService_getJobTracker_callback_data*)axis2_callback_get_data(axis_callback);
        
            IAviaryHadoopServiceCallback* callback = NULL;
            callback = callback_data->callback;
            callback->receiveError_getJobTracker(exception);
            return AXIS2_SUCCESS;
        } 

        axis2_status_t  AXIS2_CALL axis2_stub_on_complete_AviaryHadoopService_getJobTracker(axis2_callback_t *axis_callback, const axutil_env_t *env)
        {
            struct axis2_stub_AviaryHadoopService_getJobTracker_callback_data* callback_data = NULL;
            axis2_status_t status = AXIS2_SUCCESS;
            AviaryHadoop::GetJobTrackerResponse* ret_val;
            

            axiom_node_t *ret_node = NULL;
            axiom_soap_envelope_t *soap_envelope = NULL;

            

            IAviaryHadoopServiceCallback *callback = NULL;

            callback_data = (struct axis2_stub_AviaryHadoopService_getJobTracker_callback_data*)axis2_callback_get_data(axis_callback);

            callback = callback_data->callback;

            soap_envelope = axis2_callback_get_envelope(axis_callback, Environment::getEnv());
            if(soap_envelope)
            {
                axiom_soap_body_t *soap_body;
                soap_body = axiom_soap_envelope_get_body(soap_envelope, Environment::getEnv());
                if(soap_body)
                {
                    axiom_soap_fault_t *soap_fault = NULL;
                    axiom_node_t *body_node = axiom_soap_body_get_base_node(soap_body, Environment::getEnv());

                      if(body_node)
                    {
                        ret_node = axiom_node_get_first_child(body_node, Environment::getEnv());
                    }
                }
                
                
            }


            
                    if(ret_node != NULL)
                    {
                        ret_val = new AviaryHadoop::GetJobTrackerResponse();
     
                        if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                        {
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the LendResponse_deserialize: "
                                                                    "This should be due to an invalid XML");
                            delete ret_val;
                            ret_val = NULL;
                        }
                     }
                     else
                     {
                         ret_val = NULL; 
                     }

                     
                     callback->receiveResult_getJobTracker(ret_val);
                         
 
            if(callback_data)
            {
                AXIS2_FREE(Environment::getEnv()->allocator, callback_data);
            }
            return AXIS2_SUCCESS;
        }

        /**
          * auto generated method signature for asynchronous invocations
          * for "getJobTracker|http://grid.redhat.com/aviary-hadoop/" operation.
          * @param stub The stub
          * @param env environment ( mandatory)
          * @param _getJobTracker80 of the AviaryHadoop::GetJobTracker
          * @param user_data user data to be accessed by the callbacks
          * @param on_complete callback to handle on complete
          * @param on_error callback to handle on error
          */

         void WSF_CALL
        AviaryHadoopServiceStub::start_getJobTracker(AviaryHadoop::GetJobTracker*  _getJobTracker80,
                                IAviaryHadoopServiceCallback* cb)
         {

            axis2_callback_t *callback = NULL;

            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;

            const axis2_char_t *soap_action = NULL;
            axiom_node_t *payload = NULL;

            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            
            
            struct axis2_stub_AviaryHadoopService_getJobTracker_callback_data *callback_data;

            callback_data = (struct axis2_stub_AviaryHadoopService_getJobTracker_callback_data*) AXIS2_MALLOC(Environment::getEnv()->allocator, 
                                    sizeof(struct axis2_stub_AviaryHadoopService_getJobTracker_callback_data));
            if(NULL == callback_data)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_NO_MEMORY, AXIS2_FAILURE);
                AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "Can not allocate memory for the callback data structures");
                return;
            }
            

            
                                payload = _getJobTracker80->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           

	    svc_client =   serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
              AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
              AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
              return;
            }

            soap_act =axis2_options_get_soap_action (options, Environment::getEnv());
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-hadoop/jobtracker/get";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-hadoop/jobtracker/get");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);
            }
            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             

            callback = axis2_callback_create(Environment::getEnv());
            /* Set our on_complete function pointer to the callback object */
            axis2_callback_set_on_complete(callback, axis2_stub_on_complete_AviaryHadoopService_getJobTracker);
            /* Set our on_error function pointer to the callback object */
            axis2_callback_set_on_error(callback, axis2_stub_on_error_AviaryHadoopService_getJobTracker);

            callback_data->callback = cb;
            axis2_callback_set_data(callback, (void*)callback_data);

            /* Send request */
            axis2_svc_client_send_receive_non_blocking(svc_client, Environment::getEnv(), payload, callback);
            
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);
              
              axis2_options_set_action(options, Environment::getEnv(), NULL);
            }
         }

         

        struct axis2_stub_AviaryHadoopService_stopTaskTracker_callback_data
        {   
            IAviaryHadoopServiceCallback *callback;
          
        };

        static axis2_status_t WSF_CALL axis2_stub_on_error_AviaryHadoopService_stopTaskTracker(axis2_callback_t *axis_callback, const axutil_env_t *env, int exception)
        {
            struct axis2_stub_AviaryHadoopService_stopTaskTracker_callback_data* callback_data = NULL;
            callback_data = (struct axis2_stub_AviaryHadoopService_stopTaskTracker_callback_data*)axis2_callback_get_data(axis_callback);
        
            IAviaryHadoopServiceCallback* callback = NULL;
            callback = callback_data->callback;
            callback->receiveError_stopTaskTracker(exception);
            return AXIS2_SUCCESS;
        } 

        axis2_status_t  AXIS2_CALL axis2_stub_on_complete_AviaryHadoopService_stopTaskTracker(axis2_callback_t *axis_callback, const axutil_env_t *env)
        {
            struct axis2_stub_AviaryHadoopService_stopTaskTracker_callback_data* callback_data = NULL;
            axis2_status_t status = AXIS2_SUCCESS;
            AviaryHadoop::StopTaskTrackerResponse* ret_val;
            

            axiom_node_t *ret_node = NULL;
            axiom_soap_envelope_t *soap_envelope = NULL;

            

            IAviaryHadoopServiceCallback *callback = NULL;

            callback_data = (struct axis2_stub_AviaryHadoopService_stopTaskTracker_callback_data*)axis2_callback_get_data(axis_callback);

            callback = callback_data->callback;

            soap_envelope = axis2_callback_get_envelope(axis_callback, Environment::getEnv());
            if(soap_envelope)
            {
                axiom_soap_body_t *soap_body;
                soap_body = axiom_soap_envelope_get_body(soap_envelope, Environment::getEnv());
                if(soap_body)
                {
                    axiom_soap_fault_t *soap_fault = NULL;
                    axiom_node_t *body_node = axiom_soap_body_get_base_node(soap_body, Environment::getEnv());

                      if(body_node)
                    {
                        ret_node = axiom_node_get_first_child(body_node, Environment::getEnv());
                    }
                }
                
                
            }


            
                    if(ret_node != NULL)
                    {
                        ret_val = new AviaryHadoop::StopTaskTrackerResponse();
     
                        if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                        {
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the LendResponse_deserialize: "
                                                                    "This should be due to an invalid XML");
                            delete ret_val;
                            ret_val = NULL;
                        }
                     }
                     else
                     {
                         ret_val = NULL; 
                     }

                     
                     callback->receiveResult_stopTaskTracker(ret_val);
                         
 
            if(callback_data)
            {
                AXIS2_FREE(Environment::getEnv()->allocator, callback_data);
            }
            return AXIS2_SUCCESS;
        }

        /**
          * auto generated method signature for asynchronous invocations
          * for "stopTaskTracker|http://grid.redhat.com/aviary-hadoop/" operation.
          * @param stub The stub
          * @param env environment ( mandatory)
          * @param _stopTaskTracker82 of the AviaryHadoop::StopTaskTracker
          * @param user_data user data to be accessed by the callbacks
          * @param on_complete callback to handle on complete
          * @param on_error callback to handle on error
          */

         void WSF_CALL
        AviaryHadoopServiceStub::start_stopTaskTracker(AviaryHadoop::StopTaskTracker*  _stopTaskTracker82,
                                IAviaryHadoopServiceCallback* cb)
         {

            axis2_callback_t *callback = NULL;

            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;

            const axis2_char_t *soap_action = NULL;
            axiom_node_t *payload = NULL;

            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            
            
            struct axis2_stub_AviaryHadoopService_stopTaskTracker_callback_data *callback_data;

            callback_data = (struct axis2_stub_AviaryHadoopService_stopTaskTracker_callback_data*) AXIS2_MALLOC(Environment::getEnv()->allocator, 
                                    sizeof(struct axis2_stub_AviaryHadoopService_stopTaskTracker_callback_data));
            if(NULL == callback_data)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_NO_MEMORY, AXIS2_FAILURE);
                AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "Can not allocate memory for the callback data structures");
                return;
            }
            

            
                                payload = _stopTaskTracker82->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           

	    svc_client =   serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
              AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
              AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
              return;
            }

            soap_act =axis2_options_get_soap_action (options, Environment::getEnv());
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-hadoop/tasktracker/stop";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-hadoop/tasktracker/stop");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);
            }
            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             

            callback = axis2_callback_create(Environment::getEnv());
            /* Set our on_complete function pointer to the callback object */
            axis2_callback_set_on_complete(callback, axis2_stub_on_complete_AviaryHadoopService_stopTaskTracker);
            /* Set our on_error function pointer to the callback object */
            axis2_callback_set_on_error(callback, axis2_stub_on_error_AviaryHadoopService_stopTaskTracker);

            callback_data->callback = cb;
            axis2_callback_set_data(callback, (void*)callback_data);

            /* Send request */
            axis2_svc_client_send_receive_non_blocking(svc_client, Environment::getEnv(), payload, callback);
            
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);
              
              axis2_options_set_action(options, Environment::getEnv(), NULL);
            }
         }

         

        struct axis2_stub_AviaryHadoopService_startNameNode_callback_data
        {   
            IAviaryHadoopServiceCallback *callback;
          
        };

        static axis2_status_t WSF_CALL axis2_stub_on_error_AviaryHadoopService_startNameNode(axis2_callback_t *axis_callback, const axutil_env_t *env, int exception)
        {
            struct axis2_stub_AviaryHadoopService_startNameNode_callback_data* callback_data = NULL;
            callback_data = (struct axis2_stub_AviaryHadoopService_startNameNode_callback_data*)axis2_callback_get_data(axis_callback);
        
            IAviaryHadoopServiceCallback* callback = NULL;
            callback = callback_data->callback;
            callback->receiveError_startNameNode(exception);
            return AXIS2_SUCCESS;
        } 

        axis2_status_t  AXIS2_CALL axis2_stub_on_complete_AviaryHadoopService_startNameNode(axis2_callback_t *axis_callback, const axutil_env_t *env)
        {
            struct axis2_stub_AviaryHadoopService_startNameNode_callback_data* callback_data = NULL;
            axis2_status_t status = AXIS2_SUCCESS;
            AviaryHadoop::StartNameNodeResponse* ret_val;
            

            axiom_node_t *ret_node = NULL;
            axiom_soap_envelope_t *soap_envelope = NULL;

            

            IAviaryHadoopServiceCallback *callback = NULL;

            callback_data = (struct axis2_stub_AviaryHadoopService_startNameNode_callback_data*)axis2_callback_get_data(axis_callback);

            callback = callback_data->callback;

            soap_envelope = axis2_callback_get_envelope(axis_callback, Environment::getEnv());
            if(soap_envelope)
            {
                axiom_soap_body_t *soap_body;
                soap_body = axiom_soap_envelope_get_body(soap_envelope, Environment::getEnv());
                if(soap_body)
                {
                    axiom_soap_fault_t *soap_fault = NULL;
                    axiom_node_t *body_node = axiom_soap_body_get_base_node(soap_body, Environment::getEnv());

                      if(body_node)
                    {
                        ret_node = axiom_node_get_first_child(body_node, Environment::getEnv());
                    }
                }
                
                
            }


            
                    if(ret_node != NULL)
                    {
                        ret_val = new AviaryHadoop::StartNameNodeResponse();
     
                        if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                        {
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the LendResponse_deserialize: "
                                                                    "This should be due to an invalid XML");
                            delete ret_val;
                            ret_val = NULL;
                        }
                     }
                     else
                     {
                         ret_val = NULL; 
                     }

                     
                     callback->receiveResult_startNameNode(ret_val);
                         
 
            if(callback_data)
            {
                AXIS2_FREE(Environment::getEnv()->allocator, callback_data);
            }
            return AXIS2_SUCCESS;
        }

        /**
          * auto generated method signature for asynchronous invocations
          * for "startNameNode|http://grid.redhat.com/aviary-hadoop/" operation.
          * @param stub The stub
          * @param env environment ( mandatory)
          * @param _startNameNode84 of the AviaryHadoop::StartNameNode
          * @param user_data user data to be accessed by the callbacks
          * @param on_complete callback to handle on complete
          * @param on_error callback to handle on error
          */

         void WSF_CALL
        AviaryHadoopServiceStub::start_startNameNode(AviaryHadoop::StartNameNode*  _startNameNode84,
                                IAviaryHadoopServiceCallback* cb)
         {

            axis2_callback_t *callback = NULL;

            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;

            const axis2_char_t *soap_action = NULL;
            axiom_node_t *payload = NULL;

            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            
            
            struct axis2_stub_AviaryHadoopService_startNameNode_callback_data *callback_data;

            callback_data = (struct axis2_stub_AviaryHadoopService_startNameNode_callback_data*) AXIS2_MALLOC(Environment::getEnv()->allocator, 
                                    sizeof(struct axis2_stub_AviaryHadoopService_startNameNode_callback_data));
            if(NULL == callback_data)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_NO_MEMORY, AXIS2_FAILURE);
                AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "Can not allocate memory for the callback data structures");
                return;
            }
            

            
                                payload = _startNameNode84->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           

	    svc_client =   serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
              AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
              AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
              return;
            }

            soap_act =axis2_options_get_soap_action (options, Environment::getEnv());
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-hadoop/namenode/start";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-hadoop/namenode/start");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);
            }
            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             

            callback = axis2_callback_create(Environment::getEnv());
            /* Set our on_complete function pointer to the callback object */
            axis2_callback_set_on_complete(callback, axis2_stub_on_complete_AviaryHadoopService_startNameNode);
            /* Set our on_error function pointer to the callback object */
            axis2_callback_set_on_error(callback, axis2_stub_on_error_AviaryHadoopService_startNameNode);

            callback_data->callback = cb;
            axis2_callback_set_data(callback, (void*)callback_data);

            /* Send request */
            axis2_svc_client_send_receive_non_blocking(svc_client, Environment::getEnv(), payload, callback);
            
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);
              
              axis2_options_set_action(options, Environment::getEnv(), NULL);
            }
         }

         

        struct axis2_stub_AviaryHadoopService_getDataNode_callback_data
        {   
            IAviaryHadoopServiceCallback *callback;
          
        };

        static axis2_status_t WSF_CALL axis2_stub_on_error_AviaryHadoopService_getDataNode(axis2_callback_t *axis_callback, const axutil_env_t *env, int exception)
        {
            struct axis2_stub_AviaryHadoopService_getDataNode_callback_data* callback_data = NULL;
            callback_data = (struct axis2_stub_AviaryHadoopService_getDataNode_callback_data*)axis2_callback_get_data(axis_callback);
        
            IAviaryHadoopServiceCallback* callback = NULL;
            callback = callback_data->callback;
            callback->receiveError_getDataNode(exception);
            return AXIS2_SUCCESS;
        } 

        axis2_status_t  AXIS2_CALL axis2_stub_on_complete_AviaryHadoopService_getDataNode(axis2_callback_t *axis_callback, const axutil_env_t *env)
        {
            struct axis2_stub_AviaryHadoopService_getDataNode_callback_data* callback_data = NULL;
            axis2_status_t status = AXIS2_SUCCESS;
            AviaryHadoop::GetDataNodeResponse* ret_val;
            

            axiom_node_t *ret_node = NULL;
            axiom_soap_envelope_t *soap_envelope = NULL;

            

            IAviaryHadoopServiceCallback *callback = NULL;

            callback_data = (struct axis2_stub_AviaryHadoopService_getDataNode_callback_data*)axis2_callback_get_data(axis_callback);

            callback = callback_data->callback;

            soap_envelope = axis2_callback_get_envelope(axis_callback, Environment::getEnv());
            if(soap_envelope)
            {
                axiom_soap_body_t *soap_body;
                soap_body = axiom_soap_envelope_get_body(soap_envelope, Environment::getEnv());
                if(soap_body)
                {
                    axiom_soap_fault_t *soap_fault = NULL;
                    axiom_node_t *body_node = axiom_soap_body_get_base_node(soap_body, Environment::getEnv());

                      if(body_node)
                    {
                        ret_node = axiom_node_get_first_child(body_node, Environment::getEnv());
                    }
                }
                
                
            }


            
                    if(ret_node != NULL)
                    {
                        ret_val = new AviaryHadoop::GetDataNodeResponse();
     
                        if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                        {
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the LendResponse_deserialize: "
                                                                    "This should be due to an invalid XML");
                            delete ret_val;
                            ret_val = NULL;
                        }
                     }
                     else
                     {
                         ret_val = NULL; 
                     }

                     
                     callback->receiveResult_getDataNode(ret_val);
                         
 
            if(callback_data)
            {
                AXIS2_FREE(Environment::getEnv()->allocator, callback_data);
            }
            return AXIS2_SUCCESS;
        }

        /**
          * auto generated method signature for asynchronous invocations
          * for "getDataNode|http://grid.redhat.com/aviary-hadoop/" operation.
          * @param stub The stub
          * @param env environment ( mandatory)
          * @param _getDataNode86 of the AviaryHadoop::GetDataNode
          * @param user_data user data to be accessed by the callbacks
          * @param on_complete callback to handle on complete
          * @param on_error callback to handle on error
          */

         void WSF_CALL
        AviaryHadoopServiceStub::start_getDataNode(AviaryHadoop::GetDataNode*  _getDataNode86,
                                IAviaryHadoopServiceCallback* cb)
         {

            axis2_callback_t *callback = NULL;

            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;

            const axis2_char_t *soap_action = NULL;
            axiom_node_t *payload = NULL;

            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            
            
            struct axis2_stub_AviaryHadoopService_getDataNode_callback_data *callback_data;

            callback_data = (struct axis2_stub_AviaryHadoopService_getDataNode_callback_data*) AXIS2_MALLOC(Environment::getEnv()->allocator, 
                                    sizeof(struct axis2_stub_AviaryHadoopService_getDataNode_callback_data));
            if(NULL == callback_data)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_NO_MEMORY, AXIS2_FAILURE);
                AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "Can not allocate memory for the callback data structures");
                return;
            }
            

            
                                payload = _getDataNode86->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           

	    svc_client =   serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
              AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
              AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
              return;
            }

            soap_act =axis2_options_get_soap_action (options, Environment::getEnv());
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-hadoop/datanode/get";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-hadoop/datanode/get");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);
            }
            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             

            callback = axis2_callback_create(Environment::getEnv());
            /* Set our on_complete function pointer to the callback object */
            axis2_callback_set_on_complete(callback, axis2_stub_on_complete_AviaryHadoopService_getDataNode);
            /* Set our on_error function pointer to the callback object */
            axis2_callback_set_on_error(callback, axis2_stub_on_error_AviaryHadoopService_getDataNode);

            callback_data->callback = cb;
            axis2_callback_set_data(callback, (void*)callback_data);

            /* Send request */
            axis2_svc_client_send_receive_non_blocking(svc_client, Environment::getEnv(), payload, callback);
            
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);
              
              axis2_options_set_action(options, Environment::getEnv(), NULL);
            }
         }

         

        struct axis2_stub_AviaryHadoopService_stopNameNode_callback_data
        {   
            IAviaryHadoopServiceCallback *callback;
          
        };

        static axis2_status_t WSF_CALL axis2_stub_on_error_AviaryHadoopService_stopNameNode(axis2_callback_t *axis_callback, const axutil_env_t *env, int exception)
        {
            struct axis2_stub_AviaryHadoopService_stopNameNode_callback_data* callback_data = NULL;
            callback_data = (struct axis2_stub_AviaryHadoopService_stopNameNode_callback_data*)axis2_callback_get_data(axis_callback);
        
            IAviaryHadoopServiceCallback* callback = NULL;
            callback = callback_data->callback;
            callback->receiveError_stopNameNode(exception);
            return AXIS2_SUCCESS;
        } 

        axis2_status_t  AXIS2_CALL axis2_stub_on_complete_AviaryHadoopService_stopNameNode(axis2_callback_t *axis_callback, const axutil_env_t *env)
        {
            struct axis2_stub_AviaryHadoopService_stopNameNode_callback_data* callback_data = NULL;
            axis2_status_t status = AXIS2_SUCCESS;
            AviaryHadoop::StopNameNodeResponse* ret_val;
            

            axiom_node_t *ret_node = NULL;
            axiom_soap_envelope_t *soap_envelope = NULL;

            

            IAviaryHadoopServiceCallback *callback = NULL;

            callback_data = (struct axis2_stub_AviaryHadoopService_stopNameNode_callback_data*)axis2_callback_get_data(axis_callback);

            callback = callback_data->callback;

            soap_envelope = axis2_callback_get_envelope(axis_callback, Environment::getEnv());
            if(soap_envelope)
            {
                axiom_soap_body_t *soap_body;
                soap_body = axiom_soap_envelope_get_body(soap_envelope, Environment::getEnv());
                if(soap_body)
                {
                    axiom_soap_fault_t *soap_fault = NULL;
                    axiom_node_t *body_node = axiom_soap_body_get_base_node(soap_body, Environment::getEnv());

                      if(body_node)
                    {
                        ret_node = axiom_node_get_first_child(body_node, Environment::getEnv());
                    }
                }
                
                
            }


            
                    if(ret_node != NULL)
                    {
                        ret_val = new AviaryHadoop::StopNameNodeResponse();
     
                        if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                        {
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the LendResponse_deserialize: "
                                                                    "This should be due to an invalid XML");
                            delete ret_val;
                            ret_val = NULL;
                        }
                     }
                     else
                     {
                         ret_val = NULL; 
                     }

                     
                     callback->receiveResult_stopNameNode(ret_val);
                         
 
            if(callback_data)
            {
                AXIS2_FREE(Environment::getEnv()->allocator, callback_data);
            }
            return AXIS2_SUCCESS;
        }

        /**
          * auto generated method signature for asynchronous invocations
          * for "stopNameNode|http://grid.redhat.com/aviary-hadoop/" operation.
          * @param stub The stub
          * @param env environment ( mandatory)
          * @param _stopNameNode88 of the AviaryHadoop::StopNameNode
          * @param user_data user data to be accessed by the callbacks
          * @param on_complete callback to handle on complete
          * @param on_error callback to handle on error
          */

         void WSF_CALL
        AviaryHadoopServiceStub::start_stopNameNode(AviaryHadoop::StopNameNode*  _stopNameNode88,
                                IAviaryHadoopServiceCallback* cb)
         {

            axis2_callback_t *callback = NULL;

            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;

            const axis2_char_t *soap_action = NULL;
            axiom_node_t *payload = NULL;

            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            
            
            struct axis2_stub_AviaryHadoopService_stopNameNode_callback_data *callback_data;

            callback_data = (struct axis2_stub_AviaryHadoopService_stopNameNode_callback_data*) AXIS2_MALLOC(Environment::getEnv()->allocator, 
                                    sizeof(struct axis2_stub_AviaryHadoopService_stopNameNode_callback_data));
            if(NULL == callback_data)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_NO_MEMORY, AXIS2_FAILURE);
                AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "Can not allocate memory for the callback data structures");
                return;
            }
            

            
                                payload = _stopNameNode88->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           

	    svc_client =   serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
              AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
              AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
              return;
            }

            soap_act =axis2_options_get_soap_action (options, Environment::getEnv());
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-hadoop/namenode/stop";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-hadoop/namenode/stop");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);
            }
            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             

            callback = axis2_callback_create(Environment::getEnv());
            /* Set our on_complete function pointer to the callback object */
            axis2_callback_set_on_complete(callback, axis2_stub_on_complete_AviaryHadoopService_stopNameNode);
            /* Set our on_error function pointer to the callback object */
            axis2_callback_set_on_error(callback, axis2_stub_on_error_AviaryHadoopService_stopNameNode);

            callback_data->callback = cb;
            axis2_callback_set_data(callback, (void*)callback_data);

            /* Send request */
            axis2_svc_client_send_receive_non_blocking(svc_client, Environment::getEnv(), payload, callback);
            
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);
              
              axis2_options_set_action(options, Environment::getEnv(), NULL);
            }
         }

         

        struct axis2_stub_AviaryHadoopService_getNameNode_callback_data
        {   
            IAviaryHadoopServiceCallback *callback;
          
        };

        static axis2_status_t WSF_CALL axis2_stub_on_error_AviaryHadoopService_getNameNode(axis2_callback_t *axis_callback, const axutil_env_t *env, int exception)
        {
            struct axis2_stub_AviaryHadoopService_getNameNode_callback_data* callback_data = NULL;
            callback_data = (struct axis2_stub_AviaryHadoopService_getNameNode_callback_data*)axis2_callback_get_data(axis_callback);
        
            IAviaryHadoopServiceCallback* callback = NULL;
            callback = callback_data->callback;
            callback->receiveError_getNameNode(exception);
            return AXIS2_SUCCESS;
        } 

        axis2_status_t  AXIS2_CALL axis2_stub_on_complete_AviaryHadoopService_getNameNode(axis2_callback_t *axis_callback, const axutil_env_t *env)
        {
            struct axis2_stub_AviaryHadoopService_getNameNode_callback_data* callback_data = NULL;
            axis2_status_t status = AXIS2_SUCCESS;
            AviaryHadoop::GetNameNodeResponse* ret_val;
            

            axiom_node_t *ret_node = NULL;
            axiom_soap_envelope_t *soap_envelope = NULL;

            

            IAviaryHadoopServiceCallback *callback = NULL;

            callback_data = (struct axis2_stub_AviaryHadoopService_getNameNode_callback_data*)axis2_callback_get_data(axis_callback);

            callback = callback_data->callback;

            soap_envelope = axis2_callback_get_envelope(axis_callback, Environment::getEnv());
            if(soap_envelope)
            {
                axiom_soap_body_t *soap_body;
                soap_body = axiom_soap_envelope_get_body(soap_envelope, Environment::getEnv());
                if(soap_body)
                {
                    axiom_soap_fault_t *soap_fault = NULL;
                    axiom_node_t *body_node = axiom_soap_body_get_base_node(soap_body, Environment::getEnv());

                      if(body_node)
                    {
                        ret_node = axiom_node_get_first_child(body_node, Environment::getEnv());
                    }
                }
                
                
            }


            
                    if(ret_node != NULL)
                    {
                        ret_val = new AviaryHadoop::GetNameNodeResponse();
     
                        if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                        {
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the LendResponse_deserialize: "
                                                                    "This should be due to an invalid XML");
                            delete ret_val;
                            ret_val = NULL;
                        }
                     }
                     else
                     {
                         ret_val = NULL; 
                     }

                     
                     callback->receiveResult_getNameNode(ret_val);
                         
 
            if(callback_data)
            {
                AXIS2_FREE(Environment::getEnv()->allocator, callback_data);
            }
            return AXIS2_SUCCESS;
        }

        /**
          * auto generated method signature for asynchronous invocations
          * for "getNameNode|http://grid.redhat.com/aviary-hadoop/" operation.
          * @param stub The stub
          * @param env environment ( mandatory)
          * @param _getNameNode90 of the AviaryHadoop::GetNameNode
          * @param user_data user data to be accessed by the callbacks
          * @param on_complete callback to handle on complete
          * @param on_error callback to handle on error
          */

         void WSF_CALL
        AviaryHadoopServiceStub::start_getNameNode(AviaryHadoop::GetNameNode*  _getNameNode90,
                                IAviaryHadoopServiceCallback* cb)
         {

            axis2_callback_t *callback = NULL;

            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;

            const axis2_char_t *soap_action = NULL;
            axiom_node_t *payload = NULL;

            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            
            
            struct axis2_stub_AviaryHadoopService_getNameNode_callback_data *callback_data;

            callback_data = (struct axis2_stub_AviaryHadoopService_getNameNode_callback_data*) AXIS2_MALLOC(Environment::getEnv()->allocator, 
                                    sizeof(struct axis2_stub_AviaryHadoopService_getNameNode_callback_data));
            if(NULL == callback_data)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_NO_MEMORY, AXIS2_FAILURE);
                AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "Can not allocate memory for the callback data structures");
                return;
            }
            

            
                                payload = _getNameNode90->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           

	    svc_client =   serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
              AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
              AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
              return;
            }

            soap_act =axis2_options_get_soap_action (options, Environment::getEnv());
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-hadoop/namenode/get";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-hadoop/namenode/get");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);
            }
            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             

            callback = axis2_callback_create(Environment::getEnv());
            /* Set our on_complete function pointer to the callback object */
            axis2_callback_set_on_complete(callback, axis2_stub_on_complete_AviaryHadoopService_getNameNode);
            /* Set our on_error function pointer to the callback object */
            axis2_callback_set_on_error(callback, axis2_stub_on_error_AviaryHadoopService_getNameNode);

            callback_data->callback = cb;
            axis2_callback_set_data(callback, (void*)callback_data);

            /* Send request */
            axis2_svc_client_send_receive_non_blocking(svc_client, Environment::getEnv(), payload, callback);
            
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);
              
              axis2_options_set_action(options, Environment::getEnv(), NULL);
            }
         }

         

        struct axis2_stub_AviaryHadoopService_startJobTracker_callback_data
        {   
            IAviaryHadoopServiceCallback *callback;
          
        };

        static axis2_status_t WSF_CALL axis2_stub_on_error_AviaryHadoopService_startJobTracker(axis2_callback_t *axis_callback, const axutil_env_t *env, int exception)
        {
            struct axis2_stub_AviaryHadoopService_startJobTracker_callback_data* callback_data = NULL;
            callback_data = (struct axis2_stub_AviaryHadoopService_startJobTracker_callback_data*)axis2_callback_get_data(axis_callback);
        
            IAviaryHadoopServiceCallback* callback = NULL;
            callback = callback_data->callback;
            callback->receiveError_startJobTracker(exception);
            return AXIS2_SUCCESS;
        } 

        axis2_status_t  AXIS2_CALL axis2_stub_on_complete_AviaryHadoopService_startJobTracker(axis2_callback_t *axis_callback, const axutil_env_t *env)
        {
            struct axis2_stub_AviaryHadoopService_startJobTracker_callback_data* callback_data = NULL;
            axis2_status_t status = AXIS2_SUCCESS;
            AviaryHadoop::StartJobTrackerResponse* ret_val;
            

            axiom_node_t *ret_node = NULL;
            axiom_soap_envelope_t *soap_envelope = NULL;

            

            IAviaryHadoopServiceCallback *callback = NULL;

            callback_data = (struct axis2_stub_AviaryHadoopService_startJobTracker_callback_data*)axis2_callback_get_data(axis_callback);

            callback = callback_data->callback;

            soap_envelope = axis2_callback_get_envelope(axis_callback, Environment::getEnv());
            if(soap_envelope)
            {
                axiom_soap_body_t *soap_body;
                soap_body = axiom_soap_envelope_get_body(soap_envelope, Environment::getEnv());
                if(soap_body)
                {
                    axiom_soap_fault_t *soap_fault = NULL;
                    axiom_node_t *body_node = axiom_soap_body_get_base_node(soap_body, Environment::getEnv());

                      if(body_node)
                    {
                        ret_node = axiom_node_get_first_child(body_node, Environment::getEnv());
                    }
                }
                
                
            }


            
                    if(ret_node != NULL)
                    {
                        ret_val = new AviaryHadoop::StartJobTrackerResponse();
     
                        if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                        {
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the LendResponse_deserialize: "
                                                                    "This should be due to an invalid XML");
                            delete ret_val;
                            ret_val = NULL;
                        }
                     }
                     else
                     {
                         ret_val = NULL; 
                     }

                     
                     callback->receiveResult_startJobTracker(ret_val);
                         
 
            if(callback_data)
            {
                AXIS2_FREE(Environment::getEnv()->allocator, callback_data);
            }
            return AXIS2_SUCCESS;
        }

        /**
          * auto generated method signature for asynchronous invocations
          * for "startJobTracker|http://grid.redhat.com/aviary-hadoop/" operation.
          * @param stub The stub
          * @param env environment ( mandatory)
          * @param _startJobTracker92 of the AviaryHadoop::StartJobTracker
          * @param user_data user data to be accessed by the callbacks
          * @param on_complete callback to handle on complete
          * @param on_error callback to handle on error
          */

         void WSF_CALL
        AviaryHadoopServiceStub::start_startJobTracker(AviaryHadoop::StartJobTracker*  _startJobTracker92,
                                IAviaryHadoopServiceCallback* cb)
         {

            axis2_callback_t *callback = NULL;

            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;

            const axis2_char_t *soap_action = NULL;
            axiom_node_t *payload = NULL;

            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            
            
            struct axis2_stub_AviaryHadoopService_startJobTracker_callback_data *callback_data;

            callback_data = (struct axis2_stub_AviaryHadoopService_startJobTracker_callback_data*) AXIS2_MALLOC(Environment::getEnv()->allocator, 
                                    sizeof(struct axis2_stub_AviaryHadoopService_startJobTracker_callback_data));
            if(NULL == callback_data)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_NO_MEMORY, AXIS2_FAILURE);
                AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "Can not allocate memory for the callback data structures");
                return;
            }
            

            
                                payload = _startJobTracker92->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           

	    svc_client =   serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
              AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
              AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
              return;
            }

            soap_act =axis2_options_get_soap_action (options, Environment::getEnv());
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-hadoop/jobtracker/start";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-hadoop/jobtracker/start");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);
            }
            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             

            callback = axis2_callback_create(Environment::getEnv());
            /* Set our on_complete function pointer to the callback object */
            axis2_callback_set_on_complete(callback, axis2_stub_on_complete_AviaryHadoopService_startJobTracker);
            /* Set our on_error function pointer to the callback object */
            axis2_callback_set_on_error(callback, axis2_stub_on_error_AviaryHadoopService_startJobTracker);

            callback_data->callback = cb;
            axis2_callback_set_data(callback, (void*)callback_data);

            /* Send request */
            axis2_svc_client_send_receive_non_blocking(svc_client, Environment::getEnv(), payload, callback);
            
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);
              
              axis2_options_set_action(options, Environment::getEnv(), NULL);
            }
         }

         

        struct axis2_stub_AviaryHadoopService_stopDataNode_callback_data
        {   
            IAviaryHadoopServiceCallback *callback;
          
        };

        static axis2_status_t WSF_CALL axis2_stub_on_error_AviaryHadoopService_stopDataNode(axis2_callback_t *axis_callback, const axutil_env_t *env, int exception)
        {
            struct axis2_stub_AviaryHadoopService_stopDataNode_callback_data* callback_data = NULL;
            callback_data = (struct axis2_stub_AviaryHadoopService_stopDataNode_callback_data*)axis2_callback_get_data(axis_callback);
        
            IAviaryHadoopServiceCallback* callback = NULL;
            callback = callback_data->callback;
            callback->receiveError_stopDataNode(exception);
            return AXIS2_SUCCESS;
        } 

        axis2_status_t  AXIS2_CALL axis2_stub_on_complete_AviaryHadoopService_stopDataNode(axis2_callback_t *axis_callback, const axutil_env_t *env)
        {
            struct axis2_stub_AviaryHadoopService_stopDataNode_callback_data* callback_data = NULL;
            axis2_status_t status = AXIS2_SUCCESS;
            AviaryHadoop::StopDataNodeResponse* ret_val;
            

            axiom_node_t *ret_node = NULL;
            axiom_soap_envelope_t *soap_envelope = NULL;

            

            IAviaryHadoopServiceCallback *callback = NULL;

            callback_data = (struct axis2_stub_AviaryHadoopService_stopDataNode_callback_data*)axis2_callback_get_data(axis_callback);

            callback = callback_data->callback;

            soap_envelope = axis2_callback_get_envelope(axis_callback, Environment::getEnv());
            if(soap_envelope)
            {
                axiom_soap_body_t *soap_body;
                soap_body = axiom_soap_envelope_get_body(soap_envelope, Environment::getEnv());
                if(soap_body)
                {
                    axiom_soap_fault_t *soap_fault = NULL;
                    axiom_node_t *body_node = axiom_soap_body_get_base_node(soap_body, Environment::getEnv());

                      if(body_node)
                    {
                        ret_node = axiom_node_get_first_child(body_node, Environment::getEnv());
                    }
                }
                
                
            }


            
                    if(ret_node != NULL)
                    {
                        ret_val = new AviaryHadoop::StopDataNodeResponse();
     
                        if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                        {
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the LendResponse_deserialize: "
                                                                    "This should be due to an invalid XML");
                            delete ret_val;
                            ret_val = NULL;
                        }
                     }
                     else
                     {
                         ret_val = NULL; 
                     }

                     
                     callback->receiveResult_stopDataNode(ret_val);
                         
 
            if(callback_data)
            {
                AXIS2_FREE(Environment::getEnv()->allocator, callback_data);
            }
            return AXIS2_SUCCESS;
        }

        /**
          * auto generated method signature for asynchronous invocations
          * for "stopDataNode|http://grid.redhat.com/aviary-hadoop/" operation.
          * @param stub The stub
          * @param env environment ( mandatory)
          * @param _stopDataNode94 of the AviaryHadoop::StopDataNode
          * @param user_data user data to be accessed by the callbacks
          * @param on_complete callback to handle on complete
          * @param on_error callback to handle on error
          */

         void WSF_CALL
        AviaryHadoopServiceStub::start_stopDataNode(AviaryHadoop::StopDataNode*  _stopDataNode94,
                                IAviaryHadoopServiceCallback* cb)
         {

            axis2_callback_t *callback = NULL;

            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;

            const axis2_char_t *soap_action = NULL;
            axiom_node_t *payload = NULL;

            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            
            
            struct axis2_stub_AviaryHadoopService_stopDataNode_callback_data *callback_data;

            callback_data = (struct axis2_stub_AviaryHadoopService_stopDataNode_callback_data*) AXIS2_MALLOC(Environment::getEnv()->allocator, 
                                    sizeof(struct axis2_stub_AviaryHadoopService_stopDataNode_callback_data));
            if(NULL == callback_data)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_NO_MEMORY, AXIS2_FAILURE);
                AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "Can not allocate memory for the callback data structures");
                return;
            }
            

            
                                payload = _stopDataNode94->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           

	    svc_client =   serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
              AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
              AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
              return;
            }

            soap_act =axis2_options_get_soap_action (options, Environment::getEnv());
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-hadoop/datanode/stop";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-hadoop/datanode/stop");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);
            }
            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             

            callback = axis2_callback_create(Environment::getEnv());
            /* Set our on_complete function pointer to the callback object */
            axis2_callback_set_on_complete(callback, axis2_stub_on_complete_AviaryHadoopService_stopDataNode);
            /* Set our on_error function pointer to the callback object */
            axis2_callback_set_on_error(callback, axis2_stub_on_error_AviaryHadoopService_stopDataNode);

            callback_data->callback = cb;
            axis2_callback_set_data(callback, (void*)callback_data);

            /* Send request */
            axis2_svc_client_send_receive_non_blocking(svc_client, Environment::getEnv(), payload, callback);
            
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);
              
              axis2_options_set_action(options, Environment::getEnv(), NULL);
            }
         }

         

