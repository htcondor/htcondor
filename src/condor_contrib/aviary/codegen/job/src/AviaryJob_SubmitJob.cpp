

        /**
         * SubmitJob.cpp
         *
         * This file was auto-generated from WSDL
         * by the Apache Axis2/C version: SNAPSHOT  Built on : Mar 10, 2008 (08:35:52 GMT+00:00)
         */
        
            #include "AviaryJob_SubmitJob.h"
          

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
                * Implementation of the SubmitJob|http://job.aviary.grid.redhat.com Element
                */
           AviaryJob::SubmitJob::SubmitJob()
        {

        
            qname = NULL;
        
                    property_Cmd;
                
            isValidCmd  = false;
        
                    property_Args;
                
            isValidArgs  = false;
        
                    property_Owner;
                
            isValidOwner  = false;
        
                    property_Iwd;
                
            isValidIwd  = false;
        
                    property_Submission_name;
                
            isValidSubmission_name  = false;
        
                property_Requirements  = NULL;
              
            isValidRequirements  = false;
        
                property_Extra  = NULL;
              
            isValidExtra  = false;
        
            isValidAllowOverrides  = false;
        
                  qname =  axutil_qname_create (Environment::getEnv(),
                        "SubmitJob",
                        "http://job.aviary.grid.redhat.com",
                        NULL);
                
        }

       AviaryJob::SubmitJob::SubmitJob(std::string arg_Cmd,std::string arg_Args,std::string arg_Owner,std::string arg_Iwd,std::string arg_Submission_name,std::vector<AviaryCommon::ResourceConstraint*>* arg_Requirements,std::vector<AviaryCommon::Attribute*>* arg_Extra,bool arg_AllowOverrides)
        {
             
                   qname = NULL;
             
                 property_Cmd;
             
            isValidCmd  = true;
            
                 property_Args;
             
            isValidArgs  = true;
            
                 property_Owner;
             
            isValidOwner  = true;
            
                 property_Iwd;
             
            isValidIwd  = true;
            
                 property_Submission_name;
             
            isValidSubmission_name  = true;
            
               property_Requirements  = NULL;
             
            isValidRequirements  = true;
            
               property_Extra  = NULL;
             
            isValidExtra  = true;
            
            isValidAllowOverrides  = true;
            
                 qname =  axutil_qname_create (Environment::getEnv(),
                       "SubmitJob",
                       "http://job.aviary.grid.redhat.com",
                       NULL);
               
                    property_Cmd = arg_Cmd;
            
                    property_Args = arg_Args;
            
                    property_Owner = arg_Owner;
            
                    property_Iwd = arg_Iwd;
            
                    property_Submission_name = arg_Submission_name;
            
                    property_Requirements = arg_Requirements;
            
                    property_Extra = arg_Extra;
            
                    property_AllowOverrides = arg_AllowOverrides;
            
        }
        AviaryJob::SubmitJob::~SubmitJob()
        {
            resetAll();
        }

        bool WSF_CALL AviaryJob::SubmitJob::resetAll()
        {
            //calls reset method for all the properties owned by this method which are pointers.

            
             resetRequirements();//AviaryCommon::ResourceConstraint
             resetExtra();//AviaryCommon::Attribute
          if(qname != NULL)
          {
            axutil_qname_free( qname, Environment::getEnv());
            qname = NULL;
          }
        
            return true;

        }

        

        bool WSF_CALL
        AviaryJob::SubmitJob::deserialize(axiom_node_t** dp_parent,bool *dp_is_early_node_valid, bool dont_care_minoccurs)
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
                              "Failed in building adb object for SubmitJob : "
                              "Expected %s but returned %s",
                              axutil_qname_to_string(qname, Environment::getEnv()),
                              axutil_qname_to_string(mqname, Environment::getEnv()));
                        
                        return AXIS2_FAILURE;
                    }
                    
                 parent_element = (axiom_element_t *)axiom_node_get_data_element(parent, Environment::getEnv());
                 attribute_hash = axiom_element_get_all_attributes(parent_element, Environment::getEnv());
              

                     
                     /*
                      * building cmd element
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
                                   
                                 element_qname = axutil_qname_create(Environment::getEnv(), "cmd", NULL, NULL);
                                 

                           if ( 
                                (current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("cmd", axiom_element_get_localname(current_element, Environment::getEnv())))))
                           {
                              if( current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("cmd", axiom_element_get_localname(current_element, Environment::getEnv()))))
                              {
                                is_early_node_valid = true;
                              }
                              
                                 
                                      text_value = axiom_element_get_text(current_element, Environment::getEnv(), current_node);
                                      if(text_value != NULL)
                                      {
                                            status = setCmd(text_value);
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
                                                WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "NULL value is set to a non nillable element cmd");
                                                status = AXIS2_FAILURE;
                                            }
                                            else
                                            {
                                                /* after all, we found this is a empty string */
                                                status = setCmd("");
                                            }
                                      }
                                      
                                 if(AXIS2_FAILURE ==  status)
                                 {
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"failed in setting the value for cmd ");
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
				  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "non nillable or minOuccrs != 0 element cmd missing");
                                  return AXIS2_FAILURE;
                              }
                           
                  if(element_qname)
                  {
                     axutil_qname_free(element_qname, Environment::getEnv());
                     element_qname = NULL;
                  }
                 

                     
                     /*
                      * building args element
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
                                 
                                 element_qname = axutil_qname_create(Environment::getEnv(), "args", NULL, NULL);
                                 

                           if ( 
                                (current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("args", axiom_element_get_localname(current_element, Environment::getEnv())))))
                           {
                              if( current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("args", axiom_element_get_localname(current_element, Environment::getEnv()))))
                              {
                                is_early_node_valid = true;
                              }
                              
                                 
                                      text_value = axiom_element_get_text(current_element, Environment::getEnv(), current_node);
                                      if(text_value != NULL)
                                      {
                                            status = setArgs(text_value);
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
                                                WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "NULL value is set to a non nillable element args");
                                                status = AXIS2_FAILURE;
                                            }
                                            else
                                            {
                                                /* after all, we found this is a empty string */
                                                status = setArgs("");
                                            }
                                      }
                                      
                                 if(AXIS2_FAILURE ==  status)
                                 {
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"failed in setting the value for args ");
                                     if(element_qname)
                                     {
                                         axutil_qname_free(element_qname, Environment::getEnv());
                                     }
                                     return AXIS2_FAILURE;
                                 }
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
                      * building iwd element
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
                                 
                                 element_qname = axutil_qname_create(Environment::getEnv(), "iwd", NULL, NULL);
                                 

                           if ( 
                                (current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("iwd", axiom_element_get_localname(current_element, Environment::getEnv())))))
                           {
                              if( current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("iwd", axiom_element_get_localname(current_element, Environment::getEnv()))))
                              {
                                is_early_node_valid = true;
                              }
                              
                                 
                                      text_value = axiom_element_get_text(current_element, Environment::getEnv(), current_node);
                                      if(text_value != NULL)
                                      {
                                            status = setIwd(text_value);
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
                                                WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "NULL value is set to a non nillable element iwd");
                                                status = AXIS2_FAILURE;
                                            }
                                            else
                                            {
                                                /* after all, we found this is a empty string */
                                                status = setIwd("");
                                            }
                                      }
                                      
                                 if(AXIS2_FAILURE ==  status)
                                 {
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"failed in setting the value for iwd ");
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
				  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "non nillable or minOuccrs != 0 element iwd missing");
                                  return AXIS2_FAILURE;
                              }
                           
                  if(element_qname)
                  {
                     axutil_qname_free(element_qname, Environment::getEnv());
                     element_qname = NULL;
                  }
                 

                     
                     /*
                      * building submission_name element
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
                                 
                                 element_qname = axutil_qname_create(Environment::getEnv(), "submission_name", NULL, NULL);
                                 

                           if ( 
                                (current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("submission_name", axiom_element_get_localname(current_element, Environment::getEnv())))))
                           {
                              if( current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("submission_name", axiom_element_get_localname(current_element, Environment::getEnv()))))
                              {
                                is_early_node_valid = true;
                              }
                              
                                 
                                      text_value = axiom_element_get_text(current_element, Environment::getEnv(), current_node);
                                      if(text_value != NULL)
                                      {
                                            status = setSubmission_name(text_value);
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
                                                WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "NULL value is set to a non nillable element submission_name");
                                                status = AXIS2_FAILURE;
                                            }
                                            else
                                            {
                                                /* after all, we found this is a empty string */
                                                status = setSubmission_name("");
                                            }
                                      }
                                      
                                 if(AXIS2_FAILURE ==  status)
                                 {
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"failed in setting the value for submission_name ");
                                     if(element_qname)
                                     {
                                         axutil_qname_free(element_qname, Environment::getEnv());
                                     }
                                     return AXIS2_FAILURE;
                                 }
                              }
                           
                  if(element_qname)
                  {
                     axutil_qname_free(element_qname, Environment::getEnv());
                     element_qname = NULL;
                  }
                 
                       { 
                    /*
                     * building Requirements array
                     */
                       std::vector<AviaryCommon::ResourceConstraint*>* arr_list =new std::vector<AviaryCommon::ResourceConstraint*>();
                   

                     
                     /*
                      * building requirements element
                      */
                     
                     
                     
                                    element_qname = axutil_qname_create(Environment::getEnv(), "requirements", NULL, NULL);
                                  
                               
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

                                  if (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("requirements", axiom_element_get_localname(current_element, Environment::getEnv())))
                                  {
                                  
                                      is_early_node_valid = true;
                                      
                                     AviaryCommon::ResourceConstraint* element = new AviaryCommon::ResourceConstraint();
                                          
                                          status =  element->deserialize(&current_node, &is_early_node_valid, false);
                                          
                                          if(AXIS2_FAILURE ==  status)
                                          {
					  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "failed in building element requirements ");
                                          }
                                          else
                                          {
                                            arr_list->push_back(element);
                                            
                                          }
                                        
                                     if(AXIS2_FAILURE ==  status)
                                     {
                                         WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "failed in setting the value for requirements ");
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
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"requirements (@minOccurs = '0') only have %d elements", i);
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
                                    status = setRequirements(arr_list);
                               }

                              
                            } 
                        
                  if(element_qname)
                  {
                     axutil_qname_free(element_qname, Environment::getEnv());
                     element_qname = NULL;
                  }
                 
                       { 
                    /*
                     * building Extra array
                     */
                       std::vector<AviaryCommon::Attribute*>* arr_list =new std::vector<AviaryCommon::Attribute*>();
                   

                     
                     /*
                      * building extra element
                      */
                     
                     
                     
                                    element_qname = axutil_qname_create(Environment::getEnv(), "extra", NULL, NULL);
                                  
                               
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

                                  if (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("extra", axiom_element_get_localname(current_element, Environment::getEnv())))
                                  {
                                  
                                      is_early_node_valid = true;
                                      
                                     AviaryCommon::Attribute* element = new AviaryCommon::Attribute();
                                          
                                          status =  element->deserialize(&current_node, &is_early_node_valid, false);
                                          
                                          if(AXIS2_FAILURE ==  status)
                                          {
					  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "failed in building element extra ");
                                          }
                                          else
                                          {
                                            arr_list->push_back(element);
                                            
                                          }
                                        
                                     if(AXIS2_FAILURE ==  status)
                                     {
                                         WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "failed in setting the value for extra ");
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
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"extra (@minOccurs = '0') only have %d elements", i);
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
                                    status = setExtra(arr_list);
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
                           
                           
                               if(!strcmp((axis2_char_t*)key, "allowOverrides"))
                             
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
                    /* this is hoping that attribute is stored in "allowOverrides", this happnes when name is in default namespace */
                    attrib_text = axiom_element_get_attribute_value_by_name(parent_element, Environment::getEnv(), "allowOverrides");
                  }

                  if(attrib_text != NULL)
                  {
                      
                      
                           if (!axutil_strcmp(attrib_text, "TRUE") || !axutil_strcmp(attrib_text, "true"))
                           {
                               setAllowOverrides(true);
                           }
                           else
                           {
                               setAllowOverrides(false);
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
          AviaryJob::SubmitJob::isParticle()
          {
            
                 return false;
              
          }


          void WSF_CALL
          AviaryJob::SubmitJob::declareParentNamespaces(
                    axiom_element_t *parent_element,
                    axutil_hash_t *namespaces, int *next_ns_index)
          {
            
                  /* Here this is an empty function, Nothing to declare */
                 
          }

        
        
        axiom_node_t* WSF_CALL
	AviaryJob::SubmitJob::serialize(axiom_node_t *parent, 
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
                    
                    axis2_char_t *text_value_2;
                    axis2_char_t *text_value_2_temp;
                    
                    axis2_char_t *text_value_3;
                    axis2_char_t *text_value_3_temp;
                    
                    axis2_char_t *text_value_4;
                    axis2_char_t *text_value_4_temp;
                    
                    axis2_char_t *text_value_5;
                    axis2_char_t *text_value_5_temp;
                    
                    axis2_char_t text_value_6[ADB_DEFAULT_DIGIT_LIMIT];
                    
                    axis2_char_t text_value_7[ADB_DEFAULT_DIGIT_LIMIT];
                    
                    axis2_char_t text_value_8[ADB_DEFAULT_DIGIT_LIMIT];
                    
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
                                             "http://job.aviary.grid.redhat.com",
                                             "n"); 
                           axutil_hash_set(namespaces, "http://job.aviary.grid.redhat.com", AXIS2_HASH_KEY_STRING, axutil_strdup(Environment::getEnv(), "n"));
                       
                     
                    parent_element = axiom_element_create (Environment::getEnv(), NULL, "SubmitJob", ns1 , &parent);
                    
                    
                    axiom_element_set_namespace(parent_element, Environment::getEnv(), ns1, parent);


            
                    data_source = axiom_data_source_create(Environment::getEnv(), parent, &current_node);
                    stream = axiom_data_source_get_stream(data_source, Environment::getEnv());
                  
            if(!parent_tag_closed)
            {
            
                if(isValidAllowOverrides)
                {
                
                        p_prefix = NULL;
                      
                           
                           text_value = (axis2_char_t*)((property_AllowOverrides)?"true":"false");
                           string_to_stream = (axis2_char_t*) AXIS2_MALLOC (Environment::getEnv()-> allocator, sizeof (axis2_char_t) *
                                                            (5  + ADB_DEFAULT_NAMESPACE_PREFIX_LIMIT +
                                                             axutil_strlen(text_value) + 
                                                             axutil_strlen("allowOverrides")));
                           sprintf(string_to_stream, " %s%s%s=\"%s\"", p_prefix?p_prefix:"", (p_prefix && axutil_strcmp(p_prefix, ""))?":":"",
                                                "allowOverrides",  text_value);
                           axutil_stream_write(stream, Environment::getEnv(), string_to_stream, axutil_strlen(string_to_stream));
                           AXIS2_FREE(Environment::getEnv()-> allocator, string_to_stream);
                        
                   }
                   
            }
            
                       p_prefix = NULL;
                      

                   if (!isValidCmd)
                   {
                      
                            
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Nil value found in non-nillable property cmd");
                            return NULL;
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("cmd"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("cmd")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     
                     /*
                      * parsing cmd element
                      */

                    
                    
                            sprintf(start_input_str, "<%s%scmd>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                            
                        start_input_str_len = axutil_strlen(start_input_str);
                        sprintf(end_input_str, "</%s%scmd>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                        end_input_str_len = axutil_strlen(end_input_str);
                    
                           text_value_1 = (axis2_char_t*)property_Cmd.c_str();
                           
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
                           
                     
                     AXIS2_FREE(Environment::getEnv()->allocator,start_input_str);
                     AXIS2_FREE(Environment::getEnv()->allocator,end_input_str);
                 } 

                 
                       p_prefix = NULL;
                      

                   if (!isValidArgs)
                   {
                      
                           /* no need to complain for minoccurs=0 element */
                            
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("args"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("args")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     
                     /*
                      * parsing args element
                      */

                    
                    
                            sprintf(start_input_str, "<%s%sargs>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                            
                        start_input_str_len = axutil_strlen(start_input_str);
                        sprintf(end_input_str, "</%s%sargs>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                        end_input_str_len = axutil_strlen(end_input_str);
                    
                           text_value_2 = (axis2_char_t*)property_Args.c_str();
                           
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
                      

                   if (!isValidIwd)
                   {
                      
                            
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Nil value found in non-nillable property iwd");
                            return NULL;
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("iwd"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("iwd")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     
                     /*
                      * parsing iwd element
                      */

                    
                    
                            sprintf(start_input_str, "<%s%siwd>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                            
                        start_input_str_len = axutil_strlen(start_input_str);
                        sprintf(end_input_str, "</%s%siwd>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                        end_input_str_len = axutil_strlen(end_input_str);
                    
                           text_value_4 = (axis2_char_t*)property_Iwd.c_str();
                           
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
                      

                   if (!isValidSubmission_name)
                   {
                      
                           /* no need to complain for minoccurs=0 element */
                            
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("submission_name"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("submission_name")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     
                     /*
                      * parsing submission_name element
                      */

                    
                    
                            sprintf(start_input_str, "<%s%ssubmission_name>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                            
                        start_input_str_len = axutil_strlen(start_input_str);
                        sprintf(end_input_str, "</%s%ssubmission_name>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                        end_input_str_len = axutil_strlen(end_input_str);
                    
                           text_value_5 = (axis2_char_t*)property_Submission_name.c_str();
                           
                           axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                           
                            
                           text_value_5_temp = axutil_xml_quote_string(Environment::getEnv(), text_value_5, true);
                           if (text_value_5_temp)
                           {
                               axutil_stream_write(stream, Environment::getEnv(), text_value_5_temp, axutil_strlen(text_value_5_temp));
                               AXIS2_FREE(Environment::getEnv()->allocator, text_value_5_temp);
                           }
                           else
                           {
                               axutil_stream_write(stream, Environment::getEnv(), text_value_5, axutil_strlen(text_value_5));
                           }
                           
                           axutil_stream_write(stream, Environment::getEnv(), end_input_str, end_input_str_len);
                           
                     
                     AXIS2_FREE(Environment::getEnv()->allocator,start_input_str);
                     AXIS2_FREE(Environment::getEnv()->allocator,end_input_str);
                 } 

                 
                       p_prefix = NULL;
                      

                   if (!isValidRequirements)
                   {
                      
                           /* no need to complain for minoccurs=0 element */
                            
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("requirements"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("requirements")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     /*
                      * Parsing Requirements array
                      */
                     if (property_Requirements != NULL)
                     {
                        

                            sprintf(start_input_str, "<%s%srequirements",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                            
                         start_input_str_len = axutil_strlen(start_input_str);

                         sprintf(end_input_str, "</%s%srequirements>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                         end_input_str_len = axutil_strlen(end_input_str);

                         count = property_Requirements->size();
                         for(i = 0; i < count; i++)
                         {
                            AviaryCommon::ResourceConstraint* element = (*property_Requirements)[i];

                            if(NULL == element) 
                            {
                                continue;
                            }

                    
                     
                     /*
                      * parsing requirements element
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
                      

                   if (!isValidExtra)
                   {
                      
                           /* no need to complain for minoccurs=0 element */
                            
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("extra"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("extra")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     /*
                      * Parsing Extra array
                      */
                     if (property_Extra != NULL)
                     {
                        

                            sprintf(start_input_str, "<%s%sextra",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                            
                         start_input_str_len = axutil_strlen(start_input_str);

                         sprintf(end_input_str, "</%s%sextra>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                         end_input_str_len = axutil_strlen(end_input_str);

                         count = property_Extra->size();
                         for(i = 0; i < count; i++)
                         {
                            AviaryCommon::Attribute* element = (*property_Extra)[i];

                            if(NULL == element) 
                            {
                                continue;
                            }

                    
                     
                     /*
                      * parsing extra element
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

                 
                    
                    if(parent_tag_closed)
                    {
                       if(isValidAllowOverrides)
                       {
                       
                           p_prefix = NULL;
                           ns1 = NULL;
                         
                           
                           text_value =  (axis2_char_t*)((property_AllowOverrides)?axutil_strdup(Environment::getEnv(), "true"):axutil_strdup(Environment::getEnv(), "false"));
                           text_attri = axiom_attribute_create (Environment::getEnv(), "allowOverrides", text_value, ns1);
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
             * Getter for cmd by  Property Number 1
             */
            std::string WSF_CALL
            AviaryJob::SubmitJob::getProperty1()
            {
                return getCmd();
            }

            /**
             * getter for cmd.
             */
            std::string WSF_CALL
            AviaryJob::SubmitJob::getCmd()
             {
                return property_Cmd;
             }

            /**
             * setter for cmd
             */
            bool WSF_CALL
            AviaryJob::SubmitJob::setCmd(
                    const std::string  arg_Cmd)
             {
                

                if(isValidCmd &&
                        arg_Cmd == property_Cmd)
                {
                    
                    return true;
                }

                
                  if(arg_Cmd.empty())
                       
                  {
                      WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"cmd is being set to NULL, but it is not a nullable element");
                      return AXIS2_FAILURE;
                  }
                

                
                resetCmd();

                
                        property_Cmd = std::string(arg_Cmd.c_str());
                        isValidCmd = true;
                    
                return true;
             }

             

           /**
            * resetter for cmd
            */
           bool WSF_CALL
           AviaryJob::SubmitJob::resetCmd()
           {
               int i = 0;
               int count = 0;


               
               isValidCmd = false; 
               return true;
           }

           /**
            * Check whether cmd is nill
            */
           bool WSF_CALL
           AviaryJob::SubmitJob::isCmdNil()
           {
               return !isValidCmd;
           }

           /**
            * Set cmd to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryJob::SubmitJob::setCmdNil()
           {
               return resetCmd();
           }

           

            /**
             * Getter for args by  Property Number 2
             */
            std::string WSF_CALL
            AviaryJob::SubmitJob::getProperty2()
            {
                return getArgs();
            }

            /**
             * getter for args.
             */
            std::string WSF_CALL
            AviaryJob::SubmitJob::getArgs()
             {
                return property_Args;
             }

            /**
             * setter for args
             */
            bool WSF_CALL
            AviaryJob::SubmitJob::setArgs(
                    const std::string  arg_Args)
             {
                

                if(isValidArgs &&
                        arg_Args == property_Args)
                {
                    
                    return true;
                }

                

                
                resetArgs();

                
                        property_Args = std::string(arg_Args.c_str());
                        isValidArgs = true;
                    
                return true;
             }

             

           /**
            * resetter for args
            */
           bool WSF_CALL
           AviaryJob::SubmitJob::resetArgs()
           {
               int i = 0;
               int count = 0;


               
               isValidArgs = false; 
               return true;
           }

           /**
            * Check whether args is nill
            */
           bool WSF_CALL
           AviaryJob::SubmitJob::isArgsNil()
           {
               return !isValidArgs;
           }

           /**
            * Set args to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryJob::SubmitJob::setArgsNil()
           {
               return resetArgs();
           }

           

            /**
             * Getter for owner by  Property Number 3
             */
            std::string WSF_CALL
            AviaryJob::SubmitJob::getProperty3()
            {
                return getOwner();
            }

            /**
             * getter for owner.
             */
            std::string WSF_CALL
            AviaryJob::SubmitJob::getOwner()
             {
                return property_Owner;
             }

            /**
             * setter for owner
             */
            bool WSF_CALL
            AviaryJob::SubmitJob::setOwner(
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
           AviaryJob::SubmitJob::resetOwner()
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
           AviaryJob::SubmitJob::isOwnerNil()
           {
               return !isValidOwner;
           }

           /**
            * Set owner to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryJob::SubmitJob::setOwnerNil()
           {
               return resetOwner();
           }

           

            /**
             * Getter for iwd by  Property Number 4
             */
            std::string WSF_CALL
            AviaryJob::SubmitJob::getProperty4()
            {
                return getIwd();
            }

            /**
             * getter for iwd.
             */
            std::string WSF_CALL
            AviaryJob::SubmitJob::getIwd()
             {
                return property_Iwd;
             }

            /**
             * setter for iwd
             */
            bool WSF_CALL
            AviaryJob::SubmitJob::setIwd(
                    const std::string  arg_Iwd)
             {
                

                if(isValidIwd &&
                        arg_Iwd == property_Iwd)
                {
                    
                    return true;
                }

                
                  if(arg_Iwd.empty())
                       
                  {
                      WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"iwd is being set to NULL, but it is not a nullable element");
                      return AXIS2_FAILURE;
                  }
                

                
                resetIwd();

                
                        property_Iwd = std::string(arg_Iwd.c_str());
                        isValidIwd = true;
                    
                return true;
             }

             

           /**
            * resetter for iwd
            */
           bool WSF_CALL
           AviaryJob::SubmitJob::resetIwd()
           {
               int i = 0;
               int count = 0;


               
               isValidIwd = false; 
               return true;
           }

           /**
            * Check whether iwd is nill
            */
           bool WSF_CALL
           AviaryJob::SubmitJob::isIwdNil()
           {
               return !isValidIwd;
           }

           /**
            * Set iwd to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryJob::SubmitJob::setIwdNil()
           {
               return resetIwd();
           }

           

            /**
             * Getter for submission_name by  Property Number 5
             */
            std::string WSF_CALL
            AviaryJob::SubmitJob::getProperty5()
            {
                return getSubmission_name();
            }

            /**
             * getter for submission_name.
             */
            std::string WSF_CALL
            AviaryJob::SubmitJob::getSubmission_name()
             {
                return property_Submission_name;
             }

            /**
             * setter for submission_name
             */
            bool WSF_CALL
            AviaryJob::SubmitJob::setSubmission_name(
                    const std::string  arg_Submission_name)
             {
                

                if(isValidSubmission_name &&
                        arg_Submission_name == property_Submission_name)
                {
                    
                    return true;
                }

                

                
                resetSubmission_name();

                
                        property_Submission_name = std::string(arg_Submission_name.c_str());
                        isValidSubmission_name = true;
                    
                return true;
             }

             

           /**
            * resetter for submission_name
            */
           bool WSF_CALL
           AviaryJob::SubmitJob::resetSubmission_name()
           {
               int i = 0;
               int count = 0;


               
               isValidSubmission_name = false; 
               return true;
           }

           /**
            * Check whether submission_name is nill
            */
           bool WSF_CALL
           AviaryJob::SubmitJob::isSubmission_nameNil()
           {
               return !isValidSubmission_name;
           }

           /**
            * Set submission_name to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryJob::SubmitJob::setSubmission_nameNil()
           {
               return resetSubmission_name();
           }

           

            /**
             * Getter for requirements by  Property Number 6
             */
            std::vector<AviaryCommon::ResourceConstraint*>* WSF_CALL
            AviaryJob::SubmitJob::getProperty6()
            {
                return getRequirements();
            }

            /**
             * getter for requirements.
             */
            std::vector<AviaryCommon::ResourceConstraint*>* WSF_CALL
            AviaryJob::SubmitJob::getRequirements()
             {
                return property_Requirements;
             }

            /**
             * setter for requirements
             */
            bool WSF_CALL
            AviaryJob::SubmitJob::setRequirements(
                    std::vector<AviaryCommon::ResourceConstraint*>*  arg_Requirements)
             {
                
                 int size = 0;
                 int i = 0;
                 bool non_nil_exists = false;
                

                if(isValidRequirements &&
                        arg_Requirements == property_Requirements)
                {
                    
                    return true;
                }

                
                 size = arg_Requirements->size();
                 
                 if (size < 0)
                 {
                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"requirements has less than minOccurs(0)");
                     return false;
                 }
                 for(i = 0; i < size; i ++ )
                 {
                     if(NULL != (*arg_Requirements)[i])
                     {
                         non_nil_exists = true;
                         break;
                     }
                 }

                 

                
                resetRequirements();

                
                    if(NULL == arg_Requirements)
                         
                {
                    /* We are already done */
                    return true;
                }
                
                        property_Requirements = arg_Requirements;
                        if(non_nil_exists)
                        {
                            isValidRequirements = true;
                        }
                        
                    
                return true;
             }

            
            /**
             * Get ith element of requirements.
             */
            AviaryCommon::ResourceConstraint* WSF_CALL
            AviaryJob::SubmitJob::getRequirementsAt(int i)
            {
                AviaryCommon::ResourceConstraint* ret_val;
                if(property_Requirements == NULL)
                {
                    return (AviaryCommon::ResourceConstraint*)0;
                }
                ret_val =   (*property_Requirements)[i];
                
                    return ret_val;
                  
            }

            /**
             * Set the ith element of requirements.
             */
           bool WSF_CALL
            AviaryJob::SubmitJob::setRequirementsAt(int i,
                    AviaryCommon::ResourceConstraint* arg_Requirements)
            {
                 AviaryCommon::ResourceConstraint* element;
                int size = 0;

                int non_nil_count;
                bool non_nil_exists = false;

                 

                if( isValidRequirements &&
                    property_Requirements &&
                  
                    arg_Requirements == (*property_Requirements)[i])
                  
                 {
                    
                    return AXIS2_SUCCESS; 
                }

                   
                     non_nil_exists = true;
                  

                if(property_Requirements == NULL)
                {
                    property_Requirements = new std::vector<AviaryCommon::ResourceConstraint*>();
                }
                else{
                /* check whether there already exist an element */
                element = (*property_Requirements)[i];
                }

                
                        if(NULL != element)
                        {
                          
                          
                          
                                delete element;
                             
                        }
                        
                    
                    if(!non_nil_exists)
                    {
                        
                        isValidRequirements = true;
                        (*property_Requirements)[i]= NULL;
                        
                        return AXIS2_SUCCESS;
                    }
                
                    (*property_Requirements)[i] = arg_Requirements;
                  

               isValidRequirements = true;
                
                return AXIS2_SUCCESS;
            }

            /**
             * Add to requirements.
             */
            bool WSF_CALL
            AviaryJob::SubmitJob::addRequirements(
                    AviaryCommon::ResourceConstraint* arg_Requirements)
             {

                
                    if( NULL == arg_Requirements
                     )
                    {
                      
                           return true; 
                        
                    }
                  

                if(property_Requirements == NULL)
                {
                    property_Requirements = new std::vector<AviaryCommon::ResourceConstraint*>();
                }
              
               property_Requirements->push_back(arg_Requirements);
              
                isValidRequirements = true;
                return true;
             }

            /**
             * Get the size of the requirements array.
             */
            int WSF_CALL
            AviaryJob::SubmitJob::sizeofRequirements()
            {

                if(property_Requirements == NULL)
                {
                    return 0;
                }
                return property_Requirements->size();
            }

            /**
             * remove the ith element, same as set_nil_at.
             */
            bool WSF_CALL
            AviaryJob::SubmitJob::removeRequirementsAt(int i)
            {
                return setRequirementsNilAt(i);
            }

            

           /**
            * resetter for requirements
            */
           bool WSF_CALL
           AviaryJob::SubmitJob::resetRequirements()
           {
               int i = 0;
               int count = 0;


               
                if (property_Requirements != NULL)
                {
                  std::vector<AviaryCommon::ResourceConstraint*>::iterator it =  property_Requirements->begin();
                  for( ; it <  property_Requirements->end() ; ++it)
                  {
                     AviaryCommon::ResourceConstraint* element = *it;
                
            
                

                if(element != NULL)
                {
                   
                   
                         delete  element;
                     

                   }

                
                
                
               }

             }
                
                    if(NULL != property_Requirements)
                 delete property_Requirements;
                
               isValidRequirements = false; 
               return true;
           }

           /**
            * Check whether requirements is nill
            */
           bool WSF_CALL
           AviaryJob::SubmitJob::isRequirementsNil()
           {
               return !isValidRequirements;
           }

           /**
            * Set requirements to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryJob::SubmitJob::setRequirementsNil()
           {
               return resetRequirements();
           }

           
           /**
            * Check whether requirements is nill at i
            */
           bool WSF_CALL
           AviaryJob::SubmitJob::isRequirementsNilAt(int i)
           {
               return (isValidRequirements == false ||
                       NULL == property_Requirements ||
                     NULL == (*property_Requirements)[i]);
            }

           /**
            * Set requirements to nil at i
            */
           bool WSF_CALL
           AviaryJob::SubmitJob::setRequirementsNilAt(int i)
           {
                int size = 0;
                int j;
                bool non_nil_exists = false;

                int k = 0;

                if(property_Requirements == NULL ||
                            isValidRequirements == false)
                {
                    
                    non_nil_exists = false;
                }
                else
                {
                    size = property_Requirements->size();
                    for(j = 0, k = 0; j < size; j ++ )
                    {
                        if(i == j) continue; 
                        if(NULL != (*property_Requirements)[i])
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
                       WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "Size of the array of requirements is beinng set to be smaller than the specificed number of minOccurs(0)");
                       return AXIS2_FAILURE;
                }
 
                if(property_Requirements == NULL)
                {
                    isValidRequirements = false;
                    
                    return true;
                }
                 
                 /* check whether there already exist an element */
                 AviaryCommon::ResourceConstraint* element = (*property_Requirements)[i];
                if(NULL != element)
                {
                  
                  
                  
                        delete element;
                     
                 }
                 
                    if(!non_nil_exists)
                    {
                        
                        isValidRequirements = false;
                        (*property_Requirements)[i] = NULL;
                        return AXIS2_SUCCESS;
                    }
                

                
                (*property_Requirements)[i] = NULL;
                
                return AXIS2_SUCCESS;

           }

           

            /**
             * Getter for extra by  Property Number 7
             */
            std::vector<AviaryCommon::Attribute*>* WSF_CALL
            AviaryJob::SubmitJob::getProperty7()
            {
                return getExtra();
            }

            /**
             * getter for extra.
             */
            std::vector<AviaryCommon::Attribute*>* WSF_CALL
            AviaryJob::SubmitJob::getExtra()
             {
                return property_Extra;
             }

            /**
             * setter for extra
             */
            bool WSF_CALL
            AviaryJob::SubmitJob::setExtra(
                    std::vector<AviaryCommon::Attribute*>*  arg_Extra)
             {
                
                 int size = 0;
                 int i = 0;
                 bool non_nil_exists = false;
                

                if(isValidExtra &&
                        arg_Extra == property_Extra)
                {
                    
                    return true;
                }

                
                 size = arg_Extra->size();
                 
                 if (size < 0)
                 {
                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"extra has less than minOccurs(0)");
                     return false;
                 }
                 for(i = 0; i < size; i ++ )
                 {
                     if(NULL != (*arg_Extra)[i])
                     {
                         non_nil_exists = true;
                         break;
                     }
                 }

                 

                
                resetExtra();

                
                    if(NULL == arg_Extra)
                         
                {
                    /* We are already done */
                    return true;
                }
                
                        property_Extra = arg_Extra;
                        if(non_nil_exists)
                        {
                            isValidExtra = true;
                        }
                        
                    
                return true;
             }

            
            /**
             * Get ith element of extra.
             */
            AviaryCommon::Attribute* WSF_CALL
            AviaryJob::SubmitJob::getExtraAt(int i)
            {
                AviaryCommon::Attribute* ret_val;
                if(property_Extra == NULL)
                {
                    return (AviaryCommon::Attribute*)0;
                }
                ret_val =   (*property_Extra)[i];
                
                    return ret_val;
                  
            }

            /**
             * Set the ith element of extra.
             */
           bool WSF_CALL
            AviaryJob::SubmitJob::setExtraAt(int i,
                    AviaryCommon::Attribute* arg_Extra)
            {
                 AviaryCommon::Attribute* element;
                int size = 0;

                int non_nil_count;
                bool non_nil_exists = false;

                 

                if( isValidExtra &&
                    property_Extra &&
                  
                    arg_Extra == (*property_Extra)[i])
                  
                 {
                    
                    return AXIS2_SUCCESS; 
                }

                   
                     non_nil_exists = true;
                  

                if(property_Extra == NULL)
                {
                    property_Extra = new std::vector<AviaryCommon::Attribute*>();
                }
                else{
                /* check whether there already exist an element */
                element = (*property_Extra)[i];
                }

                
                        if(NULL != element)
                        {
                          
                          
                          
                                delete element;
                             
                        }
                        
                    
                    if(!non_nil_exists)
                    {
                        
                        isValidExtra = true;
                        (*property_Extra)[i]= NULL;
                        
                        return AXIS2_SUCCESS;
                    }
                
                    (*property_Extra)[i] = arg_Extra;
                  

               isValidExtra = true;
                
                return AXIS2_SUCCESS;
            }

            /**
             * Add to extra.
             */
            bool WSF_CALL
            AviaryJob::SubmitJob::addExtra(
                    AviaryCommon::Attribute* arg_Extra)
             {

                
                    if( NULL == arg_Extra
                     )
                    {
                      
                           return true; 
                        
                    }
                  

                if(property_Extra == NULL)
                {
                    property_Extra = new std::vector<AviaryCommon::Attribute*>();
                }
              
               property_Extra->push_back(arg_Extra);
              
                isValidExtra = true;
                return true;
             }

            /**
             * Get the size of the extra array.
             */
            int WSF_CALL
            AviaryJob::SubmitJob::sizeofExtra()
            {

                if(property_Extra == NULL)
                {
                    return 0;
                }
                return property_Extra->size();
            }

            /**
             * remove the ith element, same as set_nil_at.
             */
            bool WSF_CALL
            AviaryJob::SubmitJob::removeExtraAt(int i)
            {
                return setExtraNilAt(i);
            }

            

           /**
            * resetter for extra
            */
           bool WSF_CALL
           AviaryJob::SubmitJob::resetExtra()
           {
               int i = 0;
               int count = 0;


               
                if (property_Extra != NULL)
                {
                  std::vector<AviaryCommon::Attribute*>::iterator it =  property_Extra->begin();
                  for( ; it <  property_Extra->end() ; ++it)
                  {
                     AviaryCommon::Attribute* element = *it;
                
            
                

                if(element != NULL)
                {
                   
                   
                         delete  element;
                     

                   }

                
                
                
               }

             }
                
                    if(NULL != property_Extra)
                 delete property_Extra;
                
               isValidExtra = false; 
               return true;
           }

           /**
            * Check whether extra is nill
            */
           bool WSF_CALL
           AviaryJob::SubmitJob::isExtraNil()
           {
               return !isValidExtra;
           }

           /**
            * Set extra to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryJob::SubmitJob::setExtraNil()
           {
               return resetExtra();
           }

           
           /**
            * Check whether extra is nill at i
            */
           bool WSF_CALL
           AviaryJob::SubmitJob::isExtraNilAt(int i)
           {
               return (isValidExtra == false ||
                       NULL == property_Extra ||
                     NULL == (*property_Extra)[i]);
            }

           /**
            * Set extra to nil at i
            */
           bool WSF_CALL
           AviaryJob::SubmitJob::setExtraNilAt(int i)
           {
                int size = 0;
                int j;
                bool non_nil_exists = false;

                int k = 0;

                if(property_Extra == NULL ||
                            isValidExtra == false)
                {
                    
                    non_nil_exists = false;
                }
                else
                {
                    size = property_Extra->size();
                    for(j = 0, k = 0; j < size; j ++ )
                    {
                        if(i == j) continue; 
                        if(NULL != (*property_Extra)[i])
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
                       WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "Size of the array of extra is beinng set to be smaller than the specificed number of minOccurs(0)");
                       return AXIS2_FAILURE;
                }
 
                if(property_Extra == NULL)
                {
                    isValidExtra = false;
                    
                    return true;
                }
                 
                 /* check whether there already exist an element */
                 AviaryCommon::Attribute* element = (*property_Extra)[i];
                if(NULL != element)
                {
                  
                  
                  
                        delete element;
                     
                 }
                 
                    if(!non_nil_exists)
                    {
                        
                        isValidExtra = false;
                        (*property_Extra)[i] = NULL;
                        return AXIS2_SUCCESS;
                    }
                

                
                (*property_Extra)[i] = NULL;
                
                return AXIS2_SUCCESS;

           }

           

            /**
             * Getter for allowOverrides by  Property Number 8
             */
            bool WSF_CALL
            AviaryJob::SubmitJob::getProperty8()
            {
                return getAllowOverrides();
            }

            /**
             * getter for allowOverrides.
             */
            bool WSF_CALL
            AviaryJob::SubmitJob::getAllowOverrides()
             {
                return property_AllowOverrides;
             }

            /**
             * setter for allowOverrides
             */
            bool WSF_CALL
            AviaryJob::SubmitJob::setAllowOverrides(
                    bool  arg_AllowOverrides)
             {
                

                if(isValidAllowOverrides &&
                        arg_AllowOverrides == property_AllowOverrides)
                {
                    
                    return true;
                }

                

                
                resetAllowOverrides();

                
                        property_AllowOverrides = arg_AllowOverrides;
                        isValidAllowOverrides = true;
                    
                return true;
             }

             

           /**
            * resetter for allowOverrides
            */
           bool WSF_CALL
           AviaryJob::SubmitJob::resetAllowOverrides()
           {
               int i = 0;
               int count = 0;


               
               isValidAllowOverrides = false; 
               return true;
           }

           /**
            * Check whether allowOverrides is nill
            */
           bool WSF_CALL
           AviaryJob::SubmitJob::isAllowOverridesNil()
           {
               return !isValidAllowOverrides;
           }

           /**
            * Set allowOverrides to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryJob::SubmitJob::setAllowOverridesNil()
           {
               return resetAllowOverrides();
           }

           

