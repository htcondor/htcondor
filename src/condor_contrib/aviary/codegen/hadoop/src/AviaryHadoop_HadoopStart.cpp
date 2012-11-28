

        /**
         * HadoopStart.cpp
         *
         * This file was auto-generated from WSDL
         * by the Apache Axis2/C version: SNAPSHOT  Built on : Mar 10, 2008 (08:35:52 GMT+00:00)
         */
        
            #include "AviaryHadoop_HadoopStart.h"
          

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
                 * name = HadoopStart
                 * Namespace URI = http://hadoop.aviary.grid.redhat.com
                 * Namespace Prefix = ns2
                 */
           AviaryHadoop::HadoopStart::HadoopStart()
        {

        
                property_Ref  = NULL;
              
            isValidRef  = false;
        
            isValidCount  = false;
        
        }

       AviaryHadoop::HadoopStart::HadoopStart(AviaryHadoop::HadoopID* arg_Ref,int arg_Count)
        {
             
               property_Ref  = NULL;
             
            isValidRef  = true;
            
            isValidCount  = true;
            
                    property_Ref = arg_Ref;
            
                    property_Count = arg_Count;
            
        }
        AviaryHadoop::HadoopStart::~HadoopStart()
        {
            resetAll();
        }

        bool WSF_CALL AviaryHadoop::HadoopStart::resetAll()
        {
            //calls reset method for all the properties owned by this method which are pointers.

            
             resetRef();//AviaryHadoop::HadoopID
            return true;

        }

        

        bool WSF_CALL
        AviaryHadoop::HadoopStart::deserialize(axiom_node_t** dp_parent,bool *dp_is_early_node_valid, bool dont_care_minoccurs)
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
                      * building count element
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
                                 
                                 element_qname = axutil_qname_create(Environment::getEnv(), "count", NULL, NULL);
                                 

                           if ( 
                                (current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("count", axiom_element_get_localname(current_element, Environment::getEnv())))))
                           {
                              if( current_node   && current_element && (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("count", axiom_element_get_localname(current_element, Environment::getEnv()))))
                              {
                                is_early_node_valid = true;
                              }
                              
                                 
                                      text_value = axiom_element_get_text(current_element, Environment::getEnv(), current_node);
                                      if(text_value != NULL)
                                      {
                                            status = setCount(atoi(text_value));
                                      }
                                      
                                      else
                                      {
                                          WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "NULL value is set to a non nillable element count");
                                          status = AXIS2_FAILURE;
                                      }
                                      
                                 if(AXIS2_FAILURE ==  status)
                                 {
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"failed in setting the value for count ");
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
				  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "non nillable or minOuccrs != 0 element count missing");
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
          AviaryHadoop::HadoopStart::isParticle()
          {
            
                 return false;
              
          }


          void WSF_CALL
          AviaryHadoop::HadoopStart::declareParentNamespaces(
                    axiom_element_t *parent_element,
                    axutil_hash_t *namespaces, int *next_ns_index)
          {
            
                  /* Here this is an empty function, Nothing to declare */
                 
          }

        
        
        axiom_node_t* WSF_CALL
	AviaryHadoop::HadoopStart::serialize(axiom_node_t *parent, 
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
                      

                   if (!isValidCount)
                   {
                      
                            
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Nil value found in non-nillable property count");
                            return NULL;
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("count"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("count")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     
                     /*
                      * parsing count element
                      */

                    
                    
                            sprintf(start_input_str, "<%s%scount>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                            
                        start_input_str_len = axutil_strlen(start_input_str);
                        sprintf(end_input_str, "</%s%scount>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                        end_input_str_len = axutil_strlen(end_input_str);
                    
                               sprintf (text_value_2, AXIS2_PRINTF_INT32_FORMAT_SPECIFIER, property_Count);
                             
                           axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                           
                           axutil_stream_write(stream, Environment::getEnv(), text_value_2, axutil_strlen(text_value_2));
                           
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
            AviaryHadoop::HadoopStart::getProperty1()
            {
                return getRef();
            }

            /**
             * getter for ref.
             */
            AviaryHadoop::HadoopID* WSF_CALL
            AviaryHadoop::HadoopStart::getRef()
             {
                return property_Ref;
             }

            /**
             * setter for ref
             */
            bool WSF_CALL
            AviaryHadoop::HadoopStart::setRef(
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
           AviaryHadoop::HadoopStart::resetRef()
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
           AviaryHadoop::HadoopStart::isRefNil()
           {
               return !isValidRef;
           }

           /**
            * Set ref to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryHadoop::HadoopStart::setRefNil()
           {
               return resetRef();
           }

           

            /**
             * Getter for count by  Property Number 2
             */
            int WSF_CALL
            AviaryHadoop::HadoopStart::getProperty2()
            {
                return getCount();
            }

            /**
             * getter for count.
             */
            int WSF_CALL
            AviaryHadoop::HadoopStart::getCount()
             {
                return property_Count;
             }

            /**
             * setter for count
             */
            bool WSF_CALL
            AviaryHadoop::HadoopStart::setCount(
                    const int  arg_Count)
             {
                

                if(isValidCount &&
                        arg_Count == property_Count)
                {
                    
                    return true;
                }

                

                
                resetCount();

                
                        property_Count = arg_Count;
                        isValidCount = true;
                    
                return true;
             }

             

           /**
            * resetter for count
            */
           bool WSF_CALL
           AviaryHadoop::HadoopStart::resetCount()
           {
               int i = 0;
               int count = 0;


               
               isValidCount = false; 
               return true;
           }

           /**
            * Check whether count is nill
            */
           bool WSF_CALL
           AviaryHadoop::HadoopStart::isCountNil()
           {
               return !isValidCount;
           }

           /**
            * Set count to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryHadoop::HadoopStart::setCountNil()
           {
               return resetCount();
           }

           

