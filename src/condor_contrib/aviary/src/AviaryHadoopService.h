

          #ifndef AVIARYHADOOPSERVICE_H
          #define AVIARYHADOOPSERVICE_H

        /**
         * AviaryHadoopService.h
         *
         * This file was auto-generated from WSDL for "AviaryHadoopService|http://grid.redhat.com/aviary-hadoop/" service
         * by the Apache Axis2 version: 1.0  Built on : Nov 08, 2012 (09:07:16 EST)
         *  AviaryHadoopService
         */

#include <ServiceSkeleton.h>
#include <stdio.h>
#include <axis2_svc.h>

using namespace wso2wsf;


using namespace com_redhat_grid_aviary_hadoop;



#define WSF_SERVICE_SKEL_INIT(class_name) \
AviaryHadoopServiceSkeleton* wsfGetAviaryHadoopServiceSkeleton(){ return new class_name(); }

AviaryHadoopServiceSkeleton* wsfGetAviaryHadoopServiceSkeleton(); 



        class AviaryHadoopService : public ServiceSkeleton
        {
            private:
                AviaryHadoopServiceSkeleton *skel;

            public:

               union {
                     
               } fault;


              WSF_EXTERN WSF_CALL AviaryHadoopService();

              OMElement* WSF_CALL invoke(OMElement *message, MessageContext *msgCtx);

              OMElement* WSF_CALL onFault(OMElement *message);

              virtual bool WSF_CALL init();

              ~AviaryHadoopService(); 
      };



#endif    //     AVIARYHADOOPSERVICE_H

    

