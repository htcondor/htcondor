
          #ifndef AviaryJob_SUSPENDJOB_H
          #define AviaryJob_SUSPENDJOB_H
        
      
       /**
        * SuspendJob.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Sep 18, 2012 (08:44:08 EDT)
        */

       /**
        *  SuspendJob class
        */

        namespace AviaryJob{
            class SuspendJob;
        }
        

        
                #include "AviaryJob_ControlJob.h"
              
        #include <axutil_qname.h>
        

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryJob
{
        
        

        class SuspendJob {

        private:
             
                axutil_qname_t* qname;
            AviaryJob::ControlJob* property_SuspendJob;

                
                bool isValidSuspendJob;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setSuspendJobNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class SuspendJob
         */

        SuspendJob();

        /**
         * Destructor SuspendJob
         */
        ~SuspendJob();


       

        /**
         * Constructor for creating SuspendJob
         * @param 
         * @param SuspendJob AviaryJob::ControlJob*
         * @return newly created SuspendJob object
         */
        SuspendJob(AviaryJob::ControlJob* arg_SuspendJob);
        

        /**
         * resetAll for SuspendJob
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        
        

        /**
         * Getter for SuspendJob. 
         * @return AviaryJob::ControlJob*
         */
        WSF_EXTERN AviaryJob::ControlJob* WSF_CALL
        getSuspendJob();

        /**
         * Setter for SuspendJob.
         * @param arg_SuspendJob AviaryJob::ControlJob*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setSuspendJob(AviaryJob::ControlJob*  arg_SuspendJob);

        /**
         * Re setter for SuspendJob
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetSuspendJob();
        


        /******************************* Checking and Setting NIL values *********************************/
        

        /**
         * NOTE: set_nil is only available for nillable properties
         */

        

        /**
         * Check whether SuspendJob is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isSuspendJobNil();


        

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
         * @param SuspendJob_om_node node to serialize from
         * @param SuspendJob_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* SuspendJob_om_node, axiom_element_t *SuspendJob_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the SuspendJob is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for SuspendJob by property number (1)
         * @return AviaryJob::ControlJob
         */

        AviaryJob::ControlJob* WSF_CALL
        getProperty1();

    

};

}        
 #endif /* SUSPENDJOB_H */
    

