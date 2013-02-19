
          #ifndef AviaryCommon_SUBMITTERID_H
          #define AviaryCommon_SUBMITTERID_H
        
      
       /**
        * SubmitterID.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Nov 08, 2012 (09:07:42 EST)
        */

       /**
        *  SubmitterID class
        */

        namespace AviaryCommon{
            class SubmitterID;
        }
        

        

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryCommon
{
        
        

        class SubmitterID {

        private:
             std::string property_Name;

                
                bool isValidName;
            std::string property_Machine;

                
                bool isValidMachine;
            std::string property_Scheduler;

                
                bool isValidScheduler;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setNameNil();
            

        bool WSF_CALL
        setMachineNil();
            

        bool WSF_CALL
        setSchedulerNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class SubmitterID
         */

        SubmitterID();

        /**
         * Destructor SubmitterID
         */
        ~SubmitterID();


       

        /**
         * Constructor for creating SubmitterID
         * @param 
         * @param Name std::string
         * @param Machine std::string
         * @param Scheduler std::string
         * @return newly created SubmitterID object
         */
        SubmitterID(std::string arg_Name,std::string arg_Machine,std::string arg_Scheduler);
        

        /**
         * resetAll for SubmitterID
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
         * Getter for machine. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getMachine();

        /**
         * Setter for machine.
         * @param arg_Machine std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setMachine(const std::string  arg_Machine);

        /**
         * Re setter for machine
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetMachine();
        
        

        /**
         * Getter for scheduler. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getScheduler();

        /**
         * Setter for scheduler.
         * @param arg_Scheduler std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setScheduler(const std::string  arg_Scheduler);

        /**
         * Re setter for scheduler
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetScheduler();
        


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
         * Check whether machine is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isMachineNil();


        

        /**
         * Check whether scheduler is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isSchedulerNil();


        

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
         * @param SubmitterID_om_node node to serialize from
         * @param SubmitterID_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* SubmitterID_om_node, axiom_element_t *SubmitterID_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the SubmitterID is a particle class (E.g. group, inner sequence)
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
         * Getter for machine by property number (2)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty2();

    
        

        /**
         * Getter for scheduler by property number (3)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty3();

    

};

}        
 #endif /* SUBMITTERID_H */
    

