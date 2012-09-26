

        /**
         * LocateResponse.cpp
         *
         * This file was auto-generated from WSDL
         * by the Apache Axis2/C version: SNAPSHOT  Built on : Mar 10, 2008 (08:35:52 GMT+00:00)
         */
        
            #include "AviaryLocator_LocateResponse.h"
          

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
        
        using namespace AviaryLocator;
        
               /*
                * Implementation of the LocateResponse|http://locator.aviary.grid.redhat.com Element
                */
           AviaryLocator::LocateResponse::LocateResponse()
        {

        
            qname = NULL;
        
                property_Resources  = NULL;
              
            isValidResources  = false;
        
                property_Status  = NULL;
              
            isValidStatus  = false;
        
                  qname =  axutil_qname_create (Environment::getEnv(),
                        "LocateResponse",
                        "http://locator.aviary.grid.redhat.com",
                        NULL);
                
        }

       AviaryLocator::LocateResponse::LocateResponse(std::vector<AviaryCommon::ResourceLocation*>* arg_Resources,AviaryCommon::Status* arg_Status)
        {
             
                   qname = NULL;
             
               property_Resources  = NULL;
             
            isValidResources  = true;
            
               property_Status  = NULL;
             
            isValidStatus  = true;
            
                 qname =  axutil_qname_create (Environment::getEnv(),
                       "LocateResponse",
                       "http://locator.aviary.grid.redhat.com",
                       NULL);
               
                    property_Resources = arg_Resources;
            
                    property_Status = arg_Status;
            
        }
        AviaryLocator::LocateResponse::~LocateResponse()
        {
            resetAll();
        }

        bool WSF_CALL AviaryLocator::LocateResponse::resetAll()
        {
            //calls reset method for all the properties owned by this method which are pointers.

            
             resetResources();//AviaryCommon::ResourceLocation
             resetStatus();//AviaryCommon::Status
          if(qname != NULL)
          {
            axutil_qname_free( qname, Environment::getEnv());
            qname = NULL;
          }
        
            return true;

        }

        

        bool WSF_CALL
        AviaryLocator::LocateResponse::deserialize(axiom_node_t** dp_parent,bool *dp_is_early_node_valid, bool dont_care_minoccurs)
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
                              "Failed in building adb object for LocateResponse : "
                              "Expected %s but returned %s",
                              axutil_qname_to_string(qname, Environment::getEnv()),
                              axutil_qname_to_string(mqname, Environment::getEnv()));
                        
                        return AXIS2_FAILURE;
                    }
                    
                       { 
                    /*
                     * building Resources array
                     */
                       std::vector<AviaryCommon::ResourceLocation*>* arr_list =new std::vector<AviaryCommon::ResourceLocation*>();
                   

                     
                     /*
                      * building resources element
                      */
                     
                     
                     
                                    element_qname = axutil_qname_create(Environment::getEnv(), "resources", NULL, NULL);
                                  
                               
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

                                  if (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("resources", axiom_element_get_localname(current_element, Environment::getEnv())))
                                  {
                                  
                                      is_early_node_valid = true;
                                      
                                     AviaryCommon::ResourceLocation* element = new AviaryCommon::ResourceLocation();
                                          
                                          status =  element->deserialize(&current_node, &is_early_node_valid, false);
                                          
                                          if(AXIS2_FAILURE ==  status)
                                          {
					  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "failed in building element resources ");
                                          }
                                          else
                                          {
                                            arr_list->push_back(element);
                                            
                                          }
                                        
                                     if(AXIS2_FAILURE ==  status)
                                     {
                                         WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "failed in setting the value for resources ");
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
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"resources (@minOccurs = '0') only have %d elements", i);
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
                                    status = setResources(arr_list);
                               }

                              
                            } 
                        
                  if(element_qname)
                  {
                     axutil_qname_free(element_qname, Environment::getEnv());
                     element_qname = NULL;
                  }
                 

                     
                     /*
                      * building status element
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
                                 
                                 element_qname = axutil_qname_create(Environment::getEnv(), "status", NULL, NULL);
                                 

                           if (isParticle() ||  
                                (current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("status", axiom_element_get_localname(current_element, Environment::getEnv())))))
                           {
                              if( current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("status", axiom_element_get_localname(current_element, Environment::getEnv()))))
                              {
                                is_early_node_valid = true;
                              }
                              
                                 AviaryCommon::Status* element = new AviaryCommon::Status();

                                      status =  element->deserialize(&current_node, &is_early_node_valid, false);
                                      if(AXIS2_FAILURE == status)
                                      {
                                          WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "failed in building adb object for element status");
                                      }
                                      else
                                      {
                                          status = setStatus(element);
                                      }
                                    
                                 if(AXIS2_FAILURE ==  status)
                                 {
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"failed in setting the value for status ");
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
				  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "non nillable or minOuccrs != 0 element status missing");
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
          AviaryLocator::LocateResponse::isParticle()
          {
            
                 return false;
              
          }


          void WSF_CALL
          AviaryLocator::LocateResponse::declareParentNamespaces(
                    axiom_element_t *parent_element,
                    axutil_hash_t *namespaces, int *next_ns_index)
          {
            
                  /* Here this is an empty function, Nothing to declare */
                 
          }

        
        
        axiom_node_t* WSF_CALL
	AviaryLocator::LocateResponse::serialize(axiom_node_t *parent, 
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
                                             "http://locator.aviary.grid.redhat.com",
                                             "n"); 
                           axutil_hash_set(namespaces, "http://locator.aviary.grid.redhat.com", AXIS2_HASH_KEY_STRING, axutil_strdup(Environment::getEnv(), "n"));
                       
                     
                    parent_element = axiom_element_create (Environment::getEnv(), NULL, "LocateResponse", ns1 , &parent);
                    
                    
                    axiom_element_set_namespace(parent_element, Environment::getEnv(), ns1, parent);


            
                    data_source = axiom_data_source_create(Environment::getEnv(), parent, &current_node);
                    stream = axiom_data_source_get_stream(data_source, Environment::getEnv());
                  
                       p_prefix = NULL;
                      

                   if (!isValidResources)
                   {
                      
                           /* no need to complain for minoccurs=0 element */
                            
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("resources"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("resources")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     /*
                      * Parsing Resources array
                      */
                     if (property_Resources != NULL)
                     {
                        

                            sprintf(start_input_str, "<%s%sresources",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                            
                         start_input_str_len = axutil_strlen(start_input_str);

                         sprintf(end_input_str, "</%s%sresources>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                         end_input_str_len = axutil_strlen(end_input_str);

                         count = property_Resources->size();
                         for(i = 0; i < count; i++)
                         {
                            AviaryCommon::ResourceLocation* element = (*property_Resources)[i];

                            if(NULL == element) 
                            {
                                continue;
                            }

                    
                     
                     /*
                      * parsing resources element
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
                      

                   if (!isValidStatus)
                   {
                      
                            
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Nil value found in non-nillable property status");
                            return NULL;
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("status"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("status")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     
                     /*
                      * parsing status element
                      */

                    
                    
                            sprintf(start_input_str, "<%s%sstatus",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":""); 
                            
                        start_input_str_len = axutil_strlen(start_input_str);
                        sprintf(end_input_str, "</%s%sstatus>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                        end_input_str_len = axutil_strlen(end_input_str);
                     
                            if(!property_Status->isParticle())
                            {
                                axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                            }
                            property_Status->serialize(current_node, parent_element,
                                                                                 property_Status->isParticle() || false, namespaces, next_ns_index);
                            
                            if(!property_Status->isParticle())
                            {
                                axutil_stream_write(stream, Environment::getEnv(), end_input_str, end_input_str_len);
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
             * Getter for resources by  Property Number 1
             */
            std::vector<AviaryCommon::ResourceLocation*>* WSF_CALL
            AviaryLocator::LocateResponse::getProperty1()
            {
                return getResources();
            }

            /**
             * getter for resources.
             */
            std::vector<AviaryCommon::ResourceLocation*>* WSF_CALL
            AviaryLocator::LocateResponse::getResources()
             {
                return property_Resources;
             }

            /**
             * setter for resources
             */
            bool WSF_CALL
            AviaryLocator::LocateResponse::setResources(
                    std::vector<AviaryCommon::ResourceLocation*>*  arg_Resources)
             {
                
                 int size = 0;
                 int i = 0;
                 bool non_nil_exists = false;
                

                if(isValidResources &&
                        arg_Resources == property_Resources)
                {
                    
                    return true;
                }

                
                 size = arg_Resources->size();
                 
                 if (size < 0)
                 {
                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"resources has less than minOccurs(0)");
                     return false;
                 }
                 for(i = 0; i < size; i ++ )
                 {
                     if(NULL != (*arg_Resources)[i])
                     {
                         non_nil_exists = true;
                         break;
                     }
                 }

                 

                
                resetResources();

                
                    if(NULL == arg_Resources)
                         
                {
                    /* We are already done */
                    return true;
                }
                
                        property_Resources = arg_Resources;
                        if(non_nil_exists)
                        {
                            isValidResources = true;
                        }
                        
                    
                return true;
             }

            
            /**
             * Get ith element of resources.
             */
            AviaryCommon::ResourceLocation* WSF_CALL
            AviaryLocator::LocateResponse::getResourcesAt(int i)
            {
                AviaryCommon::ResourceLocation* ret_val;
                if(property_Resources == NULL)
                {
                    return (AviaryCommon::ResourceLocation*)0;
                }
                ret_val =   (*property_Resources)[i];
                
                    return ret_val;
                  
            }

            /**
             * Set the ith element of resources.
             */
           bool WSF_CALL
            AviaryLocator::LocateResponse::setResourcesAt(int i,
                    AviaryCommon::ResourceLocation* arg_Resources)
            {
                 AviaryCommon::ResourceLocation* element;
                int size = 0;

                int non_nil_count;
                bool non_nil_exists = false;

                 

                if( isValidResources &&
                    property_Resources &&
                  
                    arg_Resources == (*property_Resources)[i])
                  
                 {
                    
                    return AXIS2_SUCCESS; 
                }

                   
                     non_nil_exists = true;
                  

                if(property_Resources == NULL)
                {
                    property_Resources = new std::vector<AviaryCommon::ResourceLocation*>();
                }
                else{
                /* check whether there already exist an element */
                element = (*property_Resources)[i];
                }

                
                        if(NULL != element)
                        {
                          
                          
                          
                                delete element;
                             
                        }
                        
                    
                    if(!non_nil_exists)
                    {
                        
                        isValidResources = true;
                        (*property_Resources)[i]= NULL;
                        
                        return AXIS2_SUCCESS;
                    }
                
                    (*property_Resources)[i] = arg_Resources;
                  

               isValidResources = true;
                
                return AXIS2_SUCCESS;
            }

            /**
             * Add to resources.
             */
            bool WSF_CALL
            AviaryLocator::LocateResponse::addResources(
                    AviaryCommon::ResourceLocation* arg_Resources)
             {

                
                    if( NULL == arg_Resources
                     )
                    {
                      
                           return true; 
                        
                    }
                  

                if(property_Resources == NULL)
                {
                    property_Resources = new std::vector<AviaryCommon::ResourceLocation*>();
                }
              
               property_Resources->push_back(arg_Resources);
              
                isValidResources = true;
                return true;
             }

            /**
             * Get the size of the resources array.
             */
            int WSF_CALL
            AviaryLocator::LocateResponse::sizeofResources()
            {

                if(property_Resources == NULL)
                {
                    return 0;
                }
                return property_Resources->size();
            }

            /**
             * remove the ith element, same as set_nil_at.
             */
            bool WSF_CALL
            AviaryLocator::LocateResponse::removeResourcesAt(int i)
            {
                return setResourcesNilAt(i);
            }

            

           /**
            * resetter for resources
            */
           bool WSF_CALL
           AviaryLocator::LocateResponse::resetResources()
           {
               int i = 0;
               int count = 0;


               
                if (property_Resources != NULL)
                {
                  std::vector<AviaryCommon::ResourceLocation*>::iterator it =  property_Resources->begin();
                  for( ; it <  property_Resources->end() ; ++it)
                  {
                     AviaryCommon::ResourceLocation* element = *it;
                
            
                

                if(element != NULL)
                {
                   
                   
                         delete  element;
                     

                   }

                
                
                
               }

             }
                
                    if(NULL != property_Resources)
                 delete property_Resources;
                
               isValidResources = false; 
               return true;
           }

           /**
            * Check whether resources is nill
            */
           bool WSF_CALL
           AviaryLocator::LocateResponse::isResourcesNil()
           {
               return !isValidResources;
           }

           /**
            * Set resources to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryLocator::LocateResponse::setResourcesNil()
           {
               return resetResources();
           }

           
           /**
            * Check whether resources is nill at i
            */
           bool WSF_CALL
           AviaryLocator::LocateResponse::isResourcesNilAt(int i)
           {
               return (isValidResources == false ||
                       NULL == property_Resources ||
                     NULL == (*property_Resources)[i]);
            }

           /**
            * Set resources to nil at i
            */
           bool WSF_CALL
           AviaryLocator::LocateResponse::setResourcesNilAt(int i)
           {
                int size = 0;
                int j;
                bool non_nil_exists = false;

                int k = 0;

                if(property_Resources == NULL ||
                            isValidResources == false)
                {
                    
                    non_nil_exists = false;
                }
                else
                {
                    size = property_Resources->size();
                    for(j = 0, k = 0; j < size; j ++ )
                    {
                        if(i == j) continue; 
                        if(NULL != (*property_Resources)[i])
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
                       WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "Size of the array of resources is beinng set to be smaller than the specificed number of minOccurs(0)");
                       return AXIS2_FAILURE;
                }
 
                if(property_Resources == NULL)
                {
                    isValidResources = false;
                    
                    return true;
                }
                 
                 /* check whether there already exist an element */
                 AviaryCommon::ResourceLocation* element = (*property_Resources)[i];
                if(NULL != element)
                {
                  
                  
                  
                        delete element;
                     
                 }
                 
                    if(!non_nil_exists)
                    {
                        
                        isValidResources = false;
                        (*property_Resources)[i] = NULL;
                        return AXIS2_SUCCESS;
                    }
                

                
                (*property_Resources)[i] = NULL;
                
                return AXIS2_SUCCESS;

           }

           

            /**
             * Getter for status by  Property Number 2
             */
            AviaryCommon::Status* WSF_CALL
            AviaryLocator::LocateResponse::getProperty2()
            {
                return getStatus();
            }

            /**
             * getter for status.
             */
            AviaryCommon::Status* WSF_CALL
            AviaryLocator::LocateResponse::getStatus()
             {
                return property_Status;
             }

            /**
             * setter for status
             */
            bool WSF_CALL
            AviaryLocator::LocateResponse::setStatus(
                    AviaryCommon::Status*  arg_Status)
             {
                

                if(isValidStatus &&
                        arg_Status == property_Status)
                {
                    
                    return true;
                }

                
                  if(NULL == arg_Status)
                       
                  {
                      WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"status is being set to NULL, but it is not a nullable element");
                      return AXIS2_FAILURE;
                  }
                

                
                resetStatus();

                
                    if(NULL == arg_Status)
                         
                {
                    /* We are already done */
                    return true;
                }
                
                        property_Status = arg_Status;
                        isValidStatus = true;
                    
                return true;
             }

             

           /**
            * resetter for status
            */
           bool WSF_CALL
           AviaryLocator::LocateResponse::resetStatus()
           {
               int i = 0;
               int count = 0;


               
            
                

                if(property_Status != NULL)
                {
                   
                   
                         delete  property_Status;
                     

                   }

                
                
                
               isValidStatus = false; 
               return true;
           }

           /**
            * Check whether status is nill
            */
           bool WSF_CALL
           AviaryLocator::LocateResponse::isStatusNil()
           {
               return !isValidStatus;
           }

           /**
            * Set status to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryLocator::LocateResponse::setStatusNil()
           {
               return resetStatus();
           }

           

