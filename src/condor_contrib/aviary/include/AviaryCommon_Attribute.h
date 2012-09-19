
          #ifndef AviaryCommon_ATTRIBUTE_H
          #define AviaryCommon_ATTRIBUTE_H
        
      
       /**
        * Attribute.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Sep 18, 2012 (08:44:08 EDT)
        */

       /**
        *  Attribute class
        */

        namespace AviaryCommon{
            class Attribute;
        }
        

        
                #include "AviaryCommon_AttributeType.h"
              

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryCommon
{
        
        

        class Attribute {

        private:
             std::string property_Name;

                
                bool isValidName;
            AviaryCommon::AttributeType* property_Type;

                
                bool isValidType;
            std::string property_Value;

                
                bool isValidValue;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setNameNil();
            

        bool WSF_CALL
        setTypeNil();
            

        bool WSF_CALL
        setValueNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class Attribute
         */

        Attribute();

        /**
         * Destructor Attribute
         */
        ~Attribute();


       

        /**
         * Constructor for creating Attribute
         * @param 
         * @param Name std::string
         * @param Type AviaryCommon::AttributeType*
         * @param Value std::string
         * @return newly created Attribute object
         */
        Attribute(std::string arg_Name,AviaryCommon::AttributeType* arg_Type,std::string arg_Value);
        

        /**
         * resetAll for Attribute
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        
        

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
         * Getter for type. 
         * @return AviaryCommon::AttributeType*
         */
        WSF_EXTERN AviaryCommon::AttributeType* WSF_CALL
        getType();

        /**
         * Setter for type.
         * @param arg_Type AviaryCommon::AttributeType*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setType(AviaryCommon::AttributeType*  arg_Type);

        /**
         * Re setter for type
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetType();
        
        

        /**
         * Getter for value. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getValue();

        /**
         * Setter for value.
         * @param arg_Value std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setValue(const std::string  arg_Value);

        /**
         * Re setter for value
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetValue();
        


        /******************************* Checking and Setting NIL values *********************************/
        

        /**
         * NOTE: set_nil is only available for nillable properties
         */

        

        /**
         * Check whether name is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isNameNil();


        

        /**
         * Check whether type is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isTypeNil();


        

        /**
         * Check whether value is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isValueNil();


        

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
         * @param Attribute_om_node node to serialize from
         * @param Attribute_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* Attribute_om_node, axiom_element_t *Attribute_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the Attribute is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for name by property number (1)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty1();

    
        

        /**
         * Getter for type by property number (2)
         * @return AviaryCommon::AttributeType
         */

        AviaryCommon::AttributeType* WSF_CALL
        getProperty2();

    
        

        /**
         * Getter for value by property number (3)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty3();

    

};

}        
 #endif /* ATTRIBUTE_H */
    

