/*
 * Copyright 2009-2012 Red Hat, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef AVIARYCOLLECTORSERVICESKELETON_H
#define AVIARYCOLLECTORSERVICESKELETON_H

    #include <OMElement.h>
    #include <MessageContext.h>
   
     #include <AviaryCollector_GetSlotID.h>
    
     #include <AviaryCollector_GetSlotIDResponse.h>
    
     #include <AviaryCollector_GetNegotiator.h>
    
     #include <AviaryCollector_GetNegotiatorResponse.h>
    
     #include <AviaryCollector_GetSubmitter.h>
    
     #include <AviaryCollector_GetSubmitterResponse.h>
    
     #include <AviaryCollector_GetSlot.h>
    
     #include <AviaryCollector_GetSlotResponse.h>
    
     #include <AviaryCollector_GetAttributes.h>
    
     #include <AviaryCollector_GetAttributesResponse.h>
    
     #include <AviaryCollector_GetScheduler.h>
    
     #include <AviaryCollector_GetSchedulerResponse.h>
    
     #include <AviaryCollector_GetCollector.h>
    
     #include <AviaryCollector_GetCollectorResponse.h>
    
     #include <AviaryCollector_GetMasterID.h>
    
     #include <AviaryCollector_GetMasterIDResponse.h>
    
     #include <AviaryCollector_GetMaster.h>
    
     #include <AviaryCollector_GetMasterResponse.h>
    
namespace AviaryCollector{
    

   /** we have to reserve some error codes for adb and for custom messages */
    #define AVIARYCOLLECTORSERVICESKELETON_ERROR_CODES_START (AXIS2_ERROR_LAST + 2500)

    typedef enum
    {
        AVIARYCOLLECTORSERVICESKELETON_ERROR_NONE = AVIARYCOLLECTORSERVICESKELETON_ERROR_CODES_START,

        AVIARYCOLLECTORSERVICESKELETON_ERROR_LAST
    } AviaryCollectorServiceSkeleton_error_codes;

    


class AviaryCollectorServiceSkeleton
{
        public:
            AviaryCollectorServiceSkeleton(){}


     




		 


        /**
         * Auto generated method declaration
         * for "getSlotID|http://grid.redhat.com/aviary-collector/" operation.
         * 
         * @param _getSlotID of the AviaryCollector::GetSlotID
         *
         * @return AviaryCollector::GetSlotIDResponse*
         */
        

         virtual 
        AviaryCollector::GetSlotIDResponse* getSlotID(wso2wsf::MessageContext *outCtx ,AviaryCollector::GetSlotID* _getSlotID);


     




		 


        /**
         * Auto generated method declaration
         * for "getNegotiator|http://grid.redhat.com/aviary-collector/" operation.
         * 
         * @param _getNegotiator of the AviaryCollector::GetNegotiator
         *
         * @return AviaryCollector::GetNegotiatorResponse*
         */
        

         virtual 
        AviaryCollector::GetNegotiatorResponse* getNegotiator(wso2wsf::MessageContext *outCtx ,AviaryCollector::GetNegotiator* _getNegotiator);


     




		 


        /**
         * Auto generated method declaration
         * for "getSubmitter|http://grid.redhat.com/aviary-collector/" operation.
         * 
         * @param _getSubmitter of the AviaryCollector::GetSubmitter
         *
         * @return AviaryCollector::GetSubmitterResponse*
         */
        

         virtual 
        AviaryCollector::GetSubmitterResponse* getSubmitter(wso2wsf::MessageContext *outCtx ,AviaryCollector::GetSubmitter* _getSubmitter);


     




		 


        /**
         * Auto generated method declaration
         * for "getSlot|http://grid.redhat.com/aviary-collector/" operation.
         * 
         * @param _getSlot of the AviaryCollector::GetSlot
         *
         * @return AviaryCollector::GetSlotResponse*
         */
        

         virtual 
        AviaryCollector::GetSlotResponse* getSlot(wso2wsf::MessageContext *outCtx ,AviaryCollector::GetSlot* _getSlot);


     




		 


        /**
         * Auto generated method declaration
         * for "getAttributes|http://grid.redhat.com/aviary-collector/" operation.
         * 
         * @param _getAttributes of the AviaryCollector::GetAttributes
         *
         * @return AviaryCollector::GetAttributesResponse*
         */
        

         virtual 
        AviaryCollector::GetAttributesResponse* getAttributes(wso2wsf::MessageContext *outCtx ,AviaryCollector::GetAttributes* _getAttributes);


     




		 


        /**
         * Auto generated method declaration
         * for "getScheduler|http://grid.redhat.com/aviary-collector/" operation.
         * 
         * @param _getScheduler of the AviaryCollector::GetScheduler
         *
         * @return AviaryCollector::GetSchedulerResponse*
         */
        

         virtual 
        AviaryCollector::GetSchedulerResponse* getScheduler(wso2wsf::MessageContext *outCtx ,AviaryCollector::GetScheduler* _getScheduler);


     




		 


        /**
         * Auto generated method declaration
         * for "getCollector|http://grid.redhat.com/aviary-collector/" operation.
         * 
         * @param _getCollector of the AviaryCollector::GetCollector
         *
         * @return AviaryCollector::GetCollectorResponse*
         */
        

         virtual 
        AviaryCollector::GetCollectorResponse* getCollector(wso2wsf::MessageContext *outCtx ,AviaryCollector::GetCollector* _getCollector);


     




		 


        /**
         * Auto generated method declaration
         * for "getMasterID|http://grid.redhat.com/aviary-collector/" operation.
         * 
         * @param _getMasterID of the AviaryCollector::GetMasterID
         *
         * @return AviaryCollector::GetMasterIDResponse*
         */
        

         virtual 
        AviaryCollector::GetMasterIDResponse* getMasterID(wso2wsf::MessageContext *outCtx ,AviaryCollector::GetMasterID* _getMasterID);


     




		 


        /**
         * Auto generated method declaration
         * for "getMaster|http://grid.redhat.com/aviary-collector/" operation.
         * 
         * @param _getMaster of the AviaryCollector::GetMaster
         *
         * @return AviaryCollector::GetMasterResponse*
         */
        

         virtual 
        AviaryCollector::GetMasterResponse* getMaster(wso2wsf::MessageContext *outCtx ,AviaryCollector::GetMaster* _getMaster);


     



};


}



        
#endif // AVIARYCOLLECTORSERVICESKELETON_H
    

