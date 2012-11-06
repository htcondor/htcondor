
          #ifndef AviaryCommon_JOBDATA_H
          #define AviaryCommon_JOBDATA_H
        
      
       /**
        * JobData.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Sep 18, 2012 (08:44:08 EDT)
        */

       /**
        *  JobData class
        */

        namespace AviaryCommon{
            class JobData;
        }
        

        
                #include "AviaryCommon_JobID.h"
              
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
            AviaryCommon::JobDataType* property_Type;

                
                bool isValidType;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setIdNil();
            

        bool WSF_CALL
        setTypeNil();
            



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
         * @param Type AviaryCommon::JobDataType*
         * @return newly created JobData object
         */
        JobData(AviaryCommon::JobID* arg_Id,AviaryCommon::JobDataType* arg_Type);
        

        /**
         * resetAll for JobData
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
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
         * Check whether type is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isTypeNil();


        

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
         * Getter for type by property number (2)
         * @return AviaryCommon::JobDataType
         */

        AviaryCommon::JobDataType* WSF_CALL
        getProperty2();

    

};

}        
 #endif /* JOBDATA_H */
    

