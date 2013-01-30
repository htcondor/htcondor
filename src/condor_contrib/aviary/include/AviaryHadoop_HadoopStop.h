
          #ifndef AviaryHadoop_HADOOPSTOP_H
          #define AviaryHadoop_HADOOPSTOP_H
        
      
       /**
        * HadoopStop.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Jan 28, 2013 (02:30:05 CST)
        */

       /**
        *  HadoopStop class
        */

        namespace AviaryHadoop{
            class HadoopStop;
        }
        

        
                #include "AviaryHadoop_HadoopID.h"
              

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryHadoop
{
        
        

        class HadoopStop {

        private:
             std::vector<AviaryHadoop::HadoopID*>* property_Refs;

                
                bool isValidRefs;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setRefsNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class HadoopStop
         */

        HadoopStop();

        /**
         * Destructor HadoopStop
         */
        ~HadoopStop();


       

        /**
         * Constructor for creating HadoopStop
         * @param 
         * @param Refs std::vector<AviaryHadoop::HadoopID*>*
         * @return newly created HadoopStop object
         */
        HadoopStop(std::vector<AviaryHadoop::HadoopID*>* arg_Refs);
        

        /**
         * resetAll for HadoopStop
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        /******** Deprecated for array types, Use 'Getters and Setters for Arrays' instead ***********/
        

        /**
         * Getter for refs. Deprecated for array types, Use getRefsAt instead
         * @return Array of AviaryHadoop::HadoopID*s.
         */
        WSF_EXTERN std::vector<AviaryHadoop::HadoopID*>* WSF_CALL
        getRefs();

        /**
         * Setter for refs.Deprecated for array types, Use setRefsAt
         * or addRefs instead.
         * @param arg_Refs Array of AviaryHadoop::HadoopID*s.
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setRefs(std::vector<AviaryHadoop::HadoopID*>*  arg_Refs);

        /**
         * Re setter for refs
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetRefs();
        
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
         * Get the ith element of refs.
        * @param i index of the item to be obtained
         * @return ith AviaryHadoop::HadoopID* of the array
         */
        WSF_EXTERN AviaryHadoop::HadoopID* WSF_CALL
        getRefsAt(int i);

        /**
         * Set the ith element of refs. (If the ith already exist, it will be replaced)
         * @param i index of the item to return
         * @param arg_Refs element to set AviaryHadoop::HadoopID* to the array
         * @return ith AviaryHadoop::HadoopID* of the array
         */
        WSF_EXTERN bool WSF_CALL
        setRefsAt(int i,
                AviaryHadoop::HadoopID* arg_Refs);


        /**
         * Add to refs.
         * @param arg_Refs element to add AviaryHadoop::HadoopID* to the array
         * @return true on success, false otherwise.
         */
        WSF_EXTERN bool WSF_CALL
        addRefs(
            AviaryHadoop::HadoopID* arg_Refs);

        /**
         * Get the size of the refs array.
         * @return the size of the refs array.
         */
        WSF_EXTERN int WSF_CALL
        sizeofRefs();

        /**
         * Remove the ith element of refs.
         * @param i index of the item to remove
         * @return true on success, false otherwise.
         */
        WSF_EXTERN bool WSF_CALL
        removeRefsAt(int i);

        


        /******************************* Checking and Setting NIL values *********************************/
        /* Use 'Checking and Setting NIL values for Arrays' to check and set nil for individual elements */

        /**
         * NOTE: set_nil is only available for nillable properties
         */

        

        /**
         * Check whether refs is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isRefsNil();


        

        /*************************** Checking and Setting 'NIL' values in Arrays *****************************/

        /**
         * NOTE: You may set this to remove specific elements in the array
         *       But you can not remove elements, if the specific property is declared to be non-nillable or sizeof(array) < minOccurs
         */
        
        /**
         * Check whether refs is Nill at position i
         * @param i index of the item to return.
         * @return true if the value is Nil at position i, false otherwise
         */
        bool WSF_CALL
        isRefsNilAt(int i);
 
       
        /**
         * Set refs to NILL at the  position i.
         * @param i . The index of the item to be set Nill.
         * @return true on success, false otherwise.
         */
        bool WSF_CALL
        setRefsNilAt(int i);

        

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
         * @param HadoopStop_om_node node to serialize from
         * @param HadoopStop_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* HadoopStop_om_node, axiom_element_t *HadoopStop_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the HadoopStop is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for refs by property number (1)
         * @return Array of AviaryHadoop::HadoopIDs.
         */

        std::vector<AviaryHadoop::HadoopID*>* WSF_CALL
        getProperty1();

    

};

}        
 #endif /* HADOOPSTOP_H */
    

