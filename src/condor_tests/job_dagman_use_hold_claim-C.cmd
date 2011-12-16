executable = /bin/echo
universe = vanilla
arguments = $$([ifThenElse(isUndefined(ClaimId),0,ClaimId)]) $$([strcat(DAGNodeName,\" \",ifThenElse(isUndefined(KeepClaimIdle),\"KeepClaimIdle undefined\",KeepClaimIdle))])
output = job_dagman_use_hold_claim-C.out
error = job_dagman_use_hold_claim-C.err
log = job_dagman_use_hold_claim.log
queue
