
          #ifndef AviaryQuery_GETSUBMISSIONSUMMARYRESPONSE_H
          #define AviaryQuery_GETSUBMISSIONSUMMARYRESPONSE_H
        
      
       /**
        * GetSubmissionSummaryResponse.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Sep 18, 2012 (08:44:08 EDT)
        */

       /**
        *  GetSubmissionSummaryResponse class
        */

        namespace AviaryQuery{
            class GetSubmissionSummaryResponse;
        }
        

        
                #include "AviaryCommon_SubmissionSummary.h"
              
        #include <axutil_qname.h>
        

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryQuery
{
        
        

        class GetSubmissionSummaryResponse {

        private:
             
                axutil_qname_t* qname;
            std::vector<AviaryCommon::SubmissionSummary*>* property_Submissions;

                
                bool isValidSubmissions;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setSubmissionsNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class GetSubmissionSummaryResponse
         */

        GetSubmissionSummaryResponse();

        /**
         * Destructor GetSubmissionSummaryResponse
         */
        ~GetSubmissionSummaryResponse();


       

        /**
         * Constructor for creating GetSubmissionSummaryResponse
         * @param 
         * @param Submissions std::vector<AviaryCommon::SubmissionSummary*>*
         * @return newly created GetSubmissionSummaryResponse object
         */
        GetSubmissionSummaryResponse(std::vector<AviaryCommon::SubmissionSummary*>* arg_Submissions);
        

        /**
         * resetAll for GetSubmissionSummaryResponse
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        /******** Deprecated for array types, Use 'Getters and Setters for Arrays' instead ***********/
        

        /**
         * Getter for submissions. Deprecated for array types, Use getSubmissionsAt instead
         * @return Array of AviaryCommon::SubmissionSummary*s.
         */
        WSF_EXTERN std::vector<AviaryCommon::SubmissionSummary*>* WSF_CALL
        getSubmissions();

        /**
         * Setter for submissions.Deprecated for array types, Use setSubmissionsAt
         * or addSubmissions instead.
         * @param arg_Submissions Array of AviaryCommon::SubmissionSummary*s.
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setSubmissions(std::vector<AviaryCommon::SubmissionSummary*>*  arg_Submissions);

        /**
         * Re setter for submissions
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetSubmissions();
        
        /****************************** Get Set methods for Arrays **********************************/
        /************ Array Specific Operations: get_at, set_at, add, remove_at, sizeof *****************/

        /**
         * E.g. use of get_at, set_at, add and sizeof
         *
         * for(i = 0; i < adb_element->sizeofProperty(); i ++ )
         * {
         *     // Getting ith value to property_object variable
         *     property_object = adb_element->getPropertyAt(i);
         *
         *     // Setting ith value from property_object variable
         *     adb_element->setPropertyAt(i, property_object);
         *
         *     // Appending the value to the end of the array from property_object variable
         *     adb_element->addProperty(property_object);
         *
         *     // Removing the ith value from an array
         *     adb_element->removePropertyAt(i);
         *     
         * }
         *
         */

        
        
        /**
         * Get the ith element of submissions.
        * @param i index of the item to be obtained
         * @return ith AviaryCommon::SubmissionSummary* of the array
         */
        WSF_EXTERN AviaryCommon::SubmissionSummary* WSF_CALL
        getSubmissionsAt(int i);

        /**
         * Set the ith element of submissions. (If the ith already exist, it will be replaced)
         * @param i index of the item to return
         * @param arg_Submissions element to set AviaryCommon::SubmissionSummary* to the array
         * @return ith AviaryCommon::SubmissionSummary* of the array
         */
        WSF_EXTERN bool WSF_CALL
        setSubmissionsAt(int i,
                AviaryCommon::SubmissionSummary* arg_Submissions);


        /**
         * Add to submissions.
         * @param arg_Submissions element to add AviaryCommon::SubmissionSummary* to the array
         * @return true on success, false otherwise.
         */
        WSF_EXTERN bool WSF_CALL
        addSubmissions(
            AviaryCommon::SubmissionSummary* arg_Submissions);

        /**
         * Get the size of the submissions array.
         * @return the size of the submissions array.
         */
        WSF_EXTERN int WSF_CALL
        sizeofSubmissions();

        /**
         * Remove the ith element of submissions.
         * @param i index of the item to remove
         * @return true on success, false otherwise.
         */
        WSF_EXTERN bool WSF_CALL
        removeSubmissionsAt(int i);

        


        /******************************* Checking and Setting NIL values *********************************/
        /* Use 'Checking and Setting NIL values for Arrays' to check and set nil for individual elements */

        /**
         * NOTE: set_nil is only available for nillable properties
         */

        

        /**
         * Check whether submissions is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isSubmissionsNil();


        

        /*************************** Checking and Setting 'NIL' values in Arrays *****************************/

        /**
         * NOTE: You may set this to remove specific elements in the array
         *       But you can not remove elements, if the specific property is declared to be non-nillable or sizeof(array) < minOccurs
         */
        
        /**
         * Check whether submissions is Nill at position i
         * @param i index of the item to return.
         * @return true if the value is Nil at position i, false otherwise
         */
        bool WSF_CALL
        isSubmissionsNilAt(int i);
 
       
        /**
         * Set submissions to NILL at the  position i.
         * @param i . The index of the item to be set Nill.
         * @return true on success, false otherwise.
         */
        bool WSF_CALL
        setSubmissionsNilAt(int i);

        

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
         * @param GetSubmissionSummaryResponse_om_node node to serialize from
         * @param GetSubmissionSummaryResponse_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* GetSubmissionSummaryResponse_om_node, axiom_element_t *GetSubmissionSummaryResponse_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the GetSubmissionSummaryResponse is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for submissions by property number (1)
         * @return Array of AviaryCommon::SubmissionSummarys.
         */

        std::vector<AviaryCommon::SubmissionSummary*>* WSF_CALL
        getProperty1();

    

};

}        
 #endif /* GETSUBMISSIONSUMMARYRESPONSE_H */
    

