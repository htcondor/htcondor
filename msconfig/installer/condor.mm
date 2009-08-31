; Installer choices:
; --newpool NEWPOOL (Y/N)
; --poolhostname POOLHOSTNAME
; --poolname POOLNAME
; --runjobs RUNJOBS (N/A/I/C)
; --jvmlocation JVMLOCATION
; --smtpserver SMTPSERVER
; --condoremail CONDOREMAIL
;

;---------------------------------------------------------------------------
;--- Definitions and Includes ----------------------------------------------
;---------------------------------------------------------------------------

;--- Load MAKEMSI (via wrapper) ---------------------------------------------
#define? COMPANY_MSINAME_SUFFIX.D  

#include "condor.mmh"
#define  UISAMPLE_BLINE_TEXT  Condor for Windows <$ProductVersion>
#define UISAMPLE_BLINE_TEXT_INDENT    10
#define UISAMPLE_BLINE_TEXT_WIDTH    110
#define UISAMPLE_DIALOG_FILE_dlgbmp Leftside-condor.bmp
#define UISAMPLE_BITMAP_WHITE_BANNER banner-condor.bmp   
#define ImageRootDirectory <??*CondorReleaseDir>
#undef DIALOGTEMPLATE_LABEL_COLUMN_WIDTH 
#define DIALOGTEMPLATE_LABEL_COLUMN_WIDTH 115

;---------------------------------------------------------------------------
;--- Macros ----------------------------------------------------------------
;---------------------------------------------------------------------------

;---------------------------------------------------------------------------
;--- PersistMe -------------------------------------------------------------
;---------------------------------------------------------------------------
;--- 1. Retain these values
;--- 2. Supply default values for the dialog
;--- 3. None of these default values are validated during silent install
;--- 4. For silent install your MSIEXEC.EXE command line would supply
;---    overrides for these values as required.
;--- 5. Saving these items is vital for Windows Installer "repair" operation
;---------------------------------------------------------------------------
#define PersistMe <$Property {$?} Persist="Y" PersistHow="LOCAL_MACHINE PRODUCT FOREVER">

;---------------------------------------------------------------------------
;--- Non-persistent values -------------------------------------------------
;---------------------------------------------------------------------------

<$Property "RUNJOBS" VALUE="N">
<$Property "SUBMITJOBS" VALUE="Y">
<$Property "VACATEJOBS" VALUE="N">
<$Property "NEWPOOL" VALUE="N">
<$Property "HOSTALLOWREAD" VALUE="*">
<$Property "HOSTALLOWWRITE" VALUE="your.domain.com, *.cs.wisc.edu">
<$Property "HOSTALLOWADMINISTRATOR" VALUE="$(FULL_HOSTNAME)">
<$Property "AA" VALUE="N">
<$Property "AB" VALUE="N">
<$Property "AC" VALUE="N">
<$Property "USEVMUNIVERSE" VALUE="N">
<$Property "VMVERSION" VALUE="server1.0">
<$Property "VMMEMORY" VALUE="256">
<$Property "VMMAXNUMBER" VALUE="$(NUM_CPUS)">
<$Property "VMNETWORKING" VALUE="N">
<$Property "USEHDFS" VALUE="N">
<$Property "HDFSPORT" VALUE="9000">
<$Property "HDFSWEBPORT" VALUE="8000">

;---------------------------------------------------------------------------
;--- Persistent values -----------------------------------------------------
;---------------------------------------------------------------------------

;--- We need to persist the INSTALLDIR with no trailing slash so that the
;--- unistaller will not choke and fail
<$PersistMe "INSTALLDIR_NTS" VALUE="">

;--- Choke if we are not running on Windows 2000 and beyond ----------------
#(
<$AbortIf Condition=^not VersionNT OR (VersionNT < 500)^ 
 Message=^Condor for Windows can only be installed on Windows 2000 or greater.^>
#)

;--- Find some binaries on the system that we may need ---------------------
;#(
;<$FileFind File="cygwin1.dll" Property="CYGWINDLL" Depth="1" 
;	Path="[WindowsVolume]cygwin" Default="">
;#)
#(
<$FileFind File="JAVA.EXE" Property="JVMLOCATION" Depth="3" 
	Path="[ProgramFilesFolder]" Default="JAVA.EXE">
