

        /**
         * ScanMode.cpp
         *
         * This file was auto-generated from WSDL
         * by the Apache Axis2/C version: SNAPSHOT  Built on : Mar 10, 2008 (08:35:52 GMT+00:00)
         */
        
            #include "AviaryCommon_ScanMode.h"
          

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
                * Implementation of the ScanMode|http://common.aviary.grid.redhat.com Element
                */
           AviaryCommon::ScanMode::ScanMode()
        {

        
            qname = NULL;
        
                    property_ScanMode;
                
            isValidScanMode  = false;
        
                  qname =  axutil_qname_create (Environment::getEnv(),
                        "ScanMode",
                        "http://common.aviary.grid.redhat.com",
                        NULL);
                
        }

       AviaryCommon::ScanMode::ScanMode(std::string arg_ScanMode)
        {
             
                   qname = NULL;
             
                 property_ScanMode;
             
            isValidScanMode  = true;
            
                 qname =  axutil_qname_create (Environment::getEnv(),
                       "ScanMode",
                       "http://common.aviary.grid.redhat.com",
                       NULL);
               
                    property_ScanMode = arg_ScanMode;
            
        }
        AviaryCommon::ScanMode::~ScanMode()
        {
            resetAll();
        }

        bool WSF_CALL AviaryCommon::ScanMode::resetAll()
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
            ScanMode::deserializeFromString(
                                            const axis2_char_t *node_value,
                                            axiom_node_t *parent)
            {
              bool status = true;
            
                        setScanMode(node_value);
                    
              return status;
            }
        

        bool WSF_CALL
        AviaryCommon::ScanMode::deserialize(axiom_node_t** dp_parent,bool *dp_is_early_node_valid, bool dont_care_minoccurs)
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
                WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "NULL value is set to a non nillable element ScanMode");
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
          AviaryCommon::ScanMode::isParticle()
          {
            
                 return false;
              
          }


          void WSF_CALL
          AviaryCommon::ScanMode::declareParentNamespaces(
                    axiom_element_t *parent_element,
                    axutil_hash_t *namespaces, int *next_ns_index)
          {
            
                  /* Here this is an empty function, Nothing to declare */
                 
          }

        
            char* WSF_CALL
            AviaryCommon::ScanMode::serializeToString(axutil_hash_t *namespaces)
            {
                axis2_char_t *text_value = NULL;
                axis2_char_t *qname_uri = NULL;
                axis2_char_t *qname_prefix = NULL;
                
                         text_value = (axis2_char_t*)axutil_xml_quote_string(Environment::getEnv(), (axis2_char_t*)property_ScanMode.c_str(), false);
                         if (!text_value)
                         {
                             text_value = (axis2_char_t*)axutil_strdup(Environment::getEnv(), property_ScanMode.c_str());
                         }
                      
                return text_value;
            }
        
        
        axiom_node_t* WSF_CALL
	AviaryCommon::ScanMode::serialize(axiom_node_t *parent, 
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
             * Getter for ScanMode by  Property Number 1
             */
            std::string WSF_CALL
            AviaryCommon::ScanMode::getProperty1()
            {
                return getScanMode();
            }

            /**
             * getter for ScanMode.
             */
            std::string WSF_CALL
            AviaryCommon::ScanMode::getScanMode()
             {
                return property_ScanMode;
             }

            /**
             * setter for ScanMode
             */
            bool WSF_CALL
            AviaryCommon::ScanMode::setScanMode(
                    const std::string  arg_ScanMode)
             {
                

                if(isValidScanMode &&
                        arg_ScanMode == property_ScanMode)
                {
                    
                    return true;
                }

                
                  if(arg_ScanMode.empty())
                       
                  {
                      WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"ScanMode is being set to NULL, but it is not a nullable element");
                      return AXIS2_FAILURE;
                  }
                

                
                resetScanMode();

                
                        property_ScanMode = std::string(arg_ScanMode.c_str());
                        isValidScanMode = true;
                    
                return true;
             }

            
             /**
             * specialized enum getter for ScanMode.
             */
             ADBScanModeEnum WSF_CALL
             AviaryCommon::ScanMode::getScanModeEnum()
             {

                
                 if (axutil_strcmp(property_ScanMode.c_str(), "AFTER") == 0)
                    return ScanMode_AFTER;
             
                 if (axutil_strcmp(property_ScanMode.c_str(), "BEFORE") == 0)
                    return ScanMode_BEFORE;
             
             
                 /* Error: none of the strings matched; invalid enum value */
                 return (ADBScanModeEnum)-1;
             }
             
             
             /**
             * specialized enum setter for ScanMode.
             */
             bool WSF_CALL
            AviaryCommon::ScanMode::setScanModeEnum(const ADBScanModeEnum  arg_ScanMode)
             {
                

                
                resetScanMode();

                   
                   switch (arg_ScanMode)
                   {
                     
                       case ScanMode_AFTER :
                            
                            
                            property_ScanMode = "AFTER";
                          break;
                     
                       case ScanMode_BEFORE :
                            
                            
                            property_ScanMode = "BEFORE";
                          break;
                     
                     
                       default:
                          isValidScanMode = false;
                          property_ScanMode = "";
                          WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Error setting ScanMode: undefined enum value");
                          return false;
                   }
                
                   if(property_ScanMode.empty())
                   {
                       return AXIS2_FAILURE;
                   }
                     isValidScanMode = true;
                        
                
                return true;
             }


             /**
             * specialized enum constructor for ScanMode.
             */
            AviaryCommon::ScanMode::ScanMode(const ADBScanModeEnum  arg_ScanMode)
             {             
                   qname = NULL;
             
                 qname =  axutil_qname_create (Environment::getEnv(),
                       "ScanMode",
                       "http://common.aviary.grid.redhat.com",
                       NULL);
               

            isValidScanMode  = setScanModeEnum( arg_ScanMode );
            }

             

           /**
            * resetter for ScanMode
            */
           bool WSF_CALL
           AviaryCommon::ScanMode::resetScanMode()
           {
               int i = 0;
               int count = 0;


               
               isValidScanMode = false; 
               return true;
           }

           /**
            * Check whether ScanMode is nill
            */
           bool WSF_CALL
           AviaryCommon::ScanMode::isScanModeNil()
           {
               return !isValidScanMode;
           }

           /**
            * Set ScanMode to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCommon::ScanMode::setScanModeNil()
           {
               return resetScanMode();
           }

           

