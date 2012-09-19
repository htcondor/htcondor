
          #ifndef AviaryJob_SETJOBATTRIBUTE_H
          #define AviaryJob_SETJOBATTRIBUTE_H
        
      
       /**
        * SetJobAttribute.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Sep 18, 2012 (08:44:08 EDT)
        */

       /**
        *  SetJobAttribute class
        */

        namespace AviaryJob{
            class SetJobAttribute;
        }
        

        
                #include "AviaryCommon_JobID.h"
              
                #include "AviaryCommon_Attribute.h"
              
        #include <axutil_qname.h>
        

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryJob
{
        
        

        class SetJobAttribute {

        private:
             
                axutil_qname_t* qname;
            AviaryCommon::JobID* property_Id;

                
                bool isValidId;
            AviaryCommon::Attribute* property_Attribute;

                
                bool isValidAttribute;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setIdNil();
            

        bool WSF_CALL
        setAttributeNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class SetJobAttribute
         */

        SetJobAttribute();

        /**
         * Destructor SetJobAttribute
         */
        ~SetJobAttribute();


       

        /**
         * Constructor for creating SetJobAttribute
         * @param 
         * @param Id AviaryCommon::JobID*
         * @param Attribute AviaryCommon::Attribute*
         * @return newly created SetJobAttribute object
         */
        SetJobAttribute(AviaryCommon::JobID* arg_Id,AviaryCommon::Attribute* arg_Attribute);
        

        /**
         * resetAll for SetJobAttribute
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        
        

        /**
         * Getter for id. 
         * @return AviaryCommon::JobID*
         */
        WSF_EXTERN AviaryCommon::JobID* WSF_CALL
        getId();

        /**
         * Setter for id.
         * @param arg_Id AviaryCommon::JobID*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setId(AviaryCommon::JobID*  arg_Id);

        /**
         * Re setter for id
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetId();
        
        

        /**
         * Getter for attribute. 
         * @return AviaryCommon::Attribute*
         */
        WSF_EXTERN AviaryCommon::Attribute* WSF_CALL
        getAttribute();

        /**
         * Setter for attribute.
         * @param arg_Attribute AviaryCommon::Attribute*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setAttribute(AviaryCommon::Attribute*  arg_Attribute);

        /**
         * Re setter for attribute
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetAttribute();
        


        /******************************* Checking and Setting NIL values *********************************/
        

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
         * Check whether attribute is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isAttributeNil();


        

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
         * @param SetJobAttribute_om_node node to serialize from
         * @param SetJobAttribute_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* SetJobAttribute_om_node, axiom_element_t *SetJobAttribute_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the SetJobAttribute is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for id by property number (1)
         * @return AviaryCommon::JobID
         */

        AviaryCommon::JobID* WSF_CALL
        getProperty1();

    
        

        /**
         * Getter for attribute by property number (2)
         * @return AviaryCommon::Attribute
         */

        AviaryCommon::Attribute* WSF_CALL
        getProperty2();

    

};

}        
 #endif /* SETJOBATTRIBUTE_H */
    

