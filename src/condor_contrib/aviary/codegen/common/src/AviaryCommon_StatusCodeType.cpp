

        /**
         * StatusCodeType.cpp
         *
         * This file was auto-generated from WSDL
         * by the Apache Axis2/C version: SNAPSHOT  Built on : Mar 10, 2008 (08:35:52 GMT+00:00)
         */
        
            #include "AviaryCommon_StatusCodeType.h"
          

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
                * Implementation of the StatusCodeType|http://common.aviary.grid.redhat.com Element
                */
           AviaryCommon::StatusCodeType::StatusCodeType()
        {

        
            qname = NULL;
        
                    property_StatusCodeType;
                
            isValidStatusCodeType  = false;
        
                  qname =  axutil_qname_create (Environment::getEnv(),
                        "StatusCodeType",
                        "http://common.aviary.grid.redhat.com",
                        NULL);
                
        }

       AviaryCommon::StatusCodeType::StatusCodeType(std::string arg_StatusCodeType)
        {
             
                   qname = NULL;
             
                 property_StatusCodeType;
             
            isValidStatusCodeType  = true;
            
                 qname =  axutil_qname_create (Environment::getEnv(),
                       "StatusCodeType",
                       "http://common.aviary.grid.redhat.com",
                       NULL);
               
                    property_StatusCodeType = arg_StatusCodeType;
            
        }
        AviaryCommon::StatusCodeType::~StatusCodeType()
        {
            resetAll();
        }

        bool WSF_CALL AviaryCommon::StatusCodeType::resetAll()
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
            StatusCodeType::deserializeFromString(
                                            const axis2_char_t *node_value,
                                            axiom_node_t *parent)
            {
              bool status = true;
            
                        setStatusCodeType(node_value);
                    
              return status;
            }
        

        bool WSF_CALL
        AviaryCommon::StatusCodeType::deserialize(axiom_node_t** dp_parent,bool *dp_is_early_node_valid, bool dont_care_minoccurs)
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
                WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "NULL value is set to a non nillable element StatusCodeType");
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
          AviaryCommon::StatusCodeType::isParticle()
          {
            
                 return false;
              
          }


          void WSF_CALL
          AviaryCommon::StatusCodeType::declareParentNamespaces(
                    axiom_element_t *parent_element,
                    axutil_hash_t *namespaces, int *next_ns_index)
          {
            
                  /* Here this is an empty function, Nothing to declare */
                 
          }

        
            char* WSF_CALL
            AviaryCommon::StatusCodeType::serializeToString(axutil_hash_t *namespaces)
            {
                axis2_char_t *text_value = NULL;
                axis2_char_t *qname_uri = NULL;
                axis2_char_t *qname_prefix = NULL;
                
                         text_value = (axis2_char_t*)axutil_xml_quote_string(Environment::getEnv(), (axis2_char_t*)property_StatusCodeType.c_str(), false);
                         if (!text_value)
                         {
                             text_value = (axis2_char_t*)axutil_strdup(Environment::getEnv(), property_StatusCodeType.c_str());
                         }
                      
                return text_value;
            }
        
        
        axiom_node_t* WSF_CALL
	AviaryCommon::StatusCodeType::serialize(axiom_node_t *parent, 
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
             * Getter for StatusCodeType by  Property Number 1
             */
            std::string WSF_CALL
            AviaryCommon::StatusCodeType::getProperty1()
            {
                return getStatusCodeType();
            }

            /**
             * getter for StatusCodeType.
             */
            std::string WSF_CALL
            AviaryCommon::StatusCodeType::getStatusCodeType()
             {
                return property_StatusCodeType;
             }

            /**
             * setter for StatusCodeType
             */
            bool WSF_CALL
            AviaryCommon::StatusCodeType::setStatusCodeType(
                    const std::string  arg_StatusCodeType)
             {
                

                if(isValidStatusCodeType &&
                        arg_StatusCodeType == property_StatusCodeType)
                {
                    
                    return true;
                }

                
                  if(arg_StatusCodeType.empty())
                       
                  {
                      WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"StatusCodeType is being set to NULL, but it is not a nullable element");
                      return AXIS2_FAILURE;
                  }
                

                
                resetStatusCodeType();

                
                        property_StatusCodeType = std::string(arg_StatusCodeType.c_str());
                        isValidStatusCodeType = true;
                    
                return true;
             }

            
             /**
             * specialized enum getter for StatusCodeType.
             */
             ADBStatusCodeTypeEnum WSF_CALL
             AviaryCommon::StatusCodeType::getStatusCodeTypeEnum()
             {

                
                 if (axutil_strcmp(property_StatusCodeType.c_str(), "OK") == 0)
                    return StatusCodeType_OK;
             
                 if (axutil_strcmp(property_StatusCodeType.c_str(), "FAIL") == 0)
                    return StatusCodeType_FAIL;
             
                 if (axutil_strcmp(property_StatusCodeType.c_str(), "NO_MATCH") == 0)
                    return StatusCodeType_NO_MATCH;
             
                 if (axutil_strcmp(property_StatusCodeType.c_str(), "INVALID_OFFSET") == 0)
                    return StatusCodeType_INVALID_OFFSET;
             
                 if (axutil_strcmp(property_StatusCodeType.c_str(), "UNIMPLEMENTED") == 0)
                    return StatusCodeType_UNIMPLEMENTED;
             
                 if (axutil_strcmp(property_StatusCodeType.c_str(), "UNAVAILABLE") == 0)
                    return StatusCodeType_UNAVAILABLE;
             
             
                 /* Error: none of the strings matched; invalid enum value */
                 return (ADBStatusCodeTypeEnum)-1;
             }
             
             
             /**
             * specialized enum setter for StatusCodeType.
             */
             bool WSF_CALL
            AviaryCommon::StatusCodeType::setStatusCodeTypeEnum(const ADBStatusCodeTypeEnum  arg_StatusCodeType)
             {
                

                
                resetStatusCodeType();

                   
                   switch (arg_StatusCodeType)
                   {
                     
                       case StatusCodeType_OK :
                            
                            
                            property_StatusCodeType = "OK";
                          break;
                     
                       case StatusCodeType_FAIL :
                            
                            
                            property_StatusCodeType = "FAIL";
                          break;
                     
                       case StatusCodeType_NO_MATCH :
                            
                            
                            property_StatusCodeType = "NO_MATCH";
                          break;
                     
                       case StatusCodeType_INVALID_OFFSET :
                            
                            
                            property_StatusCodeType = "INVALID_OFFSET";
                          break;
                     
                       case StatusCodeType_UNIMPLEMENTED :
                            
                            
                            property_StatusCodeType = "UNIMPLEMENTED";
                          break;
                     
                       case StatusCodeType_UNAVAILABLE :
                            
                            
                            property_StatusCodeType = "UNAVAILABLE";
                          break;
                     
                     
                       default:
                          isValidStatusCodeType = false;
                          property_StatusCodeType = "";
                          WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Error setting StatusCodeType: undefined enum value");
                          return false;
                   }
                
                   if(property_StatusCodeType.empty())
                   {
                       return AXIS2_FAILURE;
                   }
                     isValidStatusCodeType = true;
                        
                
                return true;
             }


             /**
             * specialized enum constructor for StatusCodeType.
             */
            AviaryCommon::StatusCodeType::StatusCodeType(const ADBStatusCodeTypeEnum  arg_StatusCodeType)
             {             
                   qname = NULL;
             
                 qname =  axutil_qname_create (Environment::getEnv(),
                       "StatusCodeType",
                       "http://common.aviary.grid.redhat.com",
                       NULL);
               

            isValidStatusCodeType  = setStatusCodeTypeEnum( arg_StatusCodeType );
            }

             

           /**
            * resetter for StatusCodeType
            */
           bool WSF_CALL
           AviaryCommon::StatusCodeType::resetStatusCodeType()
           {
               int i = 0;
               int count = 0;


               
               isValidStatusCodeType = false; 
               return true;
           }

           /**
            * Check whether StatusCodeType is nill
            */
           bool WSF_CALL
           AviaryCommon::StatusCodeType::isStatusCodeTypeNil()
           {
               return !isValidStatusCodeType;
           }

           /**
            * Set StatusCodeType to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCommon::StatusCodeType::setStatusCodeTypeNil()
           {
               return resetStatusCodeType();
           }

           

