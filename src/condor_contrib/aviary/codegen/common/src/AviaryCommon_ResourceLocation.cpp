

        /**
         * ResourceLocation.cpp
         *
         * This file was auto-generated from WSDL
         * by the Apache Axis2/C version: SNAPSHOT  Built on : Mar 10, 2008 (08:35:52 GMT+00:00)
         */
        
            #include "AviaryCommon_ResourceLocation.h"
          

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
                 * name = ResourceLocation
                 * Namespace URI = http://common.aviary.grid.redhat.com
                 * Namespace Prefix = ns1
                 */
           AviaryCommon::ResourceLocation::ResourceLocation()
        {

        
                property_Id  = NULL;
              
            isValidId  = false;
        
                property_Location  = NULL;
              
            isValidLocation  = false;
        
        }

       AviaryCommon::ResourceLocation::ResourceLocation(AviaryCommon::ResourceID* arg_Id,std::vector<axutil_uri_t*>* arg_Location)
        {
             
               property_Id  = NULL;
             
            isValidId  = true;
            
               property_Location  = NULL;
             
            isValidLocation  = true;
            
                    property_Id = arg_Id;
            
                    property_Location = arg_Location;
            
        }
        AviaryCommon::ResourceLocation::~ResourceLocation()
        {
            resetAll();
        }

        bool WSF_CALL AviaryCommon::ResourceLocation::resetAll()
        {
            //calls reset method for all the properties owned by this method which are pointers.

            
             resetId();//AviaryCommon::ResourceID
             resetLocation();//axutil_uri_t*
            return true;

        }

        

        bool WSF_CALL
        AviaryCommon::ResourceLocation::deserialize(axiom_node_t** dp_parent,bool *dp_is_early_node_valid, bool dont_care_minoccurs)
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
                     * building Location array
                     */
                       std::vector<axutil_uri_t*>* arr_list =new std::vector<axutil_uri_t*>();
                   

                     
                     /*
                      * building location element
                      */
                     
                     
                     
                                    element_qname = axutil_qname_create(Environment::getEnv(), "location", NULL, NULL);
                                  
                               
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

                                  if (axutil_qname_equals(element_qname, Environment::getEnv(), mqname) || !axutil_strcmp("location", axiom_element_get_localname(current_element, Environment::getEnv())))
                                  {
                                  
                                      is_early_node_valid = true;
                                      
                                     
                                          text_value = axiom_element_get_text(current_element, Environment::getEnv(), current_node);
                                          if(text_value != NULL)
                                          {
                                              arr_list->push_back(axutil_uri_parse_string(Environment::getEnv(), text_value));
                                          }
                                          
                                          else
                                          {
					                        WSF_LOG_ERROR_MSG(Environment::getEnv()->log,WSF_LOG_SI, "NULL value is set to a non nillable element location");
                                                status = AXIS2_FAILURE;
                                          }
                                          
                                     if(AXIS2_FAILURE ==  status)
                                     {
                                         WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "failed in setting the value for location ");
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
                                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"location (@minOccurs = '0') only have %d elements", i);
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
                                    status = setLocation(arr_list);
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
          AviaryCommon::ResourceLocation::isParticle()
          {
            
                 return false;
              
          }


          void WSF_CALL
          AviaryCommon::ResourceLocation::declareParentNamespaces(
                    axiom_element_t *parent_element,
                    axutil_hash_t *namespaces, int *next_ns_index)
          {
            
                  /* Here this is an empty function, Nothing to declare */
                 
          }

        
        
        axiom_node_t* WSF_CALL
	AviaryCommon::ResourceLocation::serialize(axiom_node_t *parent, 
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
                      

                   if (!isValidLocation)
                   {
                      
                           /* no need to complain for minoccurs=0 element */
                            
                          
                   }
                   else
                   {
                     start_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (4 + axutil_strlen(p_prefix) + 
                                  axutil_strlen("location"))); 
                                 
                                 /* axutil_strlen("<:>") + 1 = 4 */
                     end_input_str = (axis2_char_t*)AXIS2_MALLOC(Environment::getEnv()->allocator, sizeof(axis2_char_t) *
                                 (5 + axutil_strlen(p_prefix) + axutil_strlen("location")));
                                  /* axutil_strlen("</:>") + 1 = 5 */
                                  
                     

                   
                   
                     /*
                      * Parsing Location array
                      */
                     if (property_Location != NULL)
                     {
                        
                            sprintf(start_input_str, "<%s%slocation>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":""); 
                            
                         start_input_str_len = axutil_strlen(start_input_str);

                         sprintf(end_input_str, "</%s%slocation>",
                                 p_prefix?p_prefix:"",
                                 (p_prefix && axutil_strcmp(p_prefix, ""))?":":"");
                         end_input_str_len = axutil_strlen(end_input_str);

                         count = property_Location->size();
                         for(i = 0; i < count; i++)
                         {
                            axutil_uri_t* element = (*property_Location)[i];

                            if(NULL == element) 
                            {
                                continue;
                            }

                    
                     
                     /*
                      * parsing location element
                      */

                    
                    
                           text_value_2 = axutil_uri_to_string(element, Environment::getEnv(), AXIS2_URI_UNP_OMITUSERINFO);
                           
                           axutil_stream_write(stream, Environment::getEnv(), start_input_str, start_input_str_len);
                           
                           axutil_stream_write(stream, Environment::getEnv(), text_value_2, axutil_strlen(text_value_2));
                           
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
            AviaryCommon::ResourceLocation::getProperty1()
            {
                return getId();
            }

            /**
             * getter for id.
             */
            AviaryCommon::ResourceID* WSF_CALL
            AviaryCommon::ResourceLocation::getId()
             {
                return property_Id;
             }

            /**
             * setter for id
             */
            bool WSF_CALL
            AviaryCommon::ResourceLocation::setId(
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
           AviaryCommon::ResourceLocation::resetId()
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
           AviaryCommon::ResourceLocation::isIdNil()
           {
               return !isValidId;
           }

           /**
            * Set id to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCommon::ResourceLocation::setIdNil()
           {
               return resetId();
           }

           

            /**
             * Getter for location by  Property Number 2
             */
            std::vector<axutil_uri_t*>* WSF_CALL
            AviaryCommon::ResourceLocation::getProperty2()
            {
                return getLocation();
            }

            /**
             * getter for location.
             */
            std::vector<axutil_uri_t*>* WSF_CALL
            AviaryCommon::ResourceLocation::getLocation()
             {
                return property_Location;
             }

            /**
             * setter for location
             */
            bool WSF_CALL
            AviaryCommon::ResourceLocation::setLocation(
                    std::vector<axutil_uri_t*>*  arg_Location)
             {
                
                 int size = 0;
                 int i = 0;
                 bool non_nil_exists = false;
                

                if(isValidLocation &&
                        arg_Location == property_Location)
                {
                    
                    return true;
                }

                
                 size = arg_Location->size();
                 
                 if (size < 0)
                 {
                     WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"location has less than minOccurs(0)");
                     return false;
                 }
                 for(i = 0; i < size; i ++ )
                 {
                     if(NULL != (*arg_Location)[i])
                     {
                         non_nil_exists = true;
                         break;
                     }
                 }

                 

                
                resetLocation();

                
                    if(NULL == arg_Location)
                         
                {
                    /* We are already done */
                    return true;
                }
                
                        property_Location = arg_Location;
                        if(non_nil_exists)
                        {
                            isValidLocation = true;
                        }
                        
                    
                return true;
             }

            
            /**
             * Get ith element of location.
             */
            axutil_uri_t* WSF_CALL
            AviaryCommon::ResourceLocation::getLocationAt(int i)
            {
                axutil_uri_t* ret_val;
                if(property_Location == NULL)
                {
                    return (axutil_uri_t*)0;
                }
                ret_val =   (*property_Location)[i];
                
                    return ret_val;
                  
            }

            /**
             * Set the ith element of location.
             */
           bool WSF_CALL
            AviaryCommon::ResourceLocation::setLocationAt(int i,
                    axutil_uri_t* arg_Location)
            {
                 axutil_uri_t* element;
                int size = 0;

                int non_nil_count;
                bool non_nil_exists = false;

                 

                if( isValidLocation &&
                    property_Location &&
                  
                    arg_Location == (*property_Location)[i])
                  
                 {
                    
                    return AXIS2_SUCCESS; 
                }

                   
                     non_nil_exists = true;
                  

                if(property_Location == NULL)
                {
                    property_Location = new std::vector<axutil_uri_t*>();
                }
                else{
                /* check whether there already exist an element */
                element = (*property_Location)[i];
                }

                
                        if(NULL != element)
                        {
                          
                          
                          
                              axutil_uri_free(element, Environment::getEnv());
                             
                        }
                        
                    
                    if(!non_nil_exists)
                    {
                        
                        isValidLocation = true;
                        (*property_Location)[i]= NULL;
                        
                        return AXIS2_SUCCESS;
                    }
                
                    (*property_Location)[i] = arg_Location;
                  

               isValidLocation = true;
                
                return AXIS2_SUCCESS;
            }

            /**
             * Add to location.
             */
            bool WSF_CALL
            AviaryCommon::ResourceLocation::addLocation(
                    axutil_uri_t* arg_Location)
             {

                
                    if( NULL == arg_Location
                     )
                    {
                      
                           return true; 
                        
                    }
                  

                if(property_Location == NULL)
                {
                    property_Location = new std::vector<axutil_uri_t*>();
                }
              
               property_Location->push_back(arg_Location);
              
                isValidLocation = true;
                return true;
             }

            /**
             * Get the size of the location array.
             */
            int WSF_CALL
            AviaryCommon::ResourceLocation::sizeofLocation()
            {

                if(property_Location == NULL)
                {
                    return 0;
                }
                return property_Location->size();
            }

            /**
             * remove the ith element, same as set_nil_at.
             */
            bool WSF_CALL
            AviaryCommon::ResourceLocation::removeLocationAt(int i)
            {
                return setLocationNilAt(i);
            }

            

           /**
            * resetter for location
            */
           bool WSF_CALL
           AviaryCommon::ResourceLocation::resetLocation()
           {
               int i = 0;
               int count = 0;


               
                if (property_Location != NULL)
                {
                  std::vector<axutil_uri_t*>::iterator it =  property_Location->begin();
                  for( ; it <  property_Location->end() ; ++it)
                  {
                     axutil_uri_t* element = *it;
                
            
                

                if(element != NULL)
                {
                   
                   
                      axutil_uri_free(element, Environment::getEnv());
                         element = NULL;
                     

                   }

                
                
                
               }

             }
                
                    if(NULL != property_Location)
                 delete property_Location;
                
               isValidLocation = false; 
               return true;
           }

           /**
            * Check whether location is nill
            */
           bool WSF_CALL
           AviaryCommon::ResourceLocation::isLocationNil()
           {
               return !isValidLocation;
           }

           /**
            * Set location to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCommon::ResourceLocation::setLocationNil()
           {
               return resetLocation();
           }

           
           /**
            * Check whether location is nill at i
            */
           bool WSF_CALL
           AviaryCommon::ResourceLocation::isLocationNilAt(int i)
           {
               return (isValidLocation == false ||
                       NULL == property_Location ||
                     NULL == (*property_Location)[i]);
            }

           /**
            * Set location to nil at i
            */
           bool WSF_CALL
           AviaryCommon::ResourceLocation::setLocationNilAt(int i)
           {
                int size = 0;
                int j;
                bool non_nil_exists = false;

                int k = 0;

                if(property_Location == NULL ||
                            isValidLocation == false)
                {
                    
                    non_nil_exists = false;
                }
                else
                {
                    size = property_Location->size();
                    for(j = 0, k = 0; j < size; j ++ )
                    {
                        if(i == j) continue; 
                        if(NULL != (*property_Location)[i])
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
                       WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "Size of the array of location is beinng set to be smaller than the specificed number of minOccurs(0)");
                       return AXIS2_FAILURE;
                }
 
                if(property_Location == NULL)
                {
                    isValidLocation = false;
                    
                    return true;
                }
                 
                 /* check whether there already exist an element */
                 axutil_uri_t* element = (*property_Location)[i];
                if(NULL != element)
                {
                  
                  
                  
                      axutil_uri_free(element, Environment::getEnv());
                     
                 }
                 
                    if(!non_nil_exists)
                    {
                        
                        isValidLocation = false;
                        (*property_Location)[i] = NULL;
                        return AXIS2_SUCCESS;
                    }
                

                
                (*property_Location)[i] = NULL;
                
                return AXIS2_SUCCESS;

           }

           