#)
#(
<$FileFind File="PERL.EXE" Property="PERLLOCATION" Depth="2" 
	Path="[WindowsVolume]\Perl" Default="PERL.EXE">
#)

<$Dialog "New Or Existing Condor Pool?" Dialog="NewOrExistingPool" INSERT="LicenseAgreementDlg">
	
	#data 'RadioButton_NEWPOOL'
		'Y' 'Create a new Condor Pool.' '' 'Use this machine as the Central Manager.'
		'N' 'Join an existing Condor Pool.' '' 'Use an existing Central Manager.'
	#data
	<$DialogEntry Property="NEWPOOL" Label="Choose install type:" Control="RB">

	#(
	<$DialogEntry Property="POOLNAME" Label="Name of New Pool:" 
		ToolTip="Enter the public name of this Condor pool" 
		Width=100 Blank="Y">
	#)

	#(
	<$DialogEntry Property="POOLHOSTNAME" 
		Label="Hostname of Central Manager:" 
		ToolTip="Enter the hostname of the new or existing CM" 
		Width=100 Blank="Y">
	#)

<$/Dialog>

;--- Flip the text fields on or off for pool hostname or pool name ---------
<$Table "ControlCondition">
	<$Row Dialog_="NewOrExistingPool" Control_="Entry.3" Action="Enable" Condition=^NEWPOOL = "N"^>
	<$Row Dialog_="NewOrExistingPool" Control_="Entry.2" Action="Disable" Condition=^NEWPOOL = "N"^>
	<$Row Dialog_="NewOrExistingPool" Control_="Entry.2" Action="Enable" Condition=^NEWPOOL = "Y"^>
	<$Row Dialog_="NewOrExistingPool" Control_="Entry.3" Action="Disable" Condition=^NEWPOOL = "Y"^>
<$/Table>


<$Dialog "Configure Execute and Submit Behavior" Dialog="RunOrSubmitJobs" INSERT="NewOrExistingPool">

	<$DialogEntry Property="SUBMITJOBS" Label="Submit jobs to Condor Pool" Control="XB:N|Y" ToolTip="Run a condor_schedd daemon on this machine.">

	#data 'RadioButton_RUNJOBS'
		'N' 'Do not run jobs on this machine.' '' 'Do not run a condor_startd on this machine.'
		'A' 'Always run jobs and never suspend them.' '' 'This machine will always accept jobs.'
		'I' 'When keyboard has been idle for 15 minutes.' '' 'Idle keyboard and mouse.'
		'C' 'When keyboard has been idle for 15 minutes and CPU is idle.' '' 'Idle keyboard and low CPU activity.'
	#data
	<$DialogEntry Property="RUNJOBS" Label="When should Condor run jobs?" Control="RB">
	
	<$DialogEntry LabelWidth="235" Property="AA" Label="When the machine becomes no longer idle, jobs are suspended." Control="Text">

	#data 'RadioButton_VACATEJOBS'
		'N' 'Keep the job in memory and restart it when you leave.' '' 'Use this machine as the Central Manager.'
		'Y' 'Restart the job on a different machine.' '' 'Use an existing Central Manager.'
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

<$Dialog "Host Permission Settings" Description="What hosts have access to this machine?" Dialog="HostPermissionSettings" INSERT="JavaSettings">
	<$DialogEntry Property="HOSTALLOWREAD" Label="Hosts with Read access:" ToolTip="Query machine status and job queues." LabelWidth=140 Width=160 Blank="N">
	<$DialogEntry Property="HOSTALLOWWRITE" Label="Hosts with Write access:" ToolTip="All machines in the pool require Write access." LabelWidth=140 Width=160 Blank="N">
	<$DialogEntry Property="HOSTALLOWADMINISTRATOR" LabelWidth=140 Label="Hosts with Administrator access:" ToolTip="Turn Condor on/off, modify job queue." Width=160 Blank="N">
<$/Dialog>

