; Installer choices:
; --newpool NEWPOOL (Y/N)
; --poolhostname POOLHOSTNAME
; --poolname POOLNAME
; --runjobs RUNJOBS (N/A/I/C)
; --jvmlocation JVMLOCATION
; --smtpserver SMTPSERVER
; --condoremail CONDOREMAIL
;

;--- Load MAKEMSI (via wrapper) ---------------------------------------------
#define? COMPANY_MSINAME_SUFFIX.D  

#include "condor.mmh"
#define  UISAMPLE_BLINE_TEXT           Condor for Windows <$ProductVersion>
#define UISAMPLE_BLINE_TEXT_INDENT    10
#define UISAMPLE_BLINE_TEXT_WIDTH    110
#define UISAMPLE_DIALOG_FILE_dlgbmp Leftside-condor.bmp
#define UISAMPLE_BITMAP_WHITE_BANNER banner-condor.bmp   
#define ImageRootDirectory <??*CondorReleaseDir>
#undef DIALOGTEMPLATE_LABEL_COLUMN_WIDTH 
#define DIALOGTEMPLATE_LABEL_COLUMN_WIDTH 115

<$Property "RUNJOBS" VALUE="N">
<$Property "SUBMITJOBS" VALUE="Y">
<$Property "VACATEJOBS" VALUE="N">
<$Property "NEWPOOL" VALUE="N">
<$Property "HOSTALLOWREAD" VALUE="your.domain.com, *.cs.wisc.edu">
<$Property "HOSTALLOWWRITE" VALUE="*">
<$Property "HOSTALLOWADMINISTRATOR" VALUE="$(FULL_HOSTNAME)">
<$Property "AA" VALUE="N">
<$Property "AB" VALUE="N">

<$FileFind File="JAVA.EXE" Property="JVMLOCATION" Depth="3" Path="[ProgramFilesFolder]" Default="JAVA.EXE">

<$Dialog "New Or Existing Condor Pool?" Dialog="NewOrExistingPool" INSERT="LicenseAgreementDlg">
   ;--- A Radio Button ------------------------------------------------------
   #data 'RadioButton_NEWPOOL'
         ;--- All buttons on same dialog line (not over 3) ------------------
         'Y'       'Create a new Condor Pool.'      ''  'Use this machine as the Central Manager.'
         'N'  'Join an existing Condor Pool.'       ''  'Use an existing Central Manager.'
   #data
	<$DialogEntry Property="NEWPOOL" Label="Choose install type:" Control="RB">

	<$DialogEntry Property="POOLNAME" Label="Name of New Pool:" ToolTip="Enter the public name of this Condor pool" Width=100 Blank="Y">

	<$DialogEntry Property="POOLHOSTNAME" Label="Hostname of Central Manager:" ToolTip="Enter the hostname of the new or existing CM" Width=100 Blank="Y">
<$/Dialog>

; flip the text fields on or off for pool hostname or pool name
<$Table "ControlCondition">
	<$Row    Dialog_="NewOrExistingPool" Control_="Entry.3" Action="Enable" Condition=^NEWPOOL = "N"^>
	<$Row    Dialog_="NewOrExistingPool" Control_="Entry.2" Action="Disable" Condition=^NEWPOOL = "N"^>

	<$Row    Dialog_="NewOrExistingPool" Control_="Entry.2" Action="Enable" Condition=^NEWPOOL = "Y"^>
	<$Row    Dialog_="NewOrExistingPool" Control_="Entry.3" Action="Disable" Condition=^NEWPOOL = "Y"^>
<$/Table>


<$Dialog "Configure Execute and Submit Behavior" Dialog="RunOrSubmitJobs" INSERT="NewOrExistingPool">
	<$DialogEntry Property="SUBMITJOBS" Label="Submit jobs to Condor Pool" Control="XB:N|Y" ToolTip="Run a condor_schedd daemon on this machine.">

   #data 'RadioButton_RUNJOBS'
         ;--- All buttons on same dialog line (not over 3) ------------------
         'N'  'Do not run jobs on this machine.' ''  'Do not run a condor_startd on this machine.'
		 'A'  'Always run jobs and never suspend them.' '' 'This machine will always accept jobs.'
         'I'  'When keyboard has been idle for 15 minutes.' ''  'Idle keyboard and mouse.'
         'C'  'When keyboard has been idle for 15 minutes and CPU is idle.' ''  'Idle keyboard and low CPU activity.'
   #data

	<$DialogEntry Property="RUNJOBS" Label="When should Condor run jobs?" Control="RB">
	
	<$DialogEntry LabelWidth="235" Property="AA" Label="When the machine becomes no longer idle, jobs are suspended." Control="Text">

   #data 'RadioButton_VACATEJOBS'
         'N'  'Keep the job in memory and restart it when you leave.' ''  'Use this machine as the Central Manager.'
         'Y'  'Restart the job on a different machine.'  ''  'Use an existing Central Manager.'
   #data

	<$DialogEntry Property="VACATEJOBS" Label="After 10 minutes:" Control="RB">
