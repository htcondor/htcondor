

        /**
         * GetCollector.cpp
         *
         * This file was auto-generated from WSDL
         * by the Apache Axis2/C version: SNAPSHOT  Built on : Mar 10, 2008 (08:35:52 GMT+00:00)
         */
        
            #include "AviaryCollector_GetCollector.h"
          

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
                * Implementation of the GetCollector|http://collector.aviary.grid.redhat.com Element
                */
           AviaryCollector::GetCollector::GetCollector()
        {

        
            qname = NULL;
        
                property_Ids  = NULL;
              
            isValidIds  = false;
        
            isValidPartialMatches  = false;
        
            isValidIncludeSummaries  = false;
        
                  qname =  axutil_qname_create (Environment::getEnv(),
                        "GetCollector",
                        "http://collector.aviary.grid.redhat.com",
                        NULL);
                
        }

       AviaryCollector::GetCollector::GetCollector(std::vector<std::string*>* arg_Ids,bool arg_PartialMatches,bool arg_IncludeSummaries)
        {
             
                   qname = NULL;
             
               property_Ids  = NULL;
             
            isValidIds  = true;
            
            isValidPartialMatches  = true;
            
            isValidIncludeSummaries  = true;
            
                 qname =  axutil_qname_create (Environment::getEnv(),
                       "GetCollector",
                       "http://collector.aviary.grid.redhat.com",
                       NULL);
               
                    property_Ids = arg_Ids;
            
                    property_PartialMatches = arg_PartialMatches;
            
                    property_IncludeSummaries = arg_IncludeSummaries;
            
        }
        AviaryCollector::GetCollector::~GetCollector()
        {
            resetAll();
        }

        bool WSF_CALL AviaryCollector::GetCollector::resetAll()
        {
            //calls reset method for all the properties owned by this method which are pointers.

            
             resetIds();//std::string
          if(qname != NULL)
          {
            axutil_qname_free( qname, Environment::getEnv());
            qname = NULL;
          }
        
            return true;

        }

        

        bool WSF_CALL
        AviaryCollector::GetCollector::deserialize(axiom_node_t** dp_parent,bool *dp_is_early_node_valid, bool dont_care_minoccurs)
        {
          axiom_node_t *parent = *dp_parent;
          
          bool status = AXIS2_SUCCESS;
          
          axiom_attribute_t *parent_attri = NULL;
          axiom_element_t *parent_element = NULL;
          axis2_char_t *attrib_text = NULL;

          axutil_hash_t *attribute_hash = NULL;

           
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
                              "Failed in building adb object for GetCollector : "
                              "Expected %s but returned %s",
                              axutil_qname_to_string(qname, Environment::getEnv()),
                              axutil_qname_to_string(mqname, Environment::getEnv()));
                        
                        return AXIS2_FAILURE;
                    }
                    
                 parent_element = (axiom_element_t *)axiom_node_get_data_element(parent, Environment::getEnv());
                 attribute_hash = axiom_element_get_all_attributes(parent_element, Environment::getEnv());
              
                       { 
                    /*
                     * building Ids array
                     */
                       std::vector<std::string*>* arr_list =new std::vector<std::string*>();
                   

                     
                     /*
                      * building ids element
                      */
                     
                     
                     
                                    element_qname = axutil_qname_create(Environment::getEnv(), "ids", NULL, NULL);
                                  
                               
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

                                  if (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("ids", axiom_element_get_localname(current_element, Environment::getEnv())))
                                  {
                                  
                                      is_early_node_valid = true;
                                      
                                     
                                          text_value = axiom_element_get_text(current_element, Environment::getEnv(), current_node);
                                          if(text_value != NULL)
                                          {
                                              arr_list->push_back(new string(text_value));
                                          }
                                          
                                          else
                                          {
                                              /*
                                               * axis2_qname_t *qname = NULL;
                                               * axiom_attribute_t *the_attri = NULL;
                                               * 
                                               * qname = axutil_qname_create(Environment::getEnv(), "nil", "http://www.w3.org/2001/XMLSchema-instance", "xsi");
                                               * the_attri = axiom_element_get_attribute(current_element, Environment::getEnv(), qname);
                                               */
                                           
                                              /* currently thereis a bug in the axiom_element_get_attribute, so we have to go to this bad method */
                                             
                                              axiom_attribute_t *the_attri = NULL;
                                              axis2_char_t *attrib_text = NULL;
                                              axutil_hash_t *attribute_hash = NULL;
                                             
                                              attribute_hash = axiom_element_get_all_attributes(current_element, Environment::getEnv());
                                             
                                              attrib_text = NULL;
                                              if(attribute_hash)
                                              {
                                                   axutil_hash_index_t *hi;
                                                   void *val;
                                                   const void *key;
                                             
                                                   for (hi = axutil_hash_first(attribute_hash, Environment::getEnv()); hi; hi = axutil_hash_next(Environment::getEnv(), hi))
                                                   {
                                                       axutil_hash_this(hi, &key, NULL, &val);
                                                       
                                                       if(strstr((axis2_char_t*)key, "nil|http://www.w3.org/2001/XMLSchema-instance"))
                                                       {
                                                           the_attri = (axiom_attribute_t*)val;
                                                           break;
                                                       }
                                                   }
                                              }
                                             
                                              if(the_attri)
                                              {
                                                  attrib_text = axiom_attribute_get_value(the_attri, Environment::getEnv());
                                              }
                                              else
                                              {
                                                  /* this is hoping that attribute is stored in "http://www.w3.org/2001/XMLSchema-instance", this happnes when name is in default namespace */
                                                  attrib_text = axiom_element_get_attribute_value_by_name(current_element, Environment::getEnv(), "nil");
                                              }
                                             
                                              if(attrib_text && 0 == axutil_strcmp(attrib_text, "1"))
                                              {
					      WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI,"NULL value is set to a non nillable element ids");
                                                  status = AXIS2_FAILURE;
                                              }
                                              else
                                              {
                                                  /* after all, we found this is a empty string */
                                                  arr_list->push_back(new string(""));
                                              }
                                          }
                                          
                                     if(AXIS2_FAILURE ==  status)
                                     {
                                         WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "failed in setting the value for ids ");
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
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"ids (@minOccurs = '0') only have %d elements", i);
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
                                    status = setIds(arr_list);
                               }

                              
                            } 
                        
                  if(element_qname)
                  {
                     axutil_qname_free(element_qname, Environment::getEnv());
                     element_qname = NULL;
                  }
                 
                
                
                  parent_attri = NULL;
                  attrib_text = NULL;
                  if(attribute_hash)
                  {
                       axutil_hash_index_t *hi;
                       void *val;
                       const void *key;

                       for (hi = axutil_hash_first(attribute_hash, Environment::getEnv()); hi; hi = axutil_hash_next(Environment::getEnv(), hi))
                       {
                           axutil_hash_this(hi, &key, NULL, &val);
                           
                           
                               if(!strcmp((axis2_char_t*)key, "partialMatches"))
                             
                               {
                                   parent_attri = (axiom_attribute_t*)val;
                                   break;
                               }
                       }
                  }

                  if(parent_attri)
                  {
                    attrib_text = axiom_attribute_get_value(parent_attri, Environment::getEnv());
                  }
                  else
                  {
                    /* this is hoping that attribute is stored in "partialMatches", this happnes when name is in default namespace */
                    attrib_text = axiom_element_get_attribute_value_by_name(parent_element, Environment::getEnv(), "partialMatches");
                  }

                  if(attrib_text != NULL)
                  {
                      
                      
                           if (!axutil_strcmp(attrib_text, "TRUE") || !axutil_strcmp(attrib_text, "true"))
                           {
                               setPartialMatches(true);
                           }
                           else
                           {
                               setPartialMatches(false);
                           }
                        
                    }
                  
                  if(element_qname)
                  {
                     axutil_qname_free(element_qname, Environment::getEnv());
                     element_qname = NULL;
                  }
                 
                
                
                  parent_attri = NULL;
                  attrib_text = NULL;
                  if(attribute_hash)
                  {
                       axutil_hash_index_t *hi;
                       void *val;
                       const void *key;

                       for (hi = axutil_hash_first(attribute_hash, Environment::getEnv()); hi; hi = axutil_hash_next(Environment::getEnv(), hi))
                       {
                           axutil_hash_this(hi, &key, NULL, &val);
                           
                           
                               if(!strcmp((axis2_char_t*)key, "includeSummaries"))
                             
                               {
                                   parent_attri = (axiom_attribute_t*)val;
                                   break;
                               }
                       }
                  }

                  if(parent_attri)
                  {
                    attrib_text = axiom_attribute_get_value(parent_attri, Environment::getEnv());
                  }
                  else
                  {
                    /* this is hoping that attribute is stored in "includeSummaries", this happnes when name is in default namespace */
                    attrib_text = axiom_element_get_attribute_value_by_name(parent_element, Environment::getEnv(), "includeSummaries");
                  }

                  if(attrib_text != NULL)
                  {
                      
                      
                           if (!axutil_strcmp(attrib_text, "TRUE") || !axutil_strcmp(attrib_text, "true"))
                           {
                               setIncludeSummaries(true);
                           }
                           else
                           {
                               setIncludeSummaries(false);
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
          AviaryCollector::GetCollector::isParticle()
          {
            
                 return false;
              
          }


          void WSF_CALL
          AviaryCollector::GetCollector::declareParentNamespaces(
                    axiom_element_t *parent_element,
                    axutil_hash_t *namespaces, int *next_ns_index)
          {
            
                  /* Here this is an empty function, Nothing to declare */
                 
          }

        
        
        axiom_node_t* WSF_CALL
	AviaryCollector::GetCollector::serialize(axiom_node_t *parent, 
			axiom_element_t *parent_element, 
			int parent_tag_closed, 
			axutil_hash_t *namespaces, 
			int *next_ns_index)
        {
            
            
               axiom_attribute_t *text_attri = NULL;
             
             axis2_char_t *string_to_stream;
            
         
         axiom_node_t *current_node = NULL;
         int tag_closed = 0;

         
         
                axiom_namespace_t *ns1 = NULL;

                axis2_char_t *qname_uri = NULL;
                axis2_char_t *qname_prefix = NULL;
                axis2_char_t *p_prefix = NULL;
            
               int i = 0;
               int count = 0;
               void *element = NULL;
             
                    axis2_char_t *text_value_1;
                    axis2_char_t *text_value_1_temp;
                    
                    axis2_char_t text_value_2[ADB_DEFAULT_DIGIT_LIMIT];
                    
                    axis2_char_t text_value_3[ADB_DEFAULT_DIGIT_LIMIT];
                    
                axis2_char_t *text_value = NULL;
             
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
                       
                     
                    parent_element = axiom_element_create (Environment::getEnv(), NULL, "GetCollector", ns1 , &parent);
                    
                    
                    axiom_element_set_namespace(parent_element, Environment::getEnv(), ns1, parent);


            
                    data_source = axiom_data_source_create(Environment::getEnv(), parent, &current_node);
                    stream = axiom_data_source_get_stream(data_source, Environment::getEnv());
                  
            if(!parent_tag_closed)
            {
            
                if(isValidPartialMatches)
                {
                
                        p_prefix = NULL;
                      
                           
                           text_value = (axis2_char_t*)((property_PartialMatches)?"true":"false");
                           string_to_stream = (axis2_char_t*) AXIS2_MALLOC (Environment::getEnv()-> allocator, sizeof (axis2_char_t) *
                                                            (5  + ADB_DEFAULT_NAMESPACE_PREFIX_LIMIT +
                                                             axutil_strlen(text_value) + 
                                                             axutil_strlen("partialMatches")));
                           sprintf(string_to_stream, " %s%s%s=\"%s\"", p_prefix?p_prefix:"", (p_prefix && axutil_strcmp(p_prefix, ""))?":":"",
                                                "partialMatches",  text_value);
                           axutil_stream_write(stream, Environment::getEnv(), string_to_stream, axutil_strlen(string_to_stream));
                           AXIS2_FREE(Environment::getEnv()-> allocator, string_to_stream);
                        
                   }
                   
                if(isValidIncludeSummaries)
                {
                
                        p_prefix = NULL;
                      
                           
                           text_value = (axis2_char_t*)((property_IncludeSummaries)?"true":"false");
                           string_to_stream = (axis2_char_t*) AXIS2_MALLOC (Environment::getEnv()-> allocator, sizeof (axis2_char_t) *
                                                            (5  + ADB_DEFAULT_NAMESPACE_PREFIX_LIMIT +
                                                             axutil_strlen(text_value) + 
                                                             axutil_strlen("includeSummaries")));
                           sprintf(string_to_stream, " %s%s%s=\"%s\"", p_prefix?p_prefix:"", (p_prefix && axutil_strcmp(p_prefix, ""))?":":"",
                                                "includeSummaries",  text_value);
                           axutil_stream_write(stream, Environment::getEnv(), string_to_stream, axutil_strlen(string_to_stream));
                           AXIS2_FREE(Environment::getEnv()-> allocator, string_to_stream);
                        
                   }
                   
            }
            
                       p_prefix = NULL;
                      

                   if (!isValidIds)
                   {
                      
                           /* no need to complain for minoccurs=0 element */
                            
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("ids"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("ids")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     /*
                      * Parsing Ids array
                      */
                     if (property_Ids != NULL)
                     {
                        
                            sprintf(start_input_str, "<%s%sids>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":""); 
                            
                         start_input_str_len = axutil_strlen(start_input_str);

                         sprintf(end_input_str, "</%s%sids>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                         end_input_str_len = axutil_strlen(end_input_str);

                         count = property_Ids->size();
                         for(i = 0; i < count; i++)
                         {
                            std::string* element = (*property_Ids)[i];

                            if(NULL == element) 
                            {
                                continue;
                            }

                    
                     
                     /*
                      * parsing ids element
                      */

                    
                    
                           text_value_1 = (axis2_char_t*)(*element).c_str();
                           
                           axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                           
                            
                           text_value_1_temp = axutil_xml_quote_string(Environment::getEnv(), text_value_1, true);
                           if (text_value_1_temp)
                           {
                               axutil_stream_write(stream, Environment::getEnv(), text_value_1_temp, axutil_strlen(text_value_1_temp));
                               AXIS2_FREE(Environment::getEnv()->allocator, text_value_1_temp);
                           }
                           else
                           {
                               axutil_stream_write(stream, Environment::getEnv(), text_value_1, axutil_strlen(text_value_1));
                           }
                           
                           axutil_stream_write(stream, Environment::getEnv(), end_input_str, end_input_str_len);
                           
                         }
                     }
                   
                     
                     AXIS2_FREE(Environment::getEnv()->allocator,start_input_str);
                     AXIS2_FREE(Environment::getEnv()->allocator,end_input_str);
                 } 

                 
                    
                    if(parent_tag_closed)
                    {
                       if(isValidPartialMatches)
                       {
                       
                           p_prefix = NULL;
                           ns1 = NULL;
                         
                           
                           text_value =  (axis2_char_t*)((property_PartialMatches)?axutil_strdup(Environment::getEnv(), "true"):axutil_strdup(Environment::getEnv(), "false"));
                           text_attri = axiom_attribute_create (Environment::getEnv(), "partialMatches", text_value, ns1);
                           axiom_element_add_attribute (parent_element, Environment::getEnv(), text_attri, parent);
                           AXIS2_FREE(Environment::getEnv()->allocator, text_value);
                        
                      }
                       
                  }
                
                    
                    if(parent_tag_closed)
                    {
                       if(isValidIncludeSummaries)
                       {
                       
                           p_prefix = NULL;
                           ns1 = NULL;
                         
                           
                           text_value =  (axis2_char_t*)((property_IncludeSummaries)?axutil_strdup(Environment::getEnv(), "true"):axutil_strdup(Environment::getEnv(), "false"));
                           text_attri = axiom_attribute_create (Environment::getEnv(), "includeSummaries", text_value, ns1);
                           axiom_element_add_attribute (parent_element, Environment::getEnv(), text_attri, parent);
                           AXIS2_FREE(Environment::getEnv()->allocator, text_value);
                        
                      }
                       
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
             * Getter for ids by  Property Number 1
             */
            std::vector<std::string*>* WSF_CALL
            AviaryCollector::GetCollector::getProperty1()
            {
                return getIds();
            }

            /**
             * getter for ids.
             */
            std::vector<std::string*>* WSF_CALL
            AviaryCollector::GetCollector::getIds()
             {
                return property_Ids;
             }

            /**
             * setter for ids
             */
            bool WSF_CALL
            AviaryCollector::GetCollector::setIds(
                    std::vector<std::string*>*  arg_Ids)
             {
                
                 int size = 0;
                 int i = 0;
                 bool non_nil_exists = false;
                

                if(isValidIds &&
                        arg_Ids == property_Ids)
                {
                    
                    return true;
                }

                
                 size = arg_Ids->size();
                 
                 if (size < 0)
                 {
                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"ids has less than minOccurs(0)");
                     return false;
                 }
                 for(i = 0; i < size; i ++ )
                 {
                     if(NULL != (*arg_Ids)[i])
                     {
                         non_nil_exists = true;
                         break;
                     }
                 }

                 

                
                resetIds();

                
                    if(NULL == arg_Ids)
                         
                {
                    /* We are already done */
                    return true;
                }
                
                        property_Ids = arg_Ids;
                        if(non_nil_exists)
                        {
                            isValidIds = true;
                        }
                        
                    
                return true;
             }

            
            /**
             * Get ith element of ids.
             */
            std::string WSF_CALL
            AviaryCollector::GetCollector::getIdsAt(int i)
            {
                std::string* ret_val;
                if(property_Ids == NULL)
                {
                    return (std::string)0;
                }
                ret_val =   (*property_Ids)[i];
                
                    if(ret_val)
                    {
                        return *ret_val;
                    }
                    return (std::string)0;
                  
            }

            /**
             * Set the ith element of ids.
             */
           bool WSF_CALL
            AviaryCollector::GetCollector::setIdsAt(int i,
                    const std::string arg_Ids)
            {
                 std::string* element;
                int size = 0;

                int non_nil_count;
                bool non_nil_exists = false;

                 

                if( isValidIds &&
                    property_Ids &&
                  
                    arg_Ids == *((*property_Ids)[i]))
                  
                 {
                    
                    return AXIS2_SUCCESS; 
                }

                   
                     non_nil_exists = true;
                  

                if(property_Ids == NULL)
                {
                    property_Ids = new std::vector<std::string*>();
                }
                else{
                /* check whether there already exist an element */
                element = (*property_Ids)[i];
                }

                
                    if(!non_nil_exists)
                    {
                        
                        isValidIds = true;
                        (*property_Ids)[i]= NULL;
                        
                        return AXIS2_SUCCESS;
                    }
                
                    (*property_Ids)[i]= new string(arg_Ids.c_str());
                  

               isValidIds = true;
                
                return AXIS2_SUCCESS;
            }

            /**
             * Add to ids.
             */
            bool WSF_CALL
            AviaryCollector::GetCollector::addIds(
                    const std::string arg_Ids)
             {

                
                    if(
                      arg_Ids.empty()
                       
                     )
                    {
                      
                           return true; 
                        
                    }
                  

                if(property_Ids == NULL)
                {
                    property_Ids = new std::vector<std::string*>();
                }
              
               property_Ids->push_back(new string(arg_Ids.c_str()));
              
                isValidIds = true;
                return true;
             }

            /**
             * Get the size of the ids array.
             */
            int WSF_CALL
            AviaryCollector::GetCollector::sizeofIds()
            {

                if(property_Ids == NULL)
                {
                    return 0;
                }
                return property_Ids->size();
            }

            /**
             * remove the ith element, same as set_nil_at.
             */
            bool WSF_CALL
            AviaryCollector::GetCollector::removeIdsAt(int i)
            {
                return setIdsNilAt(i);
            }

            

           /**
            * resetter for ids
            */
           bool WSF_CALL
           AviaryCollector::GetCollector::resetIds()
           {
               int i = 0;
               int count = 0;


               
                    if(NULL != property_Ids)
                 delete property_Ids;
                
               isValidIds = false; 
               return true;
           }

           /**
            * Check whether ids is nill
            */
           bool WSF_CALL
           AviaryCollector::GetCollector::isIdsNil()
           {
               return !isValidIds;
           }

           /**
            * Set ids to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCollector::GetCollector::setIdsNil()
           {
               return resetIds();
           }

           
           /**
            * Check whether ids is nill at i
            */
           bool WSF_CALL
           AviaryCollector::GetCollector::isIdsNilAt(int i)
           {
               return (isValidIds == false ||
                       NULL == property_Ids ||
                     NULL == (*property_Ids)[i]);
            }

           /**
            * Set ids to nil at i
            */
           bool WSF_CALL
           AviaryCollector::GetCollector::setIdsNilAt(int i)
           {
                int size = 0;
                int j;
                bool non_nil_exists = false;

                int k = 0;

                if(property_Ids == NULL ||
                            isValidIds == false)
                {
                    
                    non_nil_exists = false;
                }
                else
                {
                    size = property_Ids->size();
                    for(j = 0, k = 0; j < size; j ++ )
                    {
                        if(i == j) continue; 
                        if(NULL != (*property_Ids)[i])
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
                       WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "Size of the array of ids is beinng set to be smaller than the specificed number of minOccurs(0)");
                       return AXIS2_FAILURE;
                }
 
                if(property_Ids == NULL)
                {
                    isValidIds = false;
                    
                    return true;
                }
                 
                    if(!non_nil_exists)
                    {
                        
                        isValidIds = false;
                        (*property_Ids)[i] = NULL;
                        return AXIS2_SUCCESS;
                    }
                

                
                (*property_Ids)[i] = NULL;
                
                return AXIS2_SUCCESS;

           }

           

            /**
             * Getter for partialMatches by  Property Number 2
             */
            bool WSF_CALL
            AviaryCollector::GetCollector::getProperty2()
            {
                return getPartialMatches();
            }

            /**
             * getter for partialMatches.
             */
            bool WSF_CALL
            AviaryCollector::GetCollector::getPartialMatches()
             {
                return property_PartialMatches;
             }

            /**
             * setter for partialMatches
             */
            bool WSF_CALL
            AviaryCollector::GetCollector::setPartialMatches(
                    bool  arg_PartialMatches)
             {
                

                if(isValidPartialMatches &&
                        arg_PartialMatches == property_PartialMatches)
                {
                    
                    return true;
                }

                

                
                resetPartialMatches();

                
                        property_PartialMatches = arg_PartialMatches;
                        isValidPartialMatches = true;
                    
                return true;
             }

             

           /**
            * resetter for partialMatches
            */
           bool WSF_CALL
           AviaryCollector::GetCollector::resetPartialMatches()
           {
               int i = 0;
               int count = 0;


               
               isValidPartialMatches = false; 
               return true;
           }

           /**
            * Check whether partialMatches is nill
            */
           bool WSF_CALL
           AviaryCollector::GetCollector::isPartialMatchesNil()
           {
               return !isValidPartialMatches;
           }

           /**
            * Set partialMatches to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCollector::GetCollector::setPartialMatchesNil()
           {
               return resetPartialMatches();
           }

           

            /**
             * Getter for includeSummaries by  Property Number 3
             */
            bool WSF_CALL
            AviaryCollector::GetCollector::getProperty3()
            {
                return getIncludeSummaries();
            }

            /**
             * getter for includeSummaries.
             */
            bool WSF_CALL
            AviaryCollector::GetCollector::getIncludeSummaries()
             {
                return property_IncludeSummaries;
             }

            /**
             * setter for includeSummaries
             */
            bool WSF_CALL
            AviaryCollector::GetCollector::setIncludeSummaries(
                    bool  arg_IncludeSummaries)
             {
                

                if(isValidIncludeSummaries &&
                        arg_IncludeSummaries == property_IncludeSummaries)
                {
                    
                    return true;
                }

                

                
                resetIncludeSummaries();

                
                        property_IncludeSummaries = arg_IncludeSummaries;
                        isValidIncludeSummaries = true;
                    
                return true;
             }

             

           /**
            * resetter for includeSummaries
            */
           bool WSF_CALL
           AviaryCollector::GetCollector::resetIncludeSummaries()
           {
               int i = 0;
               int count = 0;


               
               isValidIncludeSummaries = false; 
               return true;
           }

           /**
            * Check whether includeSummaries is nill
            */
           bool WSF_CALL
           AviaryCollector::GetCollector::isIncludeSummariesNil()
           {
               return !isValidIncludeSummaries;
           }

           /**
            * Set includeSummaries to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCollector::GetCollector::setIncludeSummariesNil()
           {
               return resetIncludeSummaries();
           }

           

