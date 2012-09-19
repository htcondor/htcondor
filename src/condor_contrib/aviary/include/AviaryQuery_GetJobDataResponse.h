
          #ifndef AviaryQuery_GETJOBDATARESPONSE_H
          #define AviaryQuery_GETJOBDATARESPONSE_H
        
      
       /**
        * GetJobDataResponse.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Sep 18, 2012 (08:44:08 EDT)
        */

       /**
        *  GetJobDataResponse class
        */

        namespace AviaryQuery{
            class GetJobDataResponse;
        }
        

        
                #include "AviaryCommon_JobData.h"
              
                #include "AviaryCommon_Status.h"
              
        #include <axutil_qname.h>
        

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryQuery
{
        
        

        class GetJobDataResponse {

        private:
             
                axutil_qname_t* qname;
            AviaryCommon::JobData* property_Data;

                
                bool isValidData;
            AviaryCommon::Status* property_Status;

                
                bool isValidStatus;
            std::string property_File_name;

                
                bool isValidFile_name;
            int property_File_size;

                
                bool isValidFile_size;
            std::string property_Content;

                
                bool isValidContent;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setDataNil();
            

        bool WSF_CALL
        setStatusNil();
            

        bool WSF_CALL
        setFile_nameNil();
            

        bool WSF_CALL
        setFile_sizeNil();
            

        bool WSF_CALL
        setContentNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class GetJobDataResponse
         */

        GetJobDataResponse();

        /**
         * Destructor GetJobDataResponse
         */
        ~GetJobDataResponse();


       

        /**
         * Constructor for creating GetJobDataResponse
         * @param 
         * @param Data AviaryCommon::JobData*
         * @param Status AviaryCommon::Status*
         * @param File_name std::string
         * @param File_size int
         * @param Content std::string
         * @return newly created GetJobDataResponse object
         */
        GetJobDataResponse(AviaryCommon::JobData* arg_Data,AviaryCommon::Status* arg_Status,std::string arg_File_name,int arg_File_size,std::string arg_Content);
        

        /**
         * resetAll for GetJobDataResponse
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
         * Getter for file_name. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getFile_name();

        /**
         * Setter for file_name.
         * @param arg_File_name std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setFile_name(const std::string  arg_File_name);

        /**
         * Re setter for file_name
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetFile_name();
        
        

        /**
         * Getter for file_size. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getFile_size();

        /**
         * Setter for file_size.
         * @param arg_File_size int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setFile_size(const int  arg_File_size);

        /**
         * Re setter for file_size
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetFile_size();
        
        

        /**
         * Getter for content. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getContent();

        /**
         * Setter for content.
         * @param arg_Content std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setContent(const std::string  arg_Content);

        /**
         * Re setter for content
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetContent();
        


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
         * Check whether status is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isStatusNil();


        

        /**
         * Check whether file_name is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isFile_nameNil();


        

        /**
         * Check whether file_size is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isFile_sizeNil();


        

        /**
         * Check whether content is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isContentNil();


        

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
         * @param GetJobDataResponse_om_node node to serialize from
         * @param GetJobDataResponse_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* GetJobDataResponse_om_node, axiom_element_t *GetJobDataResponse_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the GetJobDataResponse is a particle class (E.g. group, inner sequence)
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
         * Getter for status by property number (2)
         * @return AviaryCommon::Status
         */

        AviaryCommon::Status* WSF_CALL
        getProperty2();

    
        

        /**
         * Getter for file_name by property number (3)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty3();

    
        

        /**
         * Getter for file_size by property number (4)
         * @return int
         */

        int WSF_CALL
        getProperty4();

    
        

        /**
         * Getter for content by property number (5)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty5();

    

};

}        
 #endif /* GETJOBDATARESPONSE_H */
    

