
          #ifndef AviaryHadoop_HADOOPNAMENODESTART_H
          #define AviaryHadoop_HADOOPNAMENODESTART_H
        
      
       /**
        * HadoopNameNodeStart.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Jan 28, 2013 (03:27:15 EST)
        */

       /**
        *  HadoopNameNodeStart class
        */

        namespace AviaryHadoop{
            class HadoopNameNodeStart;
        }
        

        
                #include "AviaryHadoop_HadoopID.h"
              

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryHadoop
{
        
        

        class HadoopNameNodeStart {

        private:
             std::string property_Bin_file;

                
                bool isValidBin_file;
            std::string property_Owner;

                
                bool isValidOwner;
            std::string property_Description;

                
                bool isValidDescription;
            AviaryHadoop::HadoopID* property_Unmanaged;

                
                bool isValidUnmanaged;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setBin_fileNil();
            

        bool WSF_CALL
        setOwnerNil();
            

        bool WSF_CALL
        setDescriptionNil();
            

        bool WSF_CALL
        setUnmanagedNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class HadoopNameNodeStart
         */

        HadoopNameNodeStart();

        /**
         * Destructor HadoopNameNodeStart
         */
        ~HadoopNameNodeStart();


       

        /**
         * Constructor for creating HadoopNameNodeStart
         * @param 
         * @param Bin_file std::string
         * @param Owner std::string
         * @param Description std::string
         * @param Unmanaged AviaryHadoop::HadoopID*
         * @return newly created HadoopNameNodeStart object
         */
        HadoopNameNodeStart(std::string arg_Bin_file,std::string arg_Owner,std::string arg_Description,AviaryHadoop::HadoopID* arg_Unmanaged);
        

        /**
         * resetAll for HadoopNameNodeStart
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        
        

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
         * Getter for unmanaged. 
         * @return AviaryHadoop::HadoopID*
         */
        WSF_EXTERN AviaryHadoop::HadoopID* WSF_CALL
        getUnmanaged();

        /**
         * Setter for unmanaged.
         * @param arg_Unmanaged AviaryHadoop::HadoopID*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setUnmanaged(AviaryHadoop::HadoopID*  arg_Unmanaged);

        /**
         * Re setter for unmanaged
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetUnmanaged();
        


        /******************************* Checking and Setting NIL values *********************************/
        

        /**
         * NOTE: set_nil is only available for nillable properties
         */

        

        /**
         * Check whether bin_file is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isBin_fileNil();


        

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
         * Check whether unmanaged is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isUnmanagedNil();


        

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
         * @param HadoopNameNodeStart_om_node node to serialize from
         * @param HadoopNameNodeStart_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* HadoopNameNodeStart_om_node, axiom_element_t *HadoopNameNodeStart_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the HadoopNameNodeStart is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for bin_file by property number (1)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty1();

    
        

        /**
         * Getter for owner by property number (2)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty2();

    
        

        /**
         * Getter for description by property number (3)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty3();

    
        

        /**
         * Getter for unmanaged by property number (4)
         * @return AviaryHadoop::HadoopID
         */

        AviaryHadoop::HadoopID* WSF_CALL
        getProperty4();

    

};

}        
 #endif /* HADOOPNAMENODESTART_H */
    

