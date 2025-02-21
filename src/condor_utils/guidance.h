#ifndef _CONDOR_GUIDANCE_H
#define _CONDOR_GUIDANCE_H


enum class GuidanceResult : int {

    Invalid = -3,
    MalformedRequest = -2,
    UnknownRequest = -1,
    Command = 0,

};


#define ATTR_COMMAND            "Command"
#define ATTR_DIAGNOSTIC         "Diagnostic"
#define ATTR_REQUEST_TYPE       "RequestType"
#define ATTR_EVENT_TYPE         "EventType"

#define COMMAND_RUN_DIAGNOSTIC  "RunDiagnostic"
#define COMMAND_RETRY_TRANSFER  "RetryTransfer"
#define COMMAND_ABORT           "Abort"
#define COMMAND_START_JOB       "StartJob"
#define COMMAND_CARRY_ON        "CarryOn"

#define COMMAND_START_NEW_FILE_TRANSFER     "StartNewFileTransfer"

#define DIAGNOSTIC_SEND_EP_LOGS "send_ep_logs"

#define RTYPE_JOB_ENVIRONMENT   "JobEnvironment"
#define RTYPE_JOB_SETUP         "JobSetup"

#define ETYPE_DIAGNOSTIC_RESULT "DiagnosticResult"


#endif /* defined(_CONDOR_GUIDANCE_H) */
