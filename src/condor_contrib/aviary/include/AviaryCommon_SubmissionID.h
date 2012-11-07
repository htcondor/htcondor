
          #ifndef AviaryCommon_SUBMISSIONID_H
          #define AviaryCommon_SUBMISSIONID_H
        
      
       /**
        * SubmissionID.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Sep 18, 2012 (08:44:08 EDT)
        */

       /**
        *  SubmissionID class
        */

        namespace AviaryCommon{
            class SubmissionID;
        }
        

        

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryCommon
{
        
        

        class SubmissionID {

        private:
             std::string property_Name;

                
                bool isValidName;
            std::string property_Owner;

                
                bool isValidOwner;
            int property_Qdate;

                
                bool isValidQdate;
            std::string property_Pool;

                
                bool isValidPool;
            std::string property_Scheduler;

                
                bool isValidScheduler;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setNameNil();
            

        bool WSF_CALL
        setOwnerNil();
            

        bool WSF_CALL
        setQdateNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class SubmissionID
         */

        SubmissionID();

        /**
         * Destructor SubmissionID
         */
        ~SubmissionID();


       

        /**
         * Constructor for creating SubmissionID
         * @param 
         * @param Name std::string
         * @param Owner std::string
         * @param Qdate int
         * @param Pool std::string
         * @param Scheduler std::string
         * @return newly created SubmissionID object
         */
        SubmissionID(std::string arg_Name,std::string arg_Owner,int arg_Qdate,std::string arg_Pool,std::string arg_Scheduler);
        

        /**
         * resetAll for SubmissionID
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
         * Getter for owner. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getOwner();

        /**
         * Setter for owner.
         * @param arg_Owner std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setOwner(const std::string  arg_Owner);

        /**
         * Re setter for owner
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetOwner();
        
        

        /**
         * Getter for qdate. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getQdate();

        /**
         * Setter for qdate.
         * @param arg_Qdate int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setQdate(const int  arg_Qdate);

        /**
         * Re setter for qdate
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetQdate();
        
        

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
         * Check whether owner is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isOwnerNil();


        

        /**
         * Check whether qdate is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isQdateNil();


        

        /**
         * Check whether pool is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isPoolNil();


        
        /**
         * Set pool to Nill (same as using reset)
         * @return true on success, false otherwise.
         */
        bool WSF_CALL
        setPoolNil();
        

        /**
         * Check whether scheduler is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isSchedulerNil();


        
        /**
         * Set scheduler to Nill (same as using reset)
         * @return true on success, false otherwise.
         */
        bool WSF_CALL
        setSchedulerNil();
        

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
         * @param SubmissionID_om_node node to serialize from
         * @param SubmissionID_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* SubmissionID_om_node, axiom_element_t *SubmissionID_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the SubmissionID is a particle class (E.g. group, inner sequence)
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
         * Getter for owner by property number (2)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty2();

    
        

        /**
         * Getter for qdate by property number (3)
         * @return int
         */

        int WSF_CALL
        getProperty3();

    
        

        /**
         * Getter for pool by property number (4)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty4();

    
        

        /**
         * Getter for scheduler by property number (5)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty5();

    

};

}        
 #endif /* SUBMISSIONID_H */
    

