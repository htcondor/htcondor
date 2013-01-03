

        /**
         * Slot.cpp
         *
         * This file was auto-generated from WSDL
         * by the Apache Axis2/C version: SNAPSHOT  Built on : Mar 10, 2008 (08:35:52 GMT+00:00)
         */
        
            #include "AviaryCommon_Slot.h"
          

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
                 * name = Slot
                 * Namespace URI = http://common.aviary.grid.redhat.com
                 * Namespace Prefix = ns1
                 */
           AviaryCommon::Slot::Slot()
        {

        
                property_Id  = NULL;
              
            isValidId  = false;
        
                property_Status  = NULL;
              
            isValidStatus  = false;
        
                property_Slot_type  = NULL;
              
            isValidSlot_type  = false;
        
                property_Summary  = NULL;
              
            isValidSummary  = false;
        
                property_Dynamic_slots  = NULL;
              
            isValidDynamic_slots  = false;
        
        }

       AviaryCommon::Slot::Slot(AviaryCommon::ResourceID* arg_Id,AviaryCommon::Status* arg_Status,AviaryCommon::SlotType* arg_Slot_type,AviaryCommon::SlotSummary* arg_Summary,std::vector<AviaryCommon::Slot*>* arg_Dynamic_slots)
        {
             
               property_Id  = NULL;
             
            isValidId  = true;
            
               property_Status  = NULL;
             
            isValidStatus  = true;
            
               property_Slot_type  = NULL;
             
            isValidSlot_type  = true;
            
               property_Summary  = NULL;
             
            isValidSummary  = true;
            
               property_Dynamic_slots  = NULL;
             
            isValidDynamic_slots  = true;
            
                    property_Id = arg_Id;
            
                    property_Status = arg_Status;
            
                    property_Slot_type = arg_Slot_type;
            
                    property_Summary = arg_Summary;
            
                    property_Dynamic_slots = arg_Dynamic_slots;
            
        }
        AviaryCommon::Slot::~Slot()
        {
            resetAll();
        }

        bool WSF_CALL AviaryCommon::Slot::resetAll()
        {
            //calls reset method for all the properties owned by this method which are pointers.

            
             resetId();//AviaryCommon::ResourceID
             resetStatus();//AviaryCommon::Status
             resetSlot_type();//AviaryCommon::SlotType
             resetSummary();//AviaryCommon::SlotSummary
             resetDynamic_slots();//AviaryCommon::Slot
            return true;

        }

        

        bool WSF_CALL
        AviaryCommon::Slot::deserialize(axiom_node_t** dp_parent,bool *dp_is_early_node_valid, bool dont_care_minoccurs)
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
                      * building slot_type element
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
                                 
                                 element_qname = axutil_qname_create(Environment::getEnv(), "slot_type", NULL, NULL);
                                 

                           if (isParticle() ||  
                                (current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("slot_type", axiom_element_get_localname(current_element, Environment::getEnv())))))
                           {
                              if( current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("slot_type", axiom_element_get_localname(current_element, Environment::getEnv()))))
                              {
                                is_early_node_valid = true;
                              }
                              
                                 AviaryCommon::SlotType* element = new AviaryCommon::SlotType();

                                      status =  element->deserialize(&current_node, &is_early_node_valid, false);
                                      if(AXIS2_FAILURE == status)
                                      {
                                          WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "failed in building adb object for element slot_type");
                                      }
                                      else
                                      {
                                          status = setSlot_type(element);
                                      }
                                    
                                 if(AXIS2_FAILURE ==  status)
                                 {
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"failed in setting the value for slot_type ");
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
                      * building summary element
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
                                 
                                 element_qname = axutil_qname_create(Environment::getEnv(), "summary", NULL, NULL);
                                 

                           if (isParticle() ||  
                                (current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("summary", axiom_element_get_localname(current_element, Environment::getEnv())))))
                           {
                              if( current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("summary", axiom_element_get_localname(current_element, Environment::getEnv()))))
                              {
                                is_early_node_valid = true;
                              }
                              
                                 AviaryCommon::SlotSummary* element = new AviaryCommon::SlotSummary();

                                      status =  element->deserialize(&current_node, &is_early_node_valid, false);
                                      if(AXIS2_FAILURE == status)
                                      {
                                          WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "failed in building adb object for element summary");
                                      }
                                      else
                                      {
                                          status = setSummary(element);
                                      }
                                    
                                 if(AXIS2_FAILURE ==  status)
                                 {
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"failed in setting the value for summary ");
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
                     * building Dynamic_slots array
                     */
                       std::vector<AviaryCommon::Slot*>* arr_list =new std::vector<AviaryCommon::Slot*>();
                   

                     
                     /*
                      * building dynamic_slots element
                      */
                     
                     
                     
                                    element_qname = axutil_qname_create(Environment::getEnv(), "dynamic_slots", NULL, NULL);
                                  
                               
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

                                  if (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("dynamic_slots", axiom_element_get_localname(current_element, Environment::getEnv())))
                                  {
                                  
                                      is_early_node_valid = true;
                                      
                                     AviaryCommon::Slot* element = new AviaryCommon::Slot();
                                          
                                          status =  element->deserialize(&current_node, &is_early_node_valid, false);
                                          
                                          if(AXIS2_FAILURE ==  status)
                                          {
					  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "failed in building element dynamic_slots ");
                                          }
                                          else
                                          {
                                            arr_list->push_back(element);
                                            
                                          }
                                        
                                     if(AXIS2_FAILURE ==  status)
                                     {
                                         WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "failed in setting the value for dynamic_slots ");
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
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"dynamic_slots (@minOccurs = '0') only have %d elements", i);
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
                                    status = setDynamic_slots(arr_list);
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
          AviaryCommon::Slot::isParticle()
          {
            
                 return false;
              
          }


          void WSF_CALL
          AviaryCommon::Slot::declareParentNamespaces(
                    axiom_element_t *parent_element,
                    axutil_hash_t *namespaces, int *next_ns_index)
          {
            
                  /* Here this is an empty function, Nothing to declare */
                 
          }

        
        
        axiom_node_t* WSF_CALL
	AviaryCommon::Slot::serialize(axiom_node_t *parent, 
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
                    
                    axis2_char_t text_value_2[ADB_DEFAULT_DIGIT_LIMIT];
                    
                    axis2_char_t text_value_3[ADB_DEFAULT_DIGIT_LIMIT];
                    
                    axis2_char_t text_value_4[ADB_DEFAULT_DIGIT_LIMIT];
                    
                    axis2_char_t text_value_5[ADB_DEFAULT_DIGIT_LIMIT];
                    
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
                      

                   if (!isValidSlot_type)
                   {
                      
                           /* no need to complain for minoccurs=0 element */
                            
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("slot_type"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("slot_type")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     
                     /*
                      * parsing slot_type element
                      */

                    
                    
                            sprintf(start_input_str, "<%s%sslot_type",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":""); 
                            
                        start_input_str_len = axutil_strlen(start_input_str);
                        sprintf(end_input_str, "</%s%sslot_type>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                        end_input_str_len = axutil_strlen(end_input_str);
                     
                            if(!property_Slot_type->isParticle())
                            {
                                axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                            }
                            property_Slot_type->serialize(current_node, parent_element,
                                                                                 property_Slot_type->isParticle() || false, namespaces, next_ns_index);
                            
                            if(!property_Slot_type->isParticle())
                            {
                                axutil_stream_write(stream, Environment::getEnv(), end_input_str, end_input_str_len);
                            }
                            
                     
                     AXIS2_FREE(Environment::getEnv()->allocator,start_input_str);
                     AXIS2_FREE(Environment::getEnv()->allocator,end_input_str);
                 } 

                 
                       p_prefix = NULL;
                      

                   if (!isValidSummary)
                   {
                      
                           /* no need to complain for minoccurs=0 element */
                            
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("summary"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("summary")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     
                     /*
                      * parsing summary element
                      */

                    
                    
                            sprintf(start_input_str, "<%s%ssummary",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":""); 
                            
                        start_input_str_len = axutil_strlen(start_input_str);
                        sprintf(end_input_str, "</%s%ssummary>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                        end_input_str_len = axutil_strlen(end_input_str);
                     
                            if(!property_Summary->isParticle())
                            {
                                axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                            }
                            property_Summary->serialize(current_node, parent_element,
                                                                                 property_Summary->isParticle() || false, namespaces, next_ns_index);
                            
                            if(!property_Summary->isParticle())
                            {
                                axutil_stream_write(stream, Environment::getEnv(), end_input_str, end_input_str_len);
                            }
                            
                     
                     AXIS2_FREE(Environment::getEnv()->allocator,start_input_str);
                     AXIS2_FREE(Environment::getEnv()->allocator,end_input_str);
                 } 

                 
                       p_prefix = NULL;
                      

                   if (!isValidDynamic_slots)
                   {
                      
                           /* no need to complain for minoccurs=0 element */
                            
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("dynamic_slots"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("dynamic_slots")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     /*
                      * Parsing Dynamic_slots array
                      */
                     if (property_Dynamic_slots != NULL)
                     {
                        

                            sprintf(start_input_str, "<%s%sdynamic_slots",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                            
                         start_input_str_len = axutil_strlen(start_input_str);

                         sprintf(end_input_str, "</%s%sdynamic_slots>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                         end_input_str_len = axutil_strlen(end_input_str);

                         count = property_Dynamic_slots->size();
                         for(i = 0; i < count; i++)
                         {
                            AviaryCommon::Slot* element = (*property_Dynamic_slots)[i];

                            if(NULL == element) 
                            {
                                continue;
                            }

                    
                     
                     /*
                      * parsing dynamic_slots element
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

                 

            return parent;
        }


        

            /**
             * Getter for id by  Property Number 1
             */
            AviaryCommon::ResourceID* WSF_CALL
            AviaryCommon::Slot::getProperty1()
            {
                return getId();
            }

            /**
             * getter for id.
             */
            AviaryCommon::ResourceID* WSF_CALL
            AviaryCommon::Slot::getId()
             {
                return property_Id;
             }

            /**
             * setter for id
             */
            bool WSF_CALL
            AviaryCommon::Slot::setId(
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
           AviaryCommon::Slot::resetId()
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
           AviaryCommon::Slot::isIdNil()
           {
               return !isValidId;
           }

           /**
            * Set id to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCommon::Slot::setIdNil()
           {
               return resetId();
           }

           

            /**
             * Getter for status by  Property Number 2
             */
            AviaryCommon::Status* WSF_CALL
            AviaryCommon::Slot::getProperty2()
            {
                return getStatus();
            }

            /**
             * getter for status.
             */
            AviaryCommon::Status* WSF_CALL
            AviaryCommon::Slot::getStatus()
             {
                return property_Status;
             }

            /**
             * setter for status
             */
            bool WSF_CALL
            AviaryCommon::Slot::setStatus(
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
           AviaryCommon::Slot::resetStatus()
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
           AviaryCommon::Slot::isStatusNil()
           {
               return !isValidStatus;
           }

           /**
            * Set status to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCommon::Slot::setStatusNil()
           {
               return resetStatus();
           }

           

            /**
             * Getter for slot_type by  Property Number 3
             */
            AviaryCommon::SlotType* WSF_CALL
            AviaryCommon::Slot::getProperty3()
            {
                return getSlot_type();
            }

            /**
             * getter for slot_type.
             */
            AviaryCommon::SlotType* WSF_CALL
            AviaryCommon::Slot::getSlot_type()
             {
                return property_Slot_type;
             }

            /**
             * setter for slot_type
             */
            bool WSF_CALL
            AviaryCommon::Slot::setSlot_type(
                    AviaryCommon::SlotType*  arg_Slot_type)
             {
                

                if(isValidSlot_type &&
                        arg_Slot_type == property_Slot_type)
                {
                    
                    return true;
                }

                

                
                resetSlot_type();

                
                    if(NULL == arg_Slot_type)
                         
                {
                    /* We are already done */
                    return true;
                }
                
                        property_Slot_type = arg_Slot_type;
                        isValidSlot_type = true;
                    
                return true;
             }

             

           /**
            * resetter for slot_type
            */
           bool WSF_CALL
           AviaryCommon::Slot::resetSlot_type()
           {
               int i = 0;
               int count = 0;


               
            
                

                if(property_Slot_type != NULL)
                {
                   
                   
                         delete  property_Slot_type;
                     

                   }

                
                
                
               isValidSlot_type = false; 
               return true;
           }

           /**
            * Check whether slot_type is nill
            */
           bool WSF_CALL
           AviaryCommon::Slot::isSlot_typeNil()
           {
               return !isValidSlot_type;
           }

           /**
            * Set slot_type to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCommon::Slot::setSlot_typeNil()
           {
               return resetSlot_type();
           }

           

            /**
             * Getter for summary by  Property Number 4
             */
            AviaryCommon::SlotSummary* WSF_CALL
            AviaryCommon::Slot::getProperty4()
            {
                return getSummary();
            }

            /**
             * getter for summary.
             */
            AviaryCommon::SlotSummary* WSF_CALL
            AviaryCommon::Slot::getSummary()
             {
                return property_Summary;
             }

            /**
             * setter for summary
             */
            bool WSF_CALL
            AviaryCommon::Slot::setSummary(
                    AviaryCommon::SlotSummary*  arg_Summary)
             {
                

                if(isValidSummary &&
                        arg_Summary == property_Summary)
                {
                    
                    return true;
                }

                

                
                resetSummary();

                
                    if(NULL == arg_Summary)
                         
                {
                    /* We are already done */
                    return true;
                }
                
                        property_Summary = arg_Summary;
                        isValidSummary = true;
                    
                return true;
             }

             

           /**
            * resetter for summary
            */
           bool WSF_CALL
           AviaryCommon::Slot::resetSummary()
           {
               int i = 0;
               int count = 0;


               
            
                

                if(property_Summary != NULL)
                {
                   
                   
                         delete  property_Summary;
                     

                   }

                
                
                
               isValidSummary = false; 
               return true;
           }

           /**
            * Check whether summary is nill
            */
           bool WSF_CALL
           AviaryCommon::Slot::isSummaryNil()
           {
               return !isValidSummary;
           }

           /**
            * Set summary to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCommon::Slot::setSummaryNil()
           {
               return resetSummary();
           }

           

            /**
             * Getter for dynamic_slots by  Property Number 5
             */
            std::vector<AviaryCommon::Slot*>* WSF_CALL
            AviaryCommon::Slot::getProperty5()
            {
                return getDynamic_slots();
            }

            /**
             * getter for dynamic_slots.
             */
            std::vector<AviaryCommon::Slot*>* WSF_CALL
            AviaryCommon::Slot::getDynamic_slots()
             {
                return property_Dynamic_slots;
             }

            /**
             * setter for dynamic_slots
             */
            bool WSF_CALL
            AviaryCommon::Slot::setDynamic_slots(
                    std::vector<AviaryCommon::Slot*>*  arg_Dynamic_slots)
             {
                
                 int size = 0;
                 int i = 0;
                 bool non_nil_exists = false;
                

                if(isValidDynamic_slots &&
                        arg_Dynamic_slots == property_Dynamic_slots)
                {
                    
                    return true;
                }

                
                 size = arg_Dynamic_slots->size();
                 
                 if (size < 0)
                 {
                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"dynamic_slots has less than minOccurs(0)");
                     return false;
                 }
                 for(i = 0; i < size; i ++ )
                 {
                     if(NULL != (*arg_Dynamic_slots)[i])
                     {
                         non_nil_exists = true;
                         break;
                     }
                 }

                 

                
                resetDynamic_slots();

                
                    if(NULL == arg_Dynamic_slots)
                         
                {
                    /* We are already done */
                    return true;
                }
                
                        property_Dynamic_slots = arg_Dynamic_slots;
                        if(non_nil_exists)
                        {
                            isValidDynamic_slots = true;
                        }
                        
                    
                return true;
             }

            
            /**
             * Get ith element of dynamic_slots.
             */
            AviaryCommon::Slot* WSF_CALL
            AviaryCommon::Slot::getDynamic_slotsAt(int i)
            {
                AviaryCommon::Slot* ret_val;
                if(property_Dynamic_slots == NULL)
                {
                    return (AviaryCommon::Slot*)0;
                }
                ret_val =   (*property_Dynamic_slots)[i];
                
                    return ret_val;
                  
            }

            /**
             * Set the ith element of dynamic_slots.
             */
           bool WSF_CALL
            AviaryCommon::Slot::setDynamic_slotsAt(int i,
                    AviaryCommon::Slot* arg_Dynamic_slots)
            {
                 AviaryCommon::Slot* element;
                int size = 0;

                int non_nil_count;
                bool non_nil_exists = false;

                 

                if( isValidDynamic_slots &&
                    property_Dynamic_slots &&
                  
                    arg_Dynamic_slots == (*property_Dynamic_slots)[i])
                  
                 {
                    
                    return AXIS2_SUCCESS; 
                }

                   
                     non_nil_exists = true;
                  

                if(property_Dynamic_slots == NULL)
                {
                    property_Dynamic_slots = new std::vector<AviaryCommon::Slot*>();
                }
                else{
                /* check whether there already exist an element */
                element = (*property_Dynamic_slots)[i];
                }

                
                        if(NULL != element)
                        {
                          
                          
                          
                                delete element;
                             
                        }
                        
                    
                    if(!non_nil_exists)
                    {
                        
                        isValidDynamic_slots = true;
                        (*property_Dynamic_slots)[i]= NULL;
                        
                        return AXIS2_SUCCESS;
                    }
                
                    (*property_Dynamic_slots)[i] = arg_Dynamic_slots;
                  

               isValidDynamic_slots = true;
                
                return AXIS2_SUCCESS;
            }

            /**
             * Add to dynamic_slots.
             */
            bool WSF_CALL
            AviaryCommon::Slot::addDynamic_slots(
                    AviaryCommon::Slot* arg_Dynamic_slots)
             {

                
                    if( NULL == arg_Dynamic_slots
                     )
                    {
                      
                           return true; 
                        
                    }
                  

                if(property_Dynamic_slots == NULL)
                {
                    property_Dynamic_slots = new std::vector<AviaryCommon::Slot*>();
                }
              
               property_Dynamic_slots->push_back(arg_Dynamic_slots);
              
                isValidDynamic_slots = true;
                return true;
             }

            /**
             * Get the size of the dynamic_slots array.
             */
            int WSF_CALL
            AviaryCommon::Slot::sizeofDynamic_slots()
            {

                if(property_Dynamic_slots == NULL)
                {
                    return 0;
                }
                return property_Dynamic_slots->size();
            }

            /**
             * remove the ith element, same as set_nil_at.
             */
            bool WSF_CALL
            AviaryCommon::Slot::removeDynamic_slotsAt(int i)
            {
                return setDynamic_slotsNilAt(i);
            }

            

           /**
            * resetter for dynamic_slots
            */
           bool WSF_CALL
           AviaryCommon::Slot::resetDynamic_slots()
           {
               int i = 0;
               int count = 0;


               
                if (property_Dynamic_slots != NULL)
                {
                  std::vector<AviaryCommon::Slot*>::iterator it =  property_Dynamic_slots->begin();
                  for( ; it <  property_Dynamic_slots->end() ; ++it)
                  {
                     AviaryCommon::Slot* element = *it;
                
            
                

                if(element != NULL)
                {
                   
                   
                         delete  element;
                     

                   }

                
                
                
               }

             }
                
                    if(NULL != property_Dynamic_slots)
                 delete property_Dynamic_slots;
                
               isValidDynamic_slots = false; 
               return true;
           }

           /**
            * Check whether dynamic_slots is nill
            */
           bool WSF_CALL
           AviaryCommon::Slot::isDynamic_slotsNil()
           {
               return !isValidDynamic_slots;
           }

           /**
            * Set dynamic_slots to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCommon::Slot::setDynamic_slotsNil()
           {
               return resetDynamic_slots();
           }

           
           /**
            * Check whether dynamic_slots is nill at i
            */
           bool WSF_CALL
           AviaryCommon::Slot::isDynamic_slotsNilAt(int i)
           {
               return (isValidDynamic_slots == false ||
                       NULL == property_Dynamic_slots ||
                     NULL == (*property_Dynamic_slots)[i]);
            }

           /**
            * Set dynamic_slots to nil at i
            */
           bool WSF_CALL
           AviaryCommon::Slot::setDynamic_slotsNilAt(int i)
           {
                int size = 0;
                int j;
                bool non_nil_exists = false;

                int k = 0;

                if(property_Dynamic_slots == NULL ||
                            isValidDynamic_slots == false)
                {
                    
                    non_nil_exists = false;
                }
                else
                {
                    size = property_Dynamic_slots->size();
                    for(j = 0, k = 0; j < size; j ++ )
                    {
                        if(i == j) continue; 
                        if(NULL != (*property_Dynamic_slots)[i])
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
                       WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "Size of the array of dynamic_slots is beinng set to be smaller than the specificed number of minOccurs(0)");
                       return AXIS2_FAILURE;
                }
 
                if(property_Dynamic_slots == NULL)
                {
                    isValidDynamic_slots = false;
                    
                    return true;
                }
                 
                 /* check whether there already exist an element */
                 AviaryCommon::Slot* element = (*property_Dynamic_slots)[i];
                if(NULL != element)
                {
                  
                  
                  
                        delete element;
                     
                 }
                 
                    if(!non_nil_exists)
                    {
                        
                        isValidDynamic_slots = false;
                        (*property_Dynamic_slots)[i] = NULL;
                        return AXIS2_SUCCESS;
                    }
                

                
                (*property_Dynamic_slots)[i] = NULL;
                
                return AXIS2_SUCCESS;

           }

           

