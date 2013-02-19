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

          #ifndef AVIARYHADOOPSERVICE_H
          #define AVIARYHADOOPSERVICE_H

#include <ServiceSkeleton.h>
#include <stdio.h>
#include <axis2_svc.h>

using namespace wso2wsf;


using namespace AviaryHadoop;



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

              virtual void WSF_CALL init();

              ~AviaryHadoopService(); 
      };



#endif    //     AVIARYHADOOPSERVICE_H

    