<$/Dialog>

<$Table "ControlCondition">
	<$Row    Dialog_="RunOrSubmitJobs" Control_="Entry.3" Action="Disable" Condition=^RUNJOBS = "N" or RUNJOBS = "A"^>
	<$Row    Dialog_="RunOrSubmitJobs" Control_="Entry.3" Action="Enable" Condition=^RUNJOBS = "I" or RUNJOBS = "C"^>
<$/Table>


<$Dialog "Accounting Domain" Description="What accounting (or UID) domain is this machine in?" Dialog="AccountingDomain" INSERT="RunOrSubmitJobs">

	<$DialogEntry LabelWidth="265" Property="AA" Label="Usually a DNS domain can be the accounting domain (e.g. cs.wisc.edu)." Control="Text">
	<$DialogEntry LabelWidth="200" Property="AB" Label="Leave it blank if you are unsure." Control="Text">
	<$DialogEntry Property="ACCOUNTINGDOMAIN" Label="Accounting Domain:" ToolTip="" Width=100 Blank="Y">
<$/Dialog>

<$Dialog "Email Settings" Description="From where and to whom should Condor send email?" Dialog="EmailSettings" INSERT="AccountingDomain">
	<$DialogEntry Property="SMTPSERVER" Label="Hostname of SMTP Server:" ToolTip="Hostname or IP Address of SMTP server." Width=100 Blank="Y">
	<$DialogEntry Property="CONDOREMAIL" Label="Email address of administrator:" ToolTip="Address to receive Condor-related email." Width=100 Blank="Y">
<$/Dialog>

<$Dialog "Java Settings" Description="Where is the Java virtual machine?" Dialog="JavaSettings" INSERT="EmailSettings">
	<$DialogEntry Property="JVMLOCATION" Label="Path to Java Virtual Machine:" ToolTip="Path to JAVA.EXE" Width=200 Blank="Y">
<$/Dialog>

<$Dialog "Host Permission Settings" Description="What hosts have access to this machine?" Dialog="HostPermissionSettings" INSERT="EmailSettings">
	<$DialogEntry Property="HOSTALLOWREAD" Label="Hosts with Read access:" ToolTip="Query machine status and job queues." LabelWidth=140 Width=160 Blank="N">
	<$DialogEntry Property="HOSTALLOWWRITE" Label="Hosts with Write access:" ToolTip="All machines in the pool require Write access." LabelWidth=140 Width=160 Blank="N">
	<$DialogEntry Property="HOSTALLOWADMINISTRATOR" LabelWidth=140 Label="Hosts with Administrator access:" ToolTip="Turn Condor on/off, modify job queue." Width=160 Blank="N">
<$/Dialog>

;--- Create INSTALLDIR ------------------------------------------------------
<$DirectoryTree Key="INSTALLDIR" Dir="c:\condor" CHANGE="\" PrimaryFolder="Y">

;--- Create LOG, SPOOL and EXECUTE
<$Component "CreateExecuteFolder" Create="Y" Directory_="EXECUTEDIR">
	<$DirectoryTree Key="EXECUTEDIR" Dir="[INSTALLDIR]\execute" MAKE="Y">
<$/Component>
<$Component "CreateSpoolFolder" Create="Y" Directory_="SPOOLDIR">
	<$DirectoryTree Key="SPOOLDIR" Dir="[INSTALLDIR]\spool" MAKE="Y">
<$/Component>
<$Component "CreateLogFolder" Create="Y" Directory_="LOGDIR">
	<$DirectoryTree Key="LOGDIR" Dir="[INSTALLDIR]\log" MAKE="Y">
<$/Component>


;--- Add the files (components generated) -----------------------------------
<$Files '<$ImageRootDirectory>\*.*' SubDir='TREE' DestDir='INSTALLDIR'>


