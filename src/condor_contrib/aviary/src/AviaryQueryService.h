

          #ifndef AVIARYQUERYSERVICE_H
          #define AVIARYQUERYSERVICE_H

        /**
         * AviaryQueryService.h
         *
         * This file was auto-generated from WSDL for "AviaryQueryService|http://grid.redhat.com/aviary-query/" service
         * by the Apache Axis2 version: 1.0  Built on : Jan 09, 2011 (11:40:28 EST)
         *  AviaryQueryService
         */

#include <ServiceSkeleton.h>
#include <stdio.h>
#include <axis2_svc.h>

using namespace wso2wsf;


using namespace com_redhat_grid_aviary_query;



#define WSF_SERVICE_SKEL_INIT(class_name) \
AviaryQueryServiceSkeleton* wsfGetAviaryQueryServiceSkeleton(){ return new class_name(); }

AviaryQueryServiceSkeleton* wsfGetAviaryQueryServiceSkeleton(); 



        class AviaryQueryService : public ServiceSkeleton
        {
            private:
                AviaryQueryServiceSkeleton *skel;

            public:

               union {
                     
               } fault;


              WSF_EXTERN WSF_CALL AviaryQueryService();

              OMElement* WSF_CALL invoke(OMElement *message, MessageContext *msgCtx);

              OMElement* WSF_CALL onFault(OMElement *message);

              void WSF_CALL init();

              ~AviaryQueryService(); 
      };



#endif    //     AVIARYQUERYSERVICE_H

    

