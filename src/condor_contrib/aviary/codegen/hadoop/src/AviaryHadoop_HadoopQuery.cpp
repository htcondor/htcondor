

        /**
         * HadoopQuery.cpp
         *
         * This file was auto-generated from WSDL
         * by the Apache Axis2/C version: SNAPSHOT  Built on : Mar 10, 2008 (08:35:52 GMT+00:00)
         */
        
            #include "AviaryHadoop_HadoopQuery.h"
          

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
                 * name = HadoopQuery
                 * Namespace URI = http://hadoop.aviary.grid.redhat.com
                 * Namespace Prefix = ns2
                 */
           AviaryHadoop::HadoopQuery::HadoopQuery()
        {

        
                property_Refs  = NULL;
              
            isValidRefs  = false;
        
        }

       AviaryHadoop::HadoopQuery::HadoopQuery(std::vector<AviaryHadoop::HadoopID*>* arg_Refs)
        {
             
               property_Refs  = NULL;
             
            isValidRefs  = true;
            
                    property_Refs = arg_Refs;
            
        }
        AviaryHadoop::HadoopQuery::~HadoopQuery()
        {
            resetAll();
        }

        bool WSF_CALL AviaryHadoop::HadoopQuery::resetAll()
        {
            //calls reset method for all the properties owned by this method which are pointers.

            
             resetRefs();//AviaryHadoop::HadoopID
            return true;

        }

        

        bool WSF_CALL
        AviaryHadoop::HadoopQuery::deserialize(axiom_node_t** dp_parent,bool *dp_is_early_node_valid, bool dont_care_minoccurs)
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
                     * building Refs array
                     */
                       std::vector<AviaryHadoop::HadoopID*>* arr_list =new std::vector<AviaryHadoop::HadoopID*>();
                   

                     
                     /*
                      * building refs element
                      */
                     
                     
                     
                                    element_qname = axutil_qname_create(Environment::getEnv(), "refs", NULL, NULL);
                                  
                               
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

                                  if (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("refs", axiom_element_get_localname(current_element, Environment::getEnv())))
                                  {
                                  
                                      is_early_node_valid = true;
                                      
                                     AviaryHadoop::HadoopID* element = new AviaryHadoop::HadoopID();
                                          
                                          status =  element->deserialize(&current_node, &is_early_node_valid, false);
                                          
                                          if(AXIS2_FAILURE ==  status)
                                          {
					  WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "failed in building element refs ");
                                          }
                                          else
                                          {
                                            arr_list->push_back(element);
                                            
                                          }
                                        
                                     if(AXIS2_FAILURE ==  status)
                                     {
                                         WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "failed in setting the value for refs ");
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
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"refs (@minOccurs = '0') only have %d elements", i);
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
                                    status = setRefs(arr_list);
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
          AviaryHadoop::HadoopQuery::isParticle()
          {
            
                 return false;
              
          }


          void WSF_CALL
          AviaryHadoop::HadoopQuery::declareParentNamespaces(
                    axiom_element_t *parent_element,
                    axutil_hash_t *namespaces, int *next_ns_index)
          {
            
                  /* Here this is an empty function, Nothing to declare */
                 
          }

        
        
        axiom_node_t* WSF_CALL
	AviaryHadoop::HadoopQuery::serialize(axiom_node_t *parent, 
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
                      

                   if (!isValidRefs)
                   {
                      
                           /* no need to complain for minoccurs=0 element */
                            
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("refs"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("refs")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     /*
                      * Parsing Refs array
                      */
                     if (property_Refs != NULL)
                     {
                        

                            sprintf(start_input_str, "<%s%srefs",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                            
                         start_input_str_len = axutil_strlen(start_input_str);

                         sprintf(end_input_str, "</%s%srefs>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                         end_input_str_len = axutil_strlen(end_input_str);

                         count = property_Refs->size();
                         for(i = 0; i < count; i++)
                         {
                            AviaryHadoop::HadoopID* element = (*property_Refs)[i];

                            if(NULL == element) 
                            {
                                continue;
                            }

                    
                     
                     /*
                      * parsing refs element
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
             * Getter for refs by  Property Number 1
             */
            std::vector<AviaryHadoop::HadoopID*>* WSF_CALL
            AviaryHadoop::HadoopQuery::getProperty1()
            {
                return getRefs();
            }

            /**
             * getter for refs.
             */
            std::vector<AviaryHadoop::HadoopID*>* WSF_CALL
            AviaryHadoop::HadoopQuery::getRefs()
             {
                return property_Refs;
             }

            /**
             * setter for refs
             */
            bool WSF_CALL
            AviaryHadoop::HadoopQuery::setRefs(
                    std::vector<AviaryHadoop::HadoopID*>*  arg_Refs)
             {
                
                 int size = 0;
                 int i = 0;
                 bool non_nil_exists = false;
                

                if(isValidRefs &&
                        arg_Refs == property_Refs)
                {
                    
                    return true;
                }

                
                 size = arg_Refs->size();
                 
                 if (size < 0)
                 {
                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"refs has less than minOccurs(0)");
                     return false;
                 }
                 for(i = 0; i < size; i ++ )
                 {
                     if(NULL != (*arg_Refs)[i])
                     {
                         non_nil_exists = true;
                         break;
                     }
                 }

                 

                
                resetRefs();

                
                    if(NULL == arg_Refs)
                         
                {
                    /* We are already done */
                    return true;
                }
                
                        property_Refs = arg_Refs;
                        if(non_nil_exists)
                        {
                            isValidRefs = true;
                        }
                        
                    
                return true;
             }

            
            /**
             * Get ith element of refs.
             */
            AviaryHadoop::HadoopID* WSF_CALL
            AviaryHadoop::HadoopQuery::getRefsAt(int i)
            {
                AviaryHadoop::HadoopID* ret_val;
                if(property_Refs == NULL)
                {
                    return (AviaryHadoop::HadoopID*)0;
                }
                ret_val =   (*property_Refs)[i];
                
                    return ret_val;
                  
            }

            /**
             * Set the ith element of refs.
             */
           bool WSF_CALL
            AviaryHadoop::HadoopQuery::setRefsAt(int i,
                    AviaryHadoop::HadoopID* arg_Refs)
            {
                 AviaryHadoop::HadoopID* element;
                int size = 0;

                int non_nil_count;
                bool non_nil_exists = false;

                 

                if( isValidRefs &&
                    property_Refs &&
                  
                    arg_Refs == (*property_Refs)[i])
                  
                 {
                    
                    return AXIS2_SUCCESS; 
                }

                   
                     non_nil_exists = true;
                  

                if(property_Refs == NULL)
                {
                    property_Refs = new std::vector<AviaryHadoop::HadoopID*>();
                }
                else{
                /* check whether there already exist an element */
                element = (*property_Refs)[i];
                }

                
                        if(NULL != element)
                        {
                          
                          
                          
                                delete element;
                             
                        }
                        
                    
                    if(!non_nil_exists)
                    {
                        
                        isValidRefs = true;
                        (*property_Refs)[i]= NULL;
                        
                        return AXIS2_SUCCESS;
                    }
                
                    (*property_Refs)[i] = arg_Refs;
                  

               isValidRefs = true;
                
                return AXIS2_SUCCESS;
            }

            /**
             * Add to refs.
             */
            bool WSF_CALL
            AviaryHadoop::HadoopQuery::addRefs(
                    AviaryHadoop::HadoopID* arg_Refs)
             {

                
                    if( NULL == arg_Refs
                     )
                    {
                      
                           return true; 
                        
                    }
                  

                if(property_Refs == NULL)
                {
                    property_Refs = new std::vector<AviaryHadoop::HadoopID*>();
                }
              
               property_Refs->push_back(arg_Refs);
              
                isValidRefs = true;
                return true;
             }

            /**
             * Get the size of the refs array.
             */
            int WSF_CALL
            AviaryHadoop::HadoopQuery::sizeofRefs()
            {

                if(property_Refs == NULL)
                {
                    return 0;
                }
                return property_Refs->size();
            }

            /**
             * remove the ith element, same as set_nil_at.
             */
            bool WSF_CALL
            AviaryHadoop::HadoopQuery::removeRefsAt(int i)
            {
                return setRefsNilAt(i);
            }

            

           /**
            * resetter for refs
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQuery::resetRefs()
           {
               int i = 0;
               int count = 0;


               
                if (property_Refs != NULL)
                {
                  std::vector<AviaryHadoop::HadoopID*>::iterator it =  property_Refs->begin();
                  for( ; it <  property_Refs->end() ; ++it)
                  {
                     AviaryHadoop::HadoopID* element = *it;
                
            
                

                if(element != NULL)
                {
                   
                   
                         delete  element;
                     

                   }

                
                
                
               }

             }
                
                    if(NULL != property_Refs)
                 delete property_Refs;
                
               isValidRefs = false; 
               return true;
           }

           /**
            * Check whether refs is nill
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQuery::isRefsNil()
           {
               return !isValidRefs;
           }

           /**
            * Set refs to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQuery::setRefsNil()
           {
               return resetRefs();
           }

           
           /**
            * Check whether refs is nill at i
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQuery::isRefsNilAt(int i)
           {
               return (isValidRefs == false ||
                       NULL == property_Refs ||
                     NULL == (*property_Refs)[i]);
            }

           /**
            * Set refs to nil at i
            */
           bool WSF_CALL
           AviaryHadoop::HadoopQuery::setRefsNilAt(int i)
           {
                int size = 0;
                int j;
                bool non_nil_exists = false;

                int k = 0;

                if(property_Refs == NULL ||
                            isValidRefs == false)
                {
                    
                    non_nil_exists = false;
                }
                else
                {
                    size = property_Refs->size();
                    for(j = 0, k = 0; j < size; j ++ )
                    {
                        if(i == j) continue; 
                        if(NULL != (*property_Refs)[i])
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
                       WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "Size of the array of refs is beinng set to be smaller than the specificed number of minOccurs(0)");
                       return AXIS2_FAILURE;
                }
 
                if(property_Refs == NULL)
                {
                    isValidRefs = false;
                    
                    return true;
                }
                 
                 /* check whether there already exist an element */
                 AviaryHadoop::HadoopID* element = (*property_Refs)[i];
                if(NULL != element)
                {
                  
                  
                  
                        delete element;
                     
                 }
                 
                    if(!non_nil_exists)
                    {
                        
                        isValidRefs = false;
                        (*property_Refs)[i] = NULL;
                        return AXIS2_SUCCESS;
                    }
                

                
                (*property_Refs)[i] = NULL;
                
                return AXIS2_SUCCESS;

           }

           

