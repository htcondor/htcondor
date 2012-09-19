
          #ifndef AviaryCommon_RESOURCECONSTRAINTTYPE_H
          #define AviaryCommon_RESOURCECONSTRAINTTYPE_H
        
      
       /**
        * ResourceConstraintType.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Sep 18, 2012 (08:44:08 EDT)
        */

       /**
        *  ResourceConstraintType class
        */

        namespace AviaryCommon{
            class ResourceConstraintType;
        }
        

        
        #include <axutil_qname.h>
        

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryCommon
{
        
        /* Enumeration for this type */
        typedef enum {
            ResourceConstraintType_OS,
            ResourceConstraintType_ARCH,
            ResourceConstraintType_MEMORY,
            ResourceConstraintType_DISK,
            ResourceConstraintType_FILESYSTEM
        } ADBResourceConstraintTypeEnum;
        
        

        class ResourceConstraintType {

        private:
             
                axutil_qname_t* qname;
            std::string property_ResourceConstraintType;

                
                bool isValidResourceConstraintType;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setResourceConstraintTypeNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class ResourceConstraintType
         */

        ResourceConstraintType();

        /**
         * Destructor ResourceConstraintType
         */
        ~ResourceConstraintType();


       

        /**
         * Constructor for creating ResourceConstraintType
         * @param 
         * @param ResourceConstraintType std::string
         * @return newly created ResourceConstraintType object
         */
        ResourceConstraintType(std::string arg_ResourceConstraintType);
        ResourceConstraintType(const ADBResourceConstraintTypeEnum arg_ResourceConstraintType);
        

        /**
         * resetAll for ResourceConstraintType
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        
        

        /**
         * Getter for ResourceConstraintType. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getResourceConstraintType();

        /**
         * Setter for ResourceConstraintType.
         * @param arg_ResourceConstraintType std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setResourceConstraintType(const std::string  arg_ResourceConstraintType);

        /**
         * Re setter for ResourceConstraintType
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetResourceConstraintType();
        
            
            /************************** Getters and Setters For Enumerations ********************************/
            /********************* Enumeration Specific Operations: get_enum, set_enum **********************/
            
            /**
            * Enum getter for ResourceConstraintType.
            * @return ADBResourceConstraintTypeEnum; -1 on failure
            */
            ADBResourceConstraintTypeEnum WSF_CALL
            getResourceConstraintTypeEnum();
            
            /**
            * Enum setter for ResourceConstraintType.
            * @param arg_ResourceConstraintType ADBResourceConstraintTypeEnum
            * @return AXIS2_SUCCESS on success, else AXIS2_FAILURE
            */
            bool WSF_CALL
            setResourceConstraintTypeEnum(
            const ADBResourceConstraintTypeEnum arg_ResourceConstraintType);
            
          


        /******************************* Checking and Setting NIL values *********************************/
        

        /**
         * NOTE: set_nil is only available for nillable properties
         */

        

        /**
         * Check whether ResourceConstraintType is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isResourceConstraintTypeNil();


        

        /**************************** Serialize and De serialize functions ***************************/
        /*********** These functions are for use only inside the generated code *********************/

        
        /**
         * Deserialize the content from a string to an ADB object
         * @param node_value to deserialize
         * @param parent_element The parent element if it is an element, NULL otherwise
         * @return true on success, false otherwise
         */
       bool WSF_CALL
       deserializeFromString(const axis2_char_t *node_value, axiom_node_t *parent);
        
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
         * Serialize ADB object to a string
         * @param namespaces hash which contains a mapping of namespace uris to prefixes
         * @return serialized string
         */
         char* WSF_CALL
         serializeToString(axutil_hash_t *namespaces);
        

        /**
         * Serialize the ADB object to an xml
         * @param ResourceConstraintType_om_node node to serialize from
         * @param ResourceConstraintType_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* ResourceConstraintType_om_node, axiom_element_t *ResourceConstraintType_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the ResourceConstraintType is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for ResourceConstraintType by property number (1)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty1();

    

};

}        
 #endif /* RESOURCECONSTRAINTTYPE_H */
    

