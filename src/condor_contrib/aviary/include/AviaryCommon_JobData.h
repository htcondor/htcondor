

        #ifndef AviaryCommon_JOBDATA_H
        #define AviaryCommon_JOBDATA_H

       /**
        * JobData.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Mar 02, 2011 (11:54:00 EST)
        */

       /**
        *  JobData class
        */

        namespace AviaryCommon{
            class JobData;
        }
        

        
       #include "AviaryCommon_JobID.h"
          
       #include "AviaryCommon_Status.h"
          
       #include "AviaryCommon_JobDataType.h"
          

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryCommon
{
        
        

        class JobData {

        private:
             AviaryCommon::JobID* property_Id;

                
                bool isValidId;
            AviaryCommon::Status* property_Status;

                
                bool isValidStatus;
            AviaryCommon::JobDataType* property_Type;

                
                bool isValidType;
            std::string property_Content;

                
                bool isValidContent;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setIdNil();
            

        bool WSF_CALL
        setStatusNil();
            

        bool WSF_CALL
        setTypeNil();
            

        bool WSF_CALL
        setContentNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class JobData
         */

        JobData();

        /**
         * Destructor JobData
         */
        ~JobData();


       

        /**
         * Constructor for creating JobData
         * @param 
         * @param Id AviaryCommon::JobID*
         * @param Status AviaryCommon::Status*
         * @param Type AviaryCommon::JobDataType*
         * @param Content std::string
         * @return newly created JobData object
         */
        JobData(AviaryCommon::JobID* arg_Id,AviaryCommon::Status* arg_Status,AviaryCommon::JobDataType* arg_Type,std::string arg_Content);
        
        
        /********************************** Class get set methods **************************************/
        
        

        /**
         * Getter for id. 
         * @return AviaryCommon::JobID*
         */
        WSF_EXTERN AviaryCommon::JobID* WSF_CALL
        getId();

        /**
         * Setter for id.
         * @param arg_Id AviaryCommon::JobID*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setId(AviaryCommon::JobID*  arg_Id);

        /**
         * Re setter for id
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetId();
        
        

        /**
         * Getter for status. 
         * @return AviaryCommon::Status*
         */
        WSF_EXTERN AviaryCommon::Status* WSF_CALL
        getStatus();

        /**
         * Setter for status.
         * @param arg_Status AviaryCommon::Status*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setStatus(AviaryCommon::Status*  arg_Status);

        /**
         * Re setter for status
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetStatus();
        
        

        /**
         * Getter for type. 
         * @return AviaryCommon::JobDataType*
         */
        WSF_EXTERN AviaryCommon::JobDataType* WSF_CALL
        getType();

        /**
         * Setter for type.
         * @param arg_Type AviaryCommon::JobDataType*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setType(AviaryCommon::JobDataType*  arg_Type);

        /**
         * Re setter for type
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetType();
        
        

        /**
         * Getter for content. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getContent();

        /**
         * Setter for content.
         * @param arg_Content std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setContent(const std::string  arg_Content);

        /**
         * Re setter for content
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetContent();
        


        /******************************* Checking and Setting NIL values *********************************/
        

        /**
         * NOTE: set_nil is only available for nillable properties
         */

        

        /**
         * Check whether id is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isIdNil();


        

        /**
         * Check whether status is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isStatusNil();


        

        /**
         * Check whether type is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isTypeNil();


        

        /**
         * Check whether content is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isContentNil();


        

        /**************************** Serialize and De serialize functions ***************************/
        /*********** These functions are for use only inside the generated code *********************/

        
        /**
         * Deserialize the ADB object to an XML
         * @param dp_parent double pointer to the parent node to be deserialized
         * @param dp_is_early_node_valid double pointer to a flag (is_early_node_valid?)
         * @param dont_care_minoccurs Dont set errors on validating minoccurs, 
         *              (Parent will order this in a case of choice)
         * @return true on success, false otherwise
         */
        bool WSF_CALL
        deserialize(axiom_node_t** omNode, bool *isEarlyNodeValid, bool dontCareMinoccurs);
                         
            

       /**
         * Declare namespace in the most parent node 
         * @param parent_element parent element
         * @param namespaces hash of namespace uri to prefix
         * @param next_ns_index pointer to an int which contain the next namespace index
         */
        void WSF_CALL
        declareParentNamespaces(axiom_element_t *parent_element, axutil_hash_t *namespaces, int *next_ns_index);


        

        /**
         * Serialize the ADB object to an xml
         * @param JobData_om_node node to serialize from
         * @param JobData_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* JobData_om_node, axiom_element_t *JobData_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the JobData is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for id by property number (1)
         * @return AviaryCommon::JobID
         */

        AviaryCommon::JobID* WSF_CALL
        getProperty1();

    
        

        /**
         * Getter for status by property number (2)
         * @return AviaryCommon::Status
         */

        AviaryCommon::Status* WSF_CALL
        getProperty2();

    
        

        /**
         * Getter for type by property number (3)
         * @return AviaryCommon::JobDataType
         */

        AviaryCommon::JobDataType* WSF_CALL
        getProperty3();

    
        

        /**
         * Getter for content by property number (4)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty4();

    

};

}        
 #endif /* JOBDATA_H */
    

