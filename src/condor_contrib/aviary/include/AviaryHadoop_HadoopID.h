
          #ifndef AviaryHadoop_HADOOPID_H
          #define AviaryHadoop_HADOOPID_H
        
      
       /**
        * HadoopID.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Jan 28, 2013 (03:27:15 EST)
        */

       /**
        *  HadoopID class
        */

        namespace AviaryHadoop{
            class HadoopID;
        }
        

        

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryHadoop
{
        
        

        class HadoopID {

        private:
             std::string property_Id;

                
                bool isValidId;
            std::string property_Ipc;

                
                bool isValidIpc;
            std::string property_Http;

                
                bool isValidHttp;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setIdNil();
            

        bool WSF_CALL
        setIpcNil();
            

        bool WSF_CALL
        setHttpNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class HadoopID
         */

        HadoopID();

        /**
         * Destructor HadoopID
         */
        ~HadoopID();


       

        /**
         * Constructor for creating HadoopID
         * @param 
         * @param Id std::string
         * @param Ipc std::string
         * @param Http std::string
         * @return newly created HadoopID object
         */
        HadoopID(std::string arg_Id,std::string arg_Ipc,std::string arg_Http);
        

        /**
         * resetAll for HadoopID
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        
        

        /**
         * Getter for id. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getId();

        /**
         * Setter for id.
         * @param arg_Id std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setId(const std::string  arg_Id);

        /**
         * Re setter for id
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetId();
        
        

        /**
         * Getter for ipc. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getIpc();

        /**
         * Setter for ipc.
         * @param arg_Ipc std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setIpc(const std::string  arg_Ipc);

        /**
         * Re setter for ipc
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetIpc();
        
        

        /**
         * Getter for http. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getHttp();

        /**
         * Setter for http.
         * @param arg_Http std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setHttp(const std::string  arg_Http);

        /**
         * Re setter for http
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetHttp();
        


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
         * Check whether ipc is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isIpcNil();


        

        /**
         * Check whether http is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isHttpNil();


        

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
         * @param HadoopID_om_node node to serialize from
         * @param HadoopID_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* HadoopID_om_node, axiom_element_t *HadoopID_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the HadoopID is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for id by property number (1)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty1();

    
        

        /**
         * Getter for ipc by property number (2)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty2();

    
        

        /**
         * Getter for http by property number (3)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty3();

    

};

}        
 #endif /* HADOOPID_H */
    

