
          #ifndef AviaryCollector_ATTRIBUTEREQUEST_H
          #define AviaryCollector_ATTRIBUTEREQUEST_H
        
      
       /**
        * AttributeRequest.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Nov 08, 2012 (09:07:42 EST)
        */

       /**
        *  AttributeRequest class
        */

        namespace AviaryCollector{
            class AttributeRequest;
        }
        

        
                #include "AviaryCommon_ResourceID.h"
              

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryCollector
{
        
        

        class AttributeRequest {

        private:
             AviaryCommon::ResourceID* property_Id;

                
                bool isValidId;
            std::vector<std::string*>* property_Names;

                
                bool isValidNames;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setIdNil();
            

        bool WSF_CALL
        setNamesNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class AttributeRequest
         */

        AttributeRequest();

        /**
         * Destructor AttributeRequest
         */
        ~AttributeRequest();


       

        /**
         * Constructor for creating AttributeRequest
         * @param 
         * @param Id AviaryCommon::ResourceID*
         * @param Names std::vector<std::string*>*
         * @return newly created AttributeRequest object
         */
        AttributeRequest(AviaryCommon::ResourceID* arg_Id,std::vector<std::string*>* arg_Names);
        

        /**
         * resetAll for AttributeRequest
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        /******** Deprecated for array types, Use 'Getters and Setters for Arrays' instead ***********/
        

        /**
         * Getter for id. 
         * @return AviaryCommon::ResourceID*
         */
        WSF_EXTERN AviaryCommon::ResourceID* WSF_CALL
        getId();

        /**
         * Setter for id.
         * @param arg_Id AviaryCommon::ResourceID*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setId(AviaryCommon::ResourceID*  arg_Id);

        /**
         * Re setter for id
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetId();
        
        

        /**
         * Getter for names. Deprecated for array types, Use getNamesAt instead
         * @return Array of std::strings.
         */
        WSF_EXTERN std::vector<std::string*>* WSF_CALL
        getNames();

        /**
         * Setter for names.Deprecated for array types, Use setNamesAt
         * or addNames instead.
         * @param arg_Names Array of std::strings.
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setNames(std::vector<std::string*>*  arg_Names);

        /**
         * Re setter for names
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetNames();
        
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
         * Get the ith element of names.
        * @param i index of the item to be obtained
         * @return ith std::string of the array
         */
        WSF_EXTERN std::string WSF_CALL
        getNamesAt(int i);

        /**
         * Set the ith element of names. (If the ith already exist, it will be replaced)
         * @param i index of the item to return
         * @param arg_Names element to set std::string to the array
         * @return ith std::string of the array
         */
        WSF_EXTERN bool WSF_CALL
        setNamesAt(int i,
                const std::string arg_Names);


        /**
         * Add to names.
         * @param arg_Names element to add std::string to the array
         * @return true on success, false otherwise.
         */
        WSF_EXTERN bool WSF_CALL
        addNames(
            const std::string arg_Names);

        /**
         * Get the size of the names array.
         * @return the size of the names array.
         */
        WSF_EXTERN int WSF_CALL
        sizeofNames();

        /**
         * Remove the ith element of names.
         * @param i index of the item to remove
         * @return true on success, false otherwise.
         */
        WSF_EXTERN bool WSF_CALL
        removeNamesAt(int i);

        


        /******************************* Checking and Setting NIL values *********************************/
        /* Use 'Checking and Setting NIL values for Arrays' to check and set nil for individual elements */

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
         * Check whether names is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isNamesNil();


        

        /*************************** Checking and Setting 'NIL' values in Arrays *****************************/

        /**
         * NOTE: You may set this to remove specific elements in the array
         *       But you can not remove elements, if the specific property is declared to be non-nillable or sizeof(array) < minOccurs
         */
        
        /**
         * Check whether names is Nill at position i
         * @param i index of the item to return.
         * @return true if the value is Nil at position i, false otherwise
         */
        bool WSF_CALL
        isNamesNilAt(int i);
 
       
        /**
         * Set names to NILL at the  position i.
         * @param i . The index of the item to be set Nill.
         * @return true on success, false otherwise.
         */
        bool WSF_CALL
        setNamesNilAt(int i);

        

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
         * @param AttributeRequest_om_node node to serialize from
         * @param AttributeRequest_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* AttributeRequest_om_node, axiom_element_t *AttributeRequest_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the AttributeRequest is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for id by property number (1)
         * @return AviaryCommon::ResourceID
         */

        AviaryCommon::ResourceID* WSF_CALL
        getProperty1();

    
        

        /**
         * Getter for names by property number (2)
         * @return Array of std::strings.
         */

        std::vector<std::string*>* WSF_CALL
        getProperty2();

    

};

}        
 #endif /* ATTRIBUTEREQUEST_H */
    

