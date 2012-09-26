

        /**
         * JobDataType.cpp
         *
         * This file was auto-generated from WSDL
         * by the Apache Axis2/C version: SNAPSHOT  Built on : Mar 10, 2008 (08:35:52 GMT+00:00)
         */
        
            #include "AviaryCommon_JobDataType.h"
          

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
                * Implementation of the JobDataType|http://common.aviary.grid.redhat.com Element
                */
           AviaryCommon::JobDataType::JobDataType()
        {

        
            qname = NULL;
        
                    property_JobDataType;
                
            isValidJobDataType  = false;
        
                  qname =  axutil_qname_create (Environment::getEnv(),
                        "JobDataType",
                        "http://common.aviary.grid.redhat.com",
                        NULL);
                
        }

       AviaryCommon::JobDataType::JobDataType(std::string arg_JobDataType)
        {
             
                   qname = NULL;
             
                 property_JobDataType;
             
            isValidJobDataType  = true;
            
                 qname =  axutil_qname_create (Environment::getEnv(),
                       "JobDataType",
                       "http://common.aviary.grid.redhat.com",
                       NULL);
               
                    property_JobDataType = arg_JobDataType;
            
        }
        AviaryCommon::JobDataType::~JobDataType()
        {
            resetAll();
        }

        bool WSF_CALL AviaryCommon::JobDataType::resetAll()
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
            JobDataType::deserializeFromString(
                                            const axis2_char_t *node_value,
                                            axiom_node_t *parent)
            {
              bool status = true;
            
                        setJobDataType(node_value);
                    
              return status;
            }
        

        bool WSF_CALL
        AviaryCommon::JobDataType::deserialize(axiom_node_t** dp_parent,bool *dp_is_early_node_valid, bool dont_care_minoccurs)
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
                WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "NULL value is set to a non nillable element JobDataType");
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
          AviaryCommon::JobDataType::isParticle()
          {
            
                 return false;
              
          }


          void WSF_CALL
          AviaryCommon::JobDataType::declareParentNamespaces(
                    axiom_element_t *parent_element,
                    axutil_hash_t *namespaces, int *next_ns_index)
          {
            
                  /* Here this is an empty function, Nothing to declare */
                 
          }

        
            char* WSF_CALL
            AviaryCommon::JobDataType::serializeToString(axutil_hash_t *namespaces)
            {
                axis2_char_t *text_value = NULL;
                axis2_char_t *qname_uri = NULL;
                axis2_char_t *qname_prefix = NULL;
                
                         text_value = (axis2_char_t*)axutil_xml_quote_string(Environment::getEnv(), (axis2_char_t*)property_JobDataType.c_str(), false);
                         if (!text_value)
                         {
                             text_value = (axis2_char_t*)axutil_strdup(Environment::getEnv(), property_JobDataType.c_str());
                         }
                      
                return text_value;
            }
        
        
        axiom_node_t* WSF_CALL
	AviaryCommon::JobDataType::serialize(axiom_node_t *parent, 
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
             * Getter for JobDataType by  Property Number 1
             */
            std::string WSF_CALL
            AviaryCommon::JobDataType::getProperty1()
            {
                return getJobDataType();
            }

            /**
             * getter for JobDataType.
             */
            std::string WSF_CALL
            AviaryCommon::JobDataType::getJobDataType()
             {
                return property_JobDataType;
             }

            /**
             * setter for JobDataType
             */
            bool WSF_CALL
            AviaryCommon::JobDataType::setJobDataType(
                    const std::string  arg_JobDataType)
             {
                

                if(isValidJobDataType &&
                        arg_JobDataType == property_JobDataType)
                {
                    
                    return true;
                }

                
                  if(arg_JobDataType.empty())
                       
                  {
                      WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"JobDataType is being set to NULL, but it is not a nullable element");
                      return AXIS2_FAILURE;
                  }
                

                
                resetJobDataType();

                
                        property_JobDataType = std::string(arg_JobDataType.c_str());
                        isValidJobDataType = true;
                    
                return true;
             }

            
             /**
             * specialized enum getter for JobDataType.
             */
             ADBJobDataTypeEnum WSF_CALL
             AviaryCommon::JobDataType::getJobDataTypeEnum()
             {

                
                 if (axutil_strcmp(property_JobDataType.c_str(), "ERR") == 0)
                    return JobDataType_ERR;
             
                 if (axutil_strcmp(property_JobDataType.c_str(), "LOG") == 0)
                    return JobDataType_LOG;
             
                 if (axutil_strcmp(property_JobDataType.c_str(), "OUT") == 0)
                    return JobDataType_OUT;
             
             
                 /* Error: none of the strings matched; invalid enum value */
                 return (ADBJobDataTypeEnum)-1;
             }
             
             
             /**
             * specialized enum setter for JobDataType.
             */
             bool WSF_CALL
            AviaryCommon::JobDataType::setJobDataTypeEnum(const ADBJobDataTypeEnum  arg_JobDataType)
             {
                

                
                resetJobDataType();

                   
                   switch (arg_JobDataType)
                   {
                     
                       case JobDataType_ERR :
                            
                            
                            property_JobDataType = "ERR";
                          break;
                     
                       case JobDataType_LOG :
                            
                            
                            property_JobDataType = "LOG";
                          break;
                     
                       case JobDataType_OUT :
                            
                            
                            property_JobDataType = "OUT";
                          break;
                     
                     
                       default:
                          isValidJobDataType = false;
                          property_JobDataType = "";
                          WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Error setting JobDataType: undefined enum value");
                          return false;
                   }
                
                   if(property_JobDataType.empty())
                   {
                       return AXIS2_FAILURE;
                   }
                     isValidJobDataType = true;
                        
                
                return true;
             }


             /**
             * specialized enum constructor for JobDataType.
             */
            AviaryCommon::JobDataType::JobDataType(const ADBJobDataTypeEnum  arg_JobDataType)
             {             
                   qname = NULL;
             
                 qname =  axutil_qname_create (Environment::getEnv(),
                       "JobDataType",
                       "http://common.aviary.grid.redhat.com",
                       NULL);
               

            isValidJobDataType  = setJobDataTypeEnum( arg_JobDataType );
            }

             

           /**
            * resetter for JobDataType
            */
           bool WSF_CALL
           AviaryCommon::JobDataType::resetJobDataType()
           {
               int i = 0;
               int count = 0;


               
               isValidJobDataType = false; 
               return true;
           }

           /**
            * Check whether JobDataType is nill
            */
           bool WSF_CALL
           AviaryCommon::JobDataType::isJobDataTypeNil()
           {
               return !isValidJobDataType;
           }

           /**
            * Set JobDataType to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCommon::JobDataType::setJobDataTypeNil()
           {
               return resetJobDataType();
           }

           

