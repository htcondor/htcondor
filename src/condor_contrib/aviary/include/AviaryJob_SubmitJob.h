
          #ifndef AviaryJob_SUBMITJOB_H
          #define AviaryJob_SUBMITJOB_H
        
      
       /**
        * SubmitJob.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Sep 18, 2012 (08:44:08 EDT)
        */

       /**
        *  SubmitJob class
        */

        namespace AviaryJob{
            class SubmitJob;
        }
        

        
                #include "AviaryCommon_ResourceConstraint.h"
              
                #include "AviaryCommon_Attribute.h"
              
        #include <axutil_qname.h>
        

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryJob
{
        
        

        class SubmitJob {

        private:
             
                axutil_qname_t* qname;
            std::string property_Cmd;

                
                bool isValidCmd;
            std::string property_Args;

                
                bool isValidArgs;
            std::string property_Owner;

                
                bool isValidOwner;
            std::string property_Iwd;

                
                bool isValidIwd;
            std::string property_Submission_name;

                
                bool isValidSubmission_name;
            std::vector<AviaryCommon::ResourceConstraint*>* property_Requirements;

                
                bool isValidRequirements;
            std::vector<AviaryCommon::Attribute*>* property_Extra;

                
                bool isValidExtra;
            bool property_AllowOverrides;

                
                bool isValidAllowOverrides;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setCmdNil();
            

        bool WSF_CALL
        setArgsNil();
            

        bool WSF_CALL
        setOwnerNil();
            

        bool WSF_CALL
        setIwdNil();
            

        bool WSF_CALL
        setSubmission_nameNil();
            

        bool WSF_CALL
        setRequirementsNil();
            

        bool WSF_CALL
        setExtraNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class SubmitJob
         */

        SubmitJob();

        /**
         * Destructor SubmitJob
         */
        ~SubmitJob();


       

        /**
         * Constructor for creating SubmitJob
         * @param 
         * @param Cmd std::string
         * @param Args std::string
         * @param Owner std::string
         * @param Iwd std::string
         * @param Submission_name std::string
         * @param Requirements std::vector<AviaryCommon::ResourceConstraint*>*
         * @param Extra std::vector<AviaryCommon::Attribute*>*
         * @param AllowOverrides bool
         * @return newly created SubmitJob object
         */
        SubmitJob(std::string arg_Cmd,std::string arg_Args,std::string arg_Owner,std::string arg_Iwd,std::string arg_Submission_name,std::vector<AviaryCommon::ResourceConstraint*>* arg_Requirements,std::vector<AviaryCommon::Attribute*>* arg_Extra,bool arg_AllowOverrides);
        

        /**
         * resetAll for SubmitJob
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        /******** Deprecated for array types, Use 'Getters and Setters for Arrays' instead ***********/
        

        /**
         * Getter for cmd. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getCmd();

        /**
         * Setter for cmd.
         * @param arg_Cmd std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setCmd(const std::string  arg_Cmd);

        /**
         * Re setter for cmd
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetCmd();
        
        

        /**
         * Getter for args. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getArgs();

        /**
         * Setter for args.
         * @param arg_Args std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setArgs(const std::string  arg_Args);

        /**
         * Re setter for args
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetArgs();
        
        

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
         * Getter for iwd. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getIwd();

        /**
         * Setter for iwd.
         * @param arg_Iwd std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setIwd(const std::string  arg_Iwd);

        /**
         * Re setter for iwd
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetIwd();
        
        

        /**
         * Getter for submission_name. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getSubmission_name();

        /**
         * Setter for submission_name.
         * @param arg_Submission_name std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setSubmission_name(const std::string  arg_Submission_name);

        /**
         * Re setter for submission_name
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetSubmission_name();
        
        

        /**
         * Getter for requirements. Deprecated for array types, Use getRequirementsAt instead
         * @return Array of AviaryCommon::ResourceConstraint*s.
         */
        WSF_EXTERN std::vector<AviaryCommon::ResourceConstraint*>* WSF_CALL
        getRequirements();

        /**
         * Setter for requirements.Deprecated for array types, Use setRequirementsAt
         * or addRequirements instead.
         * @param arg_Requirements Array of AviaryCommon::ResourceConstraint*s.
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setRequirements(std::vector<AviaryCommon::ResourceConstraint*>*  arg_Requirements);

        /**
         * Re setter for requirements
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetRequirements();
        
        

        /**
         * Getter for extra. Deprecated for array types, Use getExtraAt instead
         * @return Array of AviaryCommon::Attribute*s.
         */
        WSF_EXTERN std::vector<AviaryCommon::Attribute*>* WSF_CALL
        getExtra();

        /**
         * Setter for extra.Deprecated for array types, Use setExtraAt
         * or addExtra instead.
         * @param arg_Extra Array of AviaryCommon::Attribute*s.
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setExtra(std::vector<AviaryCommon::Attribute*>*  arg_Extra);

        /**
         * Re setter for extra
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetExtra();
        
        

        /**
         * Getter for allowOverrides. 
         * @return bool
         */
        WSF_EXTERN bool WSF_CALL
        getAllowOverrides();

        /**
         * Setter for allowOverrides.
         * @param arg_AllowOverrides bool
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setAllowOverrides(bool  arg_AllowOverrides);

        /**
         * Re setter for allowOverrides
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetAllowOverrides();
        
        /****************************** Get Set methods for Arrays **********************************/
        /************ Array Specific Operations: get_at, set_at, add, remove_at, sizeof *****************/

        /**
         * E.g. use of get_at, set_at, add and sizeof
         *
         * for(i = 0; i < adb_element->sizeofProperty(); i ++ )
         * {
         *     // Getting ith value to property_object variable
         *     property_object = adb_element->getPropertyAt(i);
         *
         *     // Setting ith value from property_object variable
         *     adb_element->setPropertyAt(i, property_object);
         *
         *     // Appending the value to the end of the array from property_object variable
         *     adb_element->addProperty(property_object);
         *
         *     // Removing the ith value from an array
         *     adb_element->removePropertyAt(i);
         *     
         * }
         *
         */

        
        
        /**
         * Get the ith element of requirements.
        * @param i index of the item to be obtained
         * @return ith AviaryCommon::ResourceConstraint* of the array
         */
        WSF_EXTERN AviaryCommon::ResourceConstraint* WSF_CALL
        getRequirementsAt(int i);

        /**
         * Set the ith element of requirements. (If the ith already exist, it will be replaced)
         * @param i index of the item to return
         * @param arg_Requirements element to set AviaryCommon::ResourceConstraint* to the array
         * @return ith AviaryCommon::ResourceConstraint* of the array
         */
        WSF_EXTERN bool WSF_CALL
        setRequirementsAt(int i,
                AviaryCommon::ResourceConstraint* arg_Requirements);


        /**
         * Add to requirements.
         * @param arg_Requirements element to add AviaryCommon::ResourceConstraint* to the array
         * @return true on success, false otherwise.
         */
        WSF_EXTERN bool WSF_CALL
        addRequirements(
            AviaryCommon::ResourceConstraint* arg_Requirements);

        /**
         * Get the size of the requirements array.
         * @return the size of the requirements array.
         */
        WSF_EXTERN int WSF_CALL
        sizeofRequirements();

        /**
         * Remove the ith element of requirements.
         * @param i index of the item to remove
         * @return true on success, false otherwise.
         */
        WSF_EXTERN bool WSF_CALL
        removeRequirementsAt(int i);

        
        
        /**
         * Get the ith element of extra.
        * @param i index of the item to be obtained
         * @return ith AviaryCommon::Attribute* of the array
         */
        WSF_EXTERN AviaryCommon::Attribute* WSF_CALL
        getExtraAt(int i);

        /**
         * Set the ith element of extra. (If the ith already exist, it will be replaced)
         * @param i index of the item to return
         * @param arg_Extra element to set AviaryCommon::Attribute* to the array
         * @return ith AviaryCommon::Attribute* of the array
         */
        WSF_EXTERN bool WSF_CALL
        setExtraAt(int i,
                AviaryCommon::Attribute* arg_Extra);


        /**
         * Add to extra.
         * @param arg_Extra element to add AviaryCommon::Attribute* to the array
         * @return true on success, false otherwise.
         */
        WSF_EXTERN bool WSF_CALL
        addExtra(
            AviaryCommon::Attribute* arg_Extra);

        /**
         * Get the size of the extra array.
         * @return the size of the extra array.
         */
        WSF_EXTERN int WSF_CALL
        sizeofExtra();

        /**
         * Remove the ith element of extra.
         * @param i index of the item to remove
         * @return true on success, false otherwise.
         */
        WSF_EXTERN bool WSF_CALL
        removeExtraAt(int i);

        


        /******************************* Checking and Setting NIL values *********************************/
        /* Use 'Checking and Setting NIL values for Arrays' to check and set nil for individual elements */

        /**
         * NOTE: set_nil is only available for nillable properties
         */

        

        /**
         * Check whether cmd is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isCmdNil();


        

        /**
         * Check whether args is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isArgsNil();


        

        /**
         * Check whether owner is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isOwnerNil();


        

        /**
         * Check whether iwd is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isIwdNil();


        

        /**
         * Check whether submission_name is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isSubmission_nameNil();


        

        /**
         * Check whether requirements is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isRequirementsNil();


        

        /**
         * Check whether extra is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isExtraNil();


        

        /**
         * Check whether allowOverrides is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isAllowOverridesNil();


        
        /**
         * Set allowOverrides to Nill (same as using reset)
         * @return true on success, false otherwise.
         */
        bool WSF_CALL
        setAllowOverridesNil();
        

        /*************************** Checking and Setting 'NIL' values in Arrays *****************************/

        /**
         * NOTE: You may set this to remove specific elements in the array
         *       But you can not remove elements, if the specific property is declared to be non-nillable or sizeof(array) < minOccurs
         */
        
        /**
         * Check whether requirements is Nill at position i
         * @param i index of the item to return.
         * @return true if the value is Nil at position i, false otherwise
         */
        bool WSF_CALL
        isRequirementsNilAt(int i);
 
       
        /**
         * Set requirements to NILL at the  position i.
         * @param i . The index of the item to be set Nill.
         * @return true on success, false otherwise.
         */
        bool WSF_CALL
        setRequirementsNilAt(int i);

        
        /**
         * Check whether extra is Nill at position i
         * @param i index of the item to return.
         * @return true if the value is Nil at position i, false otherwise
         */
        bool WSF_CALL
        isExtraNilAt(int i);
 
       
        /**
         * Set extra to NILL at the  position i.
         * @param i . The index of the item to be set Nill.
         * @return true on success, false otherwise.
         */
        bool WSF_CALL
        setExtraNilAt(int i);

        

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
         * @param SubmitJob_om_node node to serialize from
         * @param SubmitJob_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* SubmitJob_om_node, axiom_element_t *SubmitJob_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the SubmitJob is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for cmd by property number (1)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty1();

    
        

        /**
         * Getter for args by property number (2)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty2();

    
        

        /**
         * Getter for owner by property number (3)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty3();

    
        

        /**
         * Getter for iwd by property number (4)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty4();

    
        

        /**
         * Getter for submission_name by property number (5)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty5();

    
        

        /**
         * Getter for requirements by property number (6)
         * @return Array of AviaryCommon::ResourceConstraints.
         */

        std::vector<AviaryCommon::ResourceConstraint*>* WSF_CALL
        getProperty6();

    
        

        /**
         * Getter for extra by property number (7)
         * @return Array of AviaryCommon::Attributes.
         */

        std::vector<AviaryCommon::Attribute*>* WSF_CALL
        getProperty7();

    
        

        /**
         * Getter for allowOverrides by property number (8)
         * @return bool
         */

        bool WSF_CALL
        getProperty8();

    

};

}        
 #endif /* SUBMITJOB_H */
    

