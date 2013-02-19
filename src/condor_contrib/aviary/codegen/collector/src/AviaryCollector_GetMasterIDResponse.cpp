

        /**
         * GetMasterIDResponse.cpp
         *
         * This file was auto-generated from WSDL
         * by the Apache Axis2/C version: SNAPSHOT  Built on : Mar 10, 2008 (08:35:52 GMT+00:00)
         */
        
            #include "AviaryCollector_GetMasterIDResponse.h"
          

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
        
        using namespace AviaryCollector;
        
               /*
                * Implementation of the GetMasterIDResponse|http://collector.aviary.grid.redhat.com Element
                */
           AviaryCollector::GetMasterIDResponse::GetMasterIDResponse()
        {

        
            qname = NULL;
        
                property_Results  = NULL;
              
            isValidResults  = false;
        
            isValidRemaining  = false;
        
                  qname =  axutil_qname_create (Environment::getEnv(),
                        "GetMasterIDResponse",
                        "http://collector.aviary.grid.redhat.com",
                        NULL);
                
        }

       AviaryCollector::GetMasterIDResponse::GetMasterIDResponse(std::vector<AviaryCommon::Master*>* arg_Results,int arg_Remaining)
        {
             
                   qname = NULL;
             
               property_Results  = NULL;
             
            isValidResults  = true;
            
            isValidRemaining  = true;
            
                 qname =  axutil_qname_create (Environment::getEnv(),
                       "GetMasterIDResponse",
                       "http://collector.aviary.grid.redhat.com",
                       NULL);
               
                    property_Results = arg_Results;
            
                    property_Remaining = arg_Remaining;
            
        }
        AviaryCollector::GetMasterIDResponse::~GetMasterIDResponse()
        {
            resetAll();
        }

        bool WSF_CALL AviaryCollector::GetMasterIDResponse::resetAll()
        {
            //calls reset method for all the properties owned by this method which are pointers.

            
             resetResults();//AviaryCommon::Master
          if(qname != NULL)
          {
            axutil_qname_free( qname, Environment::getEnv());
            qname = NULL;
          }
        
            return true;

        }

        

        bool WSF_CALL
        AviaryCollector::GetMasterIDResponse::deserialize(axiom_node_t** dp_parent,bool *dp_is_early_node_valid, bool dont_care_minoccurs)
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
                              "Failed in building adb object for GetMasterIDResponse : "
                              "Expected %s but returned %s",
                              axutil_qname_to_string(qname, Environment::getEnv()),
                              axutil_qname_to_string(mqname, Environment::getEnv()));
                        
                        return AXIS2_FAILURE;
                    }
                    
                       { 
                    /*
                     * building Results array
                     */
                       std::vector<AviaryCommon::Master*>* arr_list =new std::vector<AviaryCommon::Master*>();
                   

                     
                     /*
                      * building results element
                      */
                     
                     
                     
                                    element_qname = axutil_qname_create(Environment::getEnv(), "results", NULL, NULL);
                                  
                               
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

                                  if (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("results", axiom_element_get_localname(current_element, Environment::getEnv())))
                                  {
                                  
                                      is_early_node_valid = true;
                                      
                                     AviaryCommon::Master* element = new AviaryCommon::Master();
                                          
                                          status =  element->deserialize(&current_node, &is_early_node_valid, false);
                                          
                                          if(AXIS2_FAILURE ==  status)
                                          {
					  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "failed in building element results ");
                                          }
                                          else
                                          {
                                            arr_list->push_back(element);
                                            
                                          }
                                        
                                     if(AXIS2_FAILURE ==  status)
                                     {
                                         WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "failed in setting the value for results ");
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
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"results (@minOccurs = '0') only have %d elements", i);
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
                                    status = setResults(arr_list);
                               }

                              
                            } 
                        
                  if(element_qname)
                  {
                     axutil_qname_free(element_qname, Environment::getEnv());
                     element_qname = NULL;
                  }
                 

                     
                     /*
                      * building remaining element
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
                                 
                                 element_qname = axutil_qname_create(Environment::getEnv(), "remaining", NULL, NULL);
                                 

                           if ( 
                                (current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("remaining", axiom_element_get_localname(current_element, Environment::getEnv())))))
                           {
                              if( current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("remaining", axiom_element_get_localname(current_element, Environment::getEnv()))))
                              {
                                is_early_node_valid = true;
                              }
                              
                                 
                                      text_value = axiom_element_get_text(current_element, Environment::getEnv(), current_node);
                                      if(text_value != NULL)
                                      {
                                            status = setRemaining(atoi(text_value));
                                      }
                                      
                                      else
                                      {
                                          WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "NULL value is set to a non nillable element remaining");
                                          status = AXIS2_FAILURE;
                                      }
                                      
                                 if(AXIS2_FAILURE ==  status)
                                 {
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"failed in setting the value for remaining ");
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
				  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "non nillable or minOuccrs != 0 element remaining missing");
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
          AviaryCollector::GetMasterIDResponse::isParticle()
          {
            
                 return false;
              
          }


          void WSF_CALL
          AviaryCollector::GetMasterIDResponse::declareParentNamespaces(
                    axiom_element_t *parent_element,
                    axutil_hash_t *namespaces, int *next_ns_index)
          {
            
                  /* Here this is an empty function, Nothing to declare */
                 
          }

        
        
        axiom_node_t* WSF_CALL
	AviaryCollector::GetMasterIDResponse::serialize(axiom_node_t *parent, 
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
                    
                    axis2_char_t text_value_2[ADB_DEFAULT_DIGIT_LIMIT];
                    
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
                                             "http://collector.aviary.grid.redhat.com",
                                             "n"); 
                           axutil_hash_set(namespaces, "http://collector.aviary.grid.redhat.com", AXIS2_HASH_KEY_STRING, axutil_strdup(Environment::getEnv(), "n"));
                       
                     
                    parent_element = axiom_element_create (Environment::getEnv(), NULL, "GetMasterIDResponse", ns1 , &parent);
                    
                    
                    axiom_element_set_namespace(parent_element, Environment::getEnv(), ns1, parent);


            
                    data_source = axiom_data_source_create(Environment::getEnv(), parent, &current_node);
                    stream = axiom_data_source_get_stream(data_source, Environment::getEnv());
                  
                       p_prefix = NULL;
                      

                   if (!isValidResults)
                   {
                      
                           /* no need to complain for minoccurs=0 element */
                            
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("results"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("results")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     /*
                      * Parsing Results array
                      */
                     if (property_Results != NULL)
                     {
                        

                            sprintf(start_input_str, "<%s%sresults",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                            
                         start_input_str_len = axutil_strlen(start_input_str);

                         sprintf(end_input_str, "</%s%sresults>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                         end_input_str_len = axutil_strlen(end_input_str);

                         count = property_Results->size();
                         for(i = 0; i < count; i++)
                         {
                            AviaryCommon::Master* element = (*property_Results)[i];

                            if(NULL == element) 
                            {
                                continue;
                            }

                    
                     
                     /*
                      * parsing results element
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

                 
                       p_prefix = NULL;
                      

                   if (!isValidRemaining)
                   {
                      
                            
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Nil value found in non-nillable property remaining");
                            return NULL;
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("remaining"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("remaining")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     
                     /*
                      * parsing remaining element
                      */

                    
                    
                            sprintf(start_input_str, "<%s%sremaining>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                            
                        start_input_str_len = axutil_strlen(start_input_str);
                        sprintf(end_input_str, "</%s%sremaining>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                        end_input_str_len = axutil_strlen(end_input_str);
                    
                               sprintf (text_value_2, AXIS2_PRINTF_INT32_FORMAT_SPECIFIER, property_Remaining);
                             
                           axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                           
                           axutil_stream_write(stream, Environment::getEnv(), text_value_2, axutil_strlen(text_value_2));
                           
                           axutil_stream_write(stream, Environment::getEnv(), end_input_str, end_input_str_len);
                           
                     
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
             * Getter for results by  Property Number 1
             */
            std::vector<AviaryCommon::Master*>* WSF_CALL
            AviaryCollector::GetMasterIDResponse::getProperty1()
            {
                return getResults();
            }

            /**
             * getter for results.
             */
            std::vector<AviaryCommon::Master*>* WSF_CALL
            AviaryCollector::GetMasterIDResponse::getResults()
             {
                return property_Results;
             }

            /**
             * setter for results
             */
            bool WSF_CALL
            AviaryCollector::GetMasterIDResponse::setResults(
                    std::vector<AviaryCommon::Master*>*  arg_Results)
             {
                
                 int size = 0;
                 int i = 0;
                 bool non_nil_exists = false;
                

                if(isValidResults &&
                        arg_Results == property_Results)
                {
                    
                    return true;
                }

                
                 size = arg_Results->size();
                 
                 if (size < 0)
                 {
                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"results has less than minOccurs(0)");
                     return false;
                 }
                 for(i = 0; i < size; i ++ )
                 {
                     if(NULL != (*arg_Results)[i])
                     {
                         non_nil_exists = true;
                         break;
                     }
                 }

                 

                
                resetResults();

                
                    if(NULL == arg_Results)
                         
                {
                    /* We are already done */
                    return true;
                }
                
                        property_Results = arg_Results;
                        if(non_nil_exists)
                        {
                            isValidResults = true;
                        }
                        
                    
                return true;
             }

            
            /**
             * Get ith element of results.
             */
            AviaryCommon::Master* WSF_CALL
            AviaryCollector::GetMasterIDResponse::getResultsAt(int i)
            {
                AviaryCommon::Master* ret_val;
                if(property_Results == NULL)
                {
                    return (AviaryCommon::Master*)0;
                }
                ret_val =   (*property_Results)[i];
                
                    return ret_val;
                  
            }

            /**
             * Set the ith element of results.
             */
           bool WSF_CALL
            AviaryCollector::GetMasterIDResponse::setResultsAt(int i,
                    AviaryCommon::Master* arg_Results)
            {
                 AviaryCommon::Master* element;
                int size = 0;

                int non_nil_count;
                bool non_nil_exists = false;

                 

                if( isValidResults &&
                    property_Results &&
                  
                    arg_Results == (*property_Results)[i])
                  
                 {
                    
                    return AXIS2_SUCCESS; 
                }

                   
                     non_nil_exists = true;
                  

                if(property_Results == NULL)
                {
                    property_Results = new std::vector<AviaryCommon::Master*>();
                }
                else{
                /* check whether there already exist an element */
                element = (*property_Results)[i];
                }

                
                        if(NULL != element)
                        {
                          
                          
                          
                                delete element;
                             
                        }
                        
                    
                    if(!non_nil_exists)
                    {
                        
                        isValidResults = true;
                        (*property_Results)[i]= NULL;
                        
                        return AXIS2_SUCCESS;
                    }
                
                    (*property_Results)[i] = arg_Results;
                  

               isValidResults = true;
                
                return AXIS2_SUCCESS;
            }

            /**
             * Add to results.
             */
            bool WSF_CALL
            AviaryCollector::GetMasterIDResponse::addResults(
                    AviaryCommon::Master* arg_Results)
             {

                
                    if( NULL == arg_Results
                     )
                    {
                      
                           return true; 
                        
                    }
                  

                if(property_Results == NULL)
                {
                    property_Results = new std::vector<AviaryCommon::Master*>();
                }
              
               property_Results->push_back(arg_Results);
              
                isValidResults = true;
                return true;
             }

            /**
             * Get the size of the results array.
             */
            int WSF_CALL
            AviaryCollector::GetMasterIDResponse::sizeofResults()
            {

                if(property_Results == NULL)
                {
                    return 0;
                }
                return property_Results->size();
            }

            /**
             * remove the ith element, same as set_nil_at.
             */
            bool WSF_CALL
            AviaryCollector::GetMasterIDResponse::removeResultsAt(int i)
            {
                return setResultsNilAt(i);
            }

            

           /**
            * resetter for results
            */
           bool WSF_CALL
           AviaryCollector::GetMasterIDResponse::resetResults()
           {
               int i = 0;
               int count = 0;


               
                if (property_Results != NULL)
                {
                  std::vector<AviaryCommon::Master*>::iterator it =  property_Results->begin();
                  for( ; it <  property_Results->end() ; ++it)
                  {
                     AviaryCommon::Master* element = *it;
                
            
                

                if(element != NULL)
                {
                   
                   
                         delete  element;
                     

                   }

                
                
                
               }

             }
                
                    if(NULL != property_Results)
                 delete property_Results;
                
               isValidResults = false; 
               return true;
           }

           /**
            * Check whether results is nill
            */
           bool WSF_CALL
           AviaryCollector::GetMasterIDResponse::isResultsNil()
           {
               return !isValidResults;
           }

           /**
            * Set results to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCollector::GetMasterIDResponse::setResultsNil()
           {
               return resetResults();
           }

           
           /**
            * Check whether results is nill at i
            */
           bool WSF_CALL
           AviaryCollector::GetMasterIDResponse::isResultsNilAt(int i)
           {
               return (isValidResults == false ||
                       NULL == property_Results ||
                     NULL == (*property_Results)[i]);
            }

           /**
            * Set results to nil at i
            */
           bool WSF_CALL
           AviaryCollector::GetMasterIDResponse::setResultsNilAt(int i)
           {
                int size = 0;
                int j;
                bool non_nil_exists = false;

                int k = 0;

                if(property_Results == NULL ||
                            isValidResults == false)
                {
                    
                    non_nil_exists = false;
                }
                else
                {
                    size = property_Results->size();
                    for(j = 0, k = 0; j < size; j ++ )
                    {
                        if(i == j) continue; 
                        if(NULL != (*property_Results)[i])
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
                       WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "Size of the array of results is beinng set to be smaller than the specificed number of minOccurs(0)");
                       return AXIS2_FAILURE;
                }
 
                if(property_Results == NULL)
                {
                    isValidResults = false;
                    
                    return true;
                }
                 
                 /* check whether there already exist an element */
                 AviaryCommon::Master* element = (*property_Results)[i];
                if(NULL != element)
                {
                  
                  
                  
                        delete element;
                     
                 }
                 
                    if(!non_nil_exists)
                    {
                        
                        isValidResults = false;
                        (*property_Results)[i] = NULL;
                        return AXIS2_SUCCESS;
                    }
                

                
                (*property_Results)[i] = NULL;
                
                return AXIS2_SUCCESS;

           }

           

            /**
             * Getter for remaining by  Property Number 2
             */
            int WSF_CALL
            AviaryCollector::GetMasterIDResponse::getProperty2()
            {
                return getRemaining();
            }

            /**
             * getter for remaining.
             */
            int WSF_CALL
            AviaryCollector::GetMasterIDResponse::getRemaining()
             {
                return property_Remaining;
             }

            /**
             * setter for remaining
             */
            bool WSF_CALL
            AviaryCollector::GetMasterIDResponse::setRemaining(
                    const int  arg_Remaining)
             {
                

                if(isValidRemaining &&
                        arg_Remaining == property_Remaining)
                {
                    
                    return true;
                }

                

                
                resetRemaining();

                
                        property_Remaining = arg_Remaining;
                        isValidRemaining = true;
                    
                return true;
             }

             

           /**
            * resetter for remaining
            */
           bool WSF_CALL
           AviaryCollector::GetMasterIDResponse::resetRemaining()
           {
               int i = 0;
               int count = 0;


               
               isValidRemaining = false; 
               return true;
           }

           /**
            * Check whether remaining is nill
            */
           bool WSF_CALL
           AviaryCollector::GetMasterIDResponse::isRemainingNil()
           {
               return !isValidRemaining;
           }

           /**
            * Set remaining to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCollector::GetMasterIDResponse::setRemainingNil()
           {
               return resetRemaining();
           }

           

