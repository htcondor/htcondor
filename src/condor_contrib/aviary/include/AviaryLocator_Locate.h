
          #ifndef AviaryLocator_LOCATE_H
          #define AviaryLocator_LOCATE_H
        
      
       /**
        * Locate.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Sep 18, 2012 (08:44:08 EDT)
        */

       /**
        *  Locate class
        */

        namespace AviaryLocator{
            class Locate;
        }
        

        
                #include "AviaryCommon_ResourceID.h"
              
        #include <axutil_qname.h>
        

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryLocator
{
        
        

        class Locate {

        private:
             
                axutil_qname_t* qname;
            AviaryCommon::ResourceID* property_Id;

                
                bool isValidId;
            bool property_PartialMatches;

                
                bool isValidPartialMatches;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setIdNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class Locate
         */

        Locate();

        /**
         * Destructor Locate
         */
        ~Locate();


       

        /**
         * Constructor for creating Locate
         * @param 
         * @param Id AviaryCommon::ResourceID*
         * @param PartialMatches bool
         * @return newly created Locate object
         */
        Locate(AviaryCommon::ResourceID* arg_Id,bool arg_PartialMatches);
        

        /**
         * resetAll for Locate
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        
        

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
         * Getter for partialMatches. 
         * @return bool
         */
        WSF_EXTERN bool WSF_CALL
        getPartialMatches();

        /**
         * Setter for partialMatches.
         * @param arg_PartialMatches bool
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setPartialMatches(bool  arg_PartialMatches);

        /**
         * Re setter for partialMatches
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetPartialMatches();
        


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
         * Check whether partialMatches is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isPartialMatchesNil();


        
        /**
         * Set partialMatches to Nill (same as using reset)
         * @return true on success, false otherwise.
         */
        bool WSF_CALL
        setPartialMatchesNil();
        

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
         * @param Locate_om_node node to serialize from
         * @param Locate_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* Locate_om_node, axiom_element_t *Locate_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the Locate is a particle class (E.g. group, inner sequence)
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
         * Getter for partialMatches by property number (2)
         * @return bool
         */

        bool WSF_CALL
        getProperty2();

    

};

}        
 #endif /* LOCATE_H */
    

