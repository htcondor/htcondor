

        #ifndef AviaryQuery_GETJOBDATA_H
        #define AviaryQuery_GETJOBDATA_H

       /**
        * GetJobData.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Mar 02, 2011 (11:54:00 EST)
        */

       /**
        *  GetJobData class
        */

        namespace AviaryQuery{
            class GetJobData;
        }
        

        
       #include "AviaryCommon_JobID.h"
          
        #include <axutil_qname.h>
        

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryQuery
{
        
        

        class GetJobData {

        private:
             
                axutil_qname_t* qname;
            bool property_AllowPartialMatching;

                
                bool isValidAllowPartialMatching;
            std::vector<AviaryCommon::JobID*>* property_Ids;

                
                bool isValidIds;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setIdsNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class GetJobData
         */

        GetJobData();

        /**
         * Destructor GetJobData
         */
        ~GetJobData();


       

        /**
         * Constructor for creating GetJobData
         * @param 
         * @param AllowPartialMatching bool
         * @param Ids std::vector<AviaryCommon::JobID*>*
         * @return newly created GetJobData object
         */
        GetJobData(bool arg_AllowPartialMatching,std::vector<AviaryCommon::JobID*>* arg_Ids);
        
        
        /********************************** Class get set methods **************************************/
        /******** Deprecated for array types, Use 'Getters and Setters for Arrays' instead ***********/
        

        /**
         * Getter for allowPartialMatching. 
         * @return bool
         */
        WSF_EXTERN bool WSF_CALL
        getAllowPartialMatching();

        /**
         * Setter for allowPartialMatching.
         * @param arg_AllowPartialMatching bool
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setAllowPartialMatching(bool  arg_AllowPartialMatching);

        /**
         * Re setter for allowPartialMatching
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetAllowPartialMatching();
        
        

        /**
         * Getter for ids. Deprecated for array types, Use getIdsAt instead
         * @return Array of AviaryCommon::JobID*s.
         */
        WSF_EXTERN std::vector<AviaryCommon::JobID*>* WSF_CALL
        getIds();

        /**
         * Setter for ids.Deprecated for array types, Use setIdsAt
         * or addIds instead.
         * @param arg_Ids Array of AviaryCommon::JobID*s.
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setIds(std::vector<AviaryCommon::JobID*>*  arg_Ids);

        /**
         * Re setter for ids
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetIds();
        
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
         * Get the ith element of ids.
        * @param i index of the item to be obtained
         * @return ith AviaryCommon::JobID* of the array
         */
        WSF_EXTERN AviaryCommon::JobID* WSF_CALL
        getIdsAt(int i);

        /**
         * Set the ith element of ids. (If the ith already exist, it will be replaced)
         * @param i index of the item to return
         * @param arg_Ids element to set AviaryCommon::JobID* to the array
         * @return ith AviaryCommon::JobID* of the array
         */
        WSF_EXTERN bool WSF_CALL
        setIdsAt(int i,
                AviaryCommon::JobID* arg_Ids);


        /**
         * Add to ids.
         * @param arg_Ids element to add AviaryCommon::JobID* to the array
         * @return true on success, false otherwise.
         */
        WSF_EXTERN bool WSF_CALL
        addIds(
            AviaryCommon::JobID* arg_Ids);

        /**
         * Get the size of the ids array.
         * @return the size of the ids array.
         */
        WSF_EXTERN int WSF_CALL
        sizeofIds();

        /**
         * Remove the ith element of ids.
         * @param i index of the item to remove
         * @return true on success, false otherwise.
         */
        WSF_EXTERN bool WSF_CALL
        removeIdsAt(int i);

        


        /******************************* Checking and Setting NIL values *********************************/
        /* Use 'Checking and Setting NIL values for Arrays' to check and set nil for individual elements */

        /**
         * NOTE: set_nil is only available for nillable properties
         */

        

        /**
         * Check whether allowPartialMatching is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isAllowPartialMatchingNil();


        
        /**
         * Set allowPartialMatching to Nill (same as using reset)
         * @return true on success, false otherwise.
         */
        bool WSF_CALL
        setAllowPartialMatchingNil();
        

        /**
         * Check whether ids is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isIdsNil();


        

        /*************************** Checking and Setting 'NIL' values in Arrays *****************************/

        /**
         * NOTE: You may set this to remove specific elements in the array
         *       But you can not remove elements, if the specific property is declared to be non-nillable or sizeof(array) < minOccurs
         */
        
        /**
         * Check whether ids is Nill at position i
         * @param i index of the item to return.
         * @return true if the value is Nil at position i, false otherwise
         */
        bool WSF_CALL
        isIdsNilAt(int i);
 
       
        /**
         * Set ids to NILL at the  position i.
         * @param i . The index of the item to be set Nill.
         * @return true on success, false otherwise.
         */
        bool WSF_CALL
        setIdsNilAt(int i);

        

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
         * @param GetJobData_om_node node to serialize from
         * @param GetJobData_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* GetJobData_om_node, axiom_element_t *GetJobData_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the GetJobData is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for allowPartialMatching by property number (1)
         * @return bool
         */

        bool WSF_CALL
        getProperty1();

    
        

        /**
         * Getter for ids by property number (2)
         * @return Array of AviaryCommon::JobIDs.
         */

        std::vector<AviaryCommon::JobID*>* WSF_CALL
        getProperty2();

    

};

}        
 #endif /* GETJOBDATA_H */
    

