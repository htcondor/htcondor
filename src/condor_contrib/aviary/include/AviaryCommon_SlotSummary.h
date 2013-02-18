
          #ifndef AviaryCommon_SLOTSUMMARY_H
          #define AviaryCommon_SLOTSUMMARY_H
        
      
       /**
        * SlotSummary.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Nov 08, 2012 (09:07:42 EST)
        */

       /**
        *  SlotSummary class
        */

        namespace AviaryCommon{
            class SlotSummary;
        }
        

        
                #include "AviaryCommon_ArchType.h"
              
                #include "AviaryCommon_OSType.h"
              

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryCommon
{
        
        

        class SlotSummary {

        private:
             AviaryCommon::ArchType* property_Arch;

                
                bool isValidArch;
            AviaryCommon::OSType* property_Os;

                
                bool isValidOs;
            std::string property_Activity;

                
                bool isValidActivity;
            std::string property_State;

                
                bool isValidState;
            int property_Cpus;

                
                bool isValidCpus;
            int property_Disk;

                
                bool isValidDisk;
            int property_Memory;

                
                bool isValidMemory;
            int property_Swap;

                
                bool isValidSwap;
            int property_Mips;

                
                bool isValidMips;
            double property_Load_avg;

                
                bool isValidLoad_avg;
            std::string property_Start;

                
                bool isValidStart;
            std::string property_Domain;

                
                bool isValidDomain;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setArchNil();
            

        bool WSF_CALL
        setOsNil();
            

        bool WSF_CALL
        setActivityNil();
            

        bool WSF_CALL
        setStateNil();
            

        bool WSF_CALL
        setCpusNil();
            

        bool WSF_CALL
        setDiskNil();
            

        bool WSF_CALL
        setMemoryNil();
            

        bool WSF_CALL
        setSwapNil();
            

        bool WSF_CALL
        setMipsNil();
            

        bool WSF_CALL
        setLoad_avgNil();
            

        bool WSF_CALL
        setStartNil();
            

        bool WSF_CALL
        setDomainNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class SlotSummary
         */

        SlotSummary();

        /**
         * Destructor SlotSummary
         */
        ~SlotSummary();


       

        /**
         * Constructor for creating SlotSummary
         * @param 
         * @param Arch AviaryCommon::ArchType*
         * @param Os AviaryCommon::OSType*
         * @param Activity std::string
         * @param State std::string
         * @param Cpus int
         * @param Disk int
         * @param Memory int
         * @param Swap int
         * @param Mips int
         * @param Load_avg double
         * @param Start std::string
         * @param Domain std::string
         * @return newly created SlotSummary object
         */
        SlotSummary(AviaryCommon::ArchType* arg_Arch,AviaryCommon::OSType* arg_Os,std::string arg_Activity,std::string arg_State,int arg_Cpus,int arg_Disk,int arg_Memory,int arg_Swap,int arg_Mips,double arg_Load_avg,std::string arg_Start,std::string arg_Domain);
        

        /**
         * resetAll for SlotSummary
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
         * Getter for activity. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getActivity();

        /**
         * Setter for activity.
         * @param arg_Activity std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setActivity(const std::string  arg_Activity);

        /**
         * Re setter for activity
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetActivity();
        
        

        /**
         * Getter for state. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getState();

        /**
         * Setter for state.
         * @param arg_State std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setState(const std::string  arg_State);

        /**
         * Re setter for state
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetState();
        
        

        /**
         * Getter for cpus. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getCpus();

        /**
         * Setter for cpus.
         * @param arg_Cpus int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setCpus(const int  arg_Cpus);

        /**
         * Re setter for cpus
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetCpus();
        
        

        /**
         * Getter for disk. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getDisk();

        /**
         * Setter for disk.
         * @param arg_Disk int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setDisk(const int  arg_Disk);

        /**
         * Re setter for disk
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetDisk();
        
        

        /**
         * Getter for memory. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getMemory();

        /**
         * Setter for memory.
         * @param arg_Memory int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setMemory(const int  arg_Memory);

        /**
         * Re setter for memory
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetMemory();
        
        

        /**
         * Getter for swap. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getSwap();

        /**
         * Setter for swap.
         * @param arg_Swap int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setSwap(const int  arg_Swap);

        /**
         * Re setter for swap
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetSwap();
        
        

        /**
         * Getter for mips. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getMips();

        /**
         * Setter for mips.
         * @param arg_Mips int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setMips(const int  arg_Mips);

        /**
         * Re setter for mips
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetMips();
        
        

        /**
         * Getter for load_avg. 
         * @return double*
         */
        WSF_EXTERN double WSF_CALL
        getLoad_avg();

        /**
         * Setter for load_avg.
         * @param arg_Load_avg double*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setLoad_avg(const double  arg_Load_avg);

        /**
         * Re setter for load_avg
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetLoad_avg();
        
        

        /**
         * Getter for start. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getStart();

        /**
         * Setter for start.
         * @param arg_Start std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setStart(const std::string  arg_Start);

        /**
         * Re setter for start
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetStart();
        
        

        /**
         * Getter for domain. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getDomain();

        /**
         * Setter for domain.
         * @param arg_Domain std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setDomain(const std::string  arg_Domain);

        /**
         * Re setter for domain
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetDomain();
        


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
         * Check whether activity is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isActivityNil();


        

        /**
         * Check whether state is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isStateNil();


        

        /**
         * Check whether cpus is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isCpusNil();


        

        /**
         * Check whether disk is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isDiskNil();


        

        /**
         * Check whether memory is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isMemoryNil();


        

        /**
         * Check whether swap is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isSwapNil();


        

        /**
         * Check whether mips is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isMipsNil();


        

        /**
         * Check whether load_avg is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isLoad_avgNil();


        

        /**
         * Check whether start is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isStartNil();


        

        /**
         * Check whether domain is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isDomainNil();


        

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
         * @param SlotSummary_om_node node to serialize from
         * @param SlotSummary_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* SlotSummary_om_node, axiom_element_t *SlotSummary_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the SlotSummary is a particle class (E.g. group, inner sequence)
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
         * Getter for activity by property number (3)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty3();

    
        

        /**
         * Getter for state by property number (4)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty4();

    
        

        /**
         * Getter for cpus by property number (5)
         * @return int
         */

        int WSF_CALL
        getProperty5();

    
        

        /**
         * Getter for disk by property number (6)
         * @return int
         */

        int WSF_CALL
        getProperty6();

    
        

        /**
         * Getter for memory by property number (7)
         * @return int
         */

        int WSF_CALL
        getProperty7();

    
        

        /**
         * Getter for swap by property number (8)
         * @return int
         */

        int WSF_CALL
        getProperty8();

    
        

        /**
         * Getter for mips by property number (9)
         * @return int
         */

        int WSF_CALL
        getProperty9();

    
        

        /**
         * Getter for load_avg by property number (10)
         * @return double
         */

        double WSF_CALL
        getProperty10();

    
        

        /**
         * Getter for start by property number (11)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty11();

    
        

        /**
         * Getter for domain by property number (12)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty12();

    

};

}        
 #endif /* SLOTSUMMARY_H */
    

