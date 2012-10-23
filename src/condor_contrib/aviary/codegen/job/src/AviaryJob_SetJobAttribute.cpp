

        /**
         * SetJobAttribute.cpp
         *
         * This file was auto-generated from WSDL
         * by the Apache Axis2/C version: SNAPSHOT  Built on : Mar 10, 2008 (08:35:52 GMT+00:00)
         */
        
            #include "AviaryJob_SetJobAttribute.h"
          

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
        
        using namespace AviaryJob;
        
               /*
                * Implementation of the SetJobAttribute|http://job.aviary.grid.redhat.com Element
                */
           AviaryJob::SetJobAttribute::SetJobAttribute()
        {

        
            qname = NULL;
        
                property_Id  = NULL;
              
            isValidId  = false;
        
                property_Attribute  = NULL;
              
            isValidAttribute  = false;
        
                  qname =  axutil_qname_create (Environment::getEnv(),
                        "SetJobAttribute",
                        "http://job.aviary.grid.redhat.com",
                        NULL);
                
        }

       AviaryJob::SetJobAttribute::SetJobAttribute(AviaryCommon::JobID* arg_Id,AviaryCommon::Attribute* arg_Attribute)
        {
             
                   qname = NULL;
             
               property_Id  = NULL;
             
            isValidId  = true;
            
               property_Attribute  = NULL;
             
            isValidAttribute  = true;
            
                 qname =  axutil_qname_create (Environment::getEnv(),
                       "SetJobAttribute",
                       "http://job.aviary.grid.redhat.com",
                       NULL);
               
                    property_Id = arg_Id;
            
                    property_Attribute = arg_Attribute;
            
        }
        AviaryJob::SetJobAttribute::~SetJobAttribute()
        {
            resetAll();
        }

        bool WSF_CALL AviaryJob::SetJobAttribute::resetAll()
        {
            //calls reset method for all the properties owned by this method which are pointers.

            
             resetId();//AviaryCommon::JobID
             resetAttribute();//AviaryCommon::Attribute
          if(qname != NULL)
          {
            axutil_qname_free( qname, Environment::getEnv());
            qname = NULL;
          }
        
            return true;

        }

        

        bool WSF_CALL
        AviaryJob::SetJobAttribute::deserialize(axiom_node_t** dp_parent,bool *dp_is_early_node_valid, bool dont_care_minoccurs)
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
              

                    current_element = (axiom_element_t *)axiom_node_get_data_element(parent, Environment::getEnv());
                    mqname = axiom_element_get_qname(current_element, Environment::getEnv(), parent);
                    if (axutil_qname_equals(mqname, Environment::getEnv(), this->qname))
                    {
                        
                          first_node = axiom_node_get_first_child(parent, Environment::getEnv());
                          
                    }
                    else
                    {
                        WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI,
                              "Failed in building adb object for SetJobAttribute : "
                              "Expected %s but returned %s",
                              axutil_qname_to_string(qname, Environment::getEnv()),
                              axutil_qname_to_string(mqname, Environment::getEnv()));
                        
                        return AXIS2_FAILURE;
                    }
                    

                     
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
                              
                                 AviaryCommon::JobID* element = new AviaryCommon::JobID();

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
                 

                     
                     /*
                      * building attribute element
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
                                 
                                 element_qname = axutil_qname_create(Environment::getEnv(), "attribute", NULL, NULL);
                                 

                           if (isParticle() ||  
                                (current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("attribute", axiom_element_get_localname(current_element, Environment::getEnv())))))
                           {
                              if( current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("attribute", axiom_element_get_localname(current_element, Environment::getEnv()))))
                              {
                                is_early_node_valid = true;
                              }
                              
                                 AviaryCommon::Attribute* element = new AviaryCommon::Attribute();

                                      status =  element->deserialize(&current_node, &is_early_node_valid, false);
                                      if(AXIS2_FAILURE == status)
                                      {
                                          WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "failed in building adb object for element attribute");
                                      }
                                      else
                                      {
                                          status = setAttribute(element);
                                      }
                                    
                                 if(AXIS2_FAILURE ==  status)
                                 {
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"failed in setting the value for attribute ");
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
				  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "non nillable or minOuccrs != 0 element attribute missing");
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
          AviaryJob::SetJobAttribute::isParticle()
          {
            
                 return false;
              
          }


          void WSF_CALL
          AviaryJob::SetJobAttribute::declareParentNamespaces(
                    axiom_element_t *parent_element,
                    axutil_hash_t *namespaces, int *next_ns_index)
          {
            
                  /* Here this is an empty function, Nothing to declare */
                 
          }

        
        
        axiom_node_t* WSF_CALL
	AviaryJob::SetJobAttribute::serialize(axiom_node_t *parent, 
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
                                             "http://job.aviary.grid.redhat.com",
                                             "n"); 
                           axutil_hash_set(namespaces, "http://job.aviary.grid.redhat.com", AXIS2_HASH_KEY_STRING, axutil_strdup(Environment::getEnv(), "n"));
                       
                     
                    parent_element = axiom_element_create (Environment::getEnv(), NULL, "SetJobAttribute", ns1 , &parent);
                    
                    
                    axiom_element_set_namespace(parent_element, Environment::getEnv(), ns1, parent);


            
                    data_source = axiom_data_source_create(Environment::getEnv(), parent, &current_node);
                    stream = axiom_data_source_get_stream(data_source, Environment::getEnv());
                  
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
                      

                   if (!isValidAttribute)
                   {
                      
                            
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Nil value found in non-nillable property attribute");
                            return NULL;
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("attribute"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("attribute")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     
                     /*
                      * parsing attribute element
                      */

                    
                    
                            sprintf(start_input_str, "<%s%sattribute",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":""); 
                            
                        start_input_str_len = axutil_strlen(start_input_str);
                        sprintf(end_input_str, "</%s%sattribute>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                        end_input_str_len = axutil_strlen(end_input_str);
                     
                            if(!property_Attribute->isParticle())
                            {
                                axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                            }
                            property_Attribute->serialize(current_node, parent_element,
                                                                                 property_Attribute->isParticle() || false, namespaces, next_ns_index);
                            
                            if(!property_Attribute->isParticle())
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
             * Getter for id by  Property Number 1
             */
            AviaryCommon::JobID* WSF_CALL
            AviaryJob::SetJobAttribute::getProperty1()
            {
                return getId();
            }

            /**
             * getter for id.
             */
            AviaryCommon::JobID* WSF_CALL
            AviaryJob::SetJobAttribute::getId()
             {
                return property_Id;
             }

            /**
             * setter for id
             */
            bool WSF_CALL
            AviaryJob::SetJobAttribute::setId(
                    AviaryCommon::JobID*  arg_Id)
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
           AviaryJob::SetJobAttribute::resetId()
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
           AviaryJob::SetJobAttribute::isIdNil()
           {
               return !isValidId;
           }

           /**
            * Set id to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryJob::SetJobAttribute::setIdNil()
           {
               return resetId();
           }

           

            /**
             * Getter for attribute by  Property Number 2
             */
            AviaryCommon::Attribute* WSF_CALL
            AviaryJob::SetJobAttribute::getProperty2()
            {
                return getAttribute();
            }

            /**
             * getter for attribute.
             */
            AviaryCommon::Attribute* WSF_CALL
            AviaryJob::SetJobAttribute::getAttribute()
             {
                return property_Attribute;
             }

            /**
             * setter for attribute
             */
            bool WSF_CALL
            AviaryJob::SetJobAttribute::setAttribute(
                    AviaryCommon::Attribute*  arg_Attribute)
             {
                

                if(isValidAttribute &&
                        arg_Attribute == property_Attribute)
                {
                    
                    return true;
                }

                
                  if(NULL == arg_Attribute)
                       
                  {
                      WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"attribute is being set to NULL, but it is not a nullable element");
                      return AXIS2_FAILURE;
                  }
                

                
                resetAttribute();

                
                    if(NULL == arg_Attribute)
                         
                {
                    /* We are already done */
                    return true;
                }
                
                        property_Attribute = arg_Attribute;
                        isValidAttribute = true;
                    
                return true;
             }

             

           /**
            * resetter for attribute
            */
           bool WSF_CALL
           AviaryJob::SetJobAttribute::resetAttribute()
           {
               int i = 0;
               int count = 0;


               
            
                

                if(property_Attribute != NULL)
                {
                   
                   
                         delete  property_Attribute;
                     

                   }

                
                
                
               isValidAttribute = false; 
               return true;
           }

           /**
            * Check whether attribute is nill
            */
           bool WSF_CALL
           AviaryJob::SetJobAttribute::isAttributeNil()
           {
               return !isValidAttribute;
           }

           /**
            * Set attribute to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryJob::SetJobAttribute::setAttributeNil()
           {
               return resetAttribute();
           }

           