<$Dialog "VM Universe Settings" Dialog="VMUniverseSettings" INSERT="HostPermissionSettings">
   	
	#data 'RadioButton_USEVMUNIVERSE'	         
	         'N' 'N&o' '' ''
		 'Y' '&Yes (Requires VMware Server and Perl)' '' ''
	#data
	#(
	<$DialogEntry Property="USEVMUNIVERSE" Label="Enable VM Universe:" 
		Control="RB">
	#)
	
	#(	
	<$DialogEntry Property="VMVERSION" Label="&Version:" 
		ToolTip="Placed in the Machine information Ad" 
		Width=200 Blank="N">
	#)
	
	#(
	<$DialogEntry Property="VMMEMORY" Label="Maximum &Memory (in MB):" 
		ToolTip="The maximum memory VM Universe can use."
		Control="ME:######" Blank="N">
	#)
	
	#(
	<$DialogEntry Property="VMMAXNUMBER" Label="Maximum Number of VMs:" 
		ToolTip="The maximum memory number of VMs that can be run."
		Width=100 Blank="N">
	#)
	
	#data 'ComboBox_VMNETWORKING'
            ;--- Define the networking types we support ---
            'N'	'None'
            'A'	'NAT'
            'B'	'Bridged'
            'C'	'NAT and Bridged'
        #data
	#(
	<$DialogEntry Property="VMNETWORKING" Label="N&etworking Support:" 
		Control="CL">
	#)	

	#(
	<$DialogEntry Property="PERLLOCATION" Label="Path to &Perl Executable:" 
		ToolTip="VM Universe on Windows requires PERL.EXE" 
		Width=200 Blank="N">
	#)	

<$/Dialog>

;--- Flip the fields on or off depending on if VM Universe is on ---
<$Table "ControlCondition">
	<$Row Dialog_="VMUniverseSettings" Control_="Entry.2" Action="Enable" Condition=^USEVMUNIVERSE = "Y"^>	
	<$Row Dialog_="VMUniverseSettings" Control_="Entry.2" Action="Disable" Condition=^USEVMUNIVERSE = "N"^>
	<$Row Dialog_="VMUniverseSettings" Control_="Entry.3" Action="Enable" Condition=^USEVMUNIVERSE = "Y"^>	
	<$Row Dialog_="VMUniverseSettings" Control_="Entry.3" Action="Disable" Condition=^USEVMUNIVERSE = "N"^>
	<$Row Dialog_="VMUniverseSettings" Control_="Entry.4" Action="Enable" Condition=^USEVMUNIVERSE = "Y"^>	
	<$Row Dialog_="VMUniverseSettings" Control_="Entry.4" Action="Disable" Condition=^USEVMUNIVERSE = "N"^>
	<$Row Dialog_="VMUniverseSettings" Control_="Entry.5" Action="Enable" Condition=^USEVMUNIVERSE = "Y"^>	
	<$Row Dialog_="VMUniverseSettings" Control_="Entry.5" Action="Disable" Condition=^USEVMUNIVERSE = "N"^>
	<$Row Dialog_="VMUniverseSettings" Control_="Entry.6" Action="Enable" Condition=^USEVMUNIVERSE = "Y"^>	
	<$Row Dialog_="VMUniverseSettings" Control_="Entry.6" Action="Disable" Condition=^USEVMUNIVERSE = "N"^>
<$/Table>

<$Dialog "HDFS Settings" Description="Enable HDFS support?" Dialog="HDFSSETTINGS" INSERT="VMUniverseSettings">
	#data 'RadioButton_USEHDFS'	         
	         'N' 'N&o' '' ''
		 'Y' '&Yes (Requires Java)' '' ''
	#data
	#(
	<$DialogEntry Property="USEHDFS" Label="Enable HDFS support:" 
		Control="RB">
	#)
	#data 'RadioButton_HDFSMODE'	         
	         'N' 'N&ame Node' '' ''
		 'D' '&Data Node (Requires Cygwin)' '' ''
	#data
	#(
	<$DialogEntry Property="HDFSMODE" Label="Select HDFS mode:" 
		Control="RB">
	#)

	<$DialogEntry Property="NAMENODE" Label="Primary Name Node:" ToolTip="Hostname or IP Address of primary name node." Width=100 Blank="Y">
	<$DialogEntry Property="HDFSPORT" Label="Name Node Port:" ToolTip="Port of primary name node." Width=100 Blank="N">
	<$DialogEntry Property="HDFSWEBPORT" Label="Name Node Web Port:" ToolTip="Port of primary name node web interface." Width=100 Blank="N">
