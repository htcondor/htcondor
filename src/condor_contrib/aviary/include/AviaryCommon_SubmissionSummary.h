
          #ifndef AviaryCommon_SUBMISSIONSUMMARY_H
          #define AviaryCommon_SUBMISSIONSUMMARY_H
        
      
       /**
        * SubmissionSummary.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Sep 18, 2012 (08:44:08 EDT)
        */

       /**
        *  SubmissionSummary class
        */

        namespace AviaryCommon{
            class SubmissionSummary;
        }
        

        
                #include "AviaryCommon_SubmissionID.h"
              
                #include "AviaryCommon_Status.h"
              
                #include "AviaryCommon_JobSummary.h"
              

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryCommon
{
        
        

        class SubmissionSummary {

        private:
             AviaryCommon::SubmissionID* property_Id;

                
                bool isValidId;
            AviaryCommon::Status* property_Status;

                
                bool isValidStatus;
            int property_Completed;

                
                bool isValidCompleted;
            int property_Held;

                
                bool isValidHeld;
            int property_Idle;

                
                bool isValidIdle;
            int property_Removed;

                
                bool isValidRemoved;
            int property_Running;

                
                bool isValidRunning;
            int property_Suspended;

                
                bool isValidSuspended;
            int property_Transferring_output;

                
                bool isValidTransferring_output;
            std::vector<AviaryCommon::JobSummary*>* property_Jobs;

                
                bool isValidJobs;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setIdNil();
            

        bool WSF_CALL
        setStatusNil();
            

        bool WSF_CALL
        setCompletedNil();
            

        bool WSF_CALL
        setHeldNil();
            

        bool WSF_CALL
        setIdleNil();
            

        bool WSF_CALL
        setRemovedNil();
            

        bool WSF_CALL
        setRunningNil();
            

        bool WSF_CALL
        setSuspendedNil();
            

        bool WSF_CALL
        setTransferring_outputNil();
            

        bool WSF_CALL
        setJobsNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class SubmissionSummary
         */

        SubmissionSummary();

        /**
         * Destructor SubmissionSummary
         */
        ~SubmissionSummary();


       

        /**
         * Constructor for creating SubmissionSummary
         * @param 
         * @param Id AviaryCommon::SubmissionID*
         * @param Status AviaryCommon::Status*
         * @param Completed int
         * @param Held int
         * @param Idle int
         * @param Removed int
         * @param Running int
         * @param Suspended int
         * @param Transferring_output int
         * @param Jobs std::vector<AviaryCommon::JobSummary*>*
         * @return newly created SubmissionSummary object
         */
        SubmissionSummary(AviaryCommon::SubmissionID* arg_Id,AviaryCommon::Status* arg_Status,int arg_Completed,int arg_Held,int arg_Idle,int arg_Removed,int arg_Running,int arg_Suspended,int arg_Transferring_output,std::vector<AviaryCommon::JobSummary*>* arg_Jobs);
        

        /**
         * resetAll for SubmissionSummary
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        /******** Deprecated for array types, Use 'Getters and Setters for Arrays' instead ***********/
        

        /**
         * Getter for id. 
         * @return AviaryCommon::SubmissionID*
         */
        WSF_EXTERN AviaryCommon::SubmissionID* WSF_CALL
        getId();

        /**
         * Setter for id.
         * @param arg_Id AviaryCommon::SubmissionID*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setId(AviaryCommon::SubmissionID*  arg_Id);

        /**
         * Re setter for id
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetId();
        
        

        /**
         * Getter for status. 
         * @return AviaryCommon::Status*
         */
        WSF_EXTERN AviaryCommon::Status* WSF_CALL
        getStatus();

        /**
         * Setter for status.
         * @param arg_Status AviaryCommon::Status*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setStatus(AviaryCommon::Status*  arg_Status);

        /**
         * Re setter for status
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetStatus();
        
        

        /**
         * Getter for completed. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getCompleted();

        /**
         * Setter for completed.
         * @param arg_Completed int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setCompleted(const int  arg_Completed);

        /**
         * Re setter for completed
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetCompleted();
        
        

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
         * Getter for suspended. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getSuspended();

        /**
         * Setter for suspended.
         * @param arg_Suspended int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setSuspended(const int  arg_Suspended);

        /**
         * Re setter for suspended
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetSuspended();
        
        

        /**
         * Getter for transferring_output. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getTransferring_output();

        /**
         * Setter for transferring_output.
         * @param arg_Transferring_output int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setTransferring_output(const int  arg_Transferring_output);

        /**
         * Re setter for transferring_output
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetTransferring_output();
        
        

        /**
         * Getter for jobs. Deprecated for array types, Use getJobsAt instead
         * @return Array of AviaryCommon::JobSummary*s.
         */
        WSF_EXTERN std::vector<AviaryCommon::JobSummary*>* WSF_CALL
        getJobs();

        /**
         * Setter for jobs.Deprecated for array types, Use setJobsAt
         * or addJobs instead.
         * @param arg_Jobs Array of AviaryCommon::JobSummary*s.
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setJobs(std::vector<AviaryCommon::JobSummary*>*  arg_Jobs);

        /**
         * Re setter for jobs
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetJobs();
        
        /****************************** Get Set methods for Arrays **********************************/
        /************ Array Specific Operations: get_at, set_at, add, remove_at, sizeof *****************/

        /**
         * E.g. use of get_at, set_at, add and sizeof
         *
         * for(i = 0; i < adb_element->sizeofProperty(); i ++ )
         * {
         *     // Getting ith value to property_object variable
         *     property_object = adb_element->getPropertyAt(i);
         *
         *     // Setting ith value from property_object variable
         *     adb_element->setPropertyAt(i, property_object);
         *
         *     // Appending the value to the end of the array from property_object variable
         *     adb_element->addProperty(property_object);
         *
         *     // Removing the ith value from an array
         *     adb_element->removePropertyAt(i);
         *     
         * }
         *
         */

        
        
        /**
         * Get the ith element of jobs.
        * @param i index of the item to be obtained
         * @return ith AviaryCommon::JobSummary* of the array
         */
        WSF_EXTERN AviaryCommon::JobSummary* WSF_CALL
        getJobsAt(int i);

        /**
         * Set the ith element of jobs. (If the ith already exist, it will be replaced)
         * @param i index of the item to return
         * @param arg_Jobs element to set AviaryCommon::JobSummary* to the array
         * @return ith AviaryCommon::JobSummary* of the array
         */
        WSF_EXTERN bool WSF_CALL
        setJobsAt(int i,
                AviaryCommon::JobSummary* arg_Jobs);


        /**
         * Add to jobs.
         * @param arg_Jobs element to add AviaryCommon::JobSummary* to the array
         * @return true on success, false otherwise.
         */
        WSF_EXTERN bool WSF_CALL
        addJobs(
            AviaryCommon::JobSummary* arg_Jobs);

        /**
         * Get the size of the jobs array.
         * @return the size of the jobs array.
         */
        WSF_EXTERN int WSF_CALL
        sizeofJobs();

        /**
         * Remove the ith element of jobs.
         * @param i index of the item to remove
         * @return true on success, false otherwise.
         */
        WSF_EXTERN bool WSF_CALL
        removeJobsAt(int i);

        


        /******************************* Checking and Setting NIL values *********************************/
        /* Use 'Checking and Setting NIL values for Arrays' to check and set nil for individual elements */

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
         * Check whether status is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isStatusNil();


        

        /**
         * Check whether completed is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isCompletedNil();


        

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


        

        /**
         * Check whether running is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isRunningNil();


        

        /**
         * Check whether suspended is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isSuspendedNil();


        

        /**
         * Check whether transferring_output is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isTransferring_outputNil();


        

        /**
         * Check whether jobs is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isJobsNil();


        

        /*************************** Checking and Setting 'NIL' values in Arrays *****************************/

        /**
         * NOTE: You may set this to remove specific elements in the array
         *       But you can not remove elements, if the specific property is declared to be non-nillable or sizeof(array) < minOccurs
         */
        
        /**
         * Check whether jobs is Nill at position i
         * @param i index of the item to return.
         * @return true if the value is Nil at position i, false otherwise
         */
        bool WSF_CALL
        isJobsNilAt(int i);
 
       
        /**
         * Set jobs to NILL at the  position i.
         * @param i . The index of the item to be set Nill.
         * @return true on success, false otherwise.
         */
        bool WSF_CALL
        setJobsNilAt(int i);

        

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
         * @param SubmissionSummary_om_node node to serialize from
         * @param SubmissionSummary_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* SubmissionSummary_om_node, axiom_element_t *SubmissionSummary_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the SubmissionSummary is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for id by property number (1)
         * @return AviaryCommon::SubmissionID
         */

        AviaryCommon::SubmissionID* WSF_CALL
        getProperty1();

    
        

        /**
         * Getter for status by property number (2)
         * @return AviaryCommon::Status
         */

        AviaryCommon::Status* WSF_CALL
        getProperty2();

    
        

        /**
         * Getter for completed by property number (3)
         * @return int
         */

        int WSF_CALL
        getProperty3();

    
        

        /**
         * Getter for held by property number (4)
         * @return int
         */

        int WSF_CALL
        getProperty4();

    
        

        /**
         * Getter for idle by property number (5)
         * @return int
         */

        int WSF_CALL
        getProperty5();

    
        

        /**
         * Getter for removed by property number (6)
         * @return int
         */

        int WSF_CALL
        getProperty6();

    
        

        /**
         * Getter for running by property number (7)
         * @return int
         */

        int WSF_CALL
        getProperty7();

    
        

        /**
         * Getter for suspended by property number (8)
         * @return int
         */

        int WSF_CALL
        getProperty8();

    
        

        /**
         * Getter for transferring_output by property number (9)
         * @return int
         */

        int WSF_CALL
        getProperty9();

    
        

        /**
         * Getter for jobs by property number (10)
         * @return Array of AviaryCommon::JobSummarys.
         */

        std::vector<AviaryCommon::JobSummary*>* WSF_CALL
        getProperty10();

    

};

}        
 #endif /* SUBMISSIONSUMMARY_H */
    

