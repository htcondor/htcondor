

        /**
         * JobStatusType.cpp
         *
         * This file was auto-generated from WSDL
         * by the Apache Axis2/C version: SNAPSHOT  Built on : Mar 10, 2008 (08:35:52 GMT+00:00)
         */
        
            #include "AviaryCommon_JobStatusType.h"
          

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
                * Implementation of the JobStatusType|http://common.aviary.grid.redhat.com Element
                */
           AviaryCommon::JobStatusType::JobStatusType()
        {

        
            qname = NULL;
        
                    property_JobStatusType;
                
            isValidJobStatusType  = false;
        
                  qname =  axutil_qname_create (Environment::getEnv(),
                        "JobStatusType",
                        "http://common.aviary.grid.redhat.com",
                        NULL);
                
        }

       AviaryCommon::JobStatusType::JobStatusType(std::string arg_JobStatusType)
        {
             
                   qname = NULL;
             
                 property_JobStatusType;
             
            isValidJobStatusType  = true;
            
                 qname =  axutil_qname_create (Environment::getEnv(),
                       "JobStatusType",
                       "http://common.aviary.grid.redhat.com",
                       NULL);
               
                    property_JobStatusType = arg_JobStatusType;
            
        }
        AviaryCommon::JobStatusType::~JobStatusType()
        {
            resetAll();
        }

        bool WSF_CALL AviaryCommon::JobStatusType::resetAll()
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
            JobStatusType::deserializeFromString(
                                            const axis2_char_t *node_value,
                                            axiom_node_t *parent)
            {
              bool status = true;
            
                        setJobStatusType(node_value);
                    
              return status;
            }
        

        bool WSF_CALL
        AviaryCommon::JobStatusType::deserialize(axiom_node_t** dp_parent,bool *dp_is_early_node_valid, bool dont_care_minoccurs)
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
                WSF_LOG_ERROR_MSG(Environment::getEnv()->log, WSF_LOG_SI, "NULL value is set to a non nillable element JobStatusType");
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
          AviaryCommon::JobStatusType::isParticle()
          {
            
                 return false;
              
          }


          void WSF_CALL
          AviaryCommon::JobStatusType::declareParentNamespaces(
                    axiom_element_t *parent_element,
                    axutil_hash_t *namespaces, int *next_ns_index)
          {
            
                  /* Here this is an empty function, Nothing to declare */
                 
          }

        
            char* WSF_CALL
            AviaryCommon::JobStatusType::serializeToString(axutil_hash_t *namespaces)
            {
                axis2_char_t *text_value = NULL;
                axis2_char_t *qname_uri = NULL;
                axis2_char_t *qname_prefix = NULL;
                
                         text_value = (axis2_char_t*)axutil_xml_quote_string(Environment::getEnv(), (axis2_char_t*)property_JobStatusType.c_str(), false);
                         if (!text_value)
                         {
                             text_value = (axis2_char_t*)axutil_strdup(Environment::getEnv(), property_JobStatusType.c_str());
                         }
                      
                return text_value;
            }
        
        
        axiom_node_t* WSF_CALL
	AviaryCommon::JobStatusType::serialize(axiom_node_t *parent, 
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
             * Getter for JobStatusType by  Property Number 1
             */
            std::string WSF_CALL
            AviaryCommon::JobStatusType::getProperty1()
            {
                return getJobStatusType();
            }

            /**
             * getter for JobStatusType.
             */
            std::string WSF_CALL
            AviaryCommon::JobStatusType::getJobStatusType()
             {
                return property_JobStatusType;
             }

            /**
             * setter for JobStatusType
             */
            bool WSF_CALL
            AviaryCommon::JobStatusType::setJobStatusType(
                    const std::string  arg_JobStatusType)
             {
                

                if(isValidJobStatusType &&
                        arg_JobStatusType == property_JobStatusType)
                {
                    
                    return true;
                }

                
                  if(arg_JobStatusType.empty())
                       
                  {
                      WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"JobStatusType is being set to NULL, but it is not a nullable element");
                      return AXIS2_FAILURE;
                  }
                

                
                resetJobStatusType();

                
                        property_JobStatusType = std::string(arg_JobStatusType.c_str());
                        isValidJobStatusType = true;
                    
                return true;
             }

            
             /**
             * specialized enum getter for JobStatusType.
             */
             ADBJobStatusTypeEnum WSF_CALL
             AviaryCommon::JobStatusType::getJobStatusTypeEnum()
             {

                
                 if (axutil_strcmp(property_JobStatusType.c_str(), "IDLE") == 0)
                    return JobStatusType_IDLE;
             
                 if (axutil_strcmp(property_JobStatusType.c_str(), "RUNNING") == 0)
                    return JobStatusType_RUNNING;
             
                 if (axutil_strcmp(property_JobStatusType.c_str(), "REMOVED") == 0)
                    return JobStatusType_REMOVED;
             
                 if (axutil_strcmp(property_JobStatusType.c_str(), "COMPLETED") == 0)
                    return JobStatusType_COMPLETED;
             
                 if (axutil_strcmp(property_JobStatusType.c_str(), "HELD") == 0)
                    return JobStatusType_HELD;
             
                 if (axutil_strcmp(property_JobStatusType.c_str(), "TRANSFERRING_OUTPUT") == 0)
                    return JobStatusType_TRANSFERRING_OUTPUT;
             
                 if (axutil_strcmp(property_JobStatusType.c_str(), "SUSPENDED") == 0)
                    return JobStatusType_SUSPENDED;
             
             
                 /* Error: none of the strings matched; invalid enum value */
                 return (ADBJobStatusTypeEnum)-1;
             }
             
             
             /**
             * specialized enum setter for JobStatusType.
             */
             bool WSF_CALL
            AviaryCommon::JobStatusType::setJobStatusTypeEnum(const ADBJobStatusTypeEnum  arg_JobStatusType)
             {
                

                
                resetJobStatusType();

                   
                   switch (arg_JobStatusType)
                   {
                     
                       case JobStatusType_IDLE :
                            
                            
                            property_JobStatusType = "IDLE";
                          break;
                     
                       case JobStatusType_RUNNING :
                            
                            
                            property_JobStatusType = "RUNNING";
                          break;
                     
                       case JobStatusType_REMOVED :
                            
                            
                            property_JobStatusType = "REMOVED";
                          break;
                     
                       case JobStatusType_COMPLETED :
                            
                            
                            property_JobStatusType = "COMPLETED";
                          break;
                     
                       case JobStatusType_HELD :
                            
                            
                            property_JobStatusType = "HELD";
                          break;
                     
                       case JobStatusType_TRANSFERRING_OUTPUT :
                            
                            
                            property_JobStatusType = "TRANSFERRING_OUTPUT";
                          break;
                     
                       case JobStatusType_SUSPENDED :
                            
                            
                            property_JobStatusType = "SUSPENDED";
                          break;
                     
                     
                       default:
                          isValidJobStatusType = false;
                          property_JobStatusType = "";
                          WSF_LOG_ERROR_MSG( Environment::getEnv()->log,WSF_LOG_SI,"Error setting JobStatusType: undefined enum value");
                          return false;
                   }
                
                   if(property_JobStatusType.empty())
                   {
                       return AXIS2_FAILURE;
                   }
                     isValidJobStatusType = true;
                        
                
                return true;
             }


             /**
             * specialized enum constructor for JobStatusType.
             */
            AviaryCommon::JobStatusType::JobStatusType(const ADBJobStatusTypeEnum  arg_JobStatusType)
             {             
                   qname = NULL;
             
                 qname =  axutil_qname_create (Environment::getEnv(),
                       "JobStatusType",
                       "http://common.aviary.grid.redhat.com",
                       NULL);
               

            isValidJobStatusType  = setJobStatusTypeEnum( arg_JobStatusType );
            }

             

           /**
            * resetter for JobStatusType
            */
           bool WSF_CALL
           AviaryCommon::JobStatusType::resetJobStatusType()
           {
               int i = 0;
               int count = 0;


               
               isValidJobStatusType = false; 
               return true;
           }

           /**
            * Check whether JobStatusType is nill
            */
           bool WSF_CALL
           AviaryCommon::JobStatusType::isJobStatusTypeNil()
           {
               return !isValidJobStatusType;
           }

           /**
            * Set JobStatusType to nill (currently the same as reset)
            */
           bool WSF_CALL
           AviaryCommon::JobStatusType::setJobStatusTypeNil()
           {
               return resetJobStatusType();
           }

           

