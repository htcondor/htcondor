

        /**
         * SchedulerSummary.cpp
         *
         * This file was auto-generated from WSDL
         * by the Apache Axis2/C version: SNAPSHOT  Built on : Mar 10, 2008 (08:35:52 GMT+00:00)
         */
        
            #include "AviaryCommon_SchedulerSummary.h"
          

       #ifdef __GNUC__
       # if __GNUC__ >= 4
       #pragma GCC diagnostic ignored "-Wcast-qual"
       #pragma GCC diagnostic ignored "-Wshadow"
       #pragma GCC diagnostic ignored "-Wunused-parameter"
       #pragma GCC diagnostic ignored "-Wunused-variable"
       #pragma GCC diagnostic ignored "-Wunused-value"
       #pragma GCC diagnostic ignored "-Wwrite-strings"
       #  if __GNUC_MINOR__ >= 6
       #pragma GCC diagnostic ignored "-Wenum-compare"
       #pragma GCC diagnostic ignored "-Wunused-but-set-variable"
       #pragma GCC diagnostic ignored "-Wunused-but-set-parameter"
       #  endif
       #  if __GNUC_MINOR__ >= 7
       #pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
       #  endif
       # endif
       #endif
        
        #include <Environment.h>
        #include <WSFError.h>


        using namespace wso2wsf;
        using namespace std;
        
        using namespace AviaryCommon;
        
                /*
                 * This type was generated from the piece of schema that had
                 * name = SchedulerSummary
                 * Namespace URI = http://common.aviary.grid.redhat.com
                 * Namespace Prefix = ns1
                 */
           AviaryCommon::SchedulerSummary::SchedulerSummary()
        {

        
                property_Queue_created  = NULL;
              
            isValidQueue_created  = false;
        
            isValidMax_jobs_running  = false;
        
            isValidUsers  = false;
        
            isValidAds  = false;
        
            isValidRunning  = false;
        
            isValidHeld  = false;
        
            isValidIdle  = false;
        
            isValidRemoved  = false;
        
        }

       AviaryCommon::SchedulerSummary::SchedulerSummary(axutil_date_time_t* arg_Queue_created,int arg_Max_jobs_running,int arg_Users,int arg_Ads,int arg_Running,int arg_Held,int arg_Idle,int arg_Removed)
        {
             
               property_Queue_created  = NULL;
             
            isValidQueue_created  = true;
            
            isValidMax_jobs_running  = true;
            
            isValidUsers  = true;
            
            isValidAds  = true;
            
            isValidRunning  = true;
            
            isValidHeld  = true;
            
            isValidIdle  = true;
            
            isValidRemoved  = true;
            
                    property_Queue_created = arg_Queue_created;
            
                    property_Max_jobs_running = arg_Max_jobs_running;
            
                    property_Users = arg_Users;
            
                    property_Ads = arg_Ads;
            
                    property_Running = arg_Running;
            
                    property_Held = arg_Held;
            
                    property_Idle = arg_Idle;
            
                    property_Removed = arg_Removed;
            
        }
        AviaryCommon::SchedulerSummary::~SchedulerSummary()
        {
            resetAll();
        }

        bool WSF_CALL AviaryCommon::SchedulerSummary::resetAll()
        {
            //calls reset method for all the properties owned by this method which are pointers.

            
             resetQueue_created();//axutil_date_time_t*
            return true;

        }

        

        bool WSF_CALL
        AviaryCommon::SchedulerSummary::deserialize(axiom_node_t** dp_parent,bool *dp_is_early_node_valid, bool dont_care_minoccurs)
        {
          axiom_node_t *parent = *dp_parent;
          
          bool status = AXIS2_SUCCESS;
           
         const axis2_char_t* text_value = NULL;
         axutil_qname_t *mqname = NULL;
          
            axutil_qname_t *element_qname = NULL; 
            
               axiom_node_t *first_node = NULL;
               bool is_early_node_valid = true;
               axiom_node_t *current_node = NULL;
               axiom_element_t *current_element = NULL;
            
              
              while(parent && axiom_node_get_node_type(parent, Environment::getEnv()) != AXIOM_ELEMENT)
              {
                  parent = axiom_node_get_next_sibling(parent, Environment::getEnv());
              }
              if (NULL == parent)
              {   
                return AXIS2_FAILURE;
              }
              
                      
                      first_node = axiom_node_get_first_child(parent, Environment::getEnv());
                      
                    

                     
                     /*
                      * building queue_created element
                      */
                     
                     
                     
                                   current_node = first_node;
                                   is_early_node_valid = false;
                                   
                                   
                                    while(current_node && axiom_node_get_node_type(current_node, Environment::getEnv()) != AXIOM_ELEMENT)
                                    {
                                        current_node = axiom_node_get_next_sibling(current_node, Environment::getEnv());
                                    }
                                    if(current_node != NULL)
                                    {
                                        current_element = (axiom_element_t *)axiom_node_get_data_element(current_node, Environment::getEnv());
                                        mqname = axiom_element_get_qname(current_element, Environment::getEnv(), current_node);
                                    }
                                   
                                 element_qname = axutil_qname_create(Environment::getEnv(), "queue_created", NULL, NULL);
                                 

                           if ( 
                                (current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("queue_created", axiom_element_get_localname(current_element, Environment::getEnv())))))
                           {
                              if( current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("queue_created", axiom_element_get_localname(current_element, Environment::getEnv()))))
                              {
                                is_early_node_valid = true;
                              }
                              
                                 
                                      text_value = axiom_element_get_text(current_element, Environment::getEnv(), current_node);
                                      if(text_value != NULL)
                                      {
                                          axutil_date_time_t* element = axutil_date_time_create(Environment::getEnv());
                                          status = axutil_date_time_deserialize_date_time((axutil_date_time_t*)element, Environment::getEnv(),
                                                                          text_value);
                                          if(AXIS2_FAILURE ==  status)
                                          {
                                              if(element != NULL)
                                              {
                                                  axutil_date_time_free(element, Environment::getEnv());
                                              }
					                          WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI ,"failed in building element queue_created ");
                                          }
                                          else
                                          {
                                            status = setQueue_created(element);
                                          }
                                      }
                                      
                                      else
                                      {
				                            WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "NULL value is set to a non nillable element queue_created");
                                            status = AXIS2_FAILURE;
                                      }
                                      
                                 if(AXIS2_FAILURE ==  status)
                                 {
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"failed in setting the value for queue_created ");
                                     if(element_qname)
                                     {
                                         axutil_qname_free(element_qname, Environment::getEnv());
                                     }
                                     return AXIS2_FAILURE;
                                 }
                              }
                           
                              else if(!dont_care_minoccurs)
                              {
                                  if(element_qname)
                                  {
                                      axutil_qname_free(element_qname, Environment::getEnv());
                                  }
                                  /* this is not a nillable element*/
				  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "non nillable or minOuccrs != 0 element queue_created missing");
                                  return AXIS2_FAILURE;
                              }
                           
                  if(element_qname)
                  {
                     axutil_qname_free(element_qname, Environment::getEnv());
                     element_qname = NULL;
                  }
                 

                     
                     /*
                      * building max_jobs_running element
                      */
                     
                     
                     
                                    /*
                                     * because elements are ordered this works fine
                                     */
                                  
                                   
                                   if(current_node != NULL && is_early_node_valid)
                                   {
                                       current_node = axiom_node_get_next_sibling(current_node, Environment::getEnv());
                                       
                                       
                                        while(current_node && axiom_node_get_node_type(current_node, Environment::getEnv()) != AXIOM_ELEMENT)
                                        {
                                            current_node = axiom_node_get_next_sibling(current_node, Environment::getEnv());
                                        }
                                        if(current_node != NULL)
                                        {
                                            current_element = (axiom_element_t *)axiom_node_get_data_element(current_node, Environment::getEnv());
                                            mqname = axiom_element_get_qname(current_element, Environment::getEnv(), current_node);
                                        }
                                       
                                   }
                                   is_early_node_valid = false;
                                 
                                 element_qname = axutil_qname_create(Environment::getEnv(), "max_jobs_running", NULL, NULL);
                                 

                           if ( 
                                (current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("max_jobs_running", axiom_element_get_localname(current_element, Environment::getEnv())))))
                           {
                              if( current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("max_jobs_running", axiom_element_get_localname(current_element, Environment::getEnv()))))
                              {
                                is_early_node_valid = true;
                              }
                              
                                 
                                      text_value = axiom_element_get_text(current_element, Environment::getEnv(), current_node);
                                      if(text_value != NULL)
                                      {
                                            status = setMax_jobs_running(atoi(text_value));
                                      }
                                      
                                      else
                                      {
                                          WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "NULL value is set to a non nillable element max_jobs_running");
                                          status = AXIS2_FAILURE;
                                      }
                                      
                                 if(AXIS2_FAILURE ==  status)
                                 {
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"failed in setting the value for max_jobs_running ");
                                     if(element_qname)
                                     {
                                         axutil_qname_free(element_qname, Environment::getEnv());
                                     }
                                     return AXIS2_FAILURE;
                                 }
                              }
                           
                              else if(!dont_care_minoccurs)
                              {
                                  if(element_qname)
                                  {
                                      axutil_qname_free(element_qname, Environment::getEnv());
                                  }
                                  /* this is not a nillable element*/
				  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "non nillable or minOuccrs != 0 element max_jobs_running missing");
                                  return AXIS2_FAILURE;
                              }
                           
                  if(element_qname)
                  {
                     axutil_qname_free(element_qname, Environment::getEnv());
                     element_qname = NULL;
                  }
                 

                     
                     /*
                      * building users element
                      */
                     
                     
                     
                                    /*
                                     * because elements are ordered this works fine
                                     */
                                  
                                   
                                   if(current_node != NULL && is_early_node_valid)
                                   {
                                       current_node = axiom_node_get_next_sibling(current_node, Environment::getEnv());
                                       
                                       
                                        while(current_node && axiom_node_get_node_type(current_node, Environment::getEnv()) != AXIOM_ELEMENT)
                                        {
                                            current_node = axiom_node_get_next_sibling(current_node, Environment::getEnv());
                                        }
                                        if(current_node != NULL)
                                        {
                                            current_element = (axiom_element_t *)axiom_node_get_data_element(current_node, Environment::getEnv());
                                            mqname = axiom_element_get_qname(current_element, Environment::getEnv(), current_node);
                                        }
                                       
                                   }
                                   is_early_node_valid = false;
                                 
                                 element_qname = axutil_qname_create(Environment::getEnv(), "users", NULL, NULL);
                                 

                           if ( 
                                (current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("users", axiom_element_get_localname(current_element, Environment::getEnv())))))
                           {
                              if( current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("users", axiom_element_get_localname(current_element, Environment::getEnv()))))
                              {
                                is_early_node_valid = true;
                              }
                              
                                 
                                      text_value = axiom_element_get_text(current_element, Environment::getEnv(), current_node);
                                      if(text_value != NULL)
                                      {
                                            status = setUsers(atoi(text_value));
                                      }
                                      
                                      else
                                      {
                                          WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "NULL value is set to a non nillable element users");
                                          status = AXIS2_FAILURE;
                                      }
                                      
                                 if(AXIS2_FAILURE ==  status)
                                 {
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"failed in setting the value for users ");
                                     if(element_qname)
                                     {
                                         axutil_qname_free(element_qname, Environment::getEnv());
                                     }
                                     return AXIS2_FAILURE;
                                 }
                              }
                           
                              else if(!dont_care_minoccurs)
                              {
                                  if(element_qname)
                                  {
                                      axutil_qname_free(element_qname, Environment::getEnv());
                                  }
                                  /* this is not a nillable element*/
				  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "non nillable or minOuccrs != 0 element users missing");
                                  return AXIS2_FAILURE;
                              }
                           
                  if(element_qname)
                  {
                     axutil_qname_free(element_qname, Environment::getEnv());
                     element_qname = NULL;
                  }
                 

                     
                     /*
                      * building ads element
                      */
                     
                     
                     
                                    /*
                                     * because elements are ordered this works fine
                                     */
                                  
                                   
                                   if(current_node != NULL && is_early_node_valid)
                                   {
                                       current_node = axiom_node_get_next_sibling(current_node, Environment::getEnv());
                                       
                                       
                                        while(current_node && axiom_node_get_node_type(current_node, Environment::getEnv()) != AXIOM_ELEMENT)
                                        {
                                            current_node = axiom_node_get_next_sibling(current_node, Environment::getEnv());
                                        }
                                        if(current_node != NULL)
                                        {
                                            current_element = (axiom_element_t *)axiom_node_get_data_element(current_node, Environment::getEnv());
                                            mqname = axiom_element_get_qname(current_element, Environment::getEnv(), current_node);
                                        }
                                       
                                   }
                                   is_early_node_valid = false;
                                 
                                 element_qname = axutil_qname_create(Environment::getEnv(), "ads", NULL, NULL);
                                 

                           if ( 
                                (current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("ads", axiom_element_get_localname(current_element, Environment::getEnv())))))
                           {
                              if( current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("ads", axiom_element_get_localname(current_element, Environment::getEnv()))))
                              {
                                is_early_node_valid = true;
                              }
                              
                                 
                                      text_value = axiom_element_get_text(current_element, Environment::getEnv(), current_node);
                                      if(text_value != NULL)
                                      {
                                            status = setAds(atoi(text_value));
                                      }
                                      
                                      else
                                      {
                                          WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "NULL value is set to a non nillable element ads");
                                          status = AXIS2_FAILURE;
                                      }
                                      
                                 if(AXIS2_FAILURE ==  status)
                                 {
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"failed in setting the value for ads ");
                                     if(element_qname)
                                     {
                                         axutil_qname_free(element_qname, Environment::getEnv());
                                     }
                                     return AXIS2_FAILURE;
                                 }
                              }
                           
                              else if(!dont_care_minoccurs)
                              {
                                  if(element_qname)
                                  {
                                      axutil_qname_free(element_qname, Environment::getEnv());
                                  }
                                  /* this is not a nillable element*/
				  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "non nillable or minOuccrs != 0 element ads missing");
                                  return AXIS2_FAILURE;
                              }
                           
                  if(element_qname)
                  {
                     axutil_qname_free(element_qname, Environment::getEnv());
                     element_qname = NULL;
                  }
                 

                     
                     /*
                      * building running element
                      */
                     
                     
                     
                                    /*
                                     * because elements are ordered this works fine
                                     */
                                  
                                   
                                   if(current_node != NULL && is_early_node_valid)
                                   {
                                       current_node = axiom_node_get_next_sibling(current_node, Environment::getEnv());
                                       
                                       
                                        while(current_node && axiom_node_get_node_type(current_node, Environment::getEnv()) != AXIOM_ELEMENT)
                                        {
                                            current_node = axiom_node_get_next_sibling(current_node, Environment::getEnv());
                                        }
                                        if(current_node != NULL)
                                        {
                                            current_element = (axiom_element_t *)axiom_node_get_data_element(current_node, Environment::getEnv());
                                            mqname = axiom_element_get_qname(current_element, Environment::getEnv(), current_node);
                                        }
                                       
                                   }
                                   is_early_node_valid = false;
                                 
                                 element_qname = axutil_qname_create(Environment::getEnv(), "running", NULL, NULL);
                                 

                           if ( 
                                (current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("running", axiom_element_get_localname(current_element, Environment::getEnv())))))
                           {
                              if( current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("running", axiom_element_get_localname(current_element, Environment::getEnv()))))
                              {
                                is_early_node_valid = true;
                              }
                              
                                 
                                      text_value = axiom_element_get_text(current_element, Environment::getEnv(), current_node);
                                      if(text_value != NULL)
                                      {
                                            status = setRunning(atoi(text_value));
                                      }
                                      
                                      else
                                      {
                                          WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "NULL value is set to a non nillable element running");
                                          status = AXIS2_FAILURE;
                                      }
                                      
                                 if(AXIS2_FAILURE ==  status)
                                 {
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"failed in setting the value for running ");
                                     if(element_qname)
                                     {
                                         axutil_qname_free(element_qname, Environment::getEnv());
                                     }
                                     return AXIS2_FAILURE;
                                 }
                              }
                           
                              else if(!dont_care_minoccurs)
                              {
                                  if(element_qname)
                                  {
                                      axutil_qname_free(element_qname, Environment::getEnv());
                                  }
                                  /* this is not a nillable element*/
				  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "non nillable or minOuccrs != 0 element running missing");
                                  return AXIS2_FAILURE;
                              }
                           
                  if(element_qname)
                  {
                     axutil_qname_free(element_qname, Environment::getEnv());
                     element_qname = NULL;
                  }
                 

                     
                     /*
                      * building held element
                      */
                     
                     
                     
                                    /*
                                     * because elements are ordered this works fine
                                     */
                                  
                                   
                                   if(current_node != NULL && is_early_node_valid)
                                   {
                                       current_node = axiom_node_get_next_sibling(current_node, Environment::getEnv());
                                       
                                       
                                        while(current_node && axiom_node_get_node_type(current_node, Environment::getEnv()) != AXIOM_ELEMENT)
                                        {
                                            current_node = axiom_node_get_next_sibling(current_node, Environment::getEnv());
                                        }
                                        if(current_node != NULL)
                                        {
                                            current_element = (axiom_element_t *)axiom_node_get_data_element(current_node, Environment::getEnv());
                                            mqname = axiom_element_get_qname(current_element, Environment::getEnv(), current_node);
                                        }
                                       
                                   }
                                   is_early_node_valid = false;
                                 
                                 element_qname = axutil_qname_create(Environment::getEnv(), "held", NULL, NULL);
                                 

                           if ( 
                                (current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("held", axiom_element_get_localname(current_element, Environment::getEnv())))))
                           {
                              if( current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("held", axiom_element_get_localname(current_element, Environment::getEnv()))))
                              {
                                is_early_node_valid = true;
                              }
                              
                                 
                                      text_value = axiom_element_get_text(current_element, Environment::getEnv(), current_node);
                                      if(text_value != NULL)
                                      {
                                            status = setHeld(atoi(text_value));
                                      }
                                      
                                      else
                                      {
                                          WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "NULL value is set to a non nillable element held");
                                          status = AXIS2_FAILURE;
                                      }
                                      
                                 if(AXIS2_FAILURE ==  status)
                                 {
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"failed in setting the value for held ");
                                     if(element_qname)
                                     {
                                         axutil_qname_free(element_qname, Environment::getEnv());
                                     }
                                     return AXIS2_FAILURE;
                                 }
                              }
                           
                              else if(!dont_care_minoccurs)
                              {
                                  if(element_qname)
                                  {
                                      axutil_qname_free(element_qname, Environment::getEnv());
                                  }
                                  /* this is not a nillable element*/
				  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "non nillable or minOuccrs != 0 element held missing");
                                  return AXIS2_FAILURE;
                              }
                           
                  if(element_qname)
                  {
                     axutil_qname_free(element_qname, Environment::getEnv());
                     element_qname = NULL;
                  }
                 

                     
                     /*
                      * building idle element
                      */
                     
                     
                     
                                    /*
                                     * because elements are ordered this works fine
                                     */
                                  
                                   
                                   if(current_node != NULL && is_early_node_valid)
                                   {
                                       current_node = axiom_node_get_next_sibling(current_node, Environment::getEnv());
                                       
                                       
                                        while(current_node && axiom_node_get_node_type(current_node, Environment::getEnv()) != AXIOM_ELEMENT)
                                        {
                                            current_node = axiom_node_get_next_sibling(current_node, Environment::getEnv());
                                        }
                                        if(current_node != NULL)
                                        {
                                            current_element = (axiom_element_t *)axiom_node_get_data_element(current_node, Environment::getEnv());
                                            mqname = axiom_element_get_qname(current_element, Environment::getEnv(), current_node);
                                        }
                                       
                                   }
                                   is_early_node_valid = false;
                                 
                                 element_qname = axutil_qname_create(Environment::getEnv(), "idle", NULL, NULL);
                                 

                           if ( 
                                (current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("idle", axiom_element_get_localname(current_element, Environment::getEnv())))))
                           {
                              if( current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("idle", axiom_element_get_localname(current_element, Environment::getEnv()))))
                              {
                                is_early_node_valid = true;
                              }
                              
                                 
                                      text_value = axiom_element_get_text(current_element, Environment::getEnv(), current_node);
                                      if(text_value != NULL)
                                      {
                                            status = setIdle(atoi(text_value));
                                      }
                                      
                                      else
                                      {
                                          WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "NULL value is set to a non nillable element idle");
                                          status = AXIS2_FAILURE;
                                      }
                                      
                                 if(AXIS2_FAILURE ==  status)
                                 {
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"failed in setting the value for idle ");
                                     if(element_qname)
                                     {
                                         axutil_qname_free(element_qname, Environment::getEnv());
                                     }
                                     return AXIS2_FAILURE;
                                 }
                              }
                           
                              else if(!dont_care_minoccurs)
                              {
                                  if(element_qname)
                                  {
                                      axutil_qname_free(element_qname, Environment::getEnv());
                                  }
                                  /* this is not a nillable element*/
				  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "non nillable or minOuccrs != 0 element idle missing");
                                  return AXIS2_FAILURE;
                              }
                           
                  if(element_qname)
                  {
                     axutil_qname_free(element_qname, Environment::getEnv());
                     element_qname = NULL;
                  }
                 

                     
                     /*
                      * building removed element
                      */
                     
                     
                     
                                    /*
                                     * because elements are ordered this works fine
                                     */
                                  
                                   
                                   if(current_node != NULL && is_early_node_valid)
                                   {
                                       current_node = axiom_node_get_next_sibling(current_node, Environment::getEnv());
                                       
                                       
                                        while(current_node && axiom_node_get_node_type(current_node, Environment::getEnv()) != AXIOM_ELEMENT)
                                        {
                                            current_node = axiom_node_get_next_sibling(current_node, Environment::getEnv());
                                        }
                                        if(current_node != NULL)
                                        {
                                            current_element = (axiom_element_t *)axiom_node_get_data_element(current_node, Environment::getEnv());
                                            mqname = axiom_element_get_qname(current_element, Environment::getEnv(), current_node);
                                        }
                                       
                                   }
                                   is_early_node_valid = false;
                                 
                                 element_qname = axutil_qname_create(Environment::getEnv(), "removed", NULL, NULL);
                                 

                           if ( 
                                (current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("removed", axiom_element_get_localname(current_element, Environment::getEnv())))))
                           {
                              if( current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("removed", axiom_element_get_localname(current_element, Environment::getEnv()))))
                              {
                                is_early_node_valid = true;
                              }
                              
                                 
                                      text_value = axiom_element_get_text(current_element, Environment::getEnv(), current_node);
                                      if(text_value != NULL)
                                      {
                                            status = setRemoved(atoi(text_value));
                                      }
                                      
                                      else
                                      {
                                          WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "NULL value is set to a non nillable element removed");
                                          status = AXIS2_FAILURE;
                                      }
                                      
                                 if(AXIS2_FAILURE ==  status)
                                 {
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"failed in setting the value for removed ");
                                     if(element_qname)
                                     {
                                         axutil_qname_free(element_qname, Environment::getEnv());
                                     }
                                     return AXIS2_FAILURE;
                                 }
                              }
                           
                              else if(!dont_care_minoccurs)
                              {
                                  if(element_qname)
                                  {
                                      axutil_qname_free(element_qname, Environment::getEnv());
                                  }
                                  /* this is not a nillable element*/
				  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "non nillable or minOuccrs != 0 element removed missing");
                                  return AXIS2_FAILURE;
                              }
                           
                  if(element_qname)
                  {
                     axutil_qname_free(element_qname, Environment::getEnv());
                     element_qname = NULL;
                  }
                 
          return status;
       }

          bool WSF_CALL
          AviaryCommon::SchedulerSummary::isParticle()
          {
            
                 return false;
              
          }


          void WSF_CALL
          AviaryCommon::SchedulerSummary::declareParentNamespaces(
                    axiom_element_t *parent_element,
                    axutil_hash_t *namespaces, int *next_ns_index)
          {
            
                  /* Here this is an empty function, Nothing to declare */
                 
          }

        
        
        axiom_node_t* WSF_CALL
	AviaryCommon::SchedulerSummary::serialize(axiom_node_t *parent, 
			axiom_element_t *parent_element, 
			int parent_tag_closed, 
			axutil_hash_t *namespaces, 
			int *next_ns_index)
        {
            
            
             axis2_char_t *string_to_stream;
            
         
         axiom_node_t *current_node = NULL;
         int tag_closed = 0;

         
         
                axiom_namespace_t *ns1 = NULL;

                axis2_char_t *qname_uri = NULL;
                axis2_char_t *qname_prefix = NULL;
                axis2_char_t *p_prefix = NULL;
            
                    axis2_char_t *text_value_1;
                    axis2_char_t *text_value_1_temp;
                    
                    axis2_char_t text_value_2[ADB_DEFAULT_DIGIT_LIMIT];
                    
                    axis2_char_t text_value_3[ADB_DEFAULT_DIGIT_LIMIT];
                    
                    axis2_char_t text_value_4[ADB_DEFAULT_DIGIT_LIMIT];
                    
                    axis2_char_t text_value_5[ADB_DEFAULT_DIGIT_LIMIT];
                    
                    axis2_char_t text_value_6[ADB_DEFAULT_DIGIT_LIMIT];
                    
                    axis2_char_t text_value_7[ADB_DEFAULT_DIGIT_LIMIT];
                    
                    axis2_char_t text_value_8[ADB_DEFAULT_DIGIT_LIMIT];
                    
               axis2_char_t *start_input_str = NULL;
               axis2_char_t *end_input_str = NULL;
               unsigned int start_input_str_len = 0;
               unsigned int end_input_str_len = 0;
            
            
               axiom_data_source_t *data_source = NULL;
               axutil_stream_t *stream = NULL;

            
                    current_node = parent;
                    data_source = (axiom_data_source_t *)axiom_node_get_data_element(current_node, Environment::getEnv());
                    if (!data_source)
                        return NULL;
                    stream = axiom_data_source_get_stream(data_source, Environment::getEnv()); /* assume parent is of type data source */
                    if (!stream)
                        return NULL;
                  
            if(!parent_tag_closed)
            {
            
              string_to_stream = ">"; 
              axutil_stream_write(stream, Environment::getEnv(), string_to_stream, axutil_strlen(string_to_stream));
              tag_closed = 1;
            
            }
            
                       p_prefix = NULL;
                      

                   if (!isValidQueue_created)
                   {
                      
                            
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Nil value found in non-nillable property queue_created");
                            return NULL;
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("queue_created"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("queue_created")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     
                     /*
                      * parsing queue_created element
                      */

                    
                    
                            sprintf(start_input_str, "<%s%squeue_created>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                            
                        start_input_str_len = axutil_strlen(start_input_str);
                        sprintf(end_input_str, "</%s%squeue_created>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                        end_input_str_len = axutil_strlen(end_input_str);
                    
                          text_value_1 = axutil_date_time_serialize_date_time(property_Queue_created, Environment::getEnv());
                           
                           axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                           
                           axutil_stream_write(stream, Environment::getEnv(), text_value_1, axutil_strlen(text_value_1));
                           
                           axutil_stream_write(stream, Environment::getEnv(), end_input_str, end_input_str_len);
                           
                     
                     AXIS2_FREE(Environment::getEnv()->allocator,start_input_str);
                     AXIS2_FREE(Environment::getEnv()->allocator,end_input_str);
                 } 

                 
                       p_prefix = NULL;
                      

                   if (!isValidMax_jobs_running)
                   {
                      
                            
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Nil value found in non-nillable property max_jobs_running");
                            return NULL;
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("max_jobs_running"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("max_jobs_running")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     
                     /*
                      * parsing max_jobs_running element
                      */

                    
                    
                            sprintf(start_input_str, "<%s%smax_jobs_running>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                            
                        start_input_str_len = axutil_strlen(start_input_str);
                        sprintf(end_input_str, "</%s%smax_jobs_running>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                        end_input_str_len = axutil_strlen(end_input_str);
                    
                               sprintf (text_value_2, AXIS2_PRINTF_INT32_FORMAT_SPECIFIER, property_Max_jobs_running);
                             
                           axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                           
                           axutil_stream_write(stream, Environment::getEnv(), text_value_2, axutil_strlen(text_value_2));
                           
                           axutil_stream_write(stream, Environment::getEnv(), end_input_str, end_input_str_len);
                           
                     
                     AXIS2_FREE(Environment::getEnv()->allocator,start_input_str);
                     AXIS2_FREE(Environment::getEnv()->allocator,end_input_str);
                 } 

                 
                       p_prefix = NULL;
                      

                   if (!isValidUsers)
                   {
                      
                            
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Nil value found in non-nillable property users");
                            return NULL;
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("users"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("users")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     
                     /*
                      * parsing users element
                      */

                    
                    
                            sprintf(start_input_str, "<%s%susers>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                            
                        start_input_str_len = axutil_strlen(start_input_str);
                        sprintf(end_input_str, "</%s%susers>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                        end_input_str_len = axutil_strlen(end_input_str);
                    
                               sprintf (text_value_3, AXIS2_PRINTF_INT32_FORMAT_SPECIFIER, property_Users);
                             
                           axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                           
                           axutil_stream_write(stream, Environment::getEnv(), text_value_3, axutil_strlen(text_value_3));
                           
                           axutil_stream_write(stream, Environment::getEnv(), end_input_str, end_input_str_len);
                           
                     
                     AXIS2_FREE(Environment::getEnv()->allocator,start_input_str);
                     AXIS2_FREE(Environment::getEnv()->allocator,end_input_str);
                 } 

                 
                       p_prefix = NULL;
                      

                   if (!isValidAds)
                   {
                      
                            
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Nil value found in non-nillable property ads");
                            return NULL;
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("ads"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("ads")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     
                     /*
                      * parsing ads element
                      */

                    
                    
                            sprintf(start_input_str, "<%s%sads>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                            
                        start_input_str_len = axutil_strlen(start_input_str);
                        sprintf(end_input_str, "</%s%sads>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                        end_input_str_len = axutil_strlen(end_input_str);
                    
                               sprintf (text_value_4, AXIS2_PRINTF_INT32_FORMAT_SPECIFIER, property_Ads);
                             
                           axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                           
                           axutil_stream_write(stream, Environment::getEnv(), text_value_4, axutil_strlen(text_value_4));
                           
                           axutil_stream_write(stream, Environment::getEnv(), end_input_str, end_input_str_len);
                           
                     
                     AXIS2_FREE(Environment::getEnv()->allocator,start_input_str);
                     AXIS2_FREE(Environment::getEnv()->allocator,end_input_str);
                 } 

                 
                       p_prefix = NULL;
                      

                   if (!isValidRunning)
                   {
                      
                            
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Nil value found in non-nillable property running");
                            return NULL;
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("running"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("running")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     
                     /*
                      * parsing running element
                      */

                    
                    
                            sprintf(start_input_str, "<%s%srunning>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                            
                        start_input_str_len = axutil_strlen(start_input_str);
                        sprintf(end_input_str, "</%s%srunning>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                        end_input_str_len = axutil_strlen(end_input_str);
                    
                               sprintf (text_value_5, AXIS2_PRINTF_INT32_FORMAT_SPECIFIER, property_Running);
                             
                           axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                           
                           axutil_stream_write(stream, Environment::getEnv(), text_value_5, axutil_strlen(text_value_5));
                           
                           axutil_stream_write(stream, Environment::getEnv(), end_input_str, end_input_str_len);
                           
                     
                     AXIS2_FREE(Environment::getEnv()->allocator,start_input_str);
                     AXIS2_FREE(Environment::getEnv()->allocator,end_input_str);
                 } 

                 
                       p_prefix = NULL;
                      

                   if (!isValidHeld)
                   {
                      
                            
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Nil value found in non-nillable property held");
                            return NULL;
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("held"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("held")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     
                     /*
                      * parsing held element
                      */

                    
                    
                            sprintf(start_input_str, "<%s%sheld>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                            
                        start_input_str_len = axutil_strlen(start_input_str);
                        sprintf(end_input_str, "</%s%sheld>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                        end_input_str_len = axutil_strlen(end_input_str);
                    
                               sprintf (text_value_6, AXIS2_PRINTF_INT32_FORMAT_SPECIFIER, property_Held);
                             
                           axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                           
                           axutil_stream_write(stream, Environment::getEnv(), text_value_6, axutil_strlen(text_value_6));
                           
                           axutil_stream_write(stream, Environment::getEnv(), end_input_str, end_input_str_len);
                           
                     
                     AXIS2_FREE(Environment::getEnv()->allocator,start_input_str);
                     AXIS2_FREE(Environment::getEnv()->allocator,end_input_str);
                 } 

                 
                       p_prefix = NULL;
                      

                   if (!isValidIdle)
                   {
                      
                            
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Nil value found in non-nillable property idle");
                            return NULL;
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("idle"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("idle")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     
                     /*
                      * parsing idle element
                      */

                    
                    
                            sprintf(start_input_str, "<%s%sidle>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                            
                        start_input_str_len = axutil_strlen(start_input_str);
                        sprintf(end_input_str, "</%s%sidle>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                        end_input_str_len = axutil_strlen(end_input_str);
                    
                               sprintf (text_value_7, AXIS2_PRINTF_INT32_FORMAT_SPECIFIER, property_Idle);
                             
                           axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                           
                           axutil_stream_write(stream, Environment::getEnv(), text_value_7, axutil_strlen(text_value_7));
                           
                           axutil_stream_write(stream, Environment::getEnv(), end_input_str, end_input_str_len);
                           
                     
                     AXIS2_FREE(Environment::getEnv()->allocator,start_input_str);
                     AXIS2_FREE(Environment::getEnv()->allocator,end_input_str);
                 } 

                 
                       p_prefix = NULL;
                      

                   if (!isValidRemoved)
                   {
                      
                            
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Nil value found in non-nillable property removed");
                            return NULL;
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("removed"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("removed")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     
                     /*
                      * parsing removed element
                      */

                    
                    
                            sprintf(start_input_str, "<%s%sremoved>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                            
                        start_input_str_len = axutil_strlen(start_input_str);
                        sprintf(end_input_str, "</%s%sremoved>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                        end_input_str_len = axutil_strlen(end_input_str);
                    
                               sprintf (text_value_8, AXIS2_PRINTF_INT32_FORMAT_SPECIFIER, property_Removed);
                             
                           axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                           
                           axutil_stream_write(stream, Environment::getEnv(), text_value_8, axutil_strlen(text_value_8));
                           
                           axutil_stream_write(stream, Environment::getEnv(), end_input_str, end_input_str_len);
                           
                     
                     AXIS2_FREE(Environment::getEnv()->allocator,start_input_str);
                     AXIS2_FREE(Environment::getEnv()->allocator,end_input_str);
                 } 

                 

            return parent;
        }


        

            /**
             * Getter for queue_created by  Property Number 1
             */
            axutil_date_time_t* WSF_CALL
            AviaryCommon::SchedulerSummary::getProperty1()
            {
                return getQueue_created();
            }

            /**
             * getter for queue_created.
             */
            axutil_date_time_t* WSF_CALL
            AviaryCommon::SchedulerSummary::getQueue_created()
             {
                return property_Queue_created;
             }

            /**
             * setter for queue_created
             */
            bool WSF_CALL
            AviaryCommon::SchedulerSummary::setQueue_created(
                    axutil_date_time_t*  arg_Queue_created)
             {
                

                if(isValidQueue_created &&
                        arg_Queue_created == property_Queue_created)
                {
                    
                    return true;
                }

                
                  if(NULL == arg_Queue_created)
                       
                  {
                      WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"queue_created is being set to NULL, but it is not a nullable element");
                      return AXIS2_FAILURE;
                  }
                

                
                resetQueue_created();

                
                    if(NULL == arg_Queue_created)
                         
                {
                    /* We are already done */
                    return true;
                }
                
                        property_Queue_created = arg_Queue_created;
                        isValidQueue_created = true;
                    
                return true;
             }

             

           /**
            * resetter for queue_created
            */
           bool WSF_CALL
           AviaryCommon::SchedulerSummary::resetQueue_created()
           {
               int i = 0;
               int count = 0;


               
            
                

                if(property_Queue_created != NULL)
                {
                   
                   
                      axutil_date_time_free(property_Queue_created, Environment::getEnv());
                         property_Queue_created = NULL;
                     

                   }

                
                
                
               isValidQueue_created = false; 
               return true;
           }

           /**
            * Check whether queue_created is nill
            */
           bool WSF_CALL
           AviaryCommon::SchedulerSummary::isQueue_createdNil()
           {
               return !isValidQueue_created;
           }

           /**
            * Set queue_created to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCommon::SchedulerSummary::setQueue_createdNil()
           {
               return resetQueue_created();
           }

           

            /**
             * Getter for max_jobs_running by  Property Number 2
             */
            int WSF_CALL
            AviaryCommon::SchedulerSummary::getProperty2()
            {
                return getMax_jobs_running();
            }

            /**
             * getter for max_jobs_running.
             */
            int WSF_CALL
            AviaryCommon::SchedulerSummary::getMax_jobs_running()
             {
                return property_Max_jobs_running;
             }

            /**
             * setter for max_jobs_running
             */
            bool WSF_CALL
            AviaryCommon::SchedulerSummary::setMax_jobs_running(
                    const int  arg_Max_jobs_running)
             {
                

                if(isValidMax_jobs_running &&
                        arg_Max_jobs_running == property_Max_jobs_running)
                {
                    
                    return true;
                }

                

                
                resetMax_jobs_running();

                
                        property_Max_jobs_running = arg_Max_jobs_running;
                        isValidMax_jobs_running = true;
                    
                return true;
             }

             

           /**
            * resetter for max_jobs_running
            */
           bool WSF_CALL
           AviaryCommon::SchedulerSummary::resetMax_jobs_running()
           {
               int i = 0;
               int count = 0;


               
               isValidMax_jobs_running = false; 
               return true;
           }

           /**
            * Check whether max_jobs_running is nill
            */
           bool WSF_CALL
           AviaryCommon::SchedulerSummary::isMax_jobs_runningNil()
           {
               return !isValidMax_jobs_running;
           }

           /**
            * Set max_jobs_running to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCommon::SchedulerSummary::setMax_jobs_runningNil()
           {
               return resetMax_jobs_running();
           }

           

            /**
             * Getter for users by  Property Number 3
             */
            int WSF_CALL
            AviaryCommon::SchedulerSummary::getProperty3()
            {
                return getUsers();
            }

            /**
             * getter for users.
             */
            int WSF_CALL
            AviaryCommon::SchedulerSummary::getUsers()
             {
                return property_Users;
             }

            /**
             * setter for users
             */
            bool WSF_CALL
            AviaryCommon::SchedulerSummary::setUsers(
                    const int  arg_Users)
             {
                

                if(isValidUsers &&
                        arg_Users == property_Users)
                {
                    
                    return true;
                }

                

                
                resetUsers();

                
                        property_Users = arg_Users;
                        isValidUsers = true;
                    
                return true;
             }

             

           /**
            * resetter for users
            */
           bool WSF_CALL
           AviaryCommon::SchedulerSummary::resetUsers()
           {
               int i = 0;
               int count = 0;


               
               isValidUsers = false; 
               return true;
           }

           /**
            * Check whether users is nill
            */
           bool WSF_CALL
           AviaryCommon::SchedulerSummary::isUsersNil()
           {
               return !isValidUsers;
           }

           /**
            * Set users to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCommon::SchedulerSummary::setUsersNil()
           {
               return resetUsers();
           }

           

            /**
             * Getter for ads by  Property Number 4
             */
            int WSF_CALL
            AviaryCommon::SchedulerSummary::getProperty4()
            {
                return getAds();
            }

            /**
             * getter for ads.
             */
            int WSF_CALL
            AviaryCommon::SchedulerSummary::getAds()
             {
                return property_Ads;
             }

            /**
             * setter for ads
             */
            bool WSF_CALL
            AviaryCommon::SchedulerSummary::setAds(
                    const int  arg_Ads)
             {
                

                if(isValidAds &&
                        arg_Ads == property_Ads)
                {
                    
                    return true;
                }

                

                
                resetAds();

                
                        property_Ads = arg_Ads;
                        isValidAds = true;
                    
                return true;
             }

             

           /**
            * resetter for ads
            */
           bool WSF_CALL
           AviaryCommon::SchedulerSummary::resetAds()
           {
               int i = 0;
               int count = 0;


               
               isValidAds = false; 
               return true;
           }

           /**
            * Check whether ads is nill
            */
           bool WSF_CALL
           AviaryCommon::SchedulerSummary::isAdsNil()
           {
               return !isValidAds;
           }

           /**
            * Set ads to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCommon::SchedulerSummary::setAdsNil()
           {
               return resetAds();
           }

           

            /**
             * Getter for running by  Property Number 5
             */
            int WSF_CALL
            AviaryCommon::SchedulerSummary::getProperty5()
            {
                return getRunning();
            }

            /**
             * getter for running.
             */
            int WSF_CALL
            AviaryCommon::SchedulerSummary::getRunning()
             {
                return property_Running;
             }

            /**
             * setter for running
             */
            bool WSF_CALL
            AviaryCommon::SchedulerSummary::setRunning(
                    const int  arg_Running)
             {
                

                if(isValidRunning &&
                        arg_Running == property_Running)
                {
                    
                    return true;
                }

                

                
                resetRunning();

                
                        property_Running = arg_Running;
                        isValidRunning = true;
                    
                return true;
             }

             

           /**
            * resetter for running
            */
           bool WSF_CALL
           AviaryCommon::SchedulerSummary::resetRunning()
           {
               int i = 0;
               int count = 0;


               
               isValidRunning = false; 
               return true;
           }

           /**
            * Check whether running is nill
            */
           bool WSF_CALL
           AviaryCommon::SchedulerSummary::isRunningNil()
           {
               return !isValidRunning;
           }

           /**
            * Set running to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCommon::SchedulerSummary::setRunningNil()
           {
               return resetRunning();
           }

           

            /**
             * Getter for held by  Property Number 6
             */
            int WSF_CALL
            AviaryCommon::SchedulerSummary::getProperty6()
            {
                return getHeld();
            }

            /**
             * getter for held.
             */
            int WSF_CALL
            AviaryCommon::SchedulerSummary::getHeld()
             {
                return property_Held;
             }

            /**
             * setter for held
             */
            bool WSF_CALL
            AviaryCommon::SchedulerSummary::setHeld(
                    const int  arg_Held)
             {
                

                if(isValidHeld &&
                        arg_Held == property_Held)
                {
                    
                    return true;
                }

                

                
                resetHeld();

                
                        property_Held = arg_Held;
                        isValidHeld = true;
                    
                return true;
             }

             

           /**
            * resetter for held
            */
           bool WSF_CALL
           AviaryCommon::SchedulerSummary::resetHeld()
           {
               int i = 0;
               int count = 0;


               
               isValidHeld = false; 
               return true;
           }

           /**
            * Check whether held is nill
            */
           bool WSF_CALL
           AviaryCommon::SchedulerSummary::isHeldNil()
           {
               return !isValidHeld;
           }

           /**
            * Set held to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCommon::SchedulerSummary::setHeldNil()
           {
               return resetHeld();
           }

           

            /**
             * Getter for idle by  Property Number 7
             */
            int WSF_CALL
            AviaryCommon::SchedulerSummary::getProperty7()
            {
                return getIdle();
            }

            /**
             * getter for idle.
             */
            int WSF_CALL
            AviaryCommon::SchedulerSummary::getIdle()
             {
                return property_Idle;
             }

            /**
             * setter for idle
             */
            bool WSF_CALL
            AviaryCommon::SchedulerSummary::setIdle(
                    const int  arg_Idle)
             {
                

                if(isValidIdle &&
                        arg_Idle == property_Idle)
                {
                    
                    return true;
                }

                

                
                resetIdle();

                
                        property_Idle = arg_Idle;
                        isValidIdle = true;
                    
                return true;
             }

             

           /**
            * resetter for idle
            */
           bool WSF_CALL
           AviaryCommon::SchedulerSummary::resetIdle()
           {
               int i = 0;
               int count = 0;


               
               isValidIdle = false; 
               return true;
           }

           /**
            * Check whether idle is nill
            */
           bool WSF_CALL
           AviaryCommon::SchedulerSummary::isIdleNil()
           {
               return !isValidIdle;
           }

           /**
            * Set idle to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCommon::SchedulerSummary::setIdleNil()
           {
               return resetIdle();
           }

           

            /**
             * Getter for removed by  Property Number 8
             */
            int WSF_CALL
            AviaryCommon::SchedulerSummary::getProperty8()
            {
                return getRemoved();
            }

            /**
             * getter for removed.
             */
            int WSF_CALL
            AviaryCommon::SchedulerSummary::getRemoved()
             {
                return property_Removed;
             }

            /**
             * setter for removed
             */
            bool WSF_CALL
            AviaryCommon::SchedulerSummary::setRemoved(
                    const int  arg_Removed)
             {
                

                if(isValidRemoved &&
                        arg_Removed == property_Removed)
                {
                    
                    return true;
                }

                

                
                resetRemoved();

                
                        property_Removed = arg_Removed;
                        isValidRemoved = true;
                    
                return true;
             }

             

           /**
            * resetter for removed
            */
           bool WSF_CALL
           AviaryCommon::SchedulerSummary::resetRemoved()
           {
               int i = 0;
               int count = 0;


               
               isValidRemoved = false; 
               return true;
           }

           /**
            * Check whether removed is nill
            */
           bool WSF_CALL
           AviaryCommon::SchedulerSummary::isRemovedNil()
           {
               return !isValidRemoved;
           }

           /**
            * Set removed to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCommon::SchedulerSummary::setRemovedNil()
           {
               return resetRemoved();
           }

           

