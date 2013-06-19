
          #ifndef AviaryHadoop_HADOOPQUERYRESULT_H
          #define AviaryHadoop_HADOOPQUERYRESULT_H
        
      
       /**
        * HadoopQueryResult.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Jan 28, 2013 (03:27:15 EST)
        */

       /**
        *  HadoopQueryResult class
        */

        namespace AviaryHadoop{
            class HadoopQueryResult;
        }
        

        
                #include "AviaryHadoop_HadoopID.h"
              
                #include "AviaryHadoop_HadoopID.h"
              
                #include "AviaryHadoop_HadoopStateType.h"
              
                #include "AviaryCommon_Status.h"
              

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryHadoop
{
        
        

        class HadoopQueryResult {

        private:
             AviaryHadoop::HadoopID* property_Ref;

                
                bool isValidRef;
            AviaryHadoop::HadoopID* property_Parent;

                
                bool isValidParent;
            std::string property_Owner;

                
                bool isValidOwner;
            std::string property_Description;

                
                bool isValidDescription;
            int property_Submitted;

                
                bool isValidSubmitted;
            int property_Uptime;

                
                bool isValidUptime;
            AviaryHadoop::HadoopStateType* property_State;

                
                bool isValidState;
            AviaryCommon::Status* property_Status;

                
                bool isValidStatus;
            std::string property_Bin_file;

                
                bool isValidBin_file;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setRefNil();
            

        bool WSF_CALL
        setParentNil();
            

        bool WSF_CALL
        setOwnerNil();
            

        bool WSF_CALL
        setDescriptionNil();
            

        bool WSF_CALL
        setSubmittedNil();
            

        bool WSF_CALL
        setUptimeNil();
            

        bool WSF_CALL
        setStateNil();
            

        bool WSF_CALL
        setStatusNil();
            

        bool WSF_CALL
        setBin_fileNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class HadoopQueryResult
         */

        HadoopQueryResult();

        /**
         * Destructor HadoopQueryResult
         */
        ~HadoopQueryResult();


       

        /**
         * Constructor for creating HadoopQueryResult
         * @param 
         * @param Ref AviaryHadoop::HadoopID*
         * @param Parent AviaryHadoop::HadoopID*
         * @param Owner std::string
         * @param Description std::string
         * @param Submitted int
         * @param Uptime int
         * @param State AviaryHadoop::HadoopStateType*
         * @param Status AviaryCommon::Status*
         * @param Bin_file std::string
         * @return newly created HadoopQueryResult object
         */
        HadoopQueryResult(AviaryHadoop::HadoopID* arg_Ref,AviaryHadoop::HadoopID* arg_Parent,std::string arg_Owner,std::string arg_Description,int arg_Submitted,int arg_Uptime,AviaryHadoop::HadoopStateType* arg_State,AviaryCommon::Status* arg_Status,std::string arg_Bin_file);
        

        /**
         * resetAll for HadoopQueryResult
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
         * Getter for parent. 
         * @return AviaryHadoop::HadoopID*
         */
        WSF_EXTERN AviaryHadoop::HadoopID* WSF_CALL
        getParent();

        /**
         * Setter for parent.
         * @param arg_Parent AviaryHadoop::HadoopID*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setParent(AviaryHadoop::HadoopID*  arg_Parent);

        /**
         * Re setter for parent
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetParent();
        
        

        /**
         * Getter for owner. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getOwner();

        /**
         * Setter for owner.
         * @param arg_Owner std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setOwner(const std::string  arg_Owner);

        /**
         * Re setter for owner
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetOwner();
        
        

        /**
         * Getter for description. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getDescription();

        /**
         * Setter for description.
         * @param arg_Description std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setDescription(const std::string  arg_Description);

        /**
         * Re setter for description
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetDescription();
        
        

        /**
         * Getter for submitted. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getSubmitted();

        /**
         * Setter for submitted.
         * @param arg_Submitted int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setSubmitted(const int  arg_Submitted);

        /**
         * Re setter for submitted
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetSubmitted();
        
        

        /**
         * Getter for uptime. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getUptime();

        /**
         * Setter for uptime.
         * @param arg_Uptime int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setUptime(const int  arg_Uptime);

        /**
         * Re setter for uptime
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetUptime();
        
        

        /**
         * Getter for state. 
         * @return AviaryHadoop::HadoopStateType*
         */
        WSF_EXTERN AviaryHadoop::HadoopStateType* WSF_CALL
        getState();

        /**
         * Setter for state.
         * @param arg_State AviaryHadoop::HadoopStateType*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setState(AviaryHadoop::HadoopStateType*  arg_State);

        /**
         * Re setter for state
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetState();
        
        

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
         * Getter for bin_file. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getBin_file();

        /**
         * Setter for bin_file.
         * @param arg_Bin_file std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setBin_file(const std::string  arg_Bin_file);

        /**
         * Re setter for bin_file
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetBin_file();
        


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
         * Check whether parent is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isParentNil();


        

        /**
         * Check whether owner is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isOwnerNil();


        

        /**
         * Check whether description is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isDescriptionNil();


        

        /**
         * Check whether submitted is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isSubmittedNil();


        

        /**
         * Check whether uptime is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isUptimeNil();


        

        /**
         * Check whether state is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isStateNil();


        

        /**
         * Check whether status is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isStatusNil();


        

        /**
         * Check whether bin_file is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isBin_fileNil();


        

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
         * @param HadoopQueryResult_om_node node to serialize from
         * @param HadoopQueryResult_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* HadoopQueryResult_om_node, axiom_element_t *HadoopQueryResult_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the HadoopQueryResult is a particle class (E.g. group, inner sequence)
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
         * Getter for parent by property number (2)
         * @return AviaryHadoop::HadoopID
         */

        AviaryHadoop::HadoopID* WSF_CALL
        getProperty2();

    
        

        /**
         * Getter for owner by property number (3)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty3();

    
        

        /**
         * Getter for description by property number (4)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty4();

    
        

        /**
         * Getter for submitted by property number (5)
         * @return int
         */

        int WSF_CALL
        getProperty5();

    
        

        /**
         * Getter for uptime by property number (6)
         * @return int
         */

        int WSF_CALL
        getProperty6();

    
        

        /**
         * Getter for state by property number (7)
         * @return AviaryHadoop::HadoopStateType
         */

        AviaryHadoop::HadoopStateType* WSF_CALL
        getProperty7();

    
        

        /**
         * Getter for status by property number (8)
         * @return AviaryCommon::Status
         */

        AviaryCommon::Status* WSF_CALL
        getProperty8();

    
        

        /**
         * Getter for bin_file by property number (9)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty9();

    

};

}        
 #endif /* HADOOPQUERYRESULT_H */
    

