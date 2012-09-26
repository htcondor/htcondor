
          #ifndef AviaryJob_CONTINUEJOBRESPONSE_H
          #define AviaryJob_CONTINUEJOBRESPONSE_H
        
      
       /**
        * ContinueJobResponse.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Sep 18, 2012 (08:44:08 EDT)
        */

       /**
        *  ContinueJobResponse class
        */

        namespace AviaryJob{
            class ContinueJobResponse;
        }
        

        
                #include "AviaryJob_ControlJobResponse.h"
              
        #include <axutil_qname.h>
        

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryJob
{
        
        

        class ContinueJobResponse {

        private:
             
                axutil_qname_t* qname;
            AviaryJob::ControlJobResponse* property_ContinueJobResponse;

                
                bool isValidContinueJobResponse;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setContinueJobResponseNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class ContinueJobResponse
         */

        ContinueJobResponse();

        /**
         * Destructor ContinueJobResponse
         */
        ~ContinueJobResponse();


       

        /**
         * Constructor for creating ContinueJobResponse
         * @param 
         * @param ContinueJobResponse AviaryJob::ControlJobResponse*
         * @return newly created ContinueJobResponse object
         */
        ContinueJobResponse(AviaryJob::ControlJobResponse* arg_ContinueJobResponse);
        

        /**
         * resetAll for ContinueJobResponse
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        
        

        /**
         * Getter for ContinueJobResponse. 
         * @return AviaryJob::ControlJobResponse*
         */
        WSF_EXTERN AviaryJob::ControlJobResponse* WSF_CALL
        getContinueJobResponse();

        /**
         * Setter for ContinueJobResponse.
         * @param arg_ContinueJobResponse AviaryJob::ControlJobResponse*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setContinueJobResponse(AviaryJob::ControlJobResponse*  arg_ContinueJobResponse);

        /**
         * Re setter for ContinueJobResponse
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetContinueJobResponse();
        


        /******************************* Checking and Setting NIL values *********************************/
        

        /**
         * NOTE: set_nil is only available for nillable properties
         */

        

        /**
         * Check whether ContinueJobResponse is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isContinueJobResponseNil();


        

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
         * @param ContinueJobResponse_om_node node to serialize from
         * @param ContinueJobResponse_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* ContinueJobResponse_om_node, axiom_element_t *ContinueJobResponse_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the ContinueJobResponse is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for ContinueJobResponse by property number (1)
         * @return AviaryJob::ControlJobResponse
         */

        AviaryJob::ControlJobResponse* WSF_CALL
        getProperty1();

    

};

}        
 #endif /* CONTINUEJOBRESPONSE_H */
    

