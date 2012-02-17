

          #ifndef AVIARYLOCATORSERVICE_H
          #define AVIARYLOCATORSERVICE_H

        /**
         * AviaryLocatorService.h
         *
         * This file was auto-generated from WSDL for "AviaryLocatorService|http://grid.redhat.com/aviary-locator/" service
         * by the Apache Axis2 version: 1.0  Built on : Sep 07, 2011 (03:40:38 EDT)
         *  AviaryLocatorService
         */

#include <ServiceSkeleton.h>
#include <stdio.h>
#include <axis2_svc.h>
#include <AviaryLocator_Locate.h>

using namespace wso2wsf;


using namespace AviaryLocator;



#define WSF_SERVICE_SKEL_INIT(class_name) \
AviaryLocatorServiceSkeleton* wsfGetAviaryLocatorServiceSkeleton(){ return new class_name(); }

AviaryLocatorServiceSkeleton* wsfGetAviaryLocatorServiceSkeleton(); 



        class AviaryLocatorService : public ServiceSkeleton
        {
            private:
                AviaryLocatorServiceSkeleton *skel;

            public:

               union {
                     
               } fault;


              WSF_EXTERN WSF_CALL AviaryLocatorService();

              OMElement* WSF_CALL invoke(OMElement *message, MessageContext *msgCtx);

              OMElement* WSF_CALL onFault(OMElement *message);

              void WSF_CALL init();

              ~AviaryLocatorService(); 
      };



#endif    //     AVIARYLOCATORSERVICE_H

    

