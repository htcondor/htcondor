

        /**
         * ResourceType.cpp
         *
         * This file was auto-generated from WSDL
         * by the Apache Axis2/C version: SNAPSHOT  Built on : Mar 10, 2008 (08:35:52 GMT+00:00)
         */
        
            #include "AviaryCommon_ResourceType.h"
          

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
            resetAll();
        }

        bool WSF_CALL AviaryCommon::ResourceType::resetAll()
        {
            //calls reset method for all the properties owned by this method which are pointers.

            
          if(qname != NULL)
          {
            axutil_qname_free( qname, Environment::getEnv());
            qname = NULL;
          }
        
            return true;

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

                
                 if (axutil_strcmp(property_ResourceType.c_str(), "ANY") == 0)
                    return ResourceType_ANY;
             
                 if (axutil_strcmp(property_ResourceType.c_str(), "COLLECTOR") == 0)
                    return ResourceType_COLLECTOR;
             
                 if (axutil_strcmp(property_ResourceType.c_str(), "CUSTOM") == 0)
                    return ResourceType_CUSTOM;
             
                 if (axutil_strcmp(property_ResourceType.c_str(), "MASTER") == 0)
                    return ResourceType_MASTER;
             
                 if (axutil_strcmp(property_ResourceType.c_str(), "NEGOTIATOR") == 0)
                    return ResourceType_NEGOTIATOR;
             
                 if (axutil_strcmp(property_ResourceType.c_str(), "SCHEDULER") == 0)
                    return ResourceType_SCHEDULER;
             
                 if (axutil_strcmp(property_ResourceType.c_str(), "SLOT") == 0)
                    return ResourceType_SLOT;
             
             
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
                     
                       case ResourceType_ANY :
                            
                            
                            property_ResourceType = "ANY";
                          break;
                     
                       case ResourceType_COLLECTOR :
                            
                            
                            property_ResourceType = "COLLECTOR";
                          break;
                     
                       case ResourceType_CUSTOM :
                            
                            
                            property_ResourceType = "CUSTOM";
                          break;
                     
                       case ResourceType_MASTER :
                            
                            
                            property_ResourceType = "MASTER";
                          break;
                     
                       case ResourceType_NEGOTIATOR :
                            
                            
                            property_ResourceType = "NEGOTIATOR";
                          break;
                     
                       case ResourceType_SCHEDULER :
                            
                            
                            property_ResourceType = "SCHEDULER";
                          break;
                     
                       case ResourceType_SLOT :
                            
                            
                            property_ResourceType = "SLOT";
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
             * specialized enum constructor for ResourceType.
             */
            AviaryCommon::ResourceType::ResourceType(const ADBResourceTypeEnum  arg_ResourceType)
             {             
                   qname = NULL;
             
                 qname =  axutil_qname_create (Environment::getEnv(),
                       "ResourceType",
                       "http://common.aviary.grid.redhat.com",
                       NULL);
               

            isValidResourceType  = setResourceTypeEnum( arg_ResourceType );
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

           

