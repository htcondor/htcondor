Function CreateConfig2()
  Set fso = CreateObject("Scripting.FileSystemObject")
  path = Session.Property("INSTALLDIR")
  Set installpath = fso.GetFolder(path)
  strippedPath = installpath.ShortPath
  if Not fso.FileExists(path & "condor_config.local") Then
   fso.CreateTextFile(path & "condor_config.local")
  End if
  if fso.FileExists(path & "condor_config") then
   Exit Function
  end if

  daemonList = "MASTER"

  if Session.Property("SUBMITJOBS") = "Y" Then
   daemonList = daemonList & " SCHEDD"
  End If

  Set Configfile = fso.OpenTextFile(path & "etc\condor_config.generic", 1, True)
  configTxt = Configfile.ReadAll
  Configfile.Close

  configTxt = Replace(configTxt, "RELEASE_DIR = C:\Condor", "RELEASE_DIR = " & strippedPath)

  Select Case Session.Property("NEWPOOL")
  Case "Y"
   configTxt = Replace(configTxt, "CONDOR_HOST	= central-manager-hostname.your.domain", "CONDOR_HOST=$$(FULL_HOSTNAME)")
   configTxt = Replace(configTxt, "COLLECTOR_NAME 		= My Pool", "COLLECTOR_NAME=" & Session.Property("POOLNAME") )
   daemonList = daemonList & " COLLECTOR NEGOTIATOR"
  Case "N"
   configTxt = Replace(configTxt, "CONDOR_HOST	= central-manager-hostname.your.domain", "CONDOR_HOST=" & Session.Property("POOLHOSTNAME") )
  End Select

  configTxt = Replace(configTxt, "#UID_DOMAIN		= $$(FULL_HOSTNAME)", "UID_DOMAIN=" & Session.Property("ACCOUNTINGDOMAIN") )
  configTxt = Replace(configTxt, "CONDOR_ADMIN		= root@$$(FULL_HOSTNAME)", "CONDOR_ADMIN=" & Session.Property("CONDOREMAIL") )
  configTxt = Replace(configTxt, "SMTP_SERVER =", "SMTP_SERVER=" & Session.Property("SMTPSERVER") )
  configTxt = Replace(configTxt, "ALLOW_READ = * ", "ALLOW_READ=" & Session.Property("HOSTALLOWREAD") )
  configTxt = Replace(configTxt, "$$(FULL_HOSTNAME), $$(IP_ADDRESS)", Session.Property("HOSTALLOWWRITE") )
  configTxt = Replace(configTxt, "ALLOW_ADMINISTRATOR = $$(CONDOR_HOST)", "ALLOW_ADMINISTRATOR=" & Session.Property("HOSTALLOWADMINISTRATOR") )
  jvmLoc = Session.Property("JVMLOCATION")
  if fso.FileExists(jvmLoc) then
   Set jvmPath = fso.GetFile(jvmLoc)
   jvmLoc = jvmPath.ShortPath
   configTxt = Replace(configTxt, "JAVA =", "JAVA=" & jvmLoc )
  end if

  Select Case Session.Property("RUNJOBS")
  Case "A"
   configTxt = Replace(configTxt, "$$(UWCS_START)", "TRUE")
   configTxt = Replace(configTxt, "$$(UWCS_SUSPEND)", "FALSE")
   configTxt = Replace(configTxt, "WANT_SUSPEND 		= $$(UWCS_WANT_SUSPEND)", "WANT_SUSPEND=FALSE")
   configTxt = Replace(configTxt, "$$(UWCS_WANT_VACATE)", "FALSE")
   configTxt = Replace(configTxt, "$$(UWCS_PREEMPT)", "FALSE")
   daemonList = daemonList & " STARTD KBDD"
  Case "N"
   configTxt = Replace(configTxt, "$$(UWCS_START)", "FALSE")
  Case "I"
   configTxt = Replace(configTxt, "$$(UWCS_START)", "KeyboardIdle > $$(StartIdleTime)" )
   configTxt = Replace(configTxt, "$$(UWCS_CONTINUE)", "KeyboardIdle > $$(ContinueIdleTime)" )
   daemonList = daemonList & " STARTD KBDD"
  Case "C"
   daemonList = daemonList & " STARTD KBDD"
  End Select
  If Session.Property("VACATEJOBS") = "N" Then
   configTxt = Replace(configTxt, "$$(UWCS_WANT_VACATE)", "FALSE")
   configTxt = Replace(configTxt, "WANT_SUSPEND 		= $$(UWCS_WANT_SUSPEND)", "WANT_SUSPEND = TRUE")
  End If

  If Session.Property("USEVMUNIVERSE") = "Y" Then
   configTxt = Replace(configTxt, "VM_TYPE =", "VM_TYPE=vmware")
   configTxt = Replace(configTxt, "VM_MAX_NUMBER = $$(NUM_CPUS)", "VM_MAX_NUMBER=" & Session.Property("VMMAXNUMBER") )
   configTxt = Replace(configTxt, "VM_MEMORY = 128", "VM_MEMORY=" & Session.Property("VMMEMORY") )
   configTxt = Replace(configTxt, "VMWARE_PERL = perl", "VMWARE_PERL=" & Session.Property("PERLLOCATION") )
   Select Case Session.Property("VMNETWORKING")
   Case "A"
    configTxt = Replace(configTxt, "VM_NETWORKING = FALSE", "VM_NETWORKING = TRUE")
   Case "B"
    configTxt = Replace(configTxt, "VM_NETWORKING = FALSE", "VM_NETWORKING = TRUE")
    configTxt = Replace(configTxt, "VM_NETWORKING_TYPE = nat", "VM_NETWORKING_TYPE = bridge")
   Case "C"
    configTxt = Replace(configTxt, "VM_NETWORKING = FALSE", "VM_NETWORKING = TRUE")
    configTxt = Replace(configTxt, "VM_NETWORKING_TYPE = nat", "VM_NETWORKING_TYPE = nat, bridge")
   End Select
  End if

  If Session.Property("USEHDFS") = "Y" Then
   daemonList = daemonList & " HDFS"
   namenodeAddr = Session.Property("NAMENODE")
   configTxt = Replace(configTxt, "#HDFS_HOME = $$(RELEASE_DIR)/libexec/hdfs", "HDFS_HOME = $$(RELEASE_DIR)/hdfs")
   configTxt = Replace(configTxt, "HDFS_NODETYPE = HDFS_DATANODE", "HDFS_NODETYPE=" & Session.Property("HDFSMODE") )
   configTxt = Replace(configTxt, "hdfs://example.com:9000", namenodeAddr & ":" & Session.Property("HDFSPORT") )
   configTxt = Replace(configTxt, "example.com:8000", namenodeAddr  & ":" & Session.Property("HDFSWEBPORT") )
   configTxt = Replace(configTxt, "$$(RELEASE_DIR)/libexec/hdfs", Session.Property("HDFSHOME") )
   configTxt = Replace(configTxt, "/tmp/hadoop_name", Session.Property("HDFSNAMEDIR") )
   configTxt = Replace(configTxt, "/scratch/tmp/hadoop_data", Session.Property("HDFSDATADIR") )
  End if

  configTxt = Replace(configTxt, " MASTER, STARTD, SCHEDD", daemonList)

  Set Configfile = fso.OpenTextFile(path & "condor_config", 2, True)
  Configfile.WriteLine configTxt
  Configfile.Close
End Function

CreateConfig2