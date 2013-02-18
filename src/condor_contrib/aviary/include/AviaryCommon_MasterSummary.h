
          #ifndef AviaryCommon_MASTERSUMMARY_H
          #define AviaryCommon_MASTERSUMMARY_H
        
      
       /**
        * MasterSummary.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Nov 08, 2012 (09:07:42 EST)
        */

       /**
        *  MasterSummary class
        */

        namespace AviaryCommon{
            class MasterSummary;
        }
        

        
                #include "AviaryCommon_ArchType.h"
              
                #include "AviaryCommon_OSType.h"
              

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryCommon
{
        
        

        class MasterSummary {

        private:
             AviaryCommon::ArchType* property_Arch;

                
                bool isValidArch;
            AviaryCommon::OSType* property_Os;

                
                bool isValidOs;
            int property_Real_uid;

                
                bool isValidReal_uid;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setArchNil();
            

        bool WSF_CALL
        setOsNil();
            

        bool WSF_CALL
        setReal_uidNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class MasterSummary
         */

        MasterSummary();

        /**
         * Destructor MasterSummary
         */
        ~MasterSummary();


       

        /**
         * Constructor for creating MasterSummary
         * @param 
         * @param Arch AviaryCommon::ArchType*
         * @param Os AviaryCommon::OSType*
         * @param Real_uid int
         * @return newly created MasterSummary object
         */
        MasterSummary(AviaryCommon::ArchType* arg_Arch,AviaryCommon::OSType* arg_Os,int arg_Real_uid);
        

        /**
         * resetAll for MasterSummary
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        
        

        /**
         * Getter for arch. 
         * @return AviaryCommon::ArchType*
         */
        WSF_EXTERN AviaryCommon::ArchType* WSF_CALL
        getArch();

        /**
         * Setter for arch.
         * @param arg_Arch AviaryCommon::ArchType*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setArch(AviaryCommon::ArchType*  arg_Arch);

        /**
         * Re setter for arch
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetArch();
        
        

        /**
         * Getter for os. 
         * @return AviaryCommon::OSType*
         */
        WSF_EXTERN AviaryCommon::OSType* WSF_CALL
        getOs();

        /**
         * Setter for os.
         * @param arg_Os AviaryCommon::OSType*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setOs(AviaryCommon::OSType*  arg_Os);

        /**
         * Re setter for os
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetOs();
        
        

        /**
         * Getter for real_uid. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getReal_uid();

        /**
         * Setter for real_uid.
         * @param arg_Real_uid int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setReal_uid(const int  arg_Real_uid);

        /**
         * Re setter for real_uid
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetReal_uid();
        


        /******************************* Checking and Setting NIL values *********************************/
        

        /**
         * NOTE: set_nil is only available for nillable properties
         */

        

        /**
         * Check whether arch is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isArchNil();


        

        /**
         * Check whether os is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isOsNil();


        

        /**
         * Check whether real_uid is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isReal_uidNil();


        

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
         * @param MasterSummary_om_node node to serialize from
         * @param MasterSummary_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* MasterSummary_om_node, axiom_element_t *MasterSummary_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the MasterSummary is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for arch by property number (1)
         * @return AviaryCommon::ArchType
         */

        AviaryCommon::ArchType* WSF_CALL
        getProperty1();

    
        

        /**
         * Getter for os by property number (2)
         * @return AviaryCommon::OSType
         */

        AviaryCommon::OSType* WSF_CALL
        getProperty2();

    
        

        /**
         * Getter for real_uid by property number (3)
         * @return int
         */

        int WSF_CALL
        getProperty3();

    

};

}        
 #endif /* MASTERSUMMARY_H */
    

