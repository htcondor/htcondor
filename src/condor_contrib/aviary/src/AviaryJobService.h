

          #ifndef AVIARYJOBSERVICE_H
          #define AVIARYJOBSERVICE_H

        /**
         * AviaryJobService.h
         *
         * This file was auto-generated from WSDL for "AviaryJobService|http://grid.redhat.com/aviary-job/" service
         * by the Apache Axis2 version: 1.0  Built on : Jan 09, 2011 (11:40:28 EST)
         *  AviaryJobService
         */

#include <ServiceSkeleton.h>
#include <stdio.h>
#include <axis2_svc.h>

using namespace wso2wsf;


using namespace com_redhat_grid_aviary_job;



#define WSF_SERVICE_SKEL_INIT(class_name) \
AviaryJobServiceSkeleton* wsfGetAviaryJobServiceSkeleton(){ return new class_name(); }

AviaryJobServiceSkeleton* wsfGetAviaryJobServiceSkeleton(); 



        class AviaryJobService : public ServiceSkeleton
        {
            private:
                AviaryJobServiceSkeleton *skel;

            public:

               union {
                     
               } fault;


              WSF_EXTERN WSF_CALL AviaryJobService();

              OMElement* WSF_CALL invoke(OMElement *message, MessageContext *msgCtx);

              OMElement* WSF_CALL onFault(OMElement *message);

              void WSF_CALL init();

              ~AviaryJobService(); 
      };



#endif    //     AVIARYJOBSERVICE_H

    

