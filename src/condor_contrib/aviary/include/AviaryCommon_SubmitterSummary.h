
          #ifndef AviaryCommon_SUBMITTERSUMMARY_H
          #define AviaryCommon_SUBMITTERSUMMARY_H
        
      
       /**
        * SubmitterSummary.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Nov 08, 2012 (09:07:42 EST)
        */

       /**
        *  SubmitterSummary class
        */

        namespace AviaryCommon{
            class SubmitterSummary;
        }
        

        

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryCommon
{
        
        

        class SubmitterSummary {

        private:
             int property_Running;

                
                bool isValidRunning;
            int property_Held;

                
                bool isValidHeld;
            int property_Idle;

                
                bool isValidIdle;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setRunningNil();
            

        bool WSF_CALL
        setHeldNil();
            

        bool WSF_CALL
        setIdleNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class SubmitterSummary
         */

        SubmitterSummary();

        /**
         * Destructor SubmitterSummary
         */
        ~SubmitterSummary();


       

        /**
         * Constructor for creating SubmitterSummary
         * @param 
         * @param Running int
         * @param Held int
         * @param Idle int
         * @return newly created SubmitterSummary object
         */
        SubmitterSummary(int arg_Running,int arg_Held,int arg_Idle);
        

        /**
         * resetAll for SubmitterSummary
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        
        

        /**
         * Getter for running. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getRunning();

        /**
         * Setter for running.
         * @param arg_Running int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setRunning(const int  arg_Running);

        /**
         * Re setter for running
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetRunning();
        
        

        /**
         * Getter for held. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getHeld();

        /**
         * Setter for held.
         * @param arg_Held int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setHeld(const int  arg_Held);

        /**
         * Re setter for held
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetHeld();
        
        

        /**
         * Getter for idle. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getIdle();

        /**
         * Setter for idle.
         * @param arg_Idle int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setIdle(const int  arg_Idle);

        /**
         * Re setter for idle
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetIdle();
        


        /******************************* Checking and Setting NIL values *********************************/
        

        /**
         * NOTE: set_nil is only available for nillable properties
         */

        

        /**
         * Check whether running is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isRunningNil();


        

        /**
         * Check whether held is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isHeldNil();


        

        /**
         * Check whether idle is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isIdleNil();


        

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
         * @param SubmitterSummary_om_node node to serialize from
         * @param SubmitterSummary_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* SubmitterSummary_om_node, axiom_element_t *SubmitterSummary_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the SubmitterSummary is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for running by property number (1)
         * @return int
         */

        int WSF_CALL
        getProperty1();

    
        

        /**
         * Getter for held by property number (2)
         * @return int
         */

        int WSF_CALL
        getProperty2();

    
        

        /**
         * Getter for idle by property number (3)
         * @return int
         */

        int WSF_CALL
        getProperty3();

    

};

}        
 #endif /* SUBMITTERSUMMARY_H */
    