; ---- remove trailing slash from INSTALLDIR --------
<$VbsCa Binary="RemoveSlashFromINSTALLDIR.vbs">
   <$VbsCaEntry "RemoveTrailingSlash">
        ;--- Define any VB variables ----------------------------------------
        dim InputValue, OutputValue

        ;--- Get the properties current value -------------------------------
        InputValue = session.property("INSTALLDIR")
        CaDebug 0, "INSTALLDIR = """ & InputValue & """"

        ;--- Manipulate the value -------------------------------------------
        if  right(InputValue, 1) <> "\" then
            OutputValue = InputValue                          ;;Already "slashless"
        else
            OutputValue = left(InputValue, len(InputValue)-1) ;;Remove last character (slash)
        end if

        ;--- Return the value -----------------------------------------------
        CaDebug 0, "INSTALLDIR_NTS = """ & OutputValue & """"
        session.property("INSTALLDIR_NTS") = OutputValue
   <$/VbsCaEntry>
<$/VbsCa>
<$VbsCaSetup Binary="RemoveSlashFromINSTALLDIR.vbs" Entry="RemoveTrailingSlash" Seq=1401 CONDITION=^<$CONDITION_INSTALL_ONLY>^ Deferred="N">

;--- set CONDOR_CONFIG registry key ----------------------------------------
<$Access "AdminOnly" Users="Administrators SYSTEM" Access="GENERIC_ALL">
<$Registry HKEY="LOCAL_MACHINE" KEY="software\Condor"  VALUE="[INSTALLDIR_NTS]\condor_config" MsiFormatted="VALUE" Name="CONDOR_CONFIG" Access="AdminOnly">

;--- Install the Condor Service ----------------------------------------
<$Component "Condor" Create="Y" Directory_="[INSTALLDIR]bin">
   ;--- The service EXE MUST be the keypath of the component ----------------
   <$Files "<$ImageRootDirectory>\bin\condor_master.exe" KeyFile="*">

#(
   <$ServiceInstall
                  Name="Condor"
           DisplayName="Condor"
           Description="Condor Master Daemon"
               Process="OWN"
			     Start="AUTO"
   >
#)
;--- start/stop service -------------------------------------------------
   <$ServiceControl Name="Condor" AtInstall="stop start" AtUninstall="stop delete">

<$/Component>


;--- Set Config file parameters ---------------------------------------------
;-- we split into two calls because the arglist can only be 255 characters.
#(
<$ExeCa EXE=^[INSTALLDIR]condor_setup.exe^ Args=^
-n "[NEWPOOL]"
-r "[RUNJOBS]"
-v "[VACATEJOBS]"
-s "[SUBMITJOBS]"
-c "[CONDOREMAIL]"
-e "[HOSTALLOWREAD]"
-t "[HOSTALLOWWRITE]"
-i "[HOSTALLOWADMINISTRATOR]"
^ WorkDir=^INSTALLDIR^   Condition="<$CONDITION_EXCEPT_UNINSTALL>" Seq="StartServices-">
#)

#(
<$ExeCa EXE=^[INSTALLDIR]condor_setup.exe^ Args=^
-p "[POOLNAME]"
-o "[POOLHOSTNAME]"
-d "[INSTALLDIR_NTS]"
-a "[ACCOUNTINGDOMAIN]"
-j "[JVMLOCATION]"
-m "[SMTPSERVER]"
^ WorkDir=^INSTALLDIR^   Condition="<$CONDITION_EXCEPT_UNINSTALL>" Seq="StartServices-">
#)

;--- Set directory Permissions ----------------------------------------------
;-------- Set INSTALLDIR perms first ---
<$ExeCa EXE=^cmd.exe^ Args=^/c echo y|cacls "[INSTALLDIR_NTS]" /t /g Everyone:R Administrators:F^ WorkDir="INSTALLDIR"   Condition="<$CONDITION_EXCEPT_UNINSTALL>" Seq="StartServices-">

;-------- Set everything under INSTALLDIR ----
<$ExeCa EXE=^cmd.exe^ Args=^/c echo y|cacls "[INSTALLDIR_NTS]"\*.* /t /g Everyone:R Administrators:F^ WorkDir="INSTALLDIR"   Condition="<$CONDITION_EXCEPT_UNINSTALL>" Seq="StartServices-">

