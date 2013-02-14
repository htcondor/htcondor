
          #ifndef AviaryCommon_SUBMITTER_H
          #define AviaryCommon_SUBMITTER_H
        
      
       /**
        * Submitter.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Nov 08, 2012 (09:07:42 EST)
        */

       /**
        *  Submitter class
        */

        namespace AviaryCommon{
            class Submitter;
        }
        

        
                #include "AviaryCommon_SubmitterID.h"
              
                #include "AviaryCommon_Status.h"
              
                #include "AviaryCommon_SubmitterSummary.h"
              

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryCommon
{
        
        

        class Submitter {

        private:
             AviaryCommon::SubmitterID* property_Id;

                
                bool isValidId;
            AviaryCommon::Status* property_Status;

                
                bool isValidStatus;
            AviaryCommon::SubmitterSummary* property_Summary;

                
                bool isValidSummary;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setIdNil();
            

        bool WSF_CALL
        setStatusNil();
            

        bool WSF_CALL
        setSummaryNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class Submitter
         */

        Submitter();

        /**
         * Destructor Submitter
         */
        ~Submitter();


       

        /**
         * Constructor for creating Submitter
         * @param 
         * @param Id AviaryCommon::SubmitterID*
         * @param Status AviaryCommon::Status*
         * @param Summary AviaryCommon::SubmitterSummary*
         * @return newly created Submitter object
         */
        Submitter(AviaryCommon::SubmitterID* arg_Id,AviaryCommon::Status* arg_Status,AviaryCommon::SubmitterSummary* arg_Summary);
        

        /**
         * resetAll for Submitter
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        
        

        /**
         * Getter for id. 
         * @return AviaryCommon::SubmitterID*
         */
        WSF_EXTERN AviaryCommon::SubmitterID* WSF_CALL
        getId();

        /**
         * Setter for id.
         * @param arg_Id AviaryCommon::SubmitterID*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setId(AviaryCommon::SubmitterID*  arg_Id);

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
         * Getter for summary. 
         * @return AviaryCommon::SubmitterSummary*
         */
        WSF_EXTERN AviaryCommon::SubmitterSummary* WSF_CALL
        getSummary();

        /**
         * Setter for summary.
         * @param arg_Summary AviaryCommon::SubmitterSummary*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setSummary(AviaryCommon::SubmitterSummary*  arg_Summary);

        /**
         * Re setter for summary
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetSummary();
        


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
         * Check whether summary is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isSummaryNil();


        

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
         * @param Submitter_om_node node to serialize from
         * @param Submitter_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* Submitter_om_node, axiom_element_t *Submitter_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the Submitter is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for id by property number (1)
         * @return AviaryCommon::SubmitterID
         */

        AviaryCommon::SubmitterID* WSF_CALL
        getProperty1();

    
        

        /**
         * Getter for status by property number (2)
         * @return AviaryCommon::Status
         */

        AviaryCommon::Status* WSF_CALL
        getProperty2();

    
        

        /**
         * Getter for summary by property number (3)
         * @return AviaryCommon::SubmitterSummary
         */

        AviaryCommon::SubmitterSummary* WSF_CALL
        getProperty3();

    

};

}        
 #endif /* SUBMITTER_H */
    

