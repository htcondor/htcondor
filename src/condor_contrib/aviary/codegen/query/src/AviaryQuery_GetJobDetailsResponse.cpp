

        /**
         * GetJobDetailsResponse.cpp
         *
         * This file was auto-generated from WSDL
         * by the Apache Axis2/C version: SNAPSHOT  Built on : Mar 10, 2008 (08:35:52 GMT+00:00)
         */
        
            #include "AviaryQuery_GetJobDetailsResponse.h"
          

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
        
        using namespace AviaryQuery;
        
               /*
                * Implementation of the GetJobDetailsResponse|http://query.aviary.grid.redhat.com Element
                */
           AviaryQuery::GetJobDetailsResponse::GetJobDetailsResponse()
        {

        
            qname = NULL;
        
                property_Jobs  = NULL;
              
            isValidJobs  = false;
        
                  qname =  axutil_qname_create (Environment::getEnv(),
                        "GetJobDetailsResponse",
                        "http://query.aviary.grid.redhat.com",
                        NULL);
                
        }

       AviaryQuery::GetJobDetailsResponse::GetJobDetailsResponse(std::vector<AviaryCommon::JobDetails*>* arg_Jobs)
        {
             
                   qname = NULL;
             
               property_Jobs  = NULL;
             
            isValidJobs  = true;
            
                 qname =  axutil_qname_create (Environment::getEnv(),
                       "GetJobDetailsResponse",
                       "http://query.aviary.grid.redhat.com",
                       NULL);
               
                    property_Jobs = arg_Jobs;
            
        }
        AviaryQuery::GetJobDetailsResponse::~GetJobDetailsResponse()
        {
            resetAll();
        }

        bool WSF_CALL AviaryQuery::GetJobDetailsResponse::resetAll()
        {
            //calls reset method for all the properties owned by this method which are pointers.

            
             resetJobs();//AviaryCommon::JobDetails
          if(qname != NULL)
          {
            axutil_qname_free( qname, Environment::getEnv());
            qname = NULL;
          }
        
            return true;

        }

        

        bool WSF_CALL
        AviaryQuery::GetJobDetailsResponse::deserialize(axiom_node_t** dp_parent,bool *dp_is_early_node_valid, bool dont_care_minoccurs)
        {
          axiom_node_t *parent = *dp_parent;
          
          bool status = AXIS2_SUCCESS;
           
         const axis2_char_t* text_value = NULL;
         axutil_qname_t *mqname = NULL;
          
               int i = 0;
            
               int sequence_broken = 0;
               axiom_node_t *tmp_node = NULL;
            
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
              

                    current_element = (axiom_element_t *)axiom_node_get_data_element(parent, Environment::getEnv());
                    mqname = axiom_element_get_qname(current_element, Environment::getEnv(), parent);
                    if (axutil_qname_equals(mqname, Environment::getEnv(), this->qname))
                    {
                        
                          first_node = axiom_node_get_first_child(parent, Environment::getEnv());
                          
                    }
                    else
                    {
                        WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI,
                              "Failed in building adb object for GetJobDetailsResponse : "
                              "Expected %s but returned %s",
                              axutil_qname_to_string(qname, Environment::getEnv()),
                              axutil_qname_to_string(mqname, Environment::getEnv()));
                        
                        return AXIS2_FAILURE;
                    }
                    
                       { 
                    /*
                     * building Jobs array
                     */
                       std::vector<AviaryCommon::JobDetails*>* arr_list =new std::vector<AviaryCommon::JobDetails*>();
                   

                     
                     /*
                      * building jobs element
                      */
                     
                     
                     
                                    element_qname = axutil_qname_create(Environment::getEnv(), "jobs", NULL, NULL);
                                  
                               
                               for (i = 0, sequence_broken = 0, current_node = first_node; !sequence_broken && current_node != NULL;)
                                             
                               {
                                  if(axiom_node_get_node_type(current_node, Environment::getEnv()) != AXIOM_ELEMENT)
                                  {
                                     current_node =axiom_node_get_next_sibling(current_node, Environment::getEnv());
                                     is_early_node_valid = false;
                                     continue;
                                  }
                                  
                                  current_element = (axiom_element_t *)axiom_node_get_data_element(current_node, Environment::getEnv());
                                  mqname = axiom_element_get_qname(current_element, Environment::getEnv(), current_node);

                                  if (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("jobs", axiom_element_get_localname(current_element, Environment::getEnv())))
                                  {
                                  
                                      is_early_node_valid = true;
                                      
                                     AviaryCommon::JobDetails* element = new AviaryCommon::JobDetails();
                                          
                                          status =  element->deserialize(&current_node, &is_early_node_valid, false);
                                          
                                          if(AXIS2_FAILURE ==  status)
                                          {
					  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "failed in building element jobs ");
                                          }
                                          else
                                          {
                                            arr_list->push_back(element);
                                            
                                          }
                                        
                                     if(AXIS2_FAILURE ==  status)
                                     {
                                         WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "failed in setting the value for jobs ");
                                         if(element_qname)
                                         {
                                            axutil_qname_free(element_qname, Environment::getEnv());
                                         }
                                         if(arr_list)
                                         {
                                            delete arr_list;
                                         }
                                         return false;
                                     }

                                     i++;
                                    current_node = axiom_node_get_next_sibling(current_node, Environment::getEnv());
                                  }
                                  else
                                  {
                                      is_early_node_valid = false;
                                      sequence_broken = 1;
                                  }
                                  
                               }

                               
                                   if (i < 0)
                                   {
                                     /* found element out of order */
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"jobs (@minOccurs = '0') only have %d elements", i);
                                     if(element_qname)
                                     {
                                        axutil_qname_free(element_qname, Environment::getEnv());
                                     }
                                     if(arr_list)
                                     {
                                        delete arr_list;
                                     }
                                     return false;
                                   }
                               

                               if(0 == arr_list->size())
                               {
                                    delete arr_list;
                               }
                               else
                               {
                                    status = setJobs(arr_list);
                               }

                              
                            } 
                        
                  if(element_qname)
                  {
                     axutil_qname_free(element_qname, Environment::getEnv());
                     element_qname = NULL;
                  }
                 
          return status;
       }

          bool WSF_CALL
          AviaryQuery::GetJobDetailsResponse::isParticle()
          {
            
                 return false;
              
          }


          void WSF_CALL
          AviaryQuery::GetJobDetailsResponse::declareParentNamespaces(
                    axiom_element_t *parent_element,
                    axutil_hash_t *namespaces, int *next_ns_index)
          {
            
                  /* Here this is an empty function, Nothing to declare */
                 
          }

        
        
        axiom_node_t* WSF_CALL
	AviaryQuery::GetJobDetailsResponse::serialize(axiom_node_t *parent, 
			axiom_element_t *parent_element, 
			int parent_tag_closed, 
			axutil_hash_t *namespaces, 
			int *next_ns_index)
        {
            
            
         
         axiom_node_t *current_node = NULL;
         int tag_closed = 0;

         
         
                axiom_namespace_t *ns1 = NULL;

                axis2_char_t *qname_uri = NULL;
                axis2_char_t *qname_prefix = NULL;
                axis2_char_t *p_prefix = NULL;
            
               int i = 0;
               int count = 0;
               void *element = NULL;
             
                    axis2_char_t text_value_1[ADB_DEFAULT_DIGIT_LIMIT];
                    
               axis2_char_t *start_input_str = NULL;
               axis2_char_t *end_input_str = NULL;
               unsigned int start_input_str_len = 0;
               unsigned int end_input_str_len = 0;
            
            
               axiom_data_source_t *data_source = NULL;
               axutil_stream_t *stream = NULL;

             
                int next_ns_index_value = 0;
             
                    namespaces = axutil_hash_make(Environment::getEnv());
                    next_ns_index = &next_ns_index_value;
                     
                           ns1 = axiom_namespace_create (Environment::getEnv(),
                                             "http://query.aviary.grid.redhat.com",
                                             "n"); 
                           axutil_hash_set(namespaces, "http://query.aviary.grid.redhat.com", AXIS2_HASH_KEY_STRING, axutil_strdup(Environment::getEnv(), "n"));
                       
                     
                    parent_element = axiom_element_create (Environment::getEnv(), NULL, "GetJobDetailsResponse", ns1 , &parent);
                    
                    
                    axiom_element_set_namespace(parent_element, Environment::getEnv(), ns1, parent);


            
                    data_source = axiom_data_source_create(Environment::getEnv(), parent, &current_node);
                    stream = axiom_data_source_get_stream(data_source, Environment::getEnv());
                  
                       p_prefix = NULL;
                      

                   if (!isValidJobs)
                   {
                      
                           /* no need to complain for minoccurs=0 element */
                            
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("jobs"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("jobs")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     /*
                      * Parsing Jobs array
                      */
                     if (property_Jobs != NULL)
                     {
                        

                            sprintf(start_input_str, "<%s%sjobs",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                            
                         start_input_str_len = axutil_strlen(start_input_str);

                         sprintf(end_input_str, "</%s%sjobs>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                         end_input_str_len = axutil_strlen(end_input_str);

                         count = property_Jobs->size();
                         for(i = 0; i < count; i++)
                         {
                            AviaryCommon::JobDetails* element = (*property_Jobs)[i];

                            if(NULL == element) 
                            {
                                continue;
                            }

                    
                     
                     /*
                      * parsing jobs element
                      */

                    
                     
                            if(!element->isParticle())
                            {
                                axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                            }
                            element->serialize(current_node, parent_element,
                                                                                 element->isParticle() || false, namespaces, next_ns_index);
                            
                            if(!element->isParticle())
                            {
                                axutil_stream_write(stream, Environment::getEnv(), end_input_str, end_input_str_len);
                            }
                            
                         }
                     }
                   
                     
                     AXIS2_FREE(Environment::getEnv()->allocator,start_input_str);
                     AXIS2_FREE(Environment::getEnv()->allocator,end_input_str);
                 } 

                 
                   if(namespaces)
                   {
                       axutil_hash_index_t *hi;
                       void *val;
                       for (hi = axutil_hash_first(namespaces, Environment::getEnv()); hi; hi = axutil_hash_next(Environment::getEnv(), hi))
                       {
                           axutil_hash_this(hi, NULL, NULL, &val);
                           AXIS2_FREE(Environment::getEnv()->allocator, val);
                       }
                       axutil_hash_free(namespaces, Environment::getEnv());
                   }
                

            return parent;
        }


        

            /**
             * Getter for jobs by  Property Number 1
             */
            std::vector<AviaryCommon::JobDetails*>* WSF_CALL
            AviaryQuery::GetJobDetailsResponse::getProperty1()
            {
                return getJobs();
            }

            /**
             * getter for jobs.
             */
            std::vector<AviaryCommon::JobDetails*>* WSF_CALL
            AviaryQuery::GetJobDetailsResponse::getJobs()
             {
                return property_Jobs;
             }

            /**
             * setter for jobs
             */
            bool WSF_CALL
            AviaryQuery::GetJobDetailsResponse::setJobs(
                    std::vector<AviaryCommon::JobDetails*>*  arg_Jobs)
             {
                
                 int size = 0;
                 int i = 0;
                 bool non_nil_exists = false;
                

                if(isValidJobs &&
                        arg_Jobs == property_Jobs)
                {
                    
                    return true;
                }

                
                 size = arg_Jobs->size();
                 
                 if (size < 0)
                 {
                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"jobs has less than minOccurs(0)");
                     return false;
                 }
                 for(i = 0; i < size; i ++ )
                 {
                     if(NULL != (*arg_Jobs)[i])
                     {
                         non_nil_exists = true;
                         break;
                     }
                 }

                 

                
                resetJobs();

                
                    if(NULL == arg_Jobs)
                         
                {
                    /* We are already done */
                    return true;
                }
                
                        property_Jobs = arg_Jobs;
                        if(non_nil_exists)
                        {
                            isValidJobs = true;
                        }
                        
                    
                return true;
             }

            
            /**
             * Get ith element of jobs.
             */
            AviaryCommon::JobDetails* WSF_CALL
            AviaryQuery::GetJobDetailsResponse::getJobsAt(int i)
            {
                AviaryCommon::JobDetails* ret_val;
                if(property_Jobs == NULL)
                {
                    return (AviaryCommon::JobDetails*)0;
                }
                ret_val =   (*property_Jobs)[i];
                
                    return ret_val;
                  
            }

            /**
             * Set the ith element of jobs.
             */
           bool WSF_CALL
            AviaryQuery::GetJobDetailsResponse::setJobsAt(int i,
                    AviaryCommon::JobDetails* arg_Jobs)
            {
                 AviaryCommon::JobDetails* element;
                int size = 0;

                int non_nil_count;
                bool non_nil_exists = false;

                 

                if( isValidJobs &&
                    property_Jobs &&
                  
                    arg_Jobs == (*property_Jobs)[i])
                  
                 {
                    
                    return AXIS2_SUCCESS; 
                }

                   
                     non_nil_exists = true;
                  

                if(property_Jobs == NULL)
                {
                    property_Jobs = new std::vector<AviaryCommon::JobDetails*>();
                }
                else{
                /* check whether there already exist an element */
                element = (*property_Jobs)[i];
                }

                
                        if(NULL != element)
                        {
                          
                          
                          
                                delete element;
                             
                        }
                        
                    
                    if(!non_nil_exists)
                    {
                        
                        isValidJobs = true;
                        (*property_Jobs)[i]= NULL;
                        
                        return AXIS2_SUCCESS;
                    }
                
                    (*property_Jobs)[i] = arg_Jobs;
                  

               isValidJobs = true;
                
                return AXIS2_SUCCESS;
            }

            /**
             * Add to jobs.
             */
            bool WSF_CALL
            AviaryQuery::GetJobDetailsResponse::addJobs(
                    AviaryCommon::JobDetails* arg_Jobs)
             {

                
                    if( NULL == arg_Jobs
                     )
                    {
                      
                           return true; 
                        
                    }
                  

                if(property_Jobs == NULL)
                {
                    property_Jobs = new std::vector<AviaryCommon::JobDetails*>();
                }
              
               property_Jobs->push_back(arg_Jobs);
              
                isValidJobs = true;
                return true;
             }

            /**
             * Get the size of the jobs array.
             */
            int WSF_CALL
            AviaryQuery::GetJobDetailsResponse::sizeofJobs()
            {

                if(property_Jobs == NULL)
                {
                    return 0;
                }
                return property_Jobs->size();
            }

            /**
             * remove the ith element, same as set_nil_at.
             */
            bool WSF_CALL
            AviaryQuery::GetJobDetailsResponse::removeJobsAt(int i)
            {
                return setJobsNilAt(i);
            }

            

           /**
            * resetter for jobs
            */
           bool WSF_CALL
           AviaryQuery::GetJobDetailsResponse::resetJobs()
           {
               int i = 0;
               int count = 0;


               
                if (property_Jobs != NULL)
                {
                  std::vector<AviaryCommon::JobDetails*>::iterator it =  property_Jobs->begin();
                  for( ; it <  property_Jobs->end() ; ++it)
                  {
                     AviaryCommon::JobDetails* element = *it;
                
            
                

                if(element != NULL)
                {
                   
                   
                         delete  element;
                     

                   }

                
                
                
               }

             }
                
                    if(NULL != property_Jobs)
                 delete property_Jobs;
                
               isValidJobs = false; 
               return true;
           }

           /**
            * Check whether jobs is nill
            */
           bool WSF_CALL
           AviaryQuery::GetJobDetailsResponse::isJobsNil()
           {
               return !isValidJobs;
           }

           /**
            * Set jobs to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryQuery::GetJobDetailsResponse::setJobsNil()
           {
               return resetJobs();
           }

           
           /**
            * Check whether jobs is nill at i
            */
           bool WSF_CALL
           AviaryQuery::GetJobDetailsResponse::isJobsNilAt(int i)
           {
               return (isValidJobs == false ||
                       NULL == property_Jobs ||
                     NULL == (*property_Jobs)[i]);
            }

           /**
            * Set jobs to nil at i
            */
           bool WSF_CALL
           AviaryQuery::GetJobDetailsResponse::setJobsNilAt(int i)
           {
                int size = 0;
                int j;
                bool non_nil_exists = false;

                int k = 0;

                if(property_Jobs == NULL ||
                            isValidJobs == false)
                {
                    
                    non_nil_exists = false;
                }
                else
                {
                    size = property_Jobs->size();
                    for(j = 0, k = 0; j < size; j ++ )
                    {
                        if(i == j) continue; 
                        if(NULL != (*property_Jobs)[i])
                        {
                            k++;
                            non_nil_exists = true;
                            if( k >= 0)
                            {
                                break;
                            }
                        }
                    }
                }
                

                if( k < 0)
                {
                       WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "Size of the array of jobs is beinng set to be smaller than the specificed number of minOccurs(0)");
                       return AXIS2_FAILURE;
                }
 
                if(property_Jobs == NULL)
                {
                    isValidJobs = false;
                    
                    return true;
                }
                 
                 /* check whether there already exist an element */
                 AviaryCommon::JobDetails* element = (*property_Jobs)[i];
                if(NULL != element)
                {
                  
                  
                  
                        delete element;
                     
                 }
                 
                    if(!non_nil_exists)
                    {
                        
                        isValidJobs = false;
                        (*property_Jobs)[i] = NULL;
                        return AXIS2_SUCCESS;
                    }
                

                
                (*property_Jobs)[i] = NULL;
                
                return AXIS2_SUCCESS;

           }

           

