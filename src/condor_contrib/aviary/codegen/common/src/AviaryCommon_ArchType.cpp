

        /**
         * ArchType.cpp
         *
         * This file was auto-generated from WSDL
         * by the Apache Axis2/C version: SNAPSHOT  Built on : Mar 10, 2008 (08:35:52 GMT+00:00)
         */
        
            #include "AviaryCommon_ArchType.h"
          

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
                * Implementation of the ArchType|http://common.aviary.grid.redhat.com Element
                */
           AviaryCommon::ArchType::ArchType()
        {

        
            qname = NULL;
        
                    property_ArchType;
                
            isValidArchType  = false;
        
                  qname =  axutil_qname_create (Environment::getEnv(),
                        "ArchType",
                        "http://common.aviary.grid.redhat.com",
                        NULL);
                
        }

       AviaryCommon::ArchType::ArchType(std::string arg_ArchType)
        {
             
                   qname = NULL;
             
                 property_ArchType;
             
            isValidArchType  = true;
            
                 qname =  axutil_qname_create (Environment::getEnv(),
                       "ArchType",
                       "http://common.aviary.grid.redhat.com",
                       NULL);
               
                    property_ArchType = arg_ArchType;
            
        }
        AviaryCommon::ArchType::~ArchType()
        {
            resetAll();
        }

        bool WSF_CALL AviaryCommon::ArchType::resetAll()
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
            ArchType::deserializeFromString(
                                            const axis2_char_t *node_value,
                                            axiom_node_t *parent)
            {
              bool status = true;
            
                        setArchType(node_value);
                    
              return status;
            }
        

        bool WSF_CALL
        AviaryCommon::ArchType::deserialize(axiom_node_t** dp_parent,bool *dp_is_early_node_valid, bool dont_care_minoccurs)
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
                WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "NULL value is set to a non nillable element ArchType");
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
          AviaryCommon::ArchType::isParticle()
          {
            
                 return false;
              
          }


          void WSF_CALL
          AviaryCommon::ArchType::declareParentNamespaces(
                    axiom_element_t *parent_element,
                    axutil_hash_t *namespaces, int *next_ns_index)
          {
            
                  /* Here this is an empty function, Nothing to declare */
                 
          }

        
            char* WSF_CALL
            AviaryCommon::ArchType::serializeToString(axutil_hash_t *namespaces)
            {
                axis2_char_t *text_value = NULL;
                axis2_char_t *qname_uri = NULL;
                axis2_char_t *qname_prefix = NULL;
                
                         text_value = (axis2_char_t*)axutil_xml_quote_string(Environment::getEnv(), (axis2_char_t*)property_ArchType.c_str(), false);
                         if (!text_value)
                         {
                             text_value = (axis2_char_t*)axutil_strdup(Environment::getEnv(), property_ArchType.c_str());
                         }
                      
                return text_value;
            }
        
        
        axiom_node_t* WSF_CALL
	AviaryCommon::ArchType::serialize(axiom_node_t *parent, 
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
             * Getter for ArchType by  Property Number 1
             */
            std::string WSF_CALL
            AviaryCommon::ArchType::getProperty1()
            {
                return getArchType();
            }

            /**
             * getter for ArchType.
             */
            std::string WSF_CALL
            AviaryCommon::ArchType::getArchType()
             {
                return property_ArchType;
             }

            /**
             * setter for ArchType
             */
            bool WSF_CALL
            AviaryCommon::ArchType::setArchType(
                    const std::string  arg_ArchType)
             {
                

                if(isValidArchType &&
                        arg_ArchType == property_ArchType)
                {
                    
                    return true;
                }

                
                  if(arg_ArchType.empty())
                       
                  {
                      WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"ArchType is being set to NULL, but it is not a nullable element");
                      return AXIS2_FAILURE;
                  }
                

                
                resetArchType();

                
                        property_ArchType = std::string(arg_ArchType.c_str());
                        isValidArchType = true;
                    
                return true;
             }

            
             /**
             * specialized enum getter for ArchType.
             */
             ADBArchTypeEnum WSF_CALL
             AviaryCommon::ArchType::getArchTypeEnum()
             {

                
                 if (axutil_strcmp(property_ArchType.c_str(), "INTEL") == 0)
                    return ArchType_INTEL;
             
                 if (axutil_strcmp(property_ArchType.c_str(), "X86_64") == 0)
                    return ArchType_X86_64;
             
             
                 /* Error: none of the strings matched; invalid enum value */
                 return (ADBArchTypeEnum)-1;
             }
             
             
             /**
             * specialized enum setter for ArchType.
             */
             bool WSF_CALL
            AviaryCommon::ArchType::setArchTypeEnum(const ADBArchTypeEnum  arg_ArchType)
             {
                

                
                resetArchType();

                   
                   switch (arg_ArchType)
                   {
                     
                       case ArchType_INTEL :
                            
                            
                            property_ArchType = "INTEL";
                          break;
                     
                       case ArchType_X86_64 :
                            
                            
                            property_ArchType = "X86_64";
                          break;
                     
                     
                       default:
                          isValidArchType = false;
                          property_ArchType = "";
                          WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Error setting ArchType: undefined enum value");
                          return false;
                   }
                
                   if(property_ArchType.empty())
                   {
                       return AXIS2_FAILURE;
                   }
                     isValidArchType = true;
                        
                
                return true;
             }


             /**
             * specialized enum constructor for ArchType.
             */
            AviaryCommon::ArchType::ArchType(const ADBArchTypeEnum  arg_ArchType)
             {             
                   qname = NULL;
             
                 qname =  axutil_qname_create (Environment::getEnv(),
                       "ArchType",
                       "http://common.aviary.grid.redhat.com",
                       NULL);
               

            isValidArchType  = setArchTypeEnum( arg_ArchType );
            }

             

           /**
            * resetter for ArchType
            */
           bool WSF_CALL
           AviaryCommon::ArchType::resetArchType()
           {
               int i = 0;
               int count = 0;


               
               isValidArchType = false; 
               return true;
           }

           /**
            * Check whether ArchType is nill
            */
           bool WSF_CALL
           AviaryCommon::ArchType::isArchTypeNil()
           {
               return !isValidArchType;
           }

           /**
            * Set ArchType to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCommon::ArchType::setArchTypeNil()
           {
               return resetArchType();
           }

           