<$/Dialog>

<$Table "ControlCondition">
	<$Row Dialog_="HDFSSETTINGS" Control_="Entry.2" Action="Enable" Condition=^USEHDFS = "Y"^>	
	<$Row Dialog_="HDFSSETTINGS" Control_="Entry.2" Action="Disable" Condition=^USEHDFS = "N"^>
	<$Row Dialog_="HDFSSETTINGS" Control_="Entry.3" Action="Enable" Condition=^USEHDFS = "Y"^>	
	<$Row Dialog_="HDFSSETTINGS" Control_="Entry.3" Action="Disable" Condition=^USEHDFS = "N"^>
	<$Row Dialog_="HDFSSETTINGS" Control_="Entry.4" Action="Enable" Condition=^USEHDFS = "Y"^>	
	<$Row Dialog_="HDFSSETTINGS" Control_="Entry.4" Action="Disable" Condition=^USEHDFS = "N"^>
	<$Row Dialog_="HDFSSETTINGS" Control_="Entry.5" Action="Enable" Condition=^USEHDFS = "Y"^>	
	<$Row Dialog_="HDFSSETTINGS" Control_="Entry.5" Action="Disable" Condition=^USEHDFS = "N"^>
<$/Table>

;--- Create INSTALLDIR ------------------------------------------------------
<$DirectoryTree Key="INSTALLDIR" Dir="c:\condor" CHANGE="\" PrimaryFolder="Y">


;--- Create LOG, SPOOL and EXECUTE ------------------------------------------
<$Component "CreateExecuteFolder" Create="Y" Directory_="EXECUTEDIR">
	<$DirectoryTree Key="EXECUTEDIR" Dir="[INSTALLDIR]\execute" MAKE="Y">
<$/Component>
<$Component "CreateSpoolFolder" Create="Y" Directory_="SPOOLDIR">
	<$DirectoryTree Key="SPOOLDIR" Dir="[INSTALLDIR]\spool" MAKE="Y">
<$/Component>
<$Component "CreateLogFolder" Create="Y" Directory_="LOGDIR">
	<$DirectoryTree Key="LOGDIR" Dir="[INSTALLDIR]\log" MAKE="Y">
<$/Component>

;--- Create directories for HDFS if needed ----------------------------------
<$Component "CreateHDFSNameFolder" Create="Y" Directory_="HDFSNAMEDIR" Condition=^USEHDFS = "Y"^>
	<$DirectoryTree Key="HDFSNAMEDIR" Dir="[INSTALLDIR]\hdfs\hadoop_name" MAKE="Y">
<$/Component>
<$Component "CreateHDFSDataFolder" Create="Y" Directory_="HDFSDATADIR" Condition=^USEHDFS = "Y"^>
	<$DirectoryTree Key="HDFSDATADIR" Dir="[INSTALLDIR]\hdfs\hadoop_data" MAKE="Y">
<$/Component>


;--- Add the files (components generated) -----------------------------------
<$Files '<$ImageRootDirectory>\*.*' SubDir='TREE' DestDir='INSTALLDIR'>

;--- If using VM Universe, copy the default VMX configuration ---------------
<$Component "CondorVMUniverse" Create="Y" Directory_="[INSTALLDIR]" Condition=^USEVMUNIVERSE = "Y"^>
	<$Files '<$ImageRootDirectory>\etc\condor_vmware_local_settings' KeyFile="*">
<$/Component>


;--- Copy cygwin1.dll if we find one (to avoid version conflicts) -----------
;<$Files '[CYGWINDLL]' DestDir='[INSTALLDIR]bin'>


;--- Install the Visual Studio runtime --------------------------------------
#(
<$MergeModule "C:\Program Files\Common Files\Merge Modules\Microsoft_VC90_CRT_x86.msm" 
        language="0" 
        IgnoreErrors=^InstallExecuteSequence:AllocateRegistrySpace^>
