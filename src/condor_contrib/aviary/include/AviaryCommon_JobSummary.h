
          #ifndef AviaryCommon_JOBSUMMARY_H
          #define AviaryCommon_JOBSUMMARY_H
        
      
       /**
        * JobSummary.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Sep 18, 2012 (08:44:08 EDT)
        */

       /**
        *  JobSummary class
        */

        namespace AviaryCommon{
            class JobSummary;
        }
        

        
                #include "AviaryCommon_JobID.h"
              
                #include "AviaryCommon_Status.h"
              
                #include "AviaryCommon_JobStatusType.h"
              
        #include <axutil_date_time.h>
          

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryCommon
{
        
        

        class JobSummary {

        private:
             AviaryCommon::JobID* property_Id;

                
                bool isValidId;
            AviaryCommon::Status* property_Status;

                
                bool isValidStatus;
            axutil_date_time_t* property_Queued;

                
                bool isValidQueued;
            axutil_date_time_t* property_Last_update;

                
                bool isValidLast_update;
            AviaryCommon::JobStatusType* property_Job_status;

                
                bool isValidJob_status;
            std::string property_Cmd;

                
                bool isValidCmd;
            std::string property_Args1;

                
                bool isValidArgs1;
            std::string property_Args2;

                
                bool isValidArgs2;
            std::string property_Held;

                
                bool isValidHeld;
            std::string property_Released;

                
                bool isValidReleased;
            std::string property_Removed;

                
                bool isValidRemoved;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setIdNil();
            

        bool WSF_CALL
        setStatusNil();
            

        bool WSF_CALL
        setQueuedNil();
            

        bool WSF_CALL
        setLast_updateNil();
            

        bool WSF_CALL
        setJob_statusNil();
            

        bool WSF_CALL
        setCmdNil();
            

        bool WSF_CALL
        setArgs1Nil();
            

        bool WSF_CALL
        setArgs2Nil();
            

        bool WSF_CALL
        setHeldNil();
            

        bool WSF_CALL
        setReleasedNil();
            

        bool WSF_CALL
        setRemovedNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class JobSummary
         */

        JobSummary();

        /**
         * Destructor JobSummary
         */
        ~JobSummary();


       

        /**
         * Constructor for creating JobSummary
         * @param 
         * @param Id AviaryCommon::JobID*
         * @param Status AviaryCommon::Status*
         * @param Queued axutil_date_time_t*
         * @param Last_update axutil_date_time_t*
         * @param Job_status AviaryCommon::JobStatusType*
         * @param Cmd std::string
         * @param Args1 std::string
         * @param Args2 std::string
         * @param Held std::string
         * @param Released std::string
         * @param Removed std::string
         * @return newly created JobSummary object
         */
        JobSummary(AviaryCommon::JobID* arg_Id,AviaryCommon::Status* arg_Status,axutil_date_time_t* arg_Queued,axutil_date_time_t* arg_Last_update,AviaryCommon::JobStatusType* arg_Job_status,std::string arg_Cmd,std::string arg_Args1,std::string arg_Args2,std::string arg_Held,std::string arg_Released,std::string arg_Removed);
        

        /**
         * resetAll for JobSummary
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
         * Getter for queued. 
         * @return axutil_date_time_t*
         */
        WSF_EXTERN axutil_date_time_t* WSF_CALL
        getQueued();

        /**
         * Setter for queued.
         * @param arg_Queued axutil_date_time_t*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setQueued(axutil_date_time_t*  arg_Queued);

        /**
         * Re setter for queued
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetQueued();
        
        

        /**
         * Getter for last_update. 
         * @return axutil_date_time_t*
         */
        WSF_EXTERN axutil_date_time_t* WSF_CALL
        getLast_update();

        /**
         * Setter for last_update.
         * @param arg_Last_update axutil_date_time_t*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setLast_update(axutil_date_time_t*  arg_Last_update);

        /**
         * Re setter for last_update
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetLast_update();
        
        

        /**
         * Getter for job_status. 
         * @return AviaryCommon::JobStatusType*
         */
        WSF_EXTERN AviaryCommon::JobStatusType* WSF_CALL
        getJob_status();

        /**
         * Setter for job_status.
         * @param arg_Job_status AviaryCommon::JobStatusType*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setJob_status(AviaryCommon::JobStatusType*  arg_Job_status);

        /**
         * Re setter for job_status
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetJob_status();
        
        

        /**
         * Getter for cmd. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getCmd();

        /**
         * Setter for cmd.
         * @param arg_Cmd std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setCmd(const std::string  arg_Cmd);

        /**
         * Re setter for cmd
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetCmd();
        
        

        /**
         * Getter for args1. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getArgs1();

        /**
         * Setter for args1.
         * @param arg_Args1 std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setArgs1(const std::string  arg_Args1);

        /**
         * Re setter for args1
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetArgs1();
        
        

        /**
         * Getter for args2. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getArgs2();

        /**
         * Setter for args2.
         * @param arg_Args2 std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setArgs2(const std::string  arg_Args2);

        /**
         * Re setter for args2
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetArgs2();
        
        

        /**
         * Getter for held. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getHeld();

        /**
         * Setter for held.
         * @param arg_Held std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setHeld(const std::string  arg_Held);

        /**
         * Re setter for held
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetHeld();
        
        

        /**
         * Getter for released. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getReleased();

        /**
         * Setter for released.
         * @param arg_Released std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setReleased(const std::string  arg_Released);

        /**
         * Re setter for released
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetReleased();
        
        

        /**
         * Getter for removed. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getRemoved();

        /**
         * Setter for removed.
         * @param arg_Removed std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setRemoved(const std::string  arg_Removed);

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
         * Check whether queued is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isQueuedNil();


        

        /**
         * Check whether last_update is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isLast_updateNil();


        

        /**
         * Check whether job_status is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isJob_statusNil();


        

        /**
         * Check whether cmd is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isCmdNil();


        

        /**
         * Check whether args1 is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isArgs1Nil();


        

        /**
         * Check whether args2 is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isArgs2Nil();


        

        /**
         * Check whether held is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isHeldNil();


        

        /**
         * Check whether released is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isReleasedNil();


        

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
         * @param JobSummary_om_node node to serialize from
         * @param JobSummary_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* JobSummary_om_node, axiom_element_t *JobSummary_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the JobSummary is a particle class (E.g. group, inner sequence)
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
         * Getter for status by property number (2)
         * @return AviaryCommon::Status
         */

        AviaryCommon::Status* WSF_CALL
        getProperty2();

    
        

        /**
         * Getter for queued by property number (3)
         * @return axutil_date_time_t*
         */

        axutil_date_time_t* WSF_CALL
        getProperty3();

    
        

        /**
         * Getter for last_update by property number (4)
         * @return axutil_date_time_t*
         */

        axutil_date_time_t* WSF_CALL
        getProperty4();

    
        

        /**
         * Getter for job_status by property number (5)
         * @return AviaryCommon::JobStatusType
         */

        AviaryCommon::JobStatusType* WSF_CALL
        getProperty5();

    
        

        /**
         * Getter for cmd by property number (6)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty6();

    
        

        /**
         * Getter for args1 by property number (7)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty7();

    
        

        /**
         * Getter for args2 by property number (8)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty8();

    
        

        /**
         * Getter for held by property number (9)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty9();

    
        

        /**
         * Getter for released by property number (10)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty10();

    
        

        /**
         * Getter for removed by property number (11)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty11();

    

};

}        
 #endif /* JOBSUMMARY_H */
    

