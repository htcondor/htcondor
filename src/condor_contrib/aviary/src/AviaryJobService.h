/*
 * Copyright 2009-2011 Red Hat, Inc.
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

          #ifndef AVIARYJOBSERVICE_H
          #define AVIARYJOBSERVICE_H

#include <ServiceSkeleton.h>
#include <stdio.h>
#include <axis2_svc.h>

using namespace wso2wsf;


using namespace AviaryJob;



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

    

