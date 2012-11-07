

        /**
         * Attributes.cpp
         *
         * This file was auto-generated from WSDL
         * by the Apache Axis2/C version: SNAPSHOT  Built on : Mar 10, 2008 (08:35:52 GMT+00:00)
         */
        
            #include "AviaryCommon_Attributes.h"
          

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
        
        using namespace AviaryCommon;
        
                /*
                 * This type was generated from the piece of schema that had
                 * name = Attributes
                 * Namespace URI = http://common.aviary.grid.redhat.com
                 * Namespace Prefix = ns1
                 */
           AviaryCommon::Attributes::Attributes()
        {

        
                property_Attrs  = NULL;
              
            isValidAttrs  = false;
        
        }

       AviaryCommon::Attributes::Attributes(std::vector<AviaryCommon::Attribute*>* arg_Attrs)
        {
             
               property_Attrs  = NULL;
             
            isValidAttrs  = true;
            
                    property_Attrs = arg_Attrs;
            
        }
        AviaryCommon::Attributes::~Attributes()
        {
            resetAll();
        }

        bool WSF_CALL AviaryCommon::Attributes::resetAll()
        {
            //calls reset method for all the properties owned by this method which are pointers.

            
             resetAttrs();//AviaryCommon::Attribute
            return true;

        }

        

        bool WSF_CALL
        AviaryCommon::Attributes::deserialize(axiom_node_t** dp_parent,bool *dp_is_early_node_valid, bool dont_care_minoccurs)
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
                      
                    
                       { 
                    /*
                     * building Attrs array
                     */
                       std::vector<AviaryCommon::Attribute*>* arr_list =new std::vector<AviaryCommon::Attribute*>();
                   

                     
                     /*
                      * building attrs element
                      */
                     
                     
                     
                                    element_qname = axutil_qname_create(Environment::getEnv(), "attrs", NULL, NULL);
                                  
                               
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

                                  if (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("attrs", axiom_element_get_localname(current_element, Environment::getEnv())))
                                  {
                                  
                                      is_early_node_valid = true;
                                      
                                     AviaryCommon::Attribute* element = new AviaryCommon::Attribute();
                                          
                                          status =  element->deserialize(&current_node, &is_early_node_valid, false);
                                          
                                          if(AXIS2_FAILURE ==  status)
                                          {
					  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "failed in building element attrs ");
                                          }
                                          else
                                          {
                                            arr_list->push_back(element);
                                            
                                          }
                                        
                                     if(AXIS2_FAILURE ==  status)
                                     {
                                         WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "failed in setting the value for attrs ");
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
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"attrs (@minOccurs = '0') only have %d elements", i);
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
                                    status = setAttrs(arr_list);
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
          AviaryCommon::Attributes::isParticle()
          {
            
                 return false;
              
          }


          void WSF_CALL
          AviaryCommon::Attributes::declareParentNamespaces(
                    axiom_element_t *parent_element,
                    axutil_hash_t *namespaces, int *next_ns_index)
          {
            
                  /* Here this is an empty function, Nothing to declare */
                 
          }

        
        
        axiom_node_t* WSF_CALL
	AviaryCommon::Attributes::serialize(axiom_node_t *parent, 
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
                      

                   if (!isValidAttrs)
                   {
                      
                           /* no need to complain for minoccurs=0 element */
                            
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("attrs"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("attrs")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     /*
                      * Parsing Attrs array
                      */
                     if (property_Attrs != NULL)
                     {
                        

                            sprintf(start_input_str, "<%s%sattrs",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                            
                         start_input_str_len = axutil_strlen(start_input_str);

                         sprintf(end_input_str, "</%s%sattrs>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                         end_input_str_len = axutil_strlen(end_input_str);

                         count = property_Attrs->size();
                         for(i = 0; i < count; i++)
                         {
                            AviaryCommon::Attribute* element = (*property_Attrs)[i];

                            if(NULL == element) 
                            {
                                continue;
                            }

                    
                     
                     /*
                      * parsing attrs element
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
             * Getter for attrs by  Property Number 1
             */
            std::vector<AviaryCommon::Attribute*>* WSF_CALL
            AviaryCommon::Attributes::getProperty1()
            {
                return getAttrs();
            }

            /**
             * getter for attrs.
             */
            std::vector<AviaryCommon::Attribute*>* WSF_CALL
            AviaryCommon::Attributes::getAttrs()
             {
                return property_Attrs;
             }

            /**
             * setter for attrs
             */
            bool WSF_CALL
            AviaryCommon::Attributes::setAttrs(
                    std::vector<AviaryCommon::Attribute*>*  arg_Attrs)
             {
                
                 int size = 0;
                 int i = 0;
                 bool non_nil_exists = false;
                

                if(isValidAttrs &&
                        arg_Attrs == property_Attrs)
                {
                    
                    return true;
                }

                
                 size = arg_Attrs->size();
                 
                 if (size < 0)
                 {
                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"attrs has less than minOccurs(0)");
                     return false;
                 }
                 for(i = 0; i < size; i ++ )
                 {
                     if(NULL != (*arg_Attrs)[i])
                     {
                         non_nil_exists = true;
                         break;
                     }
                 }

                 

                
                resetAttrs();

                
                    if(NULL == arg_Attrs)
                         
                {
                    /* We are already done */
                    return true;
                }
                
                        property_Attrs = arg_Attrs;
                        if(non_nil_exists)
                        {
                            isValidAttrs = true;
                        }
                        
                    
                return true;
             }

            
            /**
             * Get ith element of attrs.
             */
            AviaryCommon::Attribute* WSF_CALL
            AviaryCommon::Attributes::getAttrsAt(int i)
            {
                AviaryCommon::Attribute* ret_val;
                if(property_Attrs == NULL)
                {
                    return (AviaryCommon::Attribute*)0;
                }
                ret_val =   (*property_Attrs)[i];
                
                    return ret_val;
                  
            }

            /**
             * Set the ith element of attrs.
             */
           bool WSF_CALL
            AviaryCommon::Attributes::setAttrsAt(int i,
                    AviaryCommon::Attribute* arg_Attrs)
            {
                 AviaryCommon::Attribute* element;
                int size = 0;

                int non_nil_count;
                bool non_nil_exists = false;

                 

                if( isValidAttrs &&
                    property_Attrs &&
                  
                    arg_Attrs == (*property_Attrs)[i])
                  
                 {
                    
                    return AXIS2_SUCCESS; 
                }

                   
                     non_nil_exists = true;
                  

                if(property_Attrs == NULL)
                {
                    property_Attrs = new std::vector<AviaryCommon::Attribute*>();
                }
                else{
                /* check whether there already exist an element */
                element = (*property_Attrs)[i];
                }

                
                        if(NULL != element)
                        {
                          
                          
                          
                                delete element;
                             
                        }
                        
                    
                    if(!non_nil_exists)
                    {
                        
                        isValidAttrs = true;
                        (*property_Attrs)[i]= NULL;
                        
                        return AXIS2_SUCCESS;
                    }
                
                    (*property_Attrs)[i] = arg_Attrs;
                  

               isValidAttrs = true;
                
                return AXIS2_SUCCESS;
            }

            /**
             * Add to attrs.
             */
            bool WSF_CALL
            AviaryCommon::Attributes::addAttrs(
                    AviaryCommon::Attribute* arg_Attrs)
             {

                
                    if( NULL == arg_Attrs
                     )
                    {
                      
                           return true; 
                        
                    }
                  

                if(property_Attrs == NULL)
                {
                    property_Attrs = new std::vector<AviaryCommon::Attribute*>();
                }
              
               property_Attrs->push_back(arg_Attrs);
              
                isValidAttrs = true;
                return true;
             }

            /**
             * Get the size of the attrs array.
             */
            int WSF_CALL
            AviaryCommon::Attributes::sizeofAttrs()
            {

                if(property_Attrs == NULL)
                {
                    return 0;
                }
                return property_Attrs->size();
            }

            /**
             * remove the ith element, same as set_nil_at.
             */
            bool WSF_CALL
            AviaryCommon::Attributes::removeAttrsAt(int i)
            {
                return setAttrsNilAt(i);
            }

            

           /**
            * resetter for attrs
            */
           bool WSF_CALL
           AviaryCommon::Attributes::resetAttrs()
           {
               int i = 0;
               int count = 0;


               
                if (property_Attrs != NULL)
                {
                  std::vector<AviaryCommon::Attribute*>::iterator it =  property_Attrs->begin();
                  for( ; it <  property_Attrs->end() ; ++it)
                  {
                     AviaryCommon::Attribute* element = *it;
                
            
                

                if(element != NULL)
                {
                   
                   
                         delete  element;
                     

                   }

                
                
                
               }

             }
                
                    if(NULL != property_Attrs)
                 delete property_Attrs;
                
               isValidAttrs = false; 
               return true;
           }

           /**
            * Check whether attrs is nill
            */
           bool WSF_CALL
           AviaryCommon::Attributes::isAttrsNil()
           {
               return !isValidAttrs;
           }

           /**
            * Set attrs to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCommon::Attributes::setAttrsNil()
           {
               return resetAttrs();
           }

           
           /**
            * Check whether attrs is nill at i
            */
           bool WSF_CALL
           AviaryCommon::Attributes::isAttrsNilAt(int i)
           {
               return (isValidAttrs == false ||
                       NULL == property_Attrs ||
                     NULL == (*property_Attrs)[i]);
            }

           /**
            * Set attrs to nil at i
            */
           bool WSF_CALL
           AviaryCommon::Attributes::setAttrsNilAt(int i)
           {
                int size = 0;
                int j;
                bool non_nil_exists = false;

                int k = 0;

                if(property_Attrs == NULL ||
                            isValidAttrs == false)
                {
                    
                    non_nil_exists = false;
                }
                else
                {
                    size = property_Attrs->size();
                    for(j = 0, k = 0; j < size; j ++ )
                    {
                        if(i == j) continue; 
                        if(NULL != (*property_Attrs)[i])
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
                       WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "Size of the array of attrs is beinng set to be smaller than the specificed number of minOccurs(0)");
                       return AXIS2_FAILURE;
                }
 
                if(property_Attrs == NULL)
                {
                    isValidAttrs = false;
                    
                    return true;
                }
                 
                 /* check whether there already exist an element */
                 AviaryCommon::Attribute* element = (*property_Attrs)[i];
                if(NULL != element)
                {
                  
                  
                  
                        delete element;
                     
                 }
                 
                    if(!non_nil_exists)
                    {
                        
                        isValidAttrs = false;
                        (*property_Attrs)[i] = NULL;
                        return AXIS2_SUCCESS;
                    }
                

                
                (*property_Attrs)[i] = NULL;
                
                return AXIS2_SUCCESS;

           }

           

