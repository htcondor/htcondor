
          #ifndef AviaryCommon_JOBID_H
          #define AviaryCommon_JOBID_H
        
      
       /**
        * JobID.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Sep 18, 2012 (08:44:08 EDT)
        */

       /**
        *  JobID class
        */

        namespace AviaryCommon{
            class JobID;
        }
        

        
                #include "AviaryCommon_SubmissionID.h"
              

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryCommon
{
        
        

        class JobID {

        private:
             std::string property_Job;

                
                bool isValidJob;
            std::string property_Pool;

                
                bool isValidPool;
            std::string property_Scheduler;

                
                bool isValidScheduler;
            AviaryCommon::SubmissionID* property_Submission;

                
                bool isValidSubmission;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setJobNil();
            

        bool WSF_CALL
        setPoolNil();
            

        bool WSF_CALL
        setSchedulerNil();
            

        bool WSF_CALL
        setSubmissionNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class JobID
         */

        JobID();

        /**
         * Destructor JobID
         */
        ~JobID();


       

        /**
         * Constructor for creating JobID
         * @param 
         * @param Job std::string
         * @param Pool std::string
         * @param Scheduler std::string
         * @param Submission AviaryCommon::SubmissionID*
         * @return newly created JobID object
         */
        JobID(std::string arg_Job,std::string arg_Pool,std::string arg_Scheduler,AviaryCommon::SubmissionID* arg_Submission);
        

        /**
         * resetAll for JobID
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        
        

        /**
         * Getter for job. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getJob();

        /**
         * Setter for job.
         * @param arg_Job std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setJob(const std::string  arg_Job);

        /**
         * Re setter for job
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetJob();
        
        

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
        
        

        /**
         * Getter for submission. 
         * @return AviaryCommon::SubmissionID*
         */
        WSF_EXTERN AviaryCommon::SubmissionID* WSF_CALL
        getSubmission();

        /**
         * Setter for submission.
         * @param arg_Submission AviaryCommon::SubmissionID*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setSubmission(AviaryCommon::SubmissionID*  arg_Submission);

        /**
         * Re setter for submission
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetSubmission();
        


        /******************************* Checking and Setting NIL values *********************************/
        

        /**
         * NOTE: set_nil is only available for nillable properties
         */

        

        /**
         * Check whether job is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isJobNil();


        

        /**
         * Check whether pool is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isPoolNil();


        

        /**
         * Check whether scheduler is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isSchedulerNil();


        

        /**
         * Check whether submission is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isSubmissionNil();


        

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
         * @param JobID_om_node node to serialize from
         * @param JobID_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* JobID_om_node, axiom_element_t *JobID_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the JobID is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for job by property number (1)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty1();

    
        

        /**
         * Getter for pool by property number (2)
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

    
        

        /**
         * Getter for submission by property number (4)
         * @return AviaryCommon::SubmissionID
         */

        AviaryCommon::SubmissionID* WSF_CALL
        getProperty4();

    

};

}        
 #endif /* JOBID_H */
    

