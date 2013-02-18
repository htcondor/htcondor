
          #ifndef AviaryHadoop_GETJOBTRACKER_H
          #define AviaryHadoop_GETJOBTRACKER_H
        
      
       /**
        * GetJobTracker.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Jan 28, 2013 (02:30:05 CST)
        */

       /**
        *  GetJobTracker class
        */

        namespace AviaryHadoop{
            class GetJobTracker;
        }
        

        
                #include "AviaryHadoop_HadoopQuery.h"
              
        #include <axutil_qname.h>
        

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryHadoop
{
        
        

        class GetJobTracker {

        private:
             
                axutil_qname_t* qname;
            AviaryHadoop::HadoopQuery* property_GetJobTracker;

                
                bool isValidGetJobTracker;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setGetJobTrackerNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class GetJobTracker
         */

        GetJobTracker();

        /**
         * Destructor GetJobTracker
         */
        ~GetJobTracker();


       

        /**
         * Constructor for creating GetJobTracker
         * @param 
         * @param GetJobTracker AviaryHadoop::HadoopQuery*
         * @return newly created GetJobTracker object
         */
        GetJobTracker(AviaryHadoop::HadoopQuery* arg_GetJobTracker);
        

        /**
         * resetAll for GetJobTracker
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        
        

        /**
         * Getter for GetJobTracker. 
         * @return AviaryHadoop::HadoopQuery*
         */
        WSF_EXTERN AviaryHadoop::HadoopQuery* WSF_CALL
        getGetJobTracker();

        /**
         * Setter for GetJobTracker.
         * @param arg_GetJobTracker AviaryHadoop::HadoopQuery*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setGetJobTracker(AviaryHadoop::HadoopQuery*  arg_GetJobTracker);

        /**
         * Re setter for GetJobTracker
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetGetJobTracker();
        


        /******************************* Checking and Setting NIL values *********************************/
        

        /**
         * NOTE: set_nil is only available for nillable properties
         */

        

        /**
         * Check whether GetJobTracker is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isGetJobTrackerNil();


        

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
         * @param GetJobTracker_om_node node to serialize from
         * @param GetJobTracker_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* GetJobTracker_om_node, axiom_element_t *GetJobTracker_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the GetJobTracker is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for GetJobTracker by property number (1)
         * @return AviaryHadoop::HadoopQuery
         */

        AviaryHadoop::HadoopQuery* WSF_CALL
        getProperty1();

    

};

}        
 #endif /* GETJOBTRACKER_H */
    

