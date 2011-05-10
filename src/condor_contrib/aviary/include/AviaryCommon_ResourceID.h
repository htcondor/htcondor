

        #ifndef AviaryCommon_RESOURCEID_H
        #define AviaryCommon_RESOURCEID_H

       /**
        * ResourceID.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Mar 02, 2011 (11:54:00 EST)
        */

       /**
        *  ResourceID class
        */

        namespace AviaryCommon{
            class ResourceID;
        }
        

        
       #include "AviaryCommon_ResourceType.h"
          

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryCommon
{
        
        

        class ResourceID {

        private:
             AviaryCommon::ResourceType* property_Subsystem_type;

                
                bool isValidSubsystem_type;
            std::string property_Pool;

                
                bool isValidPool;
            std::string property_Name;

                
                bool isValidName;
            std::string property_Custom_name;

                
                bool isValidCustom_name;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setSubsystem_typeNil();
            

        bool WSF_CALL
        setPoolNil();
            

        bool WSF_CALL
        setNameNil();
            

        bool WSF_CALL
        setCustom_nameNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class ResourceID
         */

        ResourceID();

        /**
         * Destructor ResourceID
         */
        ~ResourceID();


       

        /**
         * Constructor for creating ResourceID
         * @param 
         * @param Subsystem_type AviaryCommon::ResourceType*
         * @param Pool std::string
         * @param Name std::string
         * @param Custom_name std::string
         * @return newly created ResourceID object
         */
        ResourceID(AviaryCommon::ResourceType* arg_Subsystem_type,std::string arg_Pool,std::string arg_Name,std::string arg_Custom_name);
        
        
        /********************************** Class get set methods **************************************/
        
        

        /**
         * Getter for subsystem_type. 
         * @return AviaryCommon::ResourceType*
         */
        WSF_EXTERN AviaryCommon::ResourceType* WSF_CALL
        getSubsystem_type();

        /**
         * Setter for subsystem_type.
         * @param arg_Subsystem_type AviaryCommon::ResourceType*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setSubsystem_type(AviaryCommon::ResourceType*  arg_Subsystem_type);

        /**
         * Re setter for subsystem_type
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetSubsystem_type();
        
        

        /**
         * Getter for pool. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getPool();

        /**
         * Setter for pool.
         * @param arg_Pool std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setPool(const std::string  arg_Pool);

        /**
         * Re setter for pool
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetPool();
        
        

        /**
         * Getter for name. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getName();

        /**
         * Setter for name.
         * @param arg_Name std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setName(const std::string  arg_Name);

        /**
         * Re setter for name
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetName();
        
        

        /**
         * Getter for custom_name. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getCustom_name();

        /**
         * Setter for custom_name.
         * @param arg_Custom_name std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setCustom_name(const std::string  arg_Custom_name);

        /**
         * Re setter for custom_name
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetCustom_name();
        


        /******************************* Checking and Setting NIL values *********************************/
        

        /**
         * NOTE: set_nil is only available for nillable properties
         */

        

        /**
         * Check whether subsystem_type is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isSubsystem_typeNil();


        

        /**
         * Check whether pool is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isPoolNil();


        

        /**
         * Check whether name is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isNameNil();


        

        /**
         * Check whether custom_name is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isCustom_nameNil();


        

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
         * @param ResourceID_om_node node to serialize from
         * @param ResourceID_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* ResourceID_om_node, axiom_element_t *ResourceID_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the ResourceID is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for subsystem_type by property number (1)
         * @return AviaryCommon::ResourceType
         */

        AviaryCommon::ResourceType* WSF_CALL
        getProperty1();

    
        

        /**
         * Getter for pool by property number (2)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty2();

    
        

        /**
         * Getter for name by property number (3)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty3();

    
        

        /**
         * Getter for custom_name by property number (4)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty4();

    

};

}        
 #endif /* RESOURCEID_H */
    

