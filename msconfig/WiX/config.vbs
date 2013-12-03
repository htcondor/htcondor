Function ReplaceConfig(configName, newValue, srcTxt)
  Set re = new RegExp
  re.Global = true
  re.Multiline = true
  re.Pattern = "^" & configName & "[ \t]*=.+$"
  Set matches = re.Execute(srcTxt)
  if matches.Count = 0 then
    re.Pattern = "^#" & configName & "[ \t]*=.+$"
  end if
  ReplaceConfig = re.Replace(srcTxt, configName & "=" & newValue)
End Function

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

  configTxt = ReplaceConfig("RELEASE_DIR",strippedPath,configTxt)

  localConfig = Session.Property("LOCALCONFIG")
  if Left(localConfig, 4) = "http" Then
     localConfigCmd = "config_fetch " & localConfig & " $$(LOCAL_DIR)\condor_config.local_cache |"
     configTxt = ReplaceConfig("LOCAL_CONFIG_FILE",localConfigCmd,configTxt)
  Else
     configTxt = ReplaceConfig("LOCAL_CONFIG_FILE",localConfig,configTxt)
  End If
  

  Select Case Session.Property("NEWPOOL")
  Case "Y"
   configTxt = ReplaceConfig("CONDOR_HOST", "$$(FULL_HOSTNAME)",configTxt)
   configTxt = ReplaceConfig("COLLECTOR_NAME",Session.Property("POOLNAME"),configTxt)
   daemonList = daemonList & " COLLECTOR NEGOTIATOR"
  Case "N"
   configTxt = ReplaceConfig("CONDOR_HOST",Session.Property("POOLHOSTNAME"),configTxt)
  End Select

  configTxt = ReplaceConfig("UID_DOMAIN",Session.Property("ACCOUNTINGDOMAIN"),configTxt)
  configTxt = ReplaceConfig("CONDOR_ADMIN",Session.Property("CONDOREMAIL"),configTxt)
  configTxt = ReplaceConfig("SMTP_SERVER",Session.Property("SMTPSERVER"),configTxt)
  configTxt = ReplaceConfig("ALLOW_READ",Session.Property("HOSTALLOWREAD"),configTxt)
  configTxt = ReplaceConfig("ALLOW_WRITE",Session.Property("HOSTALLOWWRITE"),configTxt)
  configTxt = ReplaceConfig("ALLOW_ADMINISTRATOR",Session.Property("HOSTALLOWADMINISTRATOR"),configTxt)
  jvmLoc = Session.Property("JVMLOCATION")
  if fso.FileExists(jvmLoc) then
   Set jvmPath = fso.GetFile(jvmLoc)
   jvmLoc = jvmPath.ShortPath
   configTxt = ReplaceConfig("JAVA",jvmLoc,configTxt)
  end if

  Select Case Session.Property("RUNJOBS")
  Case "A"
   configTxt = ReplaceConfig("START","TRUE",configTxt)
   configTxt = ReplaceConfig("SUSPEND","FALSE",configTxt)
   configTxt = ReplaceConfig("WANT_SUSPEND","FALSE",configTxt)
   configTxt = ReplaceConfig("WANT_VACATE","FALSE",configTxt)
   configTxt = ReplaceConfig("PREEMPT","FALSE",configTxt)
   daemonList = daemonList & " STARTD"
  Case "N"
   configTxt = ReplaceConfig("START","FALSE",configTxt)
  Case "I"
   configTxt = ReplaceConfig("START","KeyboardIdle > $$(StartIdleTime)",configTxt)
   configTxt = ReplaceConfig("CONTINUE","KeyboardIdle > $$(ContinueIdleTime)",configTxt)
   daemonList = daemonList & " STARTD KBDD"
  Case "C"
   daemonList = daemonList & " STARTD KBDD"
  End Select
  
  If Session.Property("VACATEJOBS") = "N" Then
   configTxt = ReplaceConfig("WANT_VACATE","FALSE",configTxt)
   configTxt = ReplaceConfig("WANT_SUSPEND","TRUE",configTxt)
  End If

  If Session.Property("USEVMUNIVERSE") = "Y" Then
   configTxt = ReplaceConfig("VM_TYPE","vmware",configTxt)
   configTxt = ReplaceConfig("VM_MAX_NUMBER",Session.Property("VMMAXNUMBER"),configTxt)
   configTxt = ReplaceConfig("VM_MEMORY",Session.Property("VMMEMORY"),configTxt)
   configTxt = ReplaceConfig("VMWARE_PERL",Session.Property("PERLLOCATION"),configTxt)
   Select Case Session.Property("VMNETWORKING")
   Case "A"
    configTxt = ReplaceConfig("VM_NETWORKING","TRUE",configTxt)
   Case "B"
    configTxt = ReplaceConfig("VM_NETWORKING","TRUE",configTxt)
    configTxt = ReplaceConfig("VM_NETWORKING_TYPE","bridge",configTxt)
   Case "C"
    configTxt = ReplaceConfig("VM_NETWORKING","TRUE",configTxt)
    configTxt = ReplaceConfig("VM_NETWORKING_TYPE","nat, bridge",configTxt)
   End Select
  End if

  configTxt = ReplaceConfig("DAEMON_LIST",daemonList,configTxt)

  Set Configfile = fso.OpenTextFile(path & "condor_config", 2, True)
  Configfile.WriteLine configTxt
  Configfile.Close
End Function

CreateConfig2