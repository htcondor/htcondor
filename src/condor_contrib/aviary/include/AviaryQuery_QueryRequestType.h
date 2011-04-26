

        #ifndef AviaryQuery_QUERYREQUESTTYPE_H
        #define AviaryQuery_QUERYREQUESTTYPE_H

       /**
        * QueryRequestType.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Mar 02, 2011 (11:54:00 EST)
        */

       /**
        *  QueryRequestType class
        */

        namespace AviaryQuery{
            class QueryRequestType;
        }
        

        

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryQuery
{
        
        

        class QueryRequestType {

        private:
             bool property_AllowPartialMatching;

                
                bool isValidAllowPartialMatching;
            

        /*** Private methods ***/
          



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class QueryRequestType
         */

        QueryRequestType();

        /**
         * Destructor QueryRequestType
         */
        ~QueryRequestType();


       

        /**
         * Constructor for creating QueryRequestType
         * @param 
         * @param AllowPartialMatching bool
         * @return newly created QueryRequestType object
         */
        QueryRequestType(bool arg_AllowPartialMatching);
        
        
        /********************************** Class get set methods **************************************/
        
        

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
        


        /******************************* Checking and Setting NIL values *********************************/
        

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
         * @param QueryRequestType_om_node node to serialize from
         * @param QueryRequestType_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* QueryRequestType_om_node, axiom_element_t *QueryRequestType_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the QueryRequestType is a particle class (E.g. group, inner sequence)
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

    

};

}        
 #endif /* QUERYREQUESTTYPE_H */
    

