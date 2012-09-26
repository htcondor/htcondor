
          #ifndef AviaryLocator_LOCATERESPONSE_H
          #define AviaryLocator_LOCATERESPONSE_H
        
      
       /**
        * LocateResponse.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Sep 18, 2012 (08:44:08 EDT)
        */

       /**
        *  LocateResponse class
        */

        namespace AviaryLocator{
            class LocateResponse;
        }
        

        
                #include "AviaryCommon_ResourceLocation.h"
              
                #include "AviaryCommon_Status.h"
              
        #include <axutil_qname.h>
        

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryLocator
{
        
        

        class LocateResponse {

        private:
             
                axutil_qname_t* qname;
            std::vector<AviaryCommon::ResourceLocation*>* property_Resources;

                
                bool isValidResources;
            AviaryCommon::Status* property_Status;

                
                bool isValidStatus;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setResourcesNil();
            

        bool WSF_CALL
        setStatusNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class LocateResponse
         */

        LocateResponse();

        /**
         * Destructor LocateResponse
         */
        ~LocateResponse();


       

        /**
         * Constructor for creating LocateResponse
         * @param 
         * @param Resources std::vector<AviaryCommon::ResourceLocation*>*
         * @param Status AviaryCommon::Status*
         * @return newly created LocateResponse object
         */
        LocateResponse(std::vector<AviaryCommon::ResourceLocation*>* arg_Resources,AviaryCommon::Status* arg_Status);
        

        /**
         * resetAll for LocateResponse
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        /******** Deprecated for array types, Use 'Getters and Setters for Arrays' instead ***********/
        

        /**
         * Getter for resources. Deprecated for array types, Use getResourcesAt instead
         * @return Array of AviaryCommon::ResourceLocation*s.
         */
        WSF_EXTERN std::vector<AviaryCommon::ResourceLocation*>* WSF_CALL
        getResources();

        /**
         * Setter for resources.Deprecated for array types, Use setResourcesAt
         * or addResources instead.
         * @param arg_Resources Array of AviaryCommon::ResourceLocation*s.
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setResources(std::vector<AviaryCommon::ResourceLocation*>*  arg_Resources);

        /**
         * Re setter for resources
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetResources();
        
        

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
         * Get the ith element of resources.
        * @param i index of the item to be obtained
         * @return ith AviaryCommon::ResourceLocation* of the array
         */
        WSF_EXTERN AviaryCommon::ResourceLocation* WSF_CALL
        getResourcesAt(int i);

        /**
         * Set the ith element of resources. (If the ith already exist, it will be replaced)
         * @param i index of the item to return
         * @param arg_Resources element to set AviaryCommon::ResourceLocation* to the array
         * @return ith AviaryCommon::ResourceLocation* of the array
         */
        WSF_EXTERN bool WSF_CALL
        setResourcesAt(int i,
                AviaryCommon::ResourceLocation* arg_Resources);


        /**
         * Add to resources.
         * @param arg_Resources element to add AviaryCommon::ResourceLocation* to the array
         * @return true on success, false otherwise.
         */
        WSF_EXTERN bool WSF_CALL
        addResources(
            AviaryCommon::ResourceLocation* arg_Resources);

        /**
         * Get the size of the resources array.
         * @return the size of the resources array.
         */
        WSF_EXTERN int WSF_CALL
        sizeofResources();

        /**
         * Remove the ith element of resources.
         * @param i index of the item to remove
         * @return true on success, false otherwise.
         */
        WSF_EXTERN bool WSF_CALL
        removeResourcesAt(int i);

        


        /******************************* Checking and Setting NIL values *********************************/
        /* Use 'Checking and Setting NIL values for Arrays' to check and set nil for individual elements */

        /**
         * NOTE: set_nil is only available for nillable properties
         */

        

        /**
         * Check whether resources is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isResourcesNil();


        

        /**
         * Check whether status is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isStatusNil();


        

        /*************************** Checking and Setting 'NIL' values in Arrays *****************************/

        /**
         * NOTE: You may set this to remove specific elements in the array
         *       But you can not remove elements, if the specific property is declared to be non-nillable or sizeof(array) < minOccurs
         */
        
        /**
         * Check whether resources is Nill at position i
         * @param i index of the item to return.
         * @return true if the value is Nil at position i, false otherwise
         */
        bool WSF_CALL
        isResourcesNilAt(int i);
 
       
        /**
         * Set resources to NILL at the  position i.
         * @param i . The index of the item to be set Nill.
         * @return true on success, false otherwise.
         */
        bool WSF_CALL
        setResourcesNilAt(int i);

        

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
         * @param LocateResponse_om_node node to serialize from
         * @param LocateResponse_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* LocateResponse_om_node, axiom_element_t *LocateResponse_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the LocateResponse is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for resources by property number (1)
         * @return Array of AviaryCommon::ResourceLocations.
         */

        std::vector<AviaryCommon::ResourceLocation*>* WSF_CALL
        getProperty1();

    
        

        /**
         * Getter for status by property number (2)
         * @return AviaryCommon::Status
         */

        AviaryCommon::Status* WSF_CALL
        getProperty2();

    

};

}        
 #endif /* LOCATERESPONSE_H */
    

