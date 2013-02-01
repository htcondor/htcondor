
          #ifndef AviaryCommon_NEGOTIATORSUMMARY_H
          #define AviaryCommon_NEGOTIATORSUMMARY_H
        
      
       /**
        * NegotiatorSummary.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Jan 28, 2013 (03:27:15 EST)
        */

       /**
        *  NegotiatorSummary class
        */

        namespace AviaryCommon{
            class NegotiatorSummary;
        }
        

        
        #include <axutil_date_time.h>
          

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryCommon
{
        
        

        class NegotiatorSummary {

        private:
             axutil_date_time_t* property_Latest_cycle;

                
                bool isValidLatest_cycle;
            double property_Match_rate;

                
                bool isValidMatch_rate;
            int property_Matches;

                
                bool isValidMatches;
            int property_Duration;

                
                bool isValidDuration;
            int property_Schedulers;

                
                bool isValidSchedulers;
            int property_Active_submitters;

                
                bool isValidActive_submitters;
            int property_Idle_jobs;

                
                bool isValidIdle_jobs;
            int property_Jobs_considered;

                
                bool isValidJobs_considered;
            int property_Rejections;

                
                bool isValidRejections;
            int property_Total_slots;

                
                bool isValidTotal_slots;
            int property_Candidate_slots;

                
                bool isValidCandidate_slots;
            int property_Trimmed_slots;

                
                bool isValidTrimmed_slots;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setLatest_cycleNil();
            

        bool WSF_CALL
        setMatch_rateNil();
            

        bool WSF_CALL
        setMatchesNil();
            

        bool WSF_CALL
        setDurationNil();
            

        bool WSF_CALL
        setSchedulersNil();
            

        bool WSF_CALL
        setActive_submittersNil();
            

        bool WSF_CALL
        setIdle_jobsNil();
            

        bool WSF_CALL
        setJobs_consideredNil();
            

        bool WSF_CALL
        setRejectionsNil();
            

        bool WSF_CALL
        setTotal_slotsNil();
            

        bool WSF_CALL
        setCandidate_slotsNil();
            

        bool WSF_CALL
        setTrimmed_slotsNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class NegotiatorSummary
         */

        NegotiatorSummary();

        /**
         * Destructor NegotiatorSummary
         */
        ~NegotiatorSummary();


       

        /**
         * Constructor for creating NegotiatorSummary
         * @param 
         * @param Latest_cycle axutil_date_time_t*
         * @param Match_rate double
         * @param Matches int
         * @param Duration int
         * @param Schedulers int
         * @param Active_submitters int
         * @param Idle_jobs int
         * @param Jobs_considered int
         * @param Rejections int
         * @param Total_slots int
         * @param Candidate_slots int
         * @param Trimmed_slots int
         * @return newly created NegotiatorSummary object
         */
        NegotiatorSummary(axutil_date_time_t* arg_Latest_cycle,double arg_Match_rate,int arg_Matches,int arg_Duration,int arg_Schedulers,int arg_Active_submitters,int arg_Idle_jobs,int arg_Jobs_considered,int arg_Rejections,int arg_Total_slots,int arg_Candidate_slots,int arg_Trimmed_slots);
        

        /**
         * resetAll for NegotiatorSummary
         */
        WSF_EXTERN bool WSF_CALL resetAll();
        
        /********************************** Class get set methods **************************************/
        
        

        /**
         * Getter for latest_cycle. 
         * @return axutil_date_time_t*
         */
        WSF_EXTERN axutil_date_time_t* WSF_CALL
        getLatest_cycle();

        /**
         * Setter for latest_cycle.
         * @param arg_Latest_cycle axutil_date_time_t*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setLatest_cycle(axutil_date_time_t*  arg_Latest_cycle);

        /**
         * Re setter for latest_cycle
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetLatest_cycle();
        
        

        /**
         * Getter for match_rate. 
         * @return double*
         */
        WSF_EXTERN double WSF_CALL
        getMatch_rate();

        /**
         * Setter for match_rate.
         * @param arg_Match_rate double*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setMatch_rate(const double  arg_Match_rate);

        /**
         * Re setter for match_rate
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetMatch_rate();
        
        

        /**
         * Getter for matches. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getMatches();

        /**
         * Setter for matches.
         * @param arg_Matches int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setMatches(const int  arg_Matches);

        /**
         * Re setter for matches
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetMatches();
        
        

        /**
         * Getter for duration. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getDuration();

        /**
         * Setter for duration.
         * @param arg_Duration int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setDuration(const int  arg_Duration);

        /**
         * Re setter for duration
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetDuration();
        
        

        /**
         * Getter for schedulers. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getSchedulers();

        /**
         * Setter for schedulers.
         * @param arg_Schedulers int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setSchedulers(const int  arg_Schedulers);

        /**
         * Re setter for schedulers
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetSchedulers();
        
        

        /**
         * Getter for active_submitters. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getActive_submitters();

        /**
         * Setter for active_submitters.
         * @param arg_Active_submitters int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setActive_submitters(const int  arg_Active_submitters);

        /**
         * Re setter for active_submitters
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetActive_submitters();
        
        

        /**
         * Getter for idle_jobs. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getIdle_jobs();

        /**
         * Setter for idle_jobs.
         * @param arg_Idle_jobs int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setIdle_jobs(const int  arg_Idle_jobs);

        /**
         * Re setter for idle_jobs
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetIdle_jobs();
        
        

        /**
         * Getter for jobs_considered. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getJobs_considered();

        /**
         * Setter for jobs_considered.
         * @param arg_Jobs_considered int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setJobs_considered(const int  arg_Jobs_considered);

        /**
         * Re setter for jobs_considered
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetJobs_considered();
        
        

        /**
         * Getter for rejections. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getRejections();

        /**
         * Setter for rejections.
         * @param arg_Rejections int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setRejections(const int  arg_Rejections);

        /**
         * Re setter for rejections
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetRejections();
        
        

        /**
         * Getter for total_slots. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getTotal_slots();

        /**
         * Setter for total_slots.
         * @param arg_Total_slots int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setTotal_slots(const int  arg_Total_slots);

        /**
         * Re setter for total_slots
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetTotal_slots();
        
        

        /**
         * Getter for candidate_slots. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getCandidate_slots();

        /**
         * Setter for candidate_slots.
         * @param arg_Candidate_slots int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setCandidate_slots(const int  arg_Candidate_slots);

        /**
         * Re setter for candidate_slots
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetCandidate_slots();
        
        

        /**
         * Getter for trimmed_slots. 
         * @return int*
         */
        WSF_EXTERN int WSF_CALL
        getTrimmed_slots();

        /**
         * Setter for trimmed_slots.
         * @param arg_Trimmed_slots int*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setTrimmed_slots(const int  arg_Trimmed_slots);

        /**
         * Re setter for trimmed_slots
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetTrimmed_slots();
        


        /******************************* Checking and Setting NIL values *********************************/
        

        /**
         * NOTE: set_nil is only available for nillable properties
         */

        

        /**
         * Check whether latest_cycle is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isLatest_cycleNil();


        

        /**
         * Check whether match_rate is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isMatch_rateNil();


        

        /**
         * Check whether matches is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isMatchesNil();


        

        /**
         * Check whether duration is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isDurationNil();


        

        /**
         * Check whether schedulers is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isSchedulersNil();


        

        /**
         * Check whether active_submitters is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isActive_submittersNil();


        

        /**
         * Check whether idle_jobs is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isIdle_jobsNil();


        

        /**
         * Check whether jobs_considered is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isJobs_consideredNil();


        

        /**
         * Check whether rejections is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isRejectionsNil();


        

        /**
         * Check whether total_slots is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isTotal_slotsNil();


        

        /**
         * Check whether candidate_slots is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isCandidate_slotsNil();


        

        /**
         * Check whether trimmed_slots is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isTrimmed_slotsNil();


        

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
         * @param NegotiatorSummary_om_node node to serialize from
         * @param NegotiatorSummary_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* NegotiatorSummary_om_node, axiom_element_t *NegotiatorSummary_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the NegotiatorSummary is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for latest_cycle by property number (1)
         * @return axutil_date_time_t*
         */

        axutil_date_time_t* WSF_CALL
        getProperty1();

    
        

        /**
         * Getter for match_rate by property number (2)
         * @return double
         */

        double WSF_CALL
        getProperty2();

    
        

        /**
         * Getter for matches by property number (3)
         * @return int
         */

        int WSF_CALL
        getProperty3();

    
        

        /**
         * Getter for duration by property number (4)
         * @return int
         */

        int WSF_CALL
        getProperty4();

    
        

        /**
         * Getter for schedulers by property number (5)
         * @return int
         */

        int WSF_CALL
        getProperty5();

    
        

        /**
         * Getter for active_submitters by property number (6)
         * @return int
         */

        int WSF_CALL
        getProperty6();

    
        

        /**
         * Getter for idle_jobs by property number (7)
         * @return int
         */

        int WSF_CALL
        getProperty7();

    
        

        /**
         * Getter for jobs_considered by property number (8)
         * @return int
         */

        int WSF_CALL
        getProperty8();

    
        

        /**
         * Getter for rejections by property number (9)
         * @return int
         */

        int WSF_CALL
        getProperty9();

    
        

        /**
         * Getter for total_slots by property number (10)
         * @return int
         */

        int WSF_CALL
        getProperty10();

    
        

        /**
         * Getter for candidate_slots by property number (11)
         * @return int
         */

        int WSF_CALL
        getProperty11();

    
        

        /**
         * Getter for trimmed_slots by property number (12)
         * @return int
         */

        int WSF_CALL
        getProperty12();

    

};

}        
 #endif /* NEGOTIATORSUMMARY_H */
    