#)
#(
<$MergeModule "C:\Program Files\Common Files\Merge Modules\policy_9_0_Microsoft_VC90_CRT_x86.msm"
	language="0" 
        IgnoreErrors=^InstallExecuteSequence:AllocateRegistrySpace \
        InstallExecuteSequence:SxsInstallCA InstallExecuteSequence:SxsUninstallCA^>
#)


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
#(
<$Registry 
	HKEY="LOCAL_MACHINE" 
	KEY="software\Condor" 
	VALUE="[INSTALLDIR_NTS]\condor_config" 
	MsiFormatted="VALUE" 
	Name="CONDOR_CONFIG">
#)


;--- Set Config file parameters ---------------------------------------------
;----------------------------------------------------------------------------
;--- we split into several calls because the arglist can only be 255 
;--- characters
;----------------------------------------------------------------------------
#(
<$ExeCa EXE=^[INSTALLDIR]condor_setup.exe^
Args=^
-n "[NEWPOOL]"
-r "[RUNJOBS]"
-v "[VACATEJOBS]"
-s "[SUBMITJOBS]"
-c "[CONDOREMAIL]"
-e "[HOSTALLOWREAD]"
-t "[HOSTALLOWWRITE]"
-i "[HOSTALLOWADMINISTRATOR]"
-q "[USEHDFS]"^
WorkDir=^INSTALLDIR^   
Condition="<$CONDITION_EXCEPT_UNINSTALL>" 
Seq="InstallServices-"
Rc0="N"       ;; On Vista this app will not return any useful results
Type="System" ;; run as the System account
>
#)

#(
<$ExeCa EXE=^[INSTALLDIR]condor_setup.exe^
Args=^
-p "[POOLNAME]"
-o "[POOLHOSTNAME]"
-a "[ACCOUNTINGDOMAIN]"
-j "[JVMLOCATION]"
-m "[SMTPSERVER]"^
WorkDir=^INSTALLDIR^   
Condition="<$CONDITION_EXCEPT_UNINSTALL>" 
Seq="InstallServices-"
Rc0="N"       ;; On Vista this app will not return any useful results
Type="System" ;; run as the System account	
>
#)

;--- Note that we set the install dir here: we do this because the vm-gaph -
;--- needs it in order to determine the path for its control script --------
#(
<$ExeCa EXE=^[INSTALLDIR]condor_setup.exe^
Args=^
-d "[INSTALLDIR_NTS]"
-u "[USEVMUNIVERSE]"
-w "[VMMAXNUMBER]"
-x "[VMVERSION]"
-y "[VMMEMORY]"
-z "[VMNETWORKING]"
-l "[PERLLOCATION]"^ 
WorkDir=^INSTALLDIR^   
Condition="<$CONDITION_EXCEPT_UNINSTALL>" 
Seq="InstallServices-"
Rc0="N"       ;; On Vista this app will not return any useful results
Type="System" ;; run as the System account	
>
#)

;--- HDFS configuration, only run if HDFS option is elected -
#(
<$ExeCa EXE=^[INSTALLDIR]condor_setup.exe^
Args=^
-f "[NAMENODE]"
-k "[HDFSMODE]"
-g "[HDFSPORT]"
-b "[HDFSWEBPORT]"
^
WorkDir=^INSTALLDIR^   
Condition=^(<$CONDITION_EXCEPT_UNINSTALL>) AND (USEHDFS = "Y")^ 
Seq="InstallServices-"
Rc0="N"       ;; On Vista this app will not return any useful results
Type="System" ;; run as the System account	
>
#)

;--- Set directory Permissions ----------------------------------------------
#(
<$ExeCa EXE=^[INSTALLDIR]condor_set_acls.exe^ Args=^"[INSTALLDIR_NTS]"^ 
WorkDir="INSTALLDIR" Condition="<$CONDITION_EXCEPT_UNINSTALL>" 
Seq="InstallServices-"
Rc0="N"       ;; On Vista this app will not return any useful results
Type="System" ;; run as the System account			
>
#)


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
			Start="AUTO">
#)
	<$ServiceControl Name="Condor" AtInstall="" AtUninstall="stop delete">
<$/Component>


;--- Force a system reboot  -------------------------------------------------
<$Property "REBOOT" VALUE="F">
