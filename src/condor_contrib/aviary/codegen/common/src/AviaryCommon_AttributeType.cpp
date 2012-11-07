

        /**
         * AttributeType.cpp
         *
         * This file was auto-generated from WSDL
         * by the Apache Axis2/C version: SNAPSHOT  Built on : Mar 10, 2008 (08:35:52 GMT+00:00)
         */
        
            #include "AviaryCommon_AttributeType.h"
          

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
                * Implementation of the AttributeType|http://common.aviary.grid.redhat.com Element
                */
           AviaryCommon::AttributeType::AttributeType()
        {

        
            qname = NULL;
        
                    property_AttributeType;
                
            isValidAttributeType  = false;
        
                  qname =  axutil_qname_create (Environment::getEnv(),
                        "AttributeType",
                        "http://common.aviary.grid.redhat.com",
                        NULL);
                
        }

       AviaryCommon::AttributeType::AttributeType(std::string arg_AttributeType)
        {
             
                   qname = NULL;
             
                 property_AttributeType;
             
            isValidAttributeType  = true;
            
                 qname =  axutil_qname_create (Environment::getEnv(),
                       "AttributeType",
                       "http://common.aviary.grid.redhat.com",
                       NULL);
               
                    property_AttributeType = arg_AttributeType;
            
        }
        AviaryCommon::AttributeType::~AttributeType()
        {
            resetAll();
        }

        bool WSF_CALL AviaryCommon::AttributeType::resetAll()
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
            AttributeType::deserializeFromString(
                                            const axis2_char_t *node_value,
                                            axiom_node_t *parent)
            {
              bool status = true;
            
                        setAttributeType(node_value);
                    
              return status;
            }
        

        bool WSF_CALL
        AviaryCommon::AttributeType::deserialize(axiom_node_t** dp_parent,bool *dp_is_early_node_valid, bool dont_care_minoccurs)
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
                WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "NULL value is set to a non nillable element AttributeType");
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
          AviaryCommon::AttributeType::isParticle()
          {
            
                 return false;
              
          }


          void WSF_CALL
          AviaryCommon::AttributeType::declareParentNamespaces(
                    axiom_element_t *parent_element,
                    axutil_hash_t *namespaces, int *next_ns_index)
          {
            
                  /* Here this is an empty function, Nothing to declare */
                 
          }

        
            char* WSF_CALL
            AviaryCommon::AttributeType::serializeToString(axutil_hash_t *namespaces)
            {
                axis2_char_t *text_value = NULL;
                axis2_char_t *qname_uri = NULL;
                axis2_char_t *qname_prefix = NULL;
                
                         text_value = (axis2_char_t*)axutil_xml_quote_string(Environment::getEnv(), (axis2_char_t*)property_AttributeType.c_str(), false);
                         if (!text_value)
                         {
                             text_value = (axis2_char_t*)axutil_strdup(Environment::getEnv(), property_AttributeType.c_str());
                         }
                      
                return text_value;
            }
        
        
        axiom_node_t* WSF_CALL
	AviaryCommon::AttributeType::serialize(axiom_node_t *parent, 
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
             * Getter for AttributeType by  Property Number 1
             */
            std::string WSF_CALL
            AviaryCommon::AttributeType::getProperty1()
            {
                return getAttributeType();
            }

            /**
             * getter for AttributeType.
             */
            std::string WSF_CALL
            AviaryCommon::AttributeType::getAttributeType()
             {
                return property_AttributeType;
             }

            /**
             * setter for AttributeType
             */
            bool WSF_CALL
            AviaryCommon::AttributeType::setAttributeType(
                    const std::string  arg_AttributeType)
             {
                

                if(isValidAttributeType &&
                        arg_AttributeType == property_AttributeType)
                {
                    
                    return true;
                }

                
                  if(arg_AttributeType.empty())
                       
                  {
                      WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"AttributeType is being set to NULL, but it is not a nullable element");
                      return AXIS2_FAILURE;
                  }
                

                
                resetAttributeType();

                
                        property_AttributeType = std::string(arg_AttributeType.c_str());
                        isValidAttributeType = true;
                    
                return true;
             }

            
             /**
             * specialized enum getter for AttributeType.
             */
             ADBAttributeTypeEnum WSF_CALL
             AviaryCommon::AttributeType::getAttributeTypeEnum()
             {

                
                 if (axutil_strcmp(property_AttributeType.c_str(), "INTEGER") == 0)
                    return AttributeType_INTEGER;
             
                 if (axutil_strcmp(property_AttributeType.c_str(), "FLOAT") == 0)
                    return AttributeType_FLOAT;
             
                 if (axutil_strcmp(property_AttributeType.c_str(), "STRING") == 0)
                    return AttributeType_STRING;
             
                 if (axutil_strcmp(property_AttributeType.c_str(), "EXPRESSION") == 0)
                    return AttributeType_EXPRESSION;
             
                 if (axutil_strcmp(property_AttributeType.c_str(), "BOOLEAN") == 0)
                    return AttributeType_BOOLEAN;
             
                 if (axutil_strcmp(property_AttributeType.c_str(), "UNDEFINED") == 0)
                    return AttributeType_UNDEFINED;
             
                 if (axutil_strcmp(property_AttributeType.c_str(), "ERROR") == 0)
                    return AttributeType_ERROR;
             
             
                 /* Error: none of the strings matched; invalid enum value */
                 return (ADBAttributeTypeEnum)-1;
             }
             
             
             /**
             * specialized enum setter for AttributeType.
             */
             bool WSF_CALL
            AviaryCommon::AttributeType::setAttributeTypeEnum(const ADBAttributeTypeEnum  arg_AttributeType)
             {
                

                
                resetAttributeType();

                   
                   switch (arg_AttributeType)
                   {
                     
                       case AttributeType_INTEGER :
                            
                            
                            property_AttributeType = "INTEGER";
                          break;
                     
                       case AttributeType_FLOAT :
                            
                            
                            property_AttributeType = "FLOAT";
                          break;
                     
                       case AttributeType_STRING :
                            
                            
                            property_AttributeType = "STRING";
                          break;
                     
                       case AttributeType_EXPRESSION :
                            
                            
                            property_AttributeType = "EXPRESSION";
                          break;
                     
                       case AttributeType_BOOLEAN :
                            
                            
                            property_AttributeType = "BOOLEAN";
                          break;
                     
                       case AttributeType_UNDEFINED :
                            
                            
                            property_AttributeType = "UNDEFINED";
                          break;
                     
                       case AttributeType_ERROR :
                            
                            
                            property_AttributeType = "ERROR";
                          break;
                     
                     
                       default:
                          isValidAttributeType = false;
                          property_AttributeType = "";
                          WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Error setting AttributeType: undefined enum value");
                          return false;
                   }
                
                   if(property_AttributeType.empty())
                   {
                       return AXIS2_FAILURE;
                   }
                     isValidAttributeType = true;
                        
                
                return true;
             }


             /**
             * specialized enum constructor for AttributeType.
             */
            AviaryCommon::AttributeType::AttributeType(const ADBAttributeTypeEnum  arg_AttributeType)
             {             
                   qname = NULL;
             
                 qname =  axutil_qname_create (Environment::getEnv(),
                       "AttributeType",
                       "http://common.aviary.grid.redhat.com",
                       NULL);
               

            isValidAttributeType  = setAttributeTypeEnum( arg_AttributeType );
            }

             

           /**
            * resetter for AttributeType
            */
           bool WSF_CALL
           AviaryCommon::AttributeType::resetAttributeType()
           {
               int i = 0;
               int count = 0;


               
               isValidAttributeType = false; 
               return true;
           }

           /**
            * Check whether AttributeType is nill
            */
           bool WSF_CALL
           AviaryCommon::AttributeType::isAttributeTypeNil()
           {
               return !isValidAttributeType;
           }

           /**
            * Set AttributeType to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCommon::AttributeType::setAttributeTypeNil()
           {
               return resetAttributeType();
           }

           

