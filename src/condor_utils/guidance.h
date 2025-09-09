#ifndef _CONDOR_GUIDANCE_H
#define _CONDOR_GUIDANCE_H

// #include <optional>
// #include "guidance.h"

enum class GuidanceResult : int {

    Invalid = -3,
    MalformedRequest = -2,
    UnknownRequest = -1,
    Command = 0,

};


#define ATTR_COMMAND                "Command"
#define ATTR_DIAGNOSTIC             "Diagnostic"
#define ATTR_REQUEST_TYPE           "RequestType"
#define ATTR_EVENT_TYPE             "EventType"
#define ATTR_RETRY_DELAY            "RetryDelay"
#define ATTR_JOB_ENVIRONMENT_READY  "JobEnvironmentReady"

#define COMMAND_RUN_DIAGNOSTIC      "RunDiagnostic"
#define COMMAND_RETRY_TRANSFER      "RetryTransfer"
#define COMMAND_ABORT               "Abort"
#define COMMAND_START_JOB           "StartJob"
#define COMMAND_CARRY_ON            "CarryOn"
#define COMMAND_RETRY_REQUEST       "RetryReqest"
#define COMMAND_STAGE_COMMON_FILES  "StageCommonFiles"
#define COMMAND_MAP_COMMON_FILES    "MapCommonFiles"
#define COMMAND_JOB_SETUP           "DoJobSetup"

#define DIAGNOSTIC_SEND_EP_LOGS     "send_ep_logs"

#define RTYPE_JOB_ENVIRONMENT       "JobEnvironment"
#define RTYPE_JOB_SETUP             "JobSetup"

#define ETYPE_DIAGNOSTIC_RESULT     "DiagnosticResult"

enum class RequestResult : int {
    Invalid = -1,
    InternalError = 1,
};

std::optional<std::string> makeCIFName(
    const classad::ClassAd & jobAd,
    const std::string & baseName,
    const std::string & startdAddress,
    const std::string & content
);

#endif /* defined(_CONDOR_GUIDANCE_H) */
