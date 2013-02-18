
          #ifndef AviaryCommon_COLLECTORSUMMARY_H
          #define AviaryCommon_COLLECTORSUMMARY_H
        
      
       /**
        * CollectorSummary.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Nov 08, 2012 (09:07:42 EST)
        */

       /**
        *  CollectorSummary class
        */

        namespace AviaryCommon{
            class CollectorSummary;
        }
        

        

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryCommon
{
        
        

        class CollectorSummary {

        private:
             int property_Running_jobs;

                
                bool isValidRunning_jobs;
            int property_Idle_jobs;

                
                bool isValidIdle_jobs;
            int property_Total_hosts;

                
                bool isValidTotal_hosts;
            int property_Claimed_hosts;

                
                bool isValidClaimed_hosts;
            int property_Unclaimed_hosts;

                
                bool isValidUnclaimed_hosts;
            int property_Owner_hosts;

                
                bool isValidOwner_hosts;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setRunning_jobsNil();
            

        bool WSF_CALL
        setIdle_jobsNil();
            

        bool WSF_CALL
        setTotal_hostsNil();
            

        bool WSF_CALL
        setClaimed_hostsNil();
            

        bool WSF_CALL
        setUnclaimed_hostsNil();
            

        bool WSF_CALL
        setOwner_hostsNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class CollectorSummary
         */

        CollectorSummary();

        /**
         * Destructor CollectorSummary
         */
        ~CollectorSummary();


       

        /**
         * Constructor for creating CollectorSummary
         * @param 
         * @param Running_jobs int
         * @param Idle_jobs int
         * @param Total_hosts int
         * @param Claimed_hosts int
         * @param Unclaimed_hosts int
         * @param Owner_hosts int
         * @return newly created CollectorSummary object
         */
        CollectorSummary(int arg_Running_jobs,int arg_Idle_jobs,int arg_Total_hosts,int arg_Claimed_hosts,int arg_Unclaimed_hosts,int arg_Owner_hosts);
        

        /**
         * resetAll for CollectorSummary
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        
        

        /**
         * Getter for running_jobs. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getRunning_jobs();

        /**
         * Setter for running_jobs.
         * @param arg_Running_jobs int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setRunning_jobs(const int  arg_Running_jobs);

        /**
         * Re setter for running_jobs
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetRunning_jobs();
        
        

        /**
         * Getter for idle_jobs. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getIdle_jobs();

        /**
         * Setter for idle_jobs.
         * @param arg_Idle_jobs int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setIdle_jobs(const int  arg_Idle_jobs);

        /**
         * Re setter for idle_jobs
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetIdle_jobs();
        
        

        /**
         * Getter for total_hosts. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getTotal_hosts();

        /**
         * Setter for total_hosts.
         * @param arg_Total_hosts int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setTotal_hosts(const int  arg_Total_hosts);

        /**
         * Re setter for total_hosts
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetTotal_hosts();
        
        

        /**
         * Getter for claimed_hosts. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getClaimed_hosts();

        /**
         * Setter for claimed_hosts.
         * @param arg_Claimed_hosts int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setClaimed_hosts(const int  arg_Claimed_hosts);

        /**
         * Re setter for claimed_hosts
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetClaimed_hosts();
        
        

        /**
         * Getter for unclaimed_hosts. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getUnclaimed_hosts();

        /**
         * Setter for unclaimed_hosts.
         * @param arg_Unclaimed_hosts int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setUnclaimed_hosts(const int  arg_Unclaimed_hosts);

        /**
         * Re setter for unclaimed_hosts
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetUnclaimed_hosts();
        
        

        /**
         * Getter for owner_hosts. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getOwner_hosts();

        /**
         * Setter for owner_hosts.
         * @param arg_Owner_hosts int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setOwner_hosts(const int  arg_Owner_hosts);

        /**
         * Re setter for owner_hosts
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetOwner_hosts();
        


        /******************************* Checking and Setting NIL values *********************************/
        

        /**
         * NOTE: set_nil is only available for nillable properties
         */

        

        /**
         * Check whether running_jobs is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isRunning_jobsNil();


        

        /**
         * Check whether idle_jobs is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isIdle_jobsNil();


        

        /**
         * Check whether total_hosts is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isTotal_hostsNil();


        

        /**
         * Check whether claimed_hosts is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isClaimed_hostsNil();


        

        /**
         * Check whether unclaimed_hosts is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isUnclaimed_hostsNil();


        

        /**
         * Check whether owner_hosts is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isOwner_hostsNil();


        

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
         * @param CollectorSummary_om_node node to serialize from
         * @param CollectorSummary_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* CollectorSummary_om_node, axiom_element_t *CollectorSummary_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the CollectorSummary is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for running_jobs by property number (1)
         * @return int
         */

        int WSF_CALL
        getProperty1();

    
        

        /**
         * Getter for idle_jobs by property number (2)
         * @return int
         */

        int WSF_CALL
        getProperty2();

    
        

        /**
         * Getter for total_hosts by property number (3)
         * @return int
         */

        int WSF_CALL
        getProperty3();

    
        

        /**
         * Getter for claimed_hosts by property number (4)
         * @return int
         */

        int WSF_CALL
        getProperty4();

    
        

        /**
         * Getter for unclaimed_hosts by property number (5)
         * @return int
         */

        int WSF_CALL
        getProperty5();

    
        

        /**
         * Getter for owner_hosts by property number (6)
         * @return int
         */

        int WSF_CALL
        getProperty6();

    

};

}        
 #endif /* COLLECTORSUMMARY_H */
    

