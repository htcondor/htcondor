
          #ifndef AviaryCommon_STATUS_H
          #define AviaryCommon_STATUS_H
        
      
       /**
        * Status.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Sep 18, 2012 (08:44:08 EDT)
        */

       /**
        *  Status class
        */

        namespace AviaryCommon{
            class Status;
        }
        

        
                #include "AviaryCommon_StatusCodeType.h"
              

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryCommon
{
        
        

        class Status {

        private:
             AviaryCommon::StatusCodeType* property_Code;

                
                bool isValidCode;
            std::string property_Text;

                
                bool isValidText;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setCodeNil();
            

        bool WSF_CALL
        setTextNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class Status
         */

        Status();

        /**
         * Destructor Status
         */
        ~Status();


       

        /**
         * Constructor for creating Status
         * @param 
         * @param Code AviaryCommon::StatusCodeType*
         * @param Text std::string
         * @return newly created Status object
         */
        Status(AviaryCommon::StatusCodeType* arg_Code,std::string arg_Text);
        

        /**
         * resetAll for Status
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        
        

        /**
         * Getter for code. 
         * @return AviaryCommon::StatusCodeType*
         */
        WSF_EXTERN AviaryCommon::StatusCodeType* WSF_CALL
        getCode();

        /**
         * Setter for code.
         * @param arg_Code AviaryCommon::StatusCodeType*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setCode(AviaryCommon::StatusCodeType*  arg_Code);

        /**
         * Re setter for code
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetCode();
        
        

        /**
         * Getter for text. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getText();

        /**
         * Setter for text.
         * @param arg_Text std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setText(const std::string  arg_Text);

        /**
         * Re setter for text
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetText();
        


        /******************************* Checking and Setting NIL values *********************************/
        

        /**
         * NOTE: set_nil is only available for nillable properties
         */

        

        /**
         * Check whether code is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isCodeNil();


        

        /**
         * Check whether text is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isTextNil();


        

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
         * @param Status_om_node node to serialize from
         * @param Status_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* Status_om_node, axiom_element_t *Status_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the Status is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for code by property number (1)
         * @return AviaryCommon::StatusCodeType
         */

        AviaryCommon::StatusCodeType* WSF_CALL
        getProperty1();

    
        

        /**
         * Getter for text by property number (2)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty2();

    

};

}        
 #endif /* STATUS_H */
    

