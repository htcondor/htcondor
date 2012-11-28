
          #ifndef AviaryHadoop_HADOOPSTART_H
          #define AviaryHadoop_HADOOPSTART_H
        
      
       /**
        * HadoopStart.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Nov 08, 2012 (09:07:42 EST)
        */

       /**
        *  HadoopStart class
        */

        namespace AviaryHadoop{
            class HadoopStart;
        }
        

        
                #include "AviaryHadoop_HadoopID.h"
              

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryHadoop
{
        
        

        class HadoopStart {

        private:
             AviaryHadoop::HadoopID* property_Ref;

                
                bool isValidRef;
            int property_Count;

                
                bool isValidCount;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setRefNil();
            

        bool WSF_CALL
        setCountNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class HadoopStart
         */

        HadoopStart();

        /**
         * Destructor HadoopStart
         */
        ~HadoopStart();


       

        /**
         * Constructor for creating HadoopStart
         * @param 
         * @param Ref AviaryHadoop::HadoopID*
         * @param Count int
         * @return newly created HadoopStart object
         */
        HadoopStart(AviaryHadoop::HadoopID* arg_Ref,int arg_Count);
        

        /**
         * resetAll for HadoopStart
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        
        

        /**
         * Getter for ref. 
         * @return AviaryHadoop::HadoopID*
         */
        WSF_EXTERN AviaryHadoop::HadoopID* WSF_CALL
        getRef();

        /**
         * Setter for ref.
         * @param arg_Ref AviaryHadoop::HadoopID*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setRef(AviaryHadoop::HadoopID*  arg_Ref);

        /**
         * Re setter for ref
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetRef();
        
        

        /**
         * Getter for count. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getCount();

        /**
         * Setter for count.
         * @param arg_Count int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setCount(const int  arg_Count);

        /**
         * Re setter for count
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetCount();
        


        /******************************* Checking and Setting NIL values *********************************/
        

        /**
         * NOTE: set_nil is only available for nillable properties
         */

        

        /**
         * Check whether ref is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isRefNil();


        

        /**
         * Check whether count is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isCountNil();


        

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
         * @param HadoopStart_om_node node to serialize from
         * @param HadoopStart_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* HadoopStart_om_node, axiom_element_t *HadoopStart_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the HadoopStart is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for ref by property number (1)
         * @return AviaryHadoop::HadoopID
         */

        AviaryHadoop::HadoopID* WSF_CALL
        getProperty1();

    
        

        /**
         * Getter for count by property number (2)
         * @return int
         */

        int WSF_CALL
        getProperty2();

    

};

}        
 #endif /* HADOOPSTART_H */
    

