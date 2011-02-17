

#include "AviaryJobServiceSkeleton.h"
#include "AviaryJobService.h"
#include <ServiceSkeleton.h>
#include <stdio.h>
#include <axis2_svc.h>
#include <Environment.h>

using namespace wso2wsf;

using namespace com_redhat_grid_aviary_job;



/** Load the service into engine
Note:- If you are extending from the Generated Skeleton class,you need is to change the argument provided to the
macro to your derived class name.
Example
If your service is Calculator, you will have the business logic implementation class as CalculatorSkeleton.
If the extended class is CalculatorSkeletonImpl, then you change the argument to the macro WSF_SERVICE_SKEL_INIT as
WSF_SERVICE_SKEL_INIT(CalculatorSkeletonImpl). Also include the header file of the derived class, in this case CalculatorSkeletonImpl.h

*/

WSF_SERVICE_SKEL_INIT(AviaryJobServiceSkeleton)



