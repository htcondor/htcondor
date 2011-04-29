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


#include "AviaryQueryServiceSkeleton.h"
#include "AviaryQueryService.h"
#include <ServiceSkeleton.h>
#include <stdio.h>
#include <axis2_svc.h>
#include <Environment.h>

using namespace wso2wsf;

using namespace AviaryQuery;



/** Load the service into engine
Note:- If you are extending from the Generated Skeleton class,you need is to change the argument provided to the
macro to your derived class name.
Example
If your service is Calculator, you will have the business logic implementation class as CalculatorSkeleton.
If the extended class is CalculatorSkeletonImpl, then you change the argument to the macro WSF_SERVICE_SKEL_INIT as
WSF_SERVICE_SKEL_INIT(CalculatorSkeletonImpl). Also include the header file of the derived class, in this case CalculatorSkeletonImpl.h

*/

WSF_SERVICE_SKEL_INIT(AviaryQueryServiceSkeleton)



