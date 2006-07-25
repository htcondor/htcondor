@echo off
if exist ..\src\condor_master.V6\daemon.C goto happywindows
echo Making happy for the repository...
move ..\src\condor_master.V6\daemon_master.C ..\src\condor_master.V6\daemon.C
move ..\src\condor_starter.V6.1\starter_class.C ..\src\condor_starter.V6.1\starter.C
move ..\src\condor_util_lib\condor_common_c.C ..\src\condor_util_lib\condor_common.C
move ..\src\condor_c++_util\email_cpp.C ..\src\condor_c++_util\email.C
move ..\src\condor_mail\condor_email_main.cpp ..\src\condor_mail\main.cpp
move ..\src\condor_eventd\eventd_main.C ..\src\condor_eventd\main.C
move ..\src\condor_dagman\dagman_submit.C ..\src\condor_dagman\submit.C
move ..\src\condor_dagman\dagman_util.C ..\src\condor_dagman\util.C
move ..\src\condor_had\had_Version.C ..\src\condor_had\Version.C
exit /B 1
:happywindows
echo Making happy for the Windows build...
move ..\src\condor_master.V6\daemon.C ..\src\condor_master.V6\daemon_master.C
move ..\src\condor_starter.V6.1\starter.C ..\src\condor_starter.V6.1\starter_class.C
move ..\src\condor_util_lib\condor_common.C ..\src\condor_util_lib\condor_common_c.C
move ..\src\condor_c++_util\email.C ..\src\condor_c++_util\email_cpp.C
move ..\src\condor_mail\main.cpp ..\src\condor_mail\condor_email_main.cpp
move ..\src\condor_eventd\main.C ..\src\condor_eventd\eventd_main.C
move ..\src\condor_dagman\submit.C ..\src\condor_dagman\dagman_submit.C
move ..\src\condor_dagman\util.C ..\src\condor_dagman\dagman_util.C
move ..\src\condor_had\Version.C ..\src\condor_had\had_Version.C
exit /B 2
