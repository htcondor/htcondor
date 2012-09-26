
          #ifndef AviaryCommon_RESOURCELOCATION_H
          #define AviaryCommon_RESOURCELOCATION_H
        
      
       /**
        * ResourceLocation.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Sep 18, 2012 (08:44:08 EDT)
        */

       /**
        *  ResourceLocation class
        */

        namespace AviaryCommon{
            class ResourceLocation;
        }
        

        
                #include "AviaryCommon_ResourceID.h"
              

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryCommon
{
        
        

        class ResourceLocation {

        private:
             AviaryCommon::ResourceID* property_Id;

                
                bool isValidId;
            std::vector<axutil_uri_t*>* property_Location;

                
                bool isValidLocation;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setIdNil();
            

        bool WSF_CALL
        setLocationNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class ResourceLocation
         */

        ResourceLocation();

        /**
         * Destructor ResourceLocation
         */
        ~ResourceLocation();


       

        /**
         * Constructor for creating ResourceLocation
         * @param 
         * @param Id AviaryCommon::ResourceID*
         * @param Location std::vector<axutil_uri_t*>*
         * @return newly created ResourceLocation object
         */
        ResourceLocation(AviaryCommon::ResourceID* arg_Id,std::vector<axutil_uri_t*>* arg_Location);
        

        /**
         * resetAll for ResourceLocation
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
         * Getter for location. Deprecated for array types, Use getLocationAt instead
         * @return Array of axutil_uri_t*s.
         */
        WSF_EXTERN std::vector<axutil_uri_t*>* WSF_CALL
        getLocation();

        /**
         * Setter for location.Deprecated for array types, Use setLocationAt
         * or addLocation instead.
         * @param arg_Location Array of axutil_uri_t*s.
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setLocation(std::vector<axutil_uri_t*>*  arg_Location);

        /**
         * Re setter for location
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetLocation();
        
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
         * Get the ith element of location.
        * @param i index of the item to be obtained
         * @return ith axutil_uri_t* of the array
         */
        WSF_EXTERN axutil_uri_t* WSF_CALL
        getLocationAt(int i);

        /**
         * Set the ith element of location. (If the ith already exist, it will be replaced)
         * @param i index of the item to return
         * @param arg_Location element to set axutil_uri_t* to the array
         * @return ith axutil_uri_t* of the array
         */
        WSF_EXTERN bool WSF_CALL
        setLocationAt(int i,
                axutil_uri_t* arg_Location);


        /**
         * Add to location.
         * @param arg_Location element to add axutil_uri_t* to the array
         * @return true on success, false otherwise.
         */
        WSF_EXTERN bool WSF_CALL
        addLocation(
            axutil_uri_t* arg_Location);

        /**
         * Get the size of the location array.
         * @return the size of the location array.
         */
        WSF_EXTERN int WSF_CALL
        sizeofLocation();

        /**
         * Remove the ith element of location.
         * @param i index of the item to remove
         * @return true on success, false otherwise.
         */
        WSF_EXTERN bool WSF_CALL
        removeLocationAt(int i);

        


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
         * Check whether location is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isLocationNil();


        

        /*************************** Checking and Setting 'NIL' values in Arrays *****************************/

        /**
         * NOTE: You may set this to remove specific elements in the array
         *       But you can not remove elements, if the specific property is declared to be non-nillable or sizeof(array) < minOccurs
         */
        
        /**
         * Check whether location is Nill at position i
         * @param i index of the item to return.
         * @return true if the value is Nil at position i, false otherwise
         */
        bool WSF_CALL
        isLocationNilAt(int i);
 
       
        /**
         * Set location to NILL at the  position i.
         * @param i . The index of the item to be set Nill.
         * @return true on success, false otherwise.
         */
        bool WSF_CALL
        setLocationNilAt(int i);

        

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
         * @param ResourceLocation_om_node node to serialize from
         * @param ResourceLocation_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* ResourceLocation_om_node, axiom_element_t *ResourceLocation_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the ResourceLocation is a particle class (E.g. group, inner sequence)
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
         * Getter for location by property number (2)
         * @return Array of axutil_uri_t*s.
         */

        std::vector<axutil_uri_t*>* WSF_CALL
        getProperty2();

    

};

}        
 #endif /* RESOURCELOCATION_H */
    

