

        /**
         * AttributeRequest.cpp
         *
         * This file was auto-generated from WSDL
         * by the Apache Axis2/C version: SNAPSHOT  Built on : Mar 10, 2008 (08:35:52 GMT+00:00)
         */
        
            #include "AviaryCollector_AttributeRequest.h"
          

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
                 * This type was generated from the piece of schema that had
                 * name = AttributeRequest
                 * Namespace URI = http://collector.aviary.grid.redhat.com
                 * Namespace Prefix = ns2
                 */
           AviaryCollector::AttributeRequest::AttributeRequest()
        {

        
                property_Id  = NULL;
              
            isValidId  = false;
        
                property_Names  = NULL;
              
            isValidNames  = false;
        
        }

       AviaryCollector::AttributeRequest::AttributeRequest(AviaryCommon::ResourceID* arg_Id,std::vector<std::string*>* arg_Names)
        {
             
               property_Id  = NULL;
             
            isValidId  = true;
            
               property_Names  = NULL;
             
            isValidNames  = true;
            
                    property_Id = arg_Id;
            
                    property_Names = arg_Names;
            
        }
        AviaryCollector::AttributeRequest::~AttributeRequest()
        {
            resetAll();
        }

        bool WSF_CALL AviaryCollector::AttributeRequest::resetAll()
        {
            //calls reset method for all the properties owned by this method which are pointers.

            
             resetId();//AviaryCommon::ResourceID
             resetNames();//std::string
            return true;

        }

        

        bool WSF_CALL
        AviaryCollector::AttributeRequest::deserialize(axiom_node_t** dp_parent,bool *dp_is_early_node_valid, bool dont_care_minoccurs)
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
              
                      
                      first_node = axiom_node_get_first_child(parent, Environment::getEnv());
                      
                    

                     
                     /*
                      * building id element
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
                                   
                                 element_qname = axutil_qname_create(Environment::getEnv(), "id", NULL, NULL);
                                 

                           if (isParticle() ||  
                                (current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("id", axiom_element_get_localname(current_element, Environment::getEnv())))))
                           {
                              if( current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("id", axiom_element_get_localname(current_element, Environment::getEnv()))))
                              {
                                is_early_node_valid = true;
                              }
                              
                                 AviaryCommon::ResourceID* element = new AviaryCommon::ResourceID();

                                      status =  element->deserialize(&current_node, &is_early_node_valid, false);
                                      if(AXIS2_FAILURE == status)
                                      {
                                          WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "failed in building adb object for element id");
                                      }
                                      else
                                      {
                                          status = setId(element);
                                      }
                                    
                                 if(AXIS2_FAILURE ==  status)
                                 {
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"failed in setting the value for id ");
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
				  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "non nillable or minOuccrs != 0 element id missing");
                                  return AXIS2_FAILURE;
                              }
                           
                  if(element_qname)
                  {
                     axutil_qname_free(element_qname, Environment::getEnv());
                     element_qname = NULL;
                  }
                 
                       { 
                    /*
                     * building Names array
                     */
                       std::vector<std::string*>* arr_list =new std::vector<std::string*>();
                   

                     
                     /*
                      * building names element
                      */
                     
                     
                     
                                    element_qname = axutil_qname_create(Environment::getEnv(), "names", NULL, NULL);
                                  
                               
                               for (i = 0, sequence_broken = 0, current_node = (is_early_node_valid?axiom_node_get_next_sibling(current_node, Environment::getEnv()):current_node); !sequence_broken && current_node != NULL;)
                                             
                               {
                                  if(axiom_node_get_node_type(current_node, Environment::getEnv()) != AXIOM_ELEMENT)
                                  {
                                     current_node =axiom_node_get_next_sibling(current_node, Environment::getEnv());
                                     is_early_node_valid = false;
                                     continue;
                                  }
                                  
                                  current_element = (axiom_element_t *)axiom_node_get_data_element(current_node, Environment::getEnv());
                                  mqname = axiom_element_get_qname(current_element, Environment::getEnv(), current_node);

                                  if (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("names", axiom_element_get_localname(current_element, Environment::getEnv())))
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
					      WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI,"NULL value is set to a non nillable element names");
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
                                         WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "failed in setting the value for names ");
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
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"names (@minOccurs = '0') only have %d elements", i);
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
                                    status = setNames(arr_list);
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
          AviaryCollector::AttributeRequest::isParticle()
          {
            
                 return false;
              
          }


          void WSF_CALL
          AviaryCollector::AttributeRequest::declareParentNamespaces(
                    axiom_element_t *parent_element,
                    axutil_hash_t *namespaces, int *next_ns_index)
          {
            
                  /* Here this is an empty function, Nothing to declare */
                 
          }

        
        
        axiom_node_t* WSF_CALL
	AviaryCollector::AttributeRequest::serialize(axiom_node_t *parent, 
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
            
               int i = 0;
               int count = 0;
               void *element = NULL;
             
                    axis2_char_t text_value_1[ADB_DEFAULT_DIGIT_LIMIT];
                    
                    axis2_char_t *text_value_2;
                    axis2_char_t *text_value_2_temp;
                    
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
                      

                   if (!isValidId)
                   {
                      
                            
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Nil value found in non-nillable property id");
                            return NULL;
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("id"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("id")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     
                     /*
                      * parsing id element
                      */

                    
                    
                            sprintf(start_input_str, "<%s%sid",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":""); 
                            
                        start_input_str_len = axutil_strlen(start_input_str);
                        sprintf(end_input_str, "</%s%sid>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                        end_input_str_len = axutil_strlen(end_input_str);
                     
                            if(!property_Id->isParticle())
                            {
                                axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                            }
                            property_Id->serialize(current_node, parent_element,
                                                                                 property_Id->isParticle() || false, namespaces, next_ns_index);
                            
                            if(!property_Id->isParticle())
                            {
                                axutil_stream_write(stream, Environment::getEnv(), end_input_str, end_input_str_len);
                            }
                            
                     
                     AXIS2_FREE(Environment::getEnv()->allocator,start_input_str);
                     AXIS2_FREE(Environment::getEnv()->allocator,end_input_str);
                 } 

                 
                       p_prefix = NULL;
                      

                   if (!isValidNames)
                   {
                      
                           /* no need to complain for minoccurs=0 element */
                            
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("names"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("names")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     /*
                      * Parsing Names array
                      */
                     if (property_Names != NULL)
                     {
                        
                            sprintf(start_input_str, "<%s%snames>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":""); 
                            
                         start_input_str_len = axutil_strlen(start_input_str);

                         sprintf(end_input_str, "</%s%snames>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                         end_input_str_len = axutil_strlen(end_input_str);

                         count = property_Names->size();
                         for(i = 0; i < count; i++)
                         {
                            std::string* element = (*property_Names)[i];

                            if(NULL == element) 
                            {
                                continue;
                            }

                    
                     
                     /*
                      * parsing names element
                      */

                    
                    
                           text_value_2 = (axis2_char_t*)(*element).c_str();
                           
                           axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                           
                            
                           text_value_2_temp = axutil_xml_quote_string(Environment::getEnv(), text_value_2, true);
                           if (text_value_2_temp)
                           {
                               axutil_stream_write(stream, Environment::getEnv(), text_value_2_temp, axutil_strlen(text_value_2_temp));
                               AXIS2_FREE(Environment::getEnv()->allocator, text_value_2_temp);
                           }
                           else
                           {
                               axutil_stream_write(stream, Environment::getEnv(), text_value_2, axutil_strlen(text_value_2));
                           }
                           
                           axutil_stream_write(stream, Environment::getEnv(), end_input_str, end_input_str_len);
                           
                         }
                     }
                   
                     
                     AXIS2_FREE(Environment::getEnv()->allocator,start_input_str);
                     AXIS2_FREE(Environment::getEnv()->allocator,end_input_str);
                 } 

                 

            return parent;
        }


        

            /**
             * Getter for id by  Property Number 1
             */
            AviaryCommon::ResourceID* WSF_CALL
            AviaryCollector::AttributeRequest::getProperty1()
            {
                return getId();
            }

            /**
             * getter for id.
             */
            AviaryCommon::ResourceID* WSF_CALL
            AviaryCollector::AttributeRequest::getId()
             {
                return property_Id;
             }

            /**
             * setter for id
             */
            bool WSF_CALL
            AviaryCollector::AttributeRequest::setId(
                    AviaryCommon::ResourceID*  arg_Id)
             {
                

                if(isValidId &&
                        arg_Id == property_Id)
                {
                    
                    return true;
                }

                
                  if(NULL == arg_Id)
                       
                  {
                      WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"id is being set to NULL, but it is not a nullable element");
                      return AXIS2_FAILURE;
                  }
                

                
                resetId();

                
                    if(NULL == arg_Id)
                         
                {
                    /* We are already done */
                    return true;
                }
                
                        property_Id = arg_Id;
                        isValidId = true;
                    
                return true;
             }

             

           /**
            * resetter for id
            */
           bool WSF_CALL
           AviaryCollector::AttributeRequest::resetId()
           {
               int i = 0;
               int count = 0;


               
            
                

                if(property_Id != NULL)
                {
                   
                   
                         delete  property_Id;
                     

                   }

                
                
                
               isValidId = false; 
               return true;
           }

           /**
            * Check whether id is nill
            */
           bool WSF_CALL
           AviaryCollector::AttributeRequest::isIdNil()
           {
               return !isValidId;
           }

           /**
            * Set id to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCollector::AttributeRequest::setIdNil()
           {
               return resetId();
           }

           

            /**
             * Getter for names by  Property Number 2
             */
            std::vector<std::string*>* WSF_CALL
            AviaryCollector::AttributeRequest::getProperty2()
            {
                return getNames();
            }

            /**
             * getter for names.
             */
            std::vector<std::string*>* WSF_CALL
            AviaryCollector::AttributeRequest::getNames()
             {
                return property_Names;
             }

            /**
             * setter for names
             */
            bool WSF_CALL
            AviaryCollector::AttributeRequest::setNames(
                    std::vector<std::string*>*  arg_Names)
             {
                
                 int size = 0;
                 int i = 0;
                 bool non_nil_exists = false;
                

                if(isValidNames &&
                        arg_Names == property_Names)
                {
                    
                    return true;
                }

                
                 size = arg_Names->size();
                 
                 if (size < 0)
                 {
                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"names has less than minOccurs(0)");
                     return false;
                 }
                 for(i = 0; i < size; i ++ )
                 {
                     if(NULL != (*arg_Names)[i])
                     {
                         non_nil_exists = true;
                         break;
                     }
                 }

                 

                
                resetNames();

                
                    if(NULL == arg_Names)
                         
                {
                    /* We are already done */
                    return true;
                }
                
                        property_Names = arg_Names;
                        if(non_nil_exists)
                        {
                            isValidNames = true;
                        }
                        
                    
                return true;
             }

            
            /**
             * Get ith element of names.
             */
            std::string WSF_CALL
            AviaryCollector::AttributeRequest::getNamesAt(int i)
            {
                std::string* ret_val;
                if(property_Names == NULL)
                {
                    return (std::string)0;
                }
                ret_val =   (*property_Names)[i];
                
                    if(ret_val)
                    {
                        return *ret_val;
                    }
                    return (std::string)0;
                  
            }

            /**
             * Set the ith element of names.
             */
           bool WSF_CALL
            AviaryCollector::AttributeRequest::setNamesAt(int i,
                    const std::string arg_Names)
            {
                 std::string* element;
                int size = 0;

                int non_nil_count;
                bool non_nil_exists = false;

                 

                if( isValidNames &&
                    property_Names &&
                  
                    arg_Names == *((*property_Names)[i]))
                  
                 {
                    
                    return AXIS2_SUCCESS; 
                }

                   
                     non_nil_exists = true;
                  

                if(property_Names == NULL)
                {
                    property_Names = new std::vector<std::string*>();
                }
                else{
                /* check whether there already exist an element */
                element = (*property_Names)[i];
                }

                
                    if(!non_nil_exists)
                    {
                        
                        isValidNames = true;
                        (*property_Names)[i]= NULL;
                        
                        return AXIS2_SUCCESS;
                    }
                
                    (*property_Names)[i]= new string(arg_Names.c_str());
                  

               isValidNames = true;
                
                return AXIS2_SUCCESS;
            }

            /**
             * Add to names.
             */
            bool WSF_CALL
            AviaryCollector::AttributeRequest::addNames(
                    const std::string arg_Names)
             {

                
                    if(
                      arg_Names.empty()
                       
                     )
                    {
                      
                           return true; 
                        
                    }
                  

                if(property_Names == NULL)
                {
                    property_Names = new std::vector<std::string*>();
                }
              
               property_Names->push_back(new string(arg_Names.c_str()));
              
                isValidNames = true;
                return true;
             }

            /**
             * Get the size of the names array.
             */
            int WSF_CALL
            AviaryCollector::AttributeRequest::sizeofNames()
            {

                if(property_Names == NULL)
                {
                    return 0;
                }
                return property_Names->size();
            }

            /**
             * remove the ith element, same as set_nil_at.
             */
            bool WSF_CALL
            AviaryCollector::AttributeRequest::removeNamesAt(int i)
            {
                return setNamesNilAt(i);
            }

            

           /**
            * resetter for names
            */
           bool WSF_CALL
           AviaryCollector::AttributeRequest::resetNames()
           {
               int i = 0;
               int count = 0;


               
                    if(NULL != property_Names)
                 delete property_Names;
                
               isValidNames = false; 
               return true;
           }

           /**
            * Check whether names is nill
            */
           bool WSF_CALL
           AviaryCollector::AttributeRequest::isNamesNil()
           {
               return !isValidNames;
           }

           /**
            * Set names to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCollector::AttributeRequest::setNamesNil()
           {
               return resetNames();
           }

           
           /**
            * Check whether names is nill at i
            */
           bool WSF_CALL
           AviaryCollector::AttributeRequest::isNamesNilAt(int i)
           {
               return (isValidNames == false ||
                       NULL == property_Names ||
                     NULL == (*property_Names)[i]);
            }

           /**
            * Set names to nil at i
            */
           bool WSF_CALL
           AviaryCollector::AttributeRequest::setNamesNilAt(int i)
           {
                int size = 0;
                int j;
                bool non_nil_exists = false;

                int k = 0;

                if(property_Names == NULL ||
                            isValidNames == false)
                {
                    
                    non_nil_exists = false;
                }
                else
                {
                    size = property_Names->size();
                    for(j = 0, k = 0; j < size; j ++ )
                    {
                        if(i == j) continue; 
                        if(NULL != (*property_Names)[i])
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
                       WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "Size of the array of names is beinng set to be smaller than the specificed number of minOccurs(0)");
                       return AXIS2_FAILURE;
                }
 
                if(property_Names == NULL)
                {
                    isValidNames = false;
                    
                    return true;
                }
                 
                    if(!non_nil_exists)
                    {
                        
                        isValidNames = false;
                        (*property_Names)[i] = NULL;
                        return AXIS2_SUCCESS;
                    }
                

                
                (*property_Names)[i] = NULL;
                
                return AXIS2_SUCCESS;

           }

           

