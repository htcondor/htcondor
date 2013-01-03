
          #ifndef AviaryCommon_SLOT_H
          #define AviaryCommon_SLOT_H
        
      
       /**
        * Slot.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Nov 08, 2012 (09:07:42 EST)
        */

       /**
        *  Slot class
        */

        namespace AviaryCommon{
            class Slot;
        }
        

        
                #include "AviaryCommon_ResourceID.h"
              
                #include "AviaryCommon_Status.h"
              
                #include "AviaryCommon_SlotType.h"
              
                #include "AviaryCommon_SlotSummary.h"
              
                #include "AviaryCommon_Slot.h"
              

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryCommon
{
        
        

        class Slot {

        private:
             AviaryCommon::ResourceID* property_Id;

                
                bool isValidId;
            AviaryCommon::Status* property_Status;

                
                bool isValidStatus;
            AviaryCommon::SlotType* property_Slot_type;

                
                bool isValidSlot_type;
            AviaryCommon::SlotSummary* property_Summary;

                
                bool isValidSummary;
            std::vector<AviaryCommon::Slot*>* property_Dynamic_slots;

                
                bool isValidDynamic_slots;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setIdNil();
            

        bool WSF_CALL
        setStatusNil();
            

        bool WSF_CALL
        setSlot_typeNil();
            

        bool WSF_CALL
        setSummaryNil();
            

        bool WSF_CALL
        setDynamic_slotsNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class Slot
         */

        Slot();

        /**
         * Destructor Slot
         */
        ~Slot();


       

        /**
         * Constructor for creating Slot
         * @param 
         * @param Id AviaryCommon::ResourceID*
         * @param Status AviaryCommon::Status*
         * @param Slot_type AviaryCommon::SlotType*
         * @param Summary AviaryCommon::SlotSummary*
         * @param Dynamic_slots std::vector<AviaryCommon::Slot*>*
         * @return newly created Slot object
         */
        Slot(AviaryCommon::ResourceID* arg_Id,AviaryCommon::Status* arg_Status,AviaryCommon::SlotType* arg_Slot_type,AviaryCommon::SlotSummary* arg_Summary,std::vector<AviaryCommon::Slot*>* arg_Dynamic_slots);
        

        /**
         * resetAll for Slot
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        /******** Deprecated for array types, Use 'Getters and Setters for Arrays' instead ***********/
        

        /**
         * Getter for id. 
         * @return AviaryCommon::ResourceID*
         */
        WSF_EXTERN AviaryCommon::ResourceID* WSF_CALL
        getId();

        /**
         * Setter for id.
         * @param arg_Id AviaryCommon::ResourceID*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setId(AviaryCommon::ResourceID*  arg_Id);

        /**
         * Re setter for id
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetId();
        
        

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
         * Getter for slot_type. 
         * @return AviaryCommon::SlotType*
         */
        WSF_EXTERN AviaryCommon::SlotType* WSF_CALL
        getSlot_type();

        /**
         * Setter for slot_type.
         * @param arg_Slot_type AviaryCommon::SlotType*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setSlot_type(AviaryCommon::SlotType*  arg_Slot_type);

        /**
         * Re setter for slot_type
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetSlot_type();
        
        

        /**
         * Getter for summary. 
         * @return AviaryCommon::SlotSummary*
         */
        WSF_EXTERN AviaryCommon::SlotSummary* WSF_CALL
        getSummary();

        /**
         * Setter for summary.
         * @param arg_Summary AviaryCommon::SlotSummary*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setSummary(AviaryCommon::SlotSummary*  arg_Summary);

        /**
         * Re setter for summary
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetSummary();
        
        

        /**
         * Getter for dynamic_slots. Deprecated for array types, Use getDynamic_slotsAt instead
         * @return Array of AviaryCommon::Slot*s.
         */
        WSF_EXTERN std::vector<AviaryCommon::Slot*>* WSF_CALL
        getDynamic_slots();

        /**
         * Setter for dynamic_slots.Deprecated for array types, Use setDynamic_slotsAt
         * or addDynamic_slots instead.
         * @param arg_Dynamic_slots Array of AviaryCommon::Slot*s.
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setDynamic_slots(std::vector<AviaryCommon::Slot*>*  arg_Dynamic_slots);

        /**
         * Re setter for dynamic_slots
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetDynamic_slots();
        
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
         * Get the ith element of dynamic_slots.
        * @param i index of the item to be obtained
         * @return ith AviaryCommon::Slot* of the array
         */
        WSF_EXTERN AviaryCommon::Slot* WSF_CALL
        getDynamic_slotsAt(int i);

        /**
         * Set the ith element of dynamic_slots. (If the ith already exist, it will be replaced)
         * @param i index of the item to return
         * @param arg_Dynamic_slots element to set AviaryCommon::Slot* to the array
         * @return ith AviaryCommon::Slot* of the array
         */
        WSF_EXTERN bool WSF_CALL
        setDynamic_slotsAt(int i,
                AviaryCommon::Slot* arg_Dynamic_slots);


        /**
         * Add to dynamic_slots.
         * @param arg_Dynamic_slots element to add AviaryCommon::Slot* to the array
         * @return true on success, false otherwise.
         */
        WSF_EXTERN bool WSF_CALL
        addDynamic_slots(
            AviaryCommon::Slot* arg_Dynamic_slots);

        /**
         * Get the size of the dynamic_slots array.
         * @return the size of the dynamic_slots array.
         */
        WSF_EXTERN int WSF_CALL
        sizeofDynamic_slots();

        /**
         * Remove the ith element of dynamic_slots.
         * @param i index of the item to remove
         * @return true on success, false otherwise.
         */
        WSF_EXTERN bool WSF_CALL
        removeDynamic_slotsAt(int i);

        


        /******************************* Checking and Setting NIL values *********************************/
        /* Use 'Checking and Setting NIL values for Arrays' to check and set nil for individual elements */

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
         * Check whether status is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isStatusNil();


        

        /**
         * Check whether slot_type is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isSlot_typeNil();


        

        /**
         * Check whether summary is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isSummaryNil();


        

        /**
         * Check whether dynamic_slots is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isDynamic_slotsNil();


        

        /*************************** Checking and Setting 'NIL' values in Arrays *****************************/

        /**
         * NOTE: You may set this to remove specific elements in the array
         *       But you can not remove elements, if the specific property is declared to be non-nillable or sizeof(array) < minOccurs
         */
        
        /**
         * Check whether dynamic_slots is Nill at position i
         * @param i index of the item to return.
         * @return true if the value is Nil at position i, false otherwise
         */
        bool WSF_CALL
        isDynamic_slotsNilAt(int i);
 
       
        /**
         * Set dynamic_slots to NILL at the  position i.
         * @param i . The index of the item to be set Nill.
         * @return true on success, false otherwise.
         */
        bool WSF_CALL
        setDynamic_slotsNilAt(int i);

        

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
         * @param Slot_om_node node to serialize from
         * @param Slot_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* Slot_om_node, axiom_element_t *Slot_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the Slot is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for id by property number (1)
         * @return AviaryCommon::ResourceID
         */

        AviaryCommon::ResourceID* WSF_CALL
        getProperty1();

    
        

        /**
         * Getter for status by property number (2)
         * @return AviaryCommon::Status
         */

        AviaryCommon::Status* WSF_CALL
        getProperty2();

    
        

        /**
         * Getter for slot_type by property number (3)
         * @return AviaryCommon::SlotType
         */

        AviaryCommon::SlotType* WSF_CALL
        getProperty3();

    
        

        /**
         * Getter for summary by property number (4)
         * @return AviaryCommon::SlotSummary
         */

        AviaryCommon::SlotSummary* WSF_CALL
        getProperty4();

    
        

        /**
         * Getter for dynamic_slots by property number (5)
         * @return Array of AviaryCommon::Slots.
         */

        std::vector<AviaryCommon::Slot*>* WSF_CALL
        getProperty5();

    

};

}        
 #endif /* SLOT_H */
    

