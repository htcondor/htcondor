#ifndef CONDOR_SYSTRAY_COMMON_H
#define CONDOR_SYSTRAY_COMMON_H

const int CONDOR_SYSTRAY_NOTIFY_CHANGED = WM_USER + 4000;

enum ECpuStatus { kSystrayStatusIdle, kSystrayStatusClaimed, kSystrayStatusJobRunning, kSystrayStatusJobSuspended, kSystrayStatusJobPreempting };

#endif