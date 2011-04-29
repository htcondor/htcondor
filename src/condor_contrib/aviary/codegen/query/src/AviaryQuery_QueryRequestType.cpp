

        /**
         * QueryRequestType.cpp
         *
         * This file was auto-generated from WSDL
         * by the Apache Axis2/C version: SNAPSHOT  Built on : Mar 10, 2008 (08:35:52 GMT+00:00)
         */

        #include "AviaryQuery_QueryRequestType.h"
        #include <Environment.h>
        #include <WSFError.h>


        using namespace wso2wsf;
        using namespace std;
        
        using namespace AviaryQuery;
        
                /*
                 * This type was generated from the piece of schema that had
                 * name = QueryRequestType
                 * Namespace URI = http://query.aviary.grid.redhat.com
                 * Namespace Prefix = ns2
                 */
           AviaryQuery::QueryRequestType::QueryRequestType()
        {

        
            isValidAllowPartialMatching  = false;
        
        }

       AviaryQuery::QueryRequestType::QueryRequestType(bool arg_AllowPartialMatching)
        {
             
            isValidAllowPartialMatching  = true;
            
                    property_AllowPartialMatching = arg_AllowPartialMatching;
            
        }
        AviaryQuery::QueryRequestType::~QueryRequestType()
        {

        }

        

        bool WSF_CALL
        AviaryQuery::QueryRequestType::deserialize(axiom_node_t** dp_parent,bool *dp_is_early_node_valid, bool dont_care_minoccurs)
        {
          axiom_node_t *parent = *dp_parent;
          
          bool status = AXIS2_SUCCESS;
          
          axiom_attribute_t *parent_attri = NULL;
          axiom_element_t *parent_element = NULL;
          axis2_char_t *attrib_text = NULL;

          axutil_hash_t *attribute_hash = NULL;

           
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
                      
                    
                 parent_element = (axiom_element_t *)axiom_node_get_data_element(parent, Environment::getEnv());
                 attribute_hash = axiom_element_get_all_attributes(parent_element, Environment::getEnv());
              
                
                
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
                           
                           
                               if(!strcmp((axis2_char_t*)key, "allowPartialMatching"))
                             
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
                    /* this is hoping that attribute is stored in "allowPartialMatching", this happnes when name is in default namespace */
                    attrib_text = axiom_element_get_attribute_value_by_name(parent_element, Environment::getEnv(), "allowPartialMatching");
                  }

                  if(attrib_text != NULL)
                  {
                      
                      
                           if (!axutil_strcmp(attrib_text, "TRUE") || !axutil_strcmp(attrib_text, "true"))
                           {
                               setAllowPartialMatching(true);
                           }
                           else
                           {
                               setAllowPartialMatching(false);
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
          AviaryQuery::QueryRequestType::isParticle()
          {
            
                 return false;
              
          }


          void WSF_CALL
          AviaryQuery::QueryRequestType::declareParentNamespaces(
                    axiom_element_t *parent_element,
                    axutil_hash_t *namespaces, int *next_ns_index)
          {
            
                  /* Here this is an empty function, Nothing to declare */
                 
          }

        
        
        axiom_node_t* WSF_CALL
	AviaryQuery::QueryRequestType::serialize(axiom_node_t *parent, 
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
            
                    axis2_char_t text_value_1[ADB_DEFAULT_DIGIT_LIMIT];
                    
                axis2_char_t *text_value = NULL;
             
            
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
            
                if(isValidAllowPartialMatching)
                {
                
                        p_prefix = NULL;
                      
                           
                           text_value = (axis2_char_t*)((property_AllowPartialMatching)?"true":"false");
                           string_to_stream = (axis2_char_t*) AXIS2_MALLOC (Environment::getEnv()-> allocator, sizeof (axis2_char_t) *
                                                            (5  + ADB_DEFAULT_NAMESPACE_PREFIX_LIMIT +
                                                             axutil_strlen(text_value) + 
                                                             axutil_strlen("allowPartialMatching")));
                           sprintf(string_to_stream, " %s%s%s=\"%s\"", p_prefix?p_prefix:"", (p_prefix && axutil_strcmp(p_prefix, ""))?":":"",
                                                "allowPartialMatching",  text_value);
                           axutil_stream_write(stream, Environment::getEnv(), string_to_stream, axutil_strlen(string_to_stream));
                           AXIS2_FREE(Environment::getEnv()-> allocator, string_to_stream);
                        
                   }
                   
              string_to_stream = ">"; 
              axutil_stream_write(stream, Environment::getEnv(), string_to_stream, axutil_strlen(string_to_stream));
              tag_closed = 1;
            
            }
            
                    
                    if(parent_tag_closed)
                    {
                       if(isValidAllowPartialMatching)
                       {
                       
                           p_prefix = NULL;
                           ns1 = NULL;
                         
                           
                           text_value =  (axis2_char_t*)((property_AllowPartialMatching)?axutil_strdup(Environment::getEnv(), "true"):axutil_strdup(Environment::getEnv(), "false"));
                           text_attri = axiom_attribute_create (Environment::getEnv(), "allowPartialMatching", text_value, ns1);
                           axiom_element_add_attribute (parent_element, Environment::getEnv(), text_attri, parent);
                           AXIS2_FREE(Environment::getEnv()->allocator, text_value);
                        
                      }
                       
                  }
                

            return parent;
        }


        

            /**
             * Getter for allowPartialMatching by  Property Number 1
             */
            bool WSF_CALL
            AviaryQuery::QueryRequestType::getProperty1()
            {
                return getAllowPartialMatching();
            }

            /**
             * getter for allowPartialMatching.
             */
            bool WSF_CALL
            AviaryQuery::QueryRequestType::getAllowPartialMatching()
             {
                return property_AllowPartialMatching;
             }

            /**
             * setter for allowPartialMatching
             */
            bool WSF_CALL
            AviaryQuery::QueryRequestType::setAllowPartialMatching(
                    bool  arg_AllowPartialMatching)
             {
                

                if(isValidAllowPartialMatching &&
                        arg_AllowPartialMatching == property_AllowPartialMatching)
                {
                    
                    return true;
                }

                

                
                resetAllowPartialMatching();

                
                        property_AllowPartialMatching = arg_AllowPartialMatching;
                        isValidAllowPartialMatching = true;
                    
                return true;
             }

             

           /**
            * resetter for allowPartialMatching
            */
           bool WSF_CALL
           AviaryQuery::QueryRequestType::resetAllowPartialMatching()
           {
               int i = 0;
               int count = 0;


               
               isValidAllowPartialMatching = false; 
               return true;
           }

           /**
            * Check whether allowPartialMatching is nill
            */
           bool WSF_CALL
           AviaryQuery::QueryRequestType::isAllowPartialMatchingNil()
           {
               return !isValidAllowPartialMatching;
           }

           /**
            * Set allowPartialMatching to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryQuery::QueryRequestType::setAllowPartialMatchingNil()
           {
               return resetAllowPartialMatching();
           }

           

