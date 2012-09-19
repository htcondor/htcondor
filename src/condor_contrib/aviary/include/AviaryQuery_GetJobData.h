
          #ifndef AviaryQuery_GETJOBDATA_H
          #define AviaryQuery_GETJOBDATA_H
        
      
       /**
        * GetJobData.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Sep 18, 2012 (08:44:08 EDT)
        */

       /**
        *  GetJobData class
        */

        namespace AviaryQuery{
            class GetJobData;
        }
        

        
                #include "AviaryCommon_JobData.h"
              
        #include <axutil_qname.h>
        

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryQuery
{
        
        

        class GetJobData {

        private:
             
                axutil_qname_t* qname;
            AviaryCommon::JobData* property_Data;

                
                bool isValidData;
            int property_Max_bytes;

                
                bool isValidMax_bytes;
            bool property_From_end;

                
                bool isValidFrom_end;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setDataNil();
            

        bool WSF_CALL
        setMax_bytesNil();
            

        bool WSF_CALL
        setFrom_endNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class GetJobData
         */

        GetJobData();

        /**
         * Destructor GetJobData
         */
        ~GetJobData();


       

        /**
         * Constructor for creating GetJobData
         * @param 
         * @param Data AviaryCommon::JobData*
         * @param Max_bytes int
         * @param From_end bool
         * @return newly created GetJobData object
         */
        GetJobData(AviaryCommon::JobData* arg_Data,int arg_Max_bytes,bool arg_From_end);
        

        /**
         * resetAll for GetJobData
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        
        

        /**
         * Getter for data. 
         * @return AviaryCommon::JobData*
         */
        WSF_EXTERN AviaryCommon::JobData* WSF_CALL
        getData();

        /**
         * Setter for data.
         * @param arg_Data AviaryCommon::JobData*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setData(AviaryCommon::JobData*  arg_Data);

        /**
         * Re setter for data
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetData();
        
        

        /**
         * Getter for max_bytes. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getMax_bytes();

        /**
         * Setter for max_bytes.
         * @param arg_Max_bytes int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setMax_bytes(const int  arg_Max_bytes);

        /**
         * Re setter for max_bytes
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetMax_bytes();
        
        

        /**
         * Getter for from_end. 
         * @return bool
         */
        WSF_EXTERN bool WSF_CALL
        getFrom_end();

        /**
         * Setter for from_end.
         * @param arg_From_end bool
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setFrom_end(bool  arg_From_end);

        /**
         * Re setter for from_end
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetFrom_end();
        


        /******************************* Checking and Setting NIL values *********************************/
        

        /**
         * NOTE: set_nil is only available for nillable properties
         */

        

        /**
         * Check whether data is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isDataNil();


        

        /**
         * Check whether max_bytes is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isMax_bytesNil();


        

        /**
         * Check whether from_end is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isFrom_endNil();


        

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
         * @param GetJobData_om_node node to serialize from
         * @param GetJobData_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* GetJobData_om_node, axiom_element_t *GetJobData_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the GetJobData is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for data by property number (1)
         * @return AviaryCommon::JobData
         */

        AviaryCommon::JobData* WSF_CALL
        getProperty1();

    
        

        /**
         * Getter for max_bytes by property number (2)
         * @return int
         */

        int WSF_CALL
        getProperty2();

    
        

        /**
         * Getter for from_end by property number (3)
         * @return bool
         */

        bool WSF_CALL
        getProperty3();

    

};

}        
 #endif /* GETJOBDATA_H */
    

