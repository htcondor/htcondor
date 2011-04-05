

        #ifndef AviaryCommon_SUBMISSIONSUMMARY_H
        #define AviaryCommon_SUBMISSIONSUMMARY_H

       /**
        * SubmissionSummary.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Mar 02, 2011 (11:54:00 EST)
        */

       /**
        *  SubmissionSummary class
        */

        namespace AviaryCommon{
            class SubmissionSummary;
        }
        

        
       #include "AviaryCommon_SubmissionID.h"
          
       #include "AviaryCommon_Status.h"
          

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
         * @return newly created SubmissionSummary object
         */
        SubmissionSummary(AviaryCommon::SubmissionID* arg_Id,AviaryCommon::Status* arg_Status,int arg_Completed,int arg_Held,int arg_Idle,int arg_Removed,int arg_Running);
        
        
        /********************************** Class get set methods **************************************/
        
        

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

    

};

}        
 #endif /* SUBMISSIONSUMMARY_H */
    

