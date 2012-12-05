

          #ifndef AVIARYCOLLECTORSERVICE_H
          #define AVIARYCOLLECTORSERVICE_H

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

#include <ServiceSkeleton.h>
#include <stdio.h>
#include <axis2_svc.h>

using namespace wso2wsf;


using namespace AviaryCollector;



#define WSF_SERVICE_SKEL_INIT(class_name) \
AviaryCollectorServiceSkeleton* wsfGetAviaryCollectorServiceSkeleton(){ return new class_name(); }

AviaryCollectorServiceSkeleton* wsfGetAviaryCollectorServiceSkeleton(); 



        class AviaryCollectorService : public ServiceSkeleton
        {
            private:
                AviaryCollectorServiceSkeleton *skel;

            public:

               union {
                     
               } fault;


              WSF_EXTERN WSF_CALL AviaryCollectorService();

              OMElement* WSF_CALL invoke(OMElement *message, MessageContext *msgCtx);

              OMElement* WSF_CALL onFault(OMElement *message);

              void WSF_CALL init();

              ~AviaryCollectorService(); 
      };



#endif    //     AVIARYCOLLECTORSERVICE_H

    

