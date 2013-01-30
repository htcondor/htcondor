
          #ifndef AviaryHadoop_STOPNAMENODERESPONSE_H
          #define AviaryHadoop_STOPNAMENODERESPONSE_H
        
      
       /**
        * StopNameNodeResponse.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Jan 28, 2013 (02:30:05 CST)
        */

       /**
        *  StopNameNodeResponse class
        */

        namespace AviaryHadoop{
            class StopNameNodeResponse;
        }
        

        
                #include "AviaryHadoop_HadoopStopResponse.h"
              
        #include <axutil_qname.h>
        

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryHadoop
{
        
        

        class StopNameNodeResponse {

        private:
             
                axutil_qname_t* qname;
            AviaryHadoop::HadoopStopResponse* property_StopNameNodeResponse;

                
                bool isValidStopNameNodeResponse;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setStopNameNodeResponseNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class StopNameNodeResponse
         */

        StopNameNodeResponse();

        /**
         * Destructor StopNameNodeResponse
         */
        ~StopNameNodeResponse();


       

        /**
         * Constructor for creating StopNameNodeResponse
         * @param 
         * @param StopNameNodeResponse AviaryHadoop::HadoopStopResponse*
         * @return newly created StopNameNodeResponse object
         */
        StopNameNodeResponse(AviaryHadoop::HadoopStopResponse* arg_StopNameNodeResponse);
        

        /**
         * resetAll for StopNameNodeResponse
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        
        

        /**
         * Getter for StopNameNodeResponse. 
         * @return AviaryHadoop::HadoopStopResponse*
         */
        WSF_EXTERN AviaryHadoop::HadoopStopResponse* WSF_CALL
        getStopNameNodeResponse();

        /**
         * Setter for StopNameNodeResponse.
         * @param arg_StopNameNodeResponse AviaryHadoop::HadoopStopResponse*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setStopNameNodeResponse(AviaryHadoop::HadoopStopResponse*  arg_StopNameNodeResponse);

        /**
         * Re setter for StopNameNodeResponse
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetStopNameNodeResponse();
        


        /******************************* Checking and Setting NIL values *********************************/
        

        /**
         * NOTE: set_nil is only available for nillable properties
         */

        

        /**
         * Check whether StopNameNodeResponse is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isStopNameNodeResponseNil();


        

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
         * @param StopNameNodeResponse_om_node node to serialize from
         * @param StopNameNodeResponse_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* StopNameNodeResponse_om_node, axiom_element_t *StopNameNodeResponse_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the StopNameNodeResponse is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for StopNameNodeResponse by property number (1)
         * @return AviaryHadoop::HadoopStopResponse
         */

        AviaryHadoop::HadoopStopResponse* WSF_CALL
        getProperty1();

    

};

}        
 #endif /* STOPNAMENODERESPONSE_H */
    

