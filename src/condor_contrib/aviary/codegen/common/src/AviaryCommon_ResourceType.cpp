

        /**
         * ResourceType.cpp
         *
         * This file was auto-generated from WSDL
         * by the Apache Axis2/C version: SNAPSHOT  Built on : Mar 10, 2008 (08:35:52 GMT+00:00)
         */

        #include "AviaryCommon_ResourceType.h"
        #include <Environment.h>
        #include <WSFError.h>


        using namespace wso2wsf;
        using namespace std;
        
        using namespace AviaryCommon;
        
               /*
                * Implementation of the ResourceType|http://common.aviary.grid.redhat.com Element
                */
           AviaryCommon::ResourceType::ResourceType()
        {

        
            qname = NULL;
        
                    property_ResourceType;
                
            isValidResourceType  = false;
        
                  qname =  axutil_qname_create (Environment::getEnv(),
                        "ResourceType",
                        "http://common.aviary.grid.redhat.com",
                        NULL);
                
        }

       AviaryCommon::ResourceType::ResourceType(std::string arg_ResourceType)
        {
             
                   qname = NULL;
             
                 property_ResourceType;
             
            isValidResourceType  = true;
            
                 qname =  axutil_qname_create (Environment::getEnv(),
                       "ResourceType",
                       "http://common.aviary.grid.redhat.com",
                       NULL);
               
                    property_ResourceType = arg_ResourceType;
            
        }
        AviaryCommon::ResourceType::~ResourceType()
        {
            axutil_qname_free(qname,Environment::getEnv());
        }

        
            bool WSF_CALL
            ResourceType::deserializeFromString(
                                            const axis2_char_t *node_value,
                                            axiom_node_t *parent)
            {
              bool status = true;
            
                        setResourceType(node_value);
                    
              return status;
            }
        

        bool WSF_CALL
        AviaryCommon::ResourceType::deserialize(axiom_node_t** dp_parent,bool *dp_is_early_node_valid, bool dont_care_minoccurs)
        {
          axiom_node_t *parent = *dp_parent;
          
          bool status = AXIS2_SUCCESS;
           
         const axis2_char_t* text_value = NULL;
         axutil_qname_t *mqname = NULL;
          
            
        status = AXIS2_FAILURE;
        if(parent)
        {
            axis2_char_t *attrib_text = NULL;
            attrib_text = axiom_element_get_attribute_value_by_name((axiom_element_t*)axiom_node_get_data_element(parent, Environment::getEnv()), Environment::getEnv(), "nil");
            if (attrib_text != NULL && !axutil_strcasecmp(attrib_text, "true"))
            {
              
               /* but the wsdl says that, this is non nillable */
                WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "NULL value is set to a non nillable element ResourceType");
                status = AXIS2_FAILURE;
               
            }
            else
            {
                axiom_node_t *text_node = NULL;
                text_node = axiom_node_get_first_child(parent, Environment::getEnv());
                axiom_text_t *text_element = NULL;
                if (text_node &&
                        axiom_node_get_node_type(text_node, Environment::getEnv()) == AXIOM_TEXT)
                    text_element = (axiom_text_t*)axiom_node_get_data_element(text_node, Environment::getEnv());
                text_value = "";
                if(text_element && axiom_text_get_value(text_element, Environment::getEnv()))
                {
                    text_value = (axis2_char_t*)axiom_text_get_value(text_element, Environment::getEnv());
                }
                status = deserializeFromString(text_value, parent);
                }
            }
            
          return status;
       }

          bool WSF_CALL
          AviaryCommon::ResourceType::isParticle()
          {
            
                 return false;
              
          }


          void WSF_CALL
          AviaryCommon::ResourceType::declareParentNamespaces(
                    axiom_element_t *parent_element,
                    axutil_hash_t *namespaces, int *next_ns_index)
          {
            
                  /* Here this is an empty function, Nothing to declare */
                 
          }

        
            char* WSF_CALL
            AviaryCommon::ResourceType::serializeToString(axutil_hash_t *namespaces)
            {
                axis2_char_t *text_value = NULL;
                axis2_char_t *qname_uri = NULL;
                axis2_char_t *qname_prefix = NULL;
                
                         text_value = (axis2_char_t*)axutil_xml_quote_string(Environment::getEnv(), (axis2_char_t*)property_ResourceType.c_str(), false);
                         if (!text_value)
                         {
                             text_value = (axis2_char_t*)axutil_strdup(Environment::getEnv(), property_ResourceType.c_str());
                         }
                      
                return text_value;
            }
        
        
        axiom_node_t* WSF_CALL
	AviaryCommon::ResourceType::serialize(axiom_node_t *parent, 
			axiom_element_t *parent_element, 
			int parent_tag_closed, 
			axutil_hash_t *namespaces, 
			int *next_ns_index)
        {
            
            
         
         axiom_node_t *current_node = NULL;
         int tag_closed = 0;

         
         
            axiom_data_source_t *data_source = NULL;
            axutil_stream_t *stream = NULL;
            axis2_char_t *text_value;
             
                    current_node = parent;
                    data_source = (axiom_data_source_t *)axiom_node_get_data_element(current_node, Environment::getEnv());
                    if (!data_source)
                        return NULL;
                    stream = axiom_data_source_get_stream(data_source, Environment::getEnv()); /* assume parent is of type data source */
                    if (!stream)
                        return NULL;
                  
               if(!parent_tag_closed && !tag_closed)
               {
                  text_value = ">"; 
                  axutil_stream_write(stream, Environment::getEnv(), text_value, axutil_strlen(text_value));
               }
               
               text_value = serializeToString(namespaces);
               if(text_value)
               {
                    axutil_stream_write(stream, Environment::getEnv(), text_value, axutil_strlen(text_value));
                    AXIS2_FREE(Environment::getEnv()->allocator, text_value);
               }
            

            return parent;
        }


        

            /**
             * Getter for ResourceType by  Property Number 1
             */
            std::string WSF_CALL
            AviaryCommon::ResourceType::getProperty1()
            {
                return getResourceType();
            }

            /**
             * getter for ResourceType.
             */
            std::string WSF_CALL
            AviaryCommon::ResourceType::getResourceType()
             {
                return property_ResourceType;
             }

            /**
             * setter for ResourceType
             */
            bool WSF_CALL
            AviaryCommon::ResourceType::setResourceType(
                    const std::string  arg_ResourceType)
             {
                

                if(isValidResourceType &&
                        arg_ResourceType == property_ResourceType)
                {
                    
                    return true;
                }

                
                  if(arg_ResourceType.empty())
                       
                  {
                      WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"ResourceType is being set to NULL, but it is not a nullable element");
                      return AXIS2_FAILURE;
                  }
                

                
                resetResourceType();

                
                        property_ResourceType = std::string(arg_ResourceType.c_str());
                        isValidResourceType = true;
                    
                return true;
             }

            
             /**
             * specialized enum getter for ResourceType.
             */
             ADBResourceTypeEnum WSF_CALL
             AviaryCommon::ResourceType::getResourceTypeEnum()
             {

                
                 if (axutil_strcmp(property_ResourceType.c_str(), "COLLECTOR") == 0)
                    return ResourceType_COLLECTOR;
             
                 if (axutil_strcmp(property_ResourceType.c_str(), "EXECUTOR") == 0)
                    return ResourceType_EXECUTOR;
             
                 if (axutil_strcmp(property_ResourceType.c_str(), "EVENT_SERVER") == 0)
                    return ResourceType_EVENT_SERVER;
             
                 if (axutil_strcmp(property_ResourceType.c_str(), "JOB_SERVER") == 0)
                    return ResourceType_JOB_SERVER;
             
                 if (axutil_strcmp(property_ResourceType.c_str(), "LOW_LATENCY") == 0)
                    return ResourceType_LOW_LATENCY;
             
                 if (axutil_strcmp(property_ResourceType.c_str(), "MASTER") == 0)
                    return ResourceType_MASTER;
             
                 if (axutil_strcmp(property_ResourceType.c_str(), "NEGOTIATOR") == 0)
                    return ResourceType_NEGOTIATOR;
             
                 if (axutil_strcmp(property_ResourceType.c_str(), "SCHEDULER") == 0)
                    return ResourceType_SCHEDULER;
             
                 if (axutil_strcmp(property_ResourceType.c_str(), "CUSTOM") == 0)
                    return ResourceType_CUSTOM;
             
             
                 /* Error: none of the strings matched; invalid enum value */
                 return (ADBResourceTypeEnum)-1;
             }
             
             
             /**
             * specialized enum setter for ResourceType.
             */
             bool WSF_CALL
            AviaryCommon::ResourceType::setResourceTypeEnum(const ADBResourceTypeEnum  arg_ResourceType)
             {
                

                
                resetResourceType();

                   
                   switch (arg_ResourceType)
                   {
                     
                       case ResourceType_COLLECTOR :
                            property_ResourceType = ("COLLECTOR");
                          break;
                     
                       case ResourceType_EXECUTOR :
                            property_ResourceType = ("EXECUTOR");
                          break;
                     
                       case ResourceType_EVENT_SERVER :
                            property_ResourceType = ("EVENT_SERVER");
                          break;
                     
                       case ResourceType_JOB_SERVER :
                            property_ResourceType = ("JOB_SERVER");
                          break;
                     
                       case ResourceType_LOW_LATENCY :
                            property_ResourceType = ("LOW_LATENCY");
                          break;
                     
                       case ResourceType_MASTER :
                            property_ResourceType = ("MASTER");
                          break;
                     
                       case ResourceType_NEGOTIATOR :
                            property_ResourceType = ("NEGOTIATOR");
                          break;
                     
                       case ResourceType_SCHEDULER :
                            property_ResourceType = ("SCHEDULER");
                          break;
                     
                       case ResourceType_CUSTOM :
                            property_ResourceType = ("CUSTOM");
                          break;
                     
                     
                       default:
                          isValidResourceType = false;
                          property_ResourceType = "";
                          WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Error setting ResourceType: undefined enum value");
                          return false;
                   }
                
                   if(property_ResourceType.empty())
                   {
                       return AXIS2_FAILURE;
                   }
                     isValidResourceType = true;
                        
                
                return true;
             }
             

           /**
            * resetter for ResourceType
            */
           bool WSF_CALL
           AviaryCommon::ResourceType::resetResourceType()
           {
               int i = 0;
               int count = 0;


               
               isValidResourceType = false; 
               return true;
           }

           /**
            * Check whether ResourceType is nill
            */
           bool WSF_CALL
           AviaryCommon::ResourceType::isResourceTypeNil()
           {
               return !isValidResourceType;
           }

           /**
            * Set ResourceType to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCommon::ResourceType::setResourceTypeNil()
           {
               return resetResourceType();
           }

           

