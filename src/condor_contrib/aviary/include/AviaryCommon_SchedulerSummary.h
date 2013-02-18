
          #ifndef AviaryCommon_SCHEDULERSUMMARY_H
          #define AviaryCommon_SCHEDULERSUMMARY_H
        
      
       /**
        * SchedulerSummary.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Nov 08, 2012 (09:07:42 EST)
        */

       /**
        *  SchedulerSummary class
        */

        namespace AviaryCommon{
            class SchedulerSummary;
        }
        

        
        #include <axutil_date_time.h>
          

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryCommon
{
        
        

        class SchedulerSummary {

        private:
             axutil_date_time_t* property_Queue_created;

                
                bool isValidQueue_created;
            int property_Max_jobs_running;

                
                bool isValidMax_jobs_running;
            int property_Users;

                
                bool isValidUsers;
            int property_Ads;

                
                bool isValidAds;
            int property_Running;

                
                bool isValidRunning;
            int property_Held;

                
                bool isValidHeld;
            int property_Idle;

                
                bool isValidIdle;
            int property_Removed;

                
                bool isValidRemoved;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setQueue_createdNil();
            

        bool WSF_CALL
        setMax_jobs_runningNil();
            

        bool WSF_CALL
        setUsersNil();
            

        bool WSF_CALL
        setAdsNil();
            

        bool WSF_CALL
        setRunningNil();
            

        bool WSF_CALL
        setHeldNil();
            

        bool WSF_CALL
        setIdleNil();
            

        bool WSF_CALL
        setRemovedNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class SchedulerSummary
         */

        SchedulerSummary();

        /**
         * Destructor SchedulerSummary
         */
        ~SchedulerSummary();


       

        /**
         * Constructor for creating SchedulerSummary
         * @param 
         * @param Queue_created axutil_date_time_t*
         * @param Max_jobs_running int
         * @param Users int
         * @param Ads int
         * @param Running int
         * @param Held int
         * @param Idle int
         * @param Removed int
         * @return newly created SchedulerSummary object
         */
        SchedulerSummary(axutil_date_time_t* arg_Queue_created,int arg_Max_jobs_running,int arg_Users,int arg_Ads,int arg_Running,int arg_Held,int arg_Idle,int arg_Removed);
        

        /**
         * resetAll for SchedulerSummary
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        
        

        /**
         * Getter for queue_created. 
         * @return axutil_date_time_t*
         */
        WSF_EXTERN axutil_date_time_t* WSF_CALL
        getQueue_created();

        /**
         * Setter for queue_created.
         * @param arg_Queue_created axutil_date_time_t*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setQueue_created(axutil_date_time_t*  arg_Queue_created);

        /**
         * Re setter for queue_created
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetQueue_created();
        
        

        /**
         * Getter for max_jobs_running. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getMax_jobs_running();

        /**
         * Setter for max_jobs_running.
         * @param arg_Max_jobs_running int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setMax_jobs_running(const int  arg_Max_jobs_running);

        /**
         * Re setter for max_jobs_running
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetMax_jobs_running();
        
        

        /**
         * Getter for users. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getUsers();

        /**
         * Setter for users.
         * @param arg_Users int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setUsers(const int  arg_Users);

        /**
         * Re setter for users
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetUsers();
        
        

        /**
         * Getter for ads. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getAds();

        /**
         * Setter for ads.
         * @param arg_Ads int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setAds(const int  arg_Ads);

        /**
         * Re setter for ads
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetAds();
        
        

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
        
        

        /**
         * Getter for removed. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getRemoved();

        /**
         * Setter for removed.
         * @param arg_Removed int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setRemoved(const int  arg_Removed);

        /**
         * Re setter for removed
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetRemoved();
        


        /******************************* Checking and Setting NIL values *********************************/
        

        /**
         * NOTE: set_nil is only available for nillable properties
         */

        

        /**
         * Check whether queue_created is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isQueue_createdNil();


        

        /**
         * Check whether max_jobs_running is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isMax_jobs_runningNil();


        

        /**
         * Check whether users is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isUsersNil();


        

        /**
         * Check whether ads is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isAdsNil();


        

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


        

        /**
         * Check whether removed is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isRemovedNil();


        

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
         * @param SchedulerSummary_om_node node to serialize from
         * @param SchedulerSummary_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* SchedulerSummary_om_node, axiom_element_t *SchedulerSummary_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the SchedulerSummary is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for queue_created by property number (1)
         * @return axutil_date_time_t*
         */

        axutil_date_time_t* WSF_CALL
        getProperty1();

    
        

        /**
         * Getter for max_jobs_running by property number (2)
         * @return int
         */

        int WSF_CALL
        getProperty2();

    
        

        /**
         * Getter for users by property number (3)
         * @return int
         */

        int WSF_CALL
        getProperty3();

    
        

        /**
         * Getter for ads by property number (4)
         * @return int
         */

        int WSF_CALL
        getProperty4();

    
        

        /**
         * Getter for running by property number (5)
         * @return int
         */

        int WSF_CALL
        getProperty5();

    
        

        /**
         * Getter for held by property number (6)
         * @return int
         */

        int WSF_CALL
        getProperty6();

    
        

        /**
         * Getter for idle by property number (7)
         * @return int
         */

        int WSF_CALL
        getProperty7();

    
        

        /**
         * Getter for removed by property number (8)
         * @return int
         */

        int WSF_CALL
        getProperty8();

    

};

}        
 #endif /* SCHEDULERSUMMARY_H */
    

