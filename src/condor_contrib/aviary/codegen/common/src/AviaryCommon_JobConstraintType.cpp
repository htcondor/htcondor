

        /**
         * JobConstraintType.cpp
         *
         * This file was auto-generated from WSDL
         * by the Apache Axis2/C version: SNAPSHOT  Built on : Mar 10, 2008 (08:35:52 GMT+00:00)
         */
        
            #include "AviaryCommon_JobConstraintType.h"
          

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
                * Implementation of the JobConstraintType|http://common.aviary.grid.redhat.com Element
                */
           AviaryCommon::JobConstraintType::JobConstraintType()
        {

        
            qname = NULL;
        
                    property_JobConstraintType;
                
            isValidJobConstraintType  = false;
        
                  qname =  axutil_qname_create (Environment::getEnv(),
                        "JobConstraintType",
                        "http://common.aviary.grid.redhat.com",
                        NULL);
                
        }

       AviaryCommon::JobConstraintType::JobConstraintType(std::string arg_JobConstraintType)
        {
             
                   qname = NULL;
             
                 property_JobConstraintType;
             
            isValidJobConstraintType  = true;
            
                 qname =  axutil_qname_create (Environment::getEnv(),
                       "JobConstraintType",
                       "http://common.aviary.grid.redhat.com",
                       NULL);
               
                    property_JobConstraintType = arg_JobConstraintType;
            
        }
        AviaryCommon::JobConstraintType::~JobConstraintType()
        {
            resetAll();
        }

        bool WSF_CALL AviaryCommon::JobConstraintType::resetAll()
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
            JobConstraintType::deserializeFromString(
                                            const axis2_char_t *node_value,
                                            axiom_node_t *parent)
            {
              bool status = true;
            
                        setJobConstraintType(node_value);
                    
              return status;
            }
        

        bool WSF_CALL
        AviaryCommon::JobConstraintType::deserialize(axiom_node_t** dp_parent,bool *dp_is_early_node_valid, bool dont_care_minoccurs)
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
                WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "NULL value is set to a non nillable element JobConstraintType");
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
          AviaryCommon::JobConstraintType::isParticle()
          {
            
                 return false;
              
          }


          void WSF_CALL
          AviaryCommon::JobConstraintType::declareParentNamespaces(
                    axiom_element_t *parent_element,
                    axutil_hash_t *namespaces, int *next_ns_index)
          {
            
                  /* Here this is an empty function, Nothing to declare */
                 
          }

        
            char* WSF_CALL
            AviaryCommon::JobConstraintType::serializeToString(axutil_hash_t *namespaces)
            {
                axis2_char_t *text_value = NULL;
                axis2_char_t *qname_uri = NULL;
                axis2_char_t *qname_prefix = NULL;
                
                         text_value = (axis2_char_t*)axutil_xml_quote_string(Environment::getEnv(), (axis2_char_t*)property_JobConstraintType.c_str(), false);
                         if (!text_value)
                         {
                             text_value = (axis2_char_t*)axutil_strdup(Environment::getEnv(), property_JobConstraintType.c_str());
                         }
                      
                return text_value;
            }
        
        
        axiom_node_t* WSF_CALL
	AviaryCommon::JobConstraintType::serialize(axiom_node_t *parent, 
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
             * Getter for JobConstraintType by  Property Number 1
             */
            std::string WSF_CALL
            AviaryCommon::JobConstraintType::getProperty1()
            {
                return getJobConstraintType();
            }

            /**
             * getter for JobConstraintType.
             */
            std::string WSF_CALL
            AviaryCommon::JobConstraintType::getJobConstraintType()
             {
                return property_JobConstraintType;
             }

            /**
             * setter for JobConstraintType
             */
            bool WSF_CALL
            AviaryCommon::JobConstraintType::setJobConstraintType(
                    const std::string  arg_JobConstraintType)
             {
                

                if(isValidJobConstraintType &&
                        arg_JobConstraintType == property_JobConstraintType)
                {
                    
                    return true;
                }

                
                  if(arg_JobConstraintType.empty())
                       
                  {
                      WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"JobConstraintType is being set to NULL, but it is not a nullable element");
                      return AXIS2_FAILURE;
                  }
                

                
                resetJobConstraintType();

                
                        property_JobConstraintType = std::string(arg_JobConstraintType.c_str());
                        isValidJobConstraintType = true;
                    
                return true;
             }

            
             /**
             * specialized enum getter for JobConstraintType.
             */
             ADBJobConstraintTypeEnum WSF_CALL
             AviaryCommon::JobConstraintType::getJobConstraintTypeEnum()
             {

                
                 if (axutil_strcmp(property_JobConstraintType.c_str(), "CMD") == 0)
                    return JobConstraintType_CMD;
             
                 if (axutil_strcmp(property_JobConstraintType.c_str(), "ARGS") == 0)
                    return JobConstraintType_ARGS;
             
                 if (axutil_strcmp(property_JobConstraintType.c_str(), "OWNER") == 0)
                    return JobConstraintType_OWNER;
             
             
                 /* Error: none of the strings matched; invalid enum value */
                 return (ADBJobConstraintTypeEnum)-1;
             }
             
             
             /**
             * specialized enum setter for JobConstraintType.
             */
             bool WSF_CALL
            AviaryCommon::JobConstraintType::setJobConstraintTypeEnum(const ADBJobConstraintTypeEnum  arg_JobConstraintType)
             {
                

                
                resetJobConstraintType();

                   
                   switch (arg_JobConstraintType)
                   {
                     
                       case JobConstraintType_CMD :
                            
                            
                            property_JobConstraintType = "CMD";
                          break;
                     
                       case JobConstraintType_ARGS :
                            
                            
                            property_JobConstraintType = "ARGS";
                          break;
                     
                       case JobConstraintType_OWNER :
                            
                            
                            property_JobConstraintType = "OWNER";
                          break;
                     
                     
                       default:
                          isValidJobConstraintType = false;
                          property_JobConstraintType = "";
                          WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Error setting JobConstraintType: undefined enum value");
                          return false;
                   }
                
                   if(property_JobConstraintType.empty())
                   {
                       return AXIS2_FAILURE;
                   }
                     isValidJobConstraintType = true;
                        
                
                return true;
             }


             /**
             * specialized enum constructor for JobConstraintType.
             */
            AviaryCommon::JobConstraintType::JobConstraintType(const ADBJobConstraintTypeEnum  arg_JobConstraintType)
             {             
                   qname = NULL;
             
                 qname =  axutil_qname_create (Environment::getEnv(),
                       "JobConstraintType",
                       "http://common.aviary.grid.redhat.com",
                       NULL);
               

            isValidJobConstraintType  = setJobConstraintTypeEnum( arg_JobConstraintType );
            }

             

           /**
            * resetter for JobConstraintType
            */
           bool WSF_CALL
           AviaryCommon::JobConstraintType::resetJobConstraintType()
           {
               int i = 0;
               int count = 0;


               
               isValidJobConstraintType = false; 
               return true;
           }

           /**
            * Check whether JobConstraintType is nill
            */
           bool WSF_CALL
           AviaryCommon::JobConstraintType::isJobConstraintTypeNil()
           {
               return !isValidJobConstraintType;
           }

           /**
            * Set JobConstraintType to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCommon::JobConstraintType::setJobConstraintTypeNil()
           {
               return resetJobConstraintType();
           }

           

