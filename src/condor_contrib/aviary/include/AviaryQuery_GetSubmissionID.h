
          #ifndef AviaryQuery_GETSUBMISSIONID_H
          #define AviaryQuery_GETSUBMISSIONID_H
        
      
       /**
        * GetSubmissionID.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Sep 18, 2012 (08:44:08 EDT)
        */

       /**
        *  GetSubmissionID class
        */

        namespace AviaryQuery{
            class GetSubmissionID;
        }
        

        
                #include "AviaryCommon_ScanMode.h"
              
                #include "AviaryCommon_SubmissionID.h"
              
        #include <axutil_qname.h>
        

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryQuery
{
        
        

        class GetSubmissionID {

        private:
             
                axutil_qname_t* qname;
            int property_Size;

                
                bool isValidSize;
            AviaryCommon::ScanMode* property_Mode;

                
                bool isValidMode;
            AviaryCommon::SubmissionID* property_Offset;

                
                bool isValidOffset;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setSizeNil();
            

        bool WSF_CALL
        setModeNil();
            

        bool WSF_CALL
        setOffsetNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class GetSubmissionID
         */

        GetSubmissionID();

        /**
         * Destructor GetSubmissionID
         */
        ~GetSubmissionID();


       

        /**
         * Constructor for creating GetSubmissionID
         * @param 
         * @param Size int
         * @param Mode AviaryCommon::ScanMode*
         * @param Offset AviaryCommon::SubmissionID*
         * @return newly created GetSubmissionID object
         */
        GetSubmissionID(int arg_Size,AviaryCommon::ScanMode* arg_Mode,AviaryCommon::SubmissionID* arg_Offset);
        

        /**
         * resetAll for GetSubmissionID
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        
        

        /**
         * Getter for size. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getSize();

        /**
         * Setter for size.
         * @param arg_Size int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setSize(const int  arg_Size);

        /**
         * Re setter for size
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetSize();
        
        

        /**
         * Getter for mode. 
         * @return AviaryCommon::ScanMode*
         */
        WSF_EXTERN AviaryCommon::ScanMode* WSF_CALL
        getMode();

        /**
         * Setter for mode.
         * @param arg_Mode AviaryCommon::ScanMode*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setMode(AviaryCommon::ScanMode*  arg_Mode);

        /**
         * Re setter for mode
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetMode();
        
        

        /**
         * Getter for offset. 
         * @return AviaryCommon::SubmissionID*
         */
        WSF_EXTERN AviaryCommon::SubmissionID* WSF_CALL
        getOffset();

        /**
         * Setter for offset.
         * @param arg_Offset AviaryCommon::SubmissionID*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setOffset(AviaryCommon::SubmissionID*  arg_Offset);

        /**
         * Re setter for offset
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetOffset();
        


        /******************************* Checking and Setting NIL values *********************************/
        

        /**
         * NOTE: set_nil is only available for nillable properties
         */

        

        /**
         * Check whether size is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isSizeNil();


        

        /**
         * Check whether mode is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isModeNil();


        

        /**
         * Check whether offset is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isOffsetNil();


        

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
         * @param GetSubmissionID_om_node node to serialize from
         * @param GetSubmissionID_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* GetSubmissionID_om_node, axiom_element_t *GetSubmissionID_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the GetSubmissionID is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for size by property number (1)
         * @return int
         */

        int WSF_CALL
        getProperty1();

    
        

        /**
         * Getter for mode by property number (2)
         * @return AviaryCommon::ScanMode
         */

        AviaryCommon::ScanMode* WSF_CALL
        getProperty2();

    
        

        /**
         * Getter for offset by property number (3)
         * @return AviaryCommon::SubmissionID
         */

        AviaryCommon::SubmissionID* WSF_CALL
        getProperty3();

    

};

}        
 #endif /* GETSUBMISSIONID_H */
    

