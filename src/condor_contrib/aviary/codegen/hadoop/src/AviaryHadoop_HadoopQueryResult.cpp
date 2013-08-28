

        /**
         * HadoopQueryResult.cpp
         *
         * This file was auto-generated from WSDL
         * by the Apache Axis2/C version: SNAPSHOT  Built on : Mar 10, 2008 (08:35:52 GMT+00:00)
         */
        
            #include "AviaryHadoop_HadoopQueryResult.h"
          

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
        
        using namespace AviaryHadoop;
        
                /*
                 * This type was generated from the piece of schema that had
                 * name = HadoopQueryResult
                 * Namespace URI = http://hadoop.aviary.grid.redhat.com
                 * Namespace Prefix = ns2
                 */
           AviaryHadoop::HadoopQueryResult::HadoopQueryResult()
        {

        
                property_Ref  = NULL;
              
            isValidRef  = false;
        
                property_Parent  = NULL;
              
            isValidParent  = false;
        
                    property_Owner;
                
            isValidOwner  = false;
        
                    property_Description;
                
            isValidDescription  = false;
        
            isValidSubmitted  = false;
        
            isValidUptime  = false;
        
                property_State  = NULL;
              
            isValidState  = false;
        
                property_Status  = NULL;
              
            isValidStatus  = false;
        
                    property_Bin_file;
                
            isValidBin_file  = false;
        
        }

       AviaryHadoop::HadoopQueryResult::HadoopQueryResult(AviaryHadoop::HadoopID* arg_Ref,AviaryHadoop::HadoopID* arg_Parent,std::string arg_Owner,std::string arg_Description,int arg_Submitted,int arg_Uptime,AviaryHadoop::HadoopStateType* arg_State,AviaryCommon::Status* arg_Status,std::string arg_Bin_file)
        {
             
               property_Ref  = NULL;
             
            isValidRef  = true;
            
               property_Parent  = NULL;
             
            isValidParent  = true;
            
                 property_Owner;
             
            isValidOwner  = true;
            
                 property_Description;
             
            isValidDescription  = true;
            
            isValidSubmitted  = true;
            
            isValidUptime  = true;
            
               property_State  = NULL;
             
            isValidState  = true;
            
               property_Status  = NULL;
             
            isValidStatus  = true;
            
                 property_Bin_file;
             
            isValidBin_file  = true;
            
                    property_Ref = arg_Ref;
            
                    property_Parent = arg_Parent;
            
                    property_Owner = arg_Owner;
            
                    property_Description = arg_Description;
            
                    property_Submitted = arg_Submitted;
            
                    property_Uptime = arg_Uptime;
            
                    property_State = arg_State;
            
                    property_Status = arg_Status;
            
                    property_Bin_file = arg_Bin_file;
            
        }
        AviaryHadoop::HadoopQueryResult::~HadoopQueryResult()
        {
            resetAll();
        }

        bool WSF_CALL AviaryHadoop::HadoopQueryResult::resetAll()
        {
            //calls reset method for all the properties owned by this method which are pointers.

            
             resetRef();//AviaryHadoop::HadoopID
             resetParent();//AviaryHadoop::HadoopID
             resetState();//AviaryHadoop::HadoopStateType
             resetStatus();//AviaryCommon::Status
            return true;

        }

        

        bool WSF_CALL
        AviaryHadoop::HadoopQueryResult::deserialize(axiom_node_t** dp_parent,bool *dp_is_early_node_valid, bool dont_care_minoccurs)
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
                      * building ref element
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
                                   
                                 element_qname = axutil_qname_create(Environment::getEnv(), "ref", NULL, NULL);
                                 

                           if (isParticle() ||  
                                (current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("ref", axiom_element_get_localname(current_element, Environment::getEnv())))))
                           {
                              if( current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("ref", axiom_element_get_localname(current_element, Environment::getEnv()))))
                              {
                                is_early_node_valid = true;
                              }
                              
                                 AviaryHadoop::HadoopID* element = new AviaryHadoop::HadoopID();

                                      status =  element->deserialize(&current_node, &is_early_node_valid, false);
                                      if(AXIS2_FAILURE == status)
                                      {
                                          WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "failed in building adb object for element ref");
                                      }
                                      else
                                      {
                                          status = setRef(element);
                                      }
                                    
                                 if(AXIS2_FAILURE ==  status)
                                 {
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"failed in setting the value for ref ");
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
				  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "non nillable or minOuccrs != 0 element ref missing");
                                  return AXIS2_FAILURE;
                              }
                           
                  if(element_qname)
                  {
                     axutil_qname_free(element_qname, Environment::getEnv());
                     element_qname = NULL;
                  }
                 

                     
                     /*
                      * building parent element
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
                                 
                                 element_qname = axutil_qname_create(Environment::getEnv(), "parent", NULL, NULL);
                                 

                           if (isParticle() ||  
                                (current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("parent", axiom_element_get_localname(current_element, Environment::getEnv())))))
                           {
                              if( current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("parent", axiom_element_get_localname(current_element, Environment::getEnv()))))
                              {
                                is_early_node_valid = true;
                              }
                              
                                 AviaryHadoop::HadoopID* element = new AviaryHadoop::HadoopID();

                                      status =  element->deserialize(&current_node, &is_early_node_valid, false);
                                      if(AXIS2_FAILURE == status)
                                      {
                                          WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "failed in building adb object for element parent");
                                      }
                                      else
                                      {
                                          status = setParent(element);
                                      }
                                    
                                 if(AXIS2_FAILURE ==  status)
                                 {
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"failed in setting the value for parent ");
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
				  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "non nillable or minOuccrs != 0 element parent missing");
                                  return AXIS2_FAILURE;
                              }
                           
                  if(element_qname)
                  {
                     axutil_qname_free(element_qname, Environment::getEnv());
                     element_qname = NULL;
                  }
                 

                     
                     /*
                      * building owner element
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
                                 
                                 element_qname = axutil_qname_create(Environment::getEnv(), "owner", NULL, NULL);
                                 

                           if ( 
                                (current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("owner", axiom_element_get_localname(current_element, Environment::getEnv())))))
                           {
                              if( current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("owner", axiom_element_get_localname(current_element, Environment::getEnv()))))
                              {
                                is_early_node_valid = true;
                              }
                              
                                 
                                      text_value = axiom_element_get_text(current_element, Environment::getEnv(), current_node);
                                      if(text_value != NULL)
                                      {
                                            status = setOwner(text_value);
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
                                                WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "NULL value is set to a non nillable element owner");
                                                status = AXIS2_FAILURE;
                                            }
                                            else
                                            {
                                                /* after all, we found this is a empty string */
                                                status = setOwner("");
                                            }
                                      }
                                      
                                 if(AXIS2_FAILURE ==  status)
                                 {
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"failed in setting the value for owner ");
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
				  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "non nillable or minOuccrs != 0 element owner missing");
                                  return AXIS2_FAILURE;
                              }
                           
                  if(element_qname)
                  {
                     axutil_qname_free(element_qname, Environment::getEnv());
                     element_qname = NULL;
                  }
                 

                     
                     /*
                      * building description element
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
                                 
                                 element_qname = axutil_qname_create(Environment::getEnv(), "description", NULL, NULL);
                                 

                           if ( 
                                (current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("description", axiom_element_get_localname(current_element, Environment::getEnv())))))
                           {
                              if( current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("description", axiom_element_get_localname(current_element, Environment::getEnv()))))
                              {
                                is_early_node_valid = true;
                              }
                              
                                 
                                      text_value = axiom_element_get_text(current_element, Environment::getEnv(), current_node);
                                      if(text_value != NULL)
                                      {
                                            status = setDescription(text_value);
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
                                                WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "NULL value is set to a non nillable element description");
                                                status = AXIS2_FAILURE;
                                            }
                                            else
                                            {
                                                /* after all, we found this is a empty string */
                                                status = setDescription("");
                                            }
                                      }
                                      
                                 if(AXIS2_FAILURE ==  status)
                                 {
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"failed in setting the value for description ");
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
				  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "non nillable or minOuccrs != 0 element description missing");
                                  return AXIS2_FAILURE;
                              }
                           
                  if(element_qname)
                  {
                     axutil_qname_free(element_qname, Environment::getEnv());
                     element_qname = NULL;
                  }
                 

                     
                     /*
                      * building submitted element
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
                                 
                                 element_qname = axutil_qname_create(Environment::getEnv(), "submitted", NULL, NULL);
                                 

                           if ( 
                                (current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("submitted", axiom_element_get_localname(current_element, Environment::getEnv())))))
                           {
                              if( current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("submitted", axiom_element_get_localname(current_element, Environment::getEnv()))))
                              {
                                is_early_node_valid = true;
                              }
                              
                                 
                                      text_value = axiom_element_get_text(current_element, Environment::getEnv(), current_node);
                                      if(text_value != NULL)
                                      {
                                            status = setSubmitted(atoi(text_value));
                                      }
                                      
                                      else
                                      {
                                          WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "NULL value is set to a non nillable element submitted");
                                          status = AXIS2_FAILURE;
                                      }
                                      
                                 if(AXIS2_FAILURE ==  status)
                                 {
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"failed in setting the value for submitted ");
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
				  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "non nillable or minOuccrs != 0 element submitted missing");
                                  return AXIS2_FAILURE;
                              }
                           
                  if(element_qname)
                  {
                     axutil_qname_free(element_qname, Environment::getEnv());
                     element_qname = NULL;
                  }
                 

                     
                     /*
                      * building uptime element
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
                                 
                                 element_qname = axutil_qname_create(Environment::getEnv(), "uptime", NULL, NULL);
                                 

                           if ( 
                                (current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("uptime", axiom_element_get_localname(current_element, Environment::getEnv())))))
                           {
                              if( current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("uptime", axiom_element_get_localname(current_element, Environment::getEnv()))))
                              {
                                is_early_node_valid = true;
                              }
                              
                                 
                                      text_value = axiom_element_get_text(current_element, Environment::getEnv(), current_node);
                                      if(text_value != NULL)
                                      {
                                            status = setUptime(atoi(text_value));
                                      }
                                      
                                      else
                                      {
                                          WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "NULL value is set to a non nillable element uptime");
                                          status = AXIS2_FAILURE;
                                      }
                                      
                                 if(AXIS2_FAILURE ==  status)
                                 {
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"failed in setting the value for uptime ");
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
				  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "non nillable or minOuccrs != 0 element uptime missing");
                                  return AXIS2_FAILURE;
                              }
                           
                  if(element_qname)
                  {
                     axutil_qname_free(element_qname, Environment::getEnv());
                     element_qname = NULL;
                  }
                 

                     
                     /*
                      * building state element
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
                                 
                                 element_qname = axutil_qname_create(Environment::getEnv(), "state", NULL, NULL);
                                 

                           if (isParticle() ||  
                                (current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("state", axiom_element_get_localname(current_element, Environment::getEnv())))))
                           {
                              if( current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("state", axiom_element_get_localname(current_element, Environment::getEnv()))))
                              {
                                is_early_node_valid = true;
                              }
                              
                                 AviaryHadoop::HadoopStateType* element = new AviaryHadoop::HadoopStateType();

                                      status =  element->deserialize(&current_node, &is_early_node_valid, false);
                                      if(AXIS2_FAILURE == status)
                                      {
                                          WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "failed in building adb object for element state");
                                      }
                                      else
                                      {
                                          status = setState(element);
                                      }
                                    
                                 if(AXIS2_FAILURE ==  status)
                                 {
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"failed in setting the value for state ");
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
				  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "non nillable or minOuccrs != 0 element state missing");
                                  return AXIS2_FAILURE;
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
                 

                     
                     /*
                      * building bin_file element
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
                                 
                                 element_qname = axutil_qname_create(Environment::getEnv(), "bin_file", NULL, NULL);
                                 

                           if ( 
                                (current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("bin_file", axiom_element_get_localname(current_element, Environment::getEnv())))))
                           {
                              if( current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("bin_file", axiom_element_get_localname(current_element, Environment::getEnv()))))
                              {
                                is_early_node_valid = true;
                              }
                              
                                 
                                      text_value = axiom_element_get_text(current_element, Environment::getEnv(), current_node);
                                      if(text_value != NULL)
                                      {
                                            status = setBin_file(text_value);
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
                                                WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "NULL value is set to a non nillable element bin_file");
                                                status = AXIS2_FAILURE;
                                            }
                                            else
                                            {
                                                /* after all, we found this is a empty string */
                                                status = setBin_file("");
                                            }
                                      }
                                      
                                 if(AXIS2_FAILURE ==  status)
                                 {
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"failed in setting the value for bin_file ");
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
				  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "non nillable or minOuccrs != 0 element bin_file missing");
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
          AviaryHadoop::HadoopQueryResult::isParticle()
          {
            
                 return false;
              
          }


          void WSF_CALL
          AviaryHadoop::HadoopQueryResult::declareParentNamespaces(
                    axiom_element_t *parent_element,
                    axutil_hash_t *namespaces, int *next_ns_index)
          {
            
                  /* Here this is an empty function, Nothing to declare */
                 
          }

        
        
        axiom_node_t* WSF_CALL
	AviaryHadoop::HadoopQueryResult::serialize(axiom_node_t *parent, 
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
            
                    axis2_char_t text_value_1[ADB_DEFAULT_DIGIT_LIMIT];
                    
                    axis2_char_t text_value_2[ADB_DEFAULT_DIGIT_LIMIT];
                    
                    axis2_char_t *text_value_3;
                    axis2_char_t *text_value_3_temp;
                    
                    axis2_char_t *text_value_4;
                    axis2_char_t *text_value_4_temp;
                    
                    axis2_char_t text_value_5[ADB_DEFAULT_DIGIT_LIMIT];
                    
                    axis2_char_t text_value_6[ADB_DEFAULT_DIGIT_LIMIT];
                    
                    axis2_char_t text_value_7[ADB_DEFAULT_DIGIT_LIMIT];
                    
                    axis2_char_t text_value_8[ADB_DEFAULT_DIGIT_LIMIT];
                    
                    axis2_char_t *text_value_9;
                    axis2_char_t *text_value_9_temp;
                    
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
                      

                   if (!isValidRef)
                   {
                      
                            
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Nil value found in non-nillable property ref");
                            return NULL;
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("ref"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("ref")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     
                     /*
                      * parsing ref element
                      */

                    
                    
                            sprintf(start_input_str, "<%s%sref",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":""); 
                            
                        start_input_str_len = axutil_strlen(start_input_str);
                        sprintf(end_input_str, "</%s%sref>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                        end_input_str_len = axutil_strlen(end_input_str);
                     
                            if(!property_Ref->isParticle())
                            {
                                axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                            }
                            property_Ref->serialize(current_node, parent_element,
                                                                                 property_Ref->isParticle() || false, namespaces, next_ns_index);
                            
                            if(!property_Ref->isParticle())
                            {
                                axutil_stream_write(stream, Environment::getEnv(), end_input_str, end_input_str_len);
                            }
                            
                     
                     AXIS2_FREE(Environment::getEnv()->allocator,start_input_str);
                     AXIS2_FREE(Environment::getEnv()->allocator,end_input_str);
                 } 

                 
                       p_prefix = NULL;
                      

                   if (!isValidParent)
                   {
                      
                            
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Nil value found in non-nillable property parent");
                            return NULL;
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("parent"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("parent")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     
                     /*
                      * parsing parent element
                      */

                    
                    
                            sprintf(start_input_str, "<%s%sparent",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":""); 
                            
                        start_input_str_len = axutil_strlen(start_input_str);
                        sprintf(end_input_str, "</%s%sparent>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                        end_input_str_len = axutil_strlen(end_input_str);
                     
                            if(!property_Parent->isParticle())
                            {
                                axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                            }
                            property_Parent->serialize(current_node, parent_element,
                                                                                 property_Parent->isParticle() || false, namespaces, next_ns_index);
                            
                            if(!property_Parent->isParticle())
                            {
                                axutil_stream_write(stream, Environment::getEnv(), end_input_str, end_input_str_len);
                            }
                            
                     
                     AXIS2_FREE(Environment::getEnv()->allocator,start_input_str);
                     AXIS2_FREE(Environment::getEnv()->allocator,end_input_str);
                 } 

                 
                       p_prefix = NULL;
                      

                   if (!isValidOwner)
                   {
                      
                            
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Nil value found in non-nillable property owner");
                            return NULL;
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("owner"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("owner")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     
                     /*
                      * parsing owner element
                      */

                    
                    
                            sprintf(start_input_str, "<%s%sowner>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                            
                        start_input_str_len = axutil_strlen(start_input_str);
                        sprintf(end_input_str, "</%s%sowner>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                        end_input_str_len = axutil_strlen(end_input_str);
                    
                           text_value_3 = (axis2_char_t*)property_Owner.c_str();
                           
                           axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                           
                            
                           text_value_3_temp = axutil_xml_quote_string(Environment::getEnv(), text_value_3, true);
                           if (text_value_3_temp)
                           {
                               axutil_stream_write(stream, Environment::getEnv(), text_value_3_temp, axutil_strlen(text_value_3_temp));
                               AXIS2_FREE(Environment::getEnv()->allocator, text_value_3_temp);
                           }
                           else
                           {
                               axutil_stream_write(stream, Environment::getEnv(), text_value_3, axutil_strlen(text_value_3));
                           }
                           
                           axutil_stream_write(stream, Environment::getEnv(), end_input_str, end_input_str_len);
                           
                     
                     AXIS2_FREE(Environment::getEnv()->allocator,start_input_str);
                     AXIS2_FREE(Environment::getEnv()->allocator,end_input_str);
                 } 

                 
                       p_prefix = NULL;
                      

                   if (!isValidDescription)
                   {
                      
                            
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Nil value found in non-nillable property description");
                            return NULL;
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("description"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("description")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     
                     /*
                      * parsing description element
                      */

                    
                    
                            sprintf(start_input_str, "<%s%sdescription>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                            
                        start_input_str_len = axutil_strlen(start_input_str);
                        sprintf(end_input_str, "</%s%sdescription>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                        end_input_str_len = axutil_strlen(end_input_str);
                    
                           text_value_4 = (axis2_char_t*)property_Description.c_str();
                           
                           axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                           
                            
                           text_value_4_temp = axutil_xml_quote_string(Environment::getEnv(), text_value_4, true);
                           if (text_value_4_temp)
                           {
                               axutil_stream_write(stream, Environment::getEnv(), text_value_4_temp, axutil_strlen(text_value_4_temp));
                               AXIS2_FREE(Environment::getEnv()->allocator, text_value_4_temp);
                           }
                           else
                           {
                               axutil_stream_write(stream, Environment::getEnv(), text_value_4, axutil_strlen(text_value_4));
                           }
                           
                           axutil_stream_write(stream, Environment::getEnv(), end_input_str, end_input_str_len);
                           
                     
                     AXIS2_FREE(Environment::getEnv()->allocator,start_input_str);
                     AXIS2_FREE(Environment::getEnv()->allocator,end_input_str);
                 } 

                 
                       p_prefix = NULL;
                      

                   if (!isValidSubmitted)
                   {
                      
                            
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Nil value found in non-nillable property submitted");
                            return NULL;
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("submitted"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("submitted")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     
                     /*
                      * parsing submitted element
                      */

                    
                    
                            sprintf(start_input_str, "<%s%ssubmitted>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                            
                        start_input_str_len = axutil_strlen(start_input_str);
                        sprintf(end_input_str, "</%s%ssubmitted>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                        end_input_str_len = axutil_strlen(end_input_str);
                    
                               sprintf (text_value_5, AXIS2_PRINTF_INT32_FORMAT_SPECIFIER, property_Submitted);
                             
                           axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                           
                           axutil_stream_write(stream, Environment::getEnv(), text_value_5, axutil_strlen(text_value_5));
                           
                           axutil_stream_write(stream, Environment::getEnv(), end_input_str, end_input_str_len);
                           
                     
                     AXIS2_FREE(Environment::getEnv()->allocator,start_input_str);
                     AXIS2_FREE(Environment::getEnv()->allocator,end_input_str);
                 } 

                 
                       p_prefix = NULL;
                      

                   if (!isValidUptime)
                   {
                      
                            
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Nil value found in non-nillable property uptime");
                            return NULL;
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("uptime"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("uptime")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     
                     /*
                      * parsing uptime element
                      */

                    
                    
                            sprintf(start_input_str, "<%s%suptime>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                            
                        start_input_str_len = axutil_strlen(start_input_str);
                        sprintf(end_input_str, "</%s%suptime>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                        end_input_str_len = axutil_strlen(end_input_str);
                    
                               sprintf (text_value_6, AXIS2_PRINTF_INT32_FORMAT_SPECIFIER, property_Uptime);
                             
                           axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                           
                           axutil_stream_write(stream, Environment::getEnv(), text_value_6, axutil_strlen(text_value_6));
                           
                           axutil_stream_write(stream, Environment::getEnv(), end_input_str, end_input_str_len);
                           
                     
                     AXIS2_FREE(Environment::getEnv()->allocator,start_input_str);
                     AXIS2_FREE(Environment::getEnv()->allocator,end_input_str);
                 } 

                 
                       p_prefix = NULL;
                      

                   if (!isValidState)
                   {
                      
                            
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Nil value found in non-nillable property state");
                            return NULL;
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("state"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("state")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     
                     /*
                      * parsing state element
                      */

                    
                    
                            sprintf(start_input_str, "<%s%sstate",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":""); 
                            
                        start_input_str_len = axutil_strlen(start_input_str);
                        sprintf(end_input_str, "</%s%sstate>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                        end_input_str_len = axutil_strlen(end_input_str);
                     
                            if(!property_State->isParticle())
                            {
                                axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                            }
                            property_State->serialize(current_node, parent_element,
                                                                                 property_State->isParticle() || false, namespaces, next_ns_index);
                            
                            if(!property_State->isParticle())
                            {
                                axutil_stream_write(stream, Environment::getEnv(), end_input_str, end_input_str_len);
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

                 
                       p_prefix = NULL;
                      

                   if (!isValidBin_file)
                   {
                      
                            
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Nil value found in non-nillable property bin_file");
                            return NULL;
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("bin_file"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("bin_file")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     
                     /*
                      * parsing bin_file element
                      */

                    
                    
                            sprintf(start_input_str, "<%s%sbin_file>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                            
                        start_input_str_len = axutil_strlen(start_input_str);
                        sprintf(end_input_str, "</%s%sbin_file>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                        end_input_str_len = axutil_strlen(end_input_str);
                    
                           text_value_9 = (axis2_char_t*)property_Bin_file.c_str();
                           
                           axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                           
                            
                           text_value_9_temp = axutil_xml_quote_string(Environment::getEnv(), text_value_9, true);
                           if (text_value_9_temp)
                           {
                               axutil_stream_write(stream, Environment::getEnv(), text_value_9_temp, axutil_strlen(text_value_9_temp));
                               AXIS2_FREE(Environment::getEnv()->allocator, text_value_9_temp);
                           }
                           else
                           {
                               axutil_stream_write(stream, Environment::getEnv(), text_value_9, axutil_strlen(text_value_9));
                           }
                           
                           axutil_stream_write(stream, Environment::getEnv(), end_input_str, end_input_str_len);
                           
                     
                     AXIS2_FREE(Environment::getEnv()->allocator,start_input_str);
                     AXIS2_FREE(Environment::getEnv()->allocator,end_input_str);
                 } 

                 

            return parent;
        }


        

            /**
             * Getter for ref by  Property Number 1
             */
            AviaryHadoop::HadoopID* WSF_CALL
            AviaryHadoop::HadoopQueryResult::getProperty1()
            {
                return getRef();
            }

            /**
             * getter for ref.
             */
            AviaryHadoop::HadoopID* WSF_CALL
            AviaryHadoop::HadoopQueryResult::getRef()
             {
                return property_Ref;
             }

            /**
             * setter for ref
             */
            bool WSF_CALL
            AviaryHadoop::HadoopQueryResult::setRef(
                    AviaryHadoop::HadoopID*  arg_Ref)
             {
                

                if(isValidRef &&
                        arg_Ref == property_Ref)
                {
                    
                    return true;
                }

                
                  if(NULL == arg_Ref)
                       
                  {
                      WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"ref is being set to NULL, but it is not a nullable element");
                      return AXIS2_FAILURE;
                  }
                

                
                resetRef();

                
                    if(NULL == arg_Ref)
                         
                {
                    /* We are already done */
                    return true;
                }
                
                        property_Ref = arg_Ref;
                        isValidRef = true;
                    
                return true;
             }

             

           /**
            * resetter for ref
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQueryResult::resetRef()
           {
               int i = 0;
               int count = 0;


               
            
                

                if(property_Ref != NULL)
                {
                   
                   
                         delete  property_Ref;
                     

                   }

                
                
                
               isValidRef = false; 
               return true;
           }

           /**
            * Check whether ref is nill
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQueryResult::isRefNil()
           {
               return !isValidRef;
           }

           /**
            * Set ref to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQueryResult::setRefNil()
           {
               return resetRef();
           }

           

            /**
             * Getter for parent by  Property Number 2
             */
            AviaryHadoop::HadoopID* WSF_CALL
            AviaryHadoop::HadoopQueryResult::getProperty2()
            {
                return getParent();
            }

            /**
             * getter for parent.
             */
            AviaryHadoop::HadoopID* WSF_CALL
            AviaryHadoop::HadoopQueryResult::getParent()
             {
                return property_Parent;
             }

            /**
             * setter for parent
             */
            bool WSF_CALL
            AviaryHadoop::HadoopQueryResult::setParent(
                    AviaryHadoop::HadoopID*  arg_Parent)
             {
                

                if(isValidParent &&
                        arg_Parent == property_Parent)
                {
                    
                    return true;
                }

                
                  if(NULL == arg_Parent)
                       
                  {
                      WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"parent is being set to NULL, but it is not a nullable element");
                      return AXIS2_FAILURE;
                  }
                

                
                resetParent();

                
                    if(NULL == arg_Parent)
                         
                {
                    /* We are already done */
                    return true;
                }
                
                        property_Parent = arg_Parent;
                        isValidParent = true;
                    
                return true;
             }

             

           /**
            * resetter for parent
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQueryResult::resetParent()
           {
               int i = 0;
               int count = 0;


               
            
                

                if(property_Parent != NULL)
                {
                   
                   
                         delete  property_Parent;
                     

                   }

                
                
                
               isValidParent = false; 
               return true;
           }

           /**
            * Check whether parent is nill
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQueryResult::isParentNil()
           {
               return !isValidParent;
           }

           /**
            * Set parent to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQueryResult::setParentNil()
           {
               return resetParent();
           }

           

            /**
             * Getter for owner by  Property Number 3
             */
            std::string WSF_CALL
            AviaryHadoop::HadoopQueryResult::getProperty3()
            {
                return getOwner();
            }

            /**
             * getter for owner.
             */
            std::string WSF_CALL
            AviaryHadoop::HadoopQueryResult::getOwner()
             {
                return property_Owner;
             }

            /**
             * setter for owner
             */
            bool WSF_CALL
            AviaryHadoop::HadoopQueryResult::setOwner(
                    const std::string  arg_Owner)
             {
                

                if(isValidOwner &&
                        arg_Owner == property_Owner)
                {
                    
                    return true;
                }

                
                  if(arg_Owner.empty())
                       
                  {
                      WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"owner is being set to NULL, but it is not a nullable element");
                      return AXIS2_FAILURE;
                  }
                

                
                resetOwner();

                
                        property_Owner = std::string(arg_Owner.c_str());
                        isValidOwner = true;
                    
                return true;
             }

             

           /**
            * resetter for owner
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQueryResult::resetOwner()
           {
               int i = 0;
               int count = 0;


               
               isValidOwner = false; 
               return true;
           }

           /**
            * Check whether owner is nill
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQueryResult::isOwnerNil()
           {
               return !isValidOwner;
           }

           /**
            * Set owner to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQueryResult::setOwnerNil()
           {
               return resetOwner();
           }

           

            /**
             * Getter for description by  Property Number 4
             */
            std::string WSF_CALL
            AviaryHadoop::HadoopQueryResult::getProperty4()
            {
                return getDescription();
            }

            /**
             * getter for description.
             */
            std::string WSF_CALL
            AviaryHadoop::HadoopQueryResult::getDescription()
             {
                return property_Description;
             }

            /**
             * setter for description
             */
            bool WSF_CALL
            AviaryHadoop::HadoopQueryResult::setDescription(
                    const std::string  arg_Description)
             {
                

                if(isValidDescription &&
                        arg_Description == property_Description)
                {
                    
                    return true;
                }

                
                  if(arg_Description.empty())
                       
                  {
                      WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"description is being set to NULL, but it is not a nullable element");
                      return AXIS2_FAILURE;
                  }
                

                
                resetDescription();

                
                        property_Description = std::string(arg_Description.c_str());
                        isValidDescription = true;
                    
                return true;
             }

             

           /**
            * resetter for description
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQueryResult::resetDescription()
           {
               int i = 0;
               int count = 0;


               
               isValidDescription = false; 
               return true;
           }

           /**
            * Check whether description is nill
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQueryResult::isDescriptionNil()
           {
               return !isValidDescription;
           }

           /**
            * Set description to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQueryResult::setDescriptionNil()
           {
               return resetDescription();
           }

           

            /**
             * Getter for submitted by  Property Number 5
             */
            int WSF_CALL
            AviaryHadoop::HadoopQueryResult::getProperty5()
            {
                return getSubmitted();
            }

            /**
             * getter for submitted.
             */
            int WSF_CALL
            AviaryHadoop::HadoopQueryResult::getSubmitted()
             {
                return property_Submitted;
             }

            /**
             * setter for submitted
             */
            bool WSF_CALL
            AviaryHadoop::HadoopQueryResult::setSubmitted(
                    const int  arg_Submitted)
             {
                

                if(isValidSubmitted &&
                        arg_Submitted == property_Submitted)
                {
                    
                    return true;
                }

                

                
                resetSubmitted();

                
                        property_Submitted = arg_Submitted;
                        isValidSubmitted = true;
                    
                return true;
             }

             

           /**
            * resetter for submitted
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQueryResult::resetSubmitted()
           {
               int i = 0;
               int count = 0;


               
               isValidSubmitted = false; 
               return true;
           }

           /**
            * Check whether submitted is nill
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQueryResult::isSubmittedNil()
           {
               return !isValidSubmitted;
           }

           /**
            * Set submitted to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQueryResult::setSubmittedNil()
           {
               return resetSubmitted();
           }

           

            /**
             * Getter for uptime by  Property Number 6
             */
            int WSF_CALL
            AviaryHadoop::HadoopQueryResult::getProperty6()
            {
                return getUptime();
            }

            /**
             * getter for uptime.
             */
            int WSF_CALL
            AviaryHadoop::HadoopQueryResult::getUptime()
             {
                return property_Uptime;
             }

            /**
             * setter for uptime
             */
            bool WSF_CALL
            AviaryHadoop::HadoopQueryResult::setUptime(
                    const int  arg_Uptime)
             {
                

                if(isValidUptime &&
                        arg_Uptime == property_Uptime)
                {
                    
                    return true;
                }

                

                
                resetUptime();

                
                        property_Uptime = arg_Uptime;
                        isValidUptime = true;
                    
                return true;
             }

             

           /**
            * resetter for uptime
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQueryResult::resetUptime()
           {
               int i = 0;
               int count = 0;


               
               isValidUptime = false; 
               return true;
           }

           /**
            * Check whether uptime is nill
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQueryResult::isUptimeNil()
           {
               return !isValidUptime;
           }

           /**
            * Set uptime to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQueryResult::setUptimeNil()
           {
               return resetUptime();
           }

           

            /**
             * Getter for state by  Property Number 7
             */
            AviaryHadoop::HadoopStateType* WSF_CALL
            AviaryHadoop::HadoopQueryResult::getProperty7()
            {
                return getState();
            }

            /**
             * getter for state.
             */
            AviaryHadoop::HadoopStateType* WSF_CALL
            AviaryHadoop::HadoopQueryResult::getState()
             {
                return property_State;
             }

            /**
             * setter for state
             */
            bool WSF_CALL
            AviaryHadoop::HadoopQueryResult::setState(
                    AviaryHadoop::HadoopStateType*  arg_State)
             {
                

                if(isValidState &&
                        arg_State == property_State)
                {
                    
                    return true;
                }

                
                  if(NULL == arg_State)
                       
                  {
                      WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"state is being set to NULL, but it is not a nullable element");
                      return AXIS2_FAILURE;
                  }
                

                
                resetState();

                
                    if(NULL == arg_State)
                         
                {
                    /* We are already done */
                    return true;
                }
                
                        property_State = arg_State;
                        isValidState = true;
                    
                return true;
             }

             

           /**
            * resetter for state
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQueryResult::resetState()
           {
               int i = 0;
               int count = 0;


               
            
                

                if(property_State != NULL)
                {
                   
                   
                         delete  property_State;
                     

                   }

                
                
                
               isValidState = false; 
               return true;
           }

           /**
            * Check whether state is nill
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQueryResult::isStateNil()
           {
               return !isValidState;
           }

           /**
            * Set state to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQueryResult::setStateNil()
           {
               return resetState();
           }

           

            /**
             * Getter for status by  Property Number 8
             */
            AviaryCommon::Status* WSF_CALL
            AviaryHadoop::HadoopQueryResult::getProperty8()
            {
                return getStatus();
            }

            /**
             * getter for status.
             */
            AviaryCommon::Status* WSF_CALL
            AviaryHadoop::HadoopQueryResult::getStatus()
             {
                return property_Status;
             }

            /**
             * setter for status
             */
            bool WSF_CALL
            AviaryHadoop::HadoopQueryResult::setStatus(
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
           AviaryHadoop::HadoopQueryResult::resetStatus()
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
           AviaryHadoop::HadoopQueryResult::isStatusNil()
           {
               return !isValidStatus;
           }

           /**
            * Set status to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQueryResult::setStatusNil()
           {
               return resetStatus();
           }

           

            /**
             * Getter for bin_file by  Property Number 9
             */
            std::string WSF_CALL
            AviaryHadoop::HadoopQueryResult::getProperty9()
            {
                return getBin_file();
            }

            /**
             * getter for bin_file.
             */
            std::string WSF_CALL
            AviaryHadoop::HadoopQueryResult::getBin_file()
             {
                return property_Bin_file;
             }

            /**
             * setter for bin_file
             */
            bool WSF_CALL
            AviaryHadoop::HadoopQueryResult::setBin_file(
                    const std::string  arg_Bin_file)
             {
                

                if(isValidBin_file &&
                        arg_Bin_file == property_Bin_file)
                {
                    
                    return true;
                }

                
                  if(arg_Bin_file.empty())
                       
                  {
                      WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"bin_file is being set to NULL, but it is not a nullable element");
                      return AXIS2_FAILURE;
                  }
                

                
                resetBin_file();

                
                        property_Bin_file = std::string(arg_Bin_file.c_str());
                        isValidBin_file = true;
                    
                return true;
             }

             

           /**
            * resetter for bin_file
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQueryResult::resetBin_file()
           {
               int i = 0;
               int count = 0;


               
               isValidBin_file = false; 
               return true;
           }

           /**
            * Check whether bin_file is nill
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQueryResult::isBin_fileNil()
           {
               return !isValidBin_file;
           }

           /**
            * Set bin_file to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQueryResult::setBin_fileNil()
           {
               return resetBin_file();
           }

           

