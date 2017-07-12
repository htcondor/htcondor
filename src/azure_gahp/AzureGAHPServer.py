import sys
import platform

import AzureGAHPLib

agce = AzureGAHPLib.AzureGAHPCommandExec()
version = "$GahpVersion 2.0.1 April 19 2017 HTCondor\ GAHP $"
#### MAIN PROGRAM ####
print(version)
unique_str = "##**//,,@@$$"
while(True):
    python_version = platform.python_version_tuple()
    if(int(python_version[0]) >= 3):
        input_line = input().strip()        
    else:
        input_line = raw_input().strip()
    if not input_line:
        print("E Invalid command")
        continue
    # Spec: If a string argument contains a backslash character, it must be
    # escaped by preceding it with another backslash character
    input_line = input_line.replace(AzureGAHPLib.double_backslash_separator, AzureGAHPLib.single_backslash_separator)
    # Spec: In the event that a string argument needs to contain a <SP> within
    # the string itself, it must be escaped by placing a backslash ("\")
    input_line = input_line.replace(AzureGAHPLib.single_backslash_space_separator, unique_str)
    # Replace "\ " with unqiue string with chars is not allowed so that the

    #set running mode
    agce.check_mode(input_line)
    #remove debug and verbose string
    input_line = input_line.replace(AzureGAHPLib.debug,'').replace(AzureGAHPLib.verbose,'').strip()

    # rest of the string can be split by space
    parts = input_line.split(AzureGAHPLib.space_separator)
    # Validate input parameters
    if (len(parts) < 1):
        print("E Invalid command")
        continue

    for i in range(len(parts)):
        parts[i] = parts[i].replace(unique_str, AzureGAHPLib.space_separator)

    command = parts[0]
    if(command.upper() == "COMMANDS"):
        print("S COMMANDS QUIT VERSION RESULTS VERSION AZURE_PING AZURE_VM_CREATE AZURE_VM_DELETE AZURE_VM_LIST AZURE_VMSS_CREATE AZURE_VMSS_DELETE AZURE_VMSS_START AZURE_VMSS_STOP AZURE_VMSS_RESTART AZURE_VMSS_SCALE")
        continue
    elif(command.upper() == "VERSION"):
        print("S " + version)
        continue
    elif(command.upper() == "RESULTS"):
        agce.deque_all_results_and_print()
        continue
    elif(command.upper() == "QUIT"):
        print("S")
        sys.exit()
    elif(command.upper() == "AZURE_PING"):
        if (len(parts) < 4):
            print("E Invalid number of command args. Usage: AZURE_PING<SP><request-ID><SP><cred-file><SP><subscription><CRLF>")
            continue
        if not agce.handle_command(parts):
            print("E Invalid command args. Usage: AZURE_PING<SP><request-ID><SP><cred-file><SP><subscription><CRLF>")
            continue
        print("S")
    elif(command.upper() == "AZURE_VM_CREATE"):
        if (len(parts) < 5):
            print("E Insufficient number of command args. Usage: AZURE_VM_CREATE<SP><request-ID><SP><cred-file><SP><subscription><SP>name=vmName<SP>location=region<SP>size=vmSize<SP>image=osImage<SP>[dataDisks=disk1,disk2,...<SP>adminUsername=userName<SP>adminSshKey=sshKey<SP>vnetName=azureVnetName<SP>publicIPAddress=vmPublicIpAddress<SP>customData=textOrScriptFile<SP>tag=vmTagName]<CRLF>")
            continue
        if not agce.handle_command(parts):
            print("E Invalid command args. Usage: AZURE_VM_CREATE<SP><request-ID><SP><cred-file><SP><subscription><SP>name=vmName,location=region,size=vmSize,image=osImage,[dataDisks=disk1,disk2,...,adminUsername=userName,adminSshKey=sshKey,vnetName=azureVnetName,publicIPAddress=vmPublicIpAddress,customData=textOrScriptFile,tag=vmTagName]<CRLF>")
            continue
        print("S")
    elif(command.upper() == "AZURE_VM_DELETE"):
        if (len(parts) < 5):
            print("E Invalid number of command args. Usage: AZURE_VM_DELETE<SP><request-ID><SP><cred-file><SP><subscription><CRLF>vmName")
            continue
        if not agce.handle_command(parts):
            print("E Invalid command args. Usage: AZURE_VM_DELETE<SP><request-ID><SP><cred-file><SP><subscription><CRLF>vmName")
            continue
        print("S")
    elif(command.upper() == "AZURE_VM_LIST"):
        if (len(parts) < 4):
            print("E Invalid number of command args. Usage: AZURE_VM_LIST<SP><request-ID><SP><cred-file><SP><subscription><CRLF>")
            continue
        if not agce.handle_command(parts):
            print("E Invalid command args. Usage: AZURE_VM_LIST<SP><request-ID><SP><cred-file><SP><subscription><CRLF>")
            continue
        print("S")
    elif(command.upper() == "AZURE_VMSS_CREATE"):
        if (len(parts) < 5):
            print("E Insufficient number of command args. Usage: AZURE_VMSS_CREATE<SP><request-ID><SP><cred-file><SP><subscription><SP>name=vmName<SP>location=region<SP>size=vmSize<SP>image=osImage<SP>[dataDisks=disk1,disk2,...<SP>adminUsername=userName<SP>adminSshKey=sshKey<SP>vnetName=azureVnetName<SP>publicIPAddress=vmPublicIpAddress<SP>customData=textOrScriptFile<SP>tag=vmTagName]<CRLF>")
            continue
        if not agce.handle_command(parts):
            print("E Invalid command args. Usage: AZURE_VMSS_CREATE<SP><request-ID><SP><cred-file><SP><subscription><SP>name=vmName,location=region,size=vmSize,image=osImage,[dataDisks=disk1,disk2,...,adminUsername=userName,adminSshKey=sshKey,vnetName=azureVnetName,publicIPAddress=vmPublicIpAddress,customData=textOrScriptFile,tag=vmTagName]<CRLF>")
            continue
        print("S")
    elif(command.upper() == "AZURE_VMSS_DELETE"):
        if (len(parts) < 5):            
            print("E Invalid number of command args. Usage: AZURE_VMSS_DELETE<SP><request-ID><SP><cred-file><SP><subscription><CRLF>vmssName")
            continue
        if not agce.handle_command(parts):
            print("E Invalid number of command args. Usage: AZURE_VMSS_DELETE<SP><request-ID><SP><cred-file><SP><subscription><CRLF>vmssName")
            continue
        print("S")
    elif(command.upper() == "AZURE_VMSS_START" or command.upper() == "AZURE_VMSS_STOP" or command.upper() == "AZURE_VMSS_RESTART"):
        if (len(parts) < 5 or not agce.handle_command(parts)):
            if(command.upper() == "AZURE_VMSS_START"):
                print("E Invalid number of command args. Usage: AZURE_VMSS_START<SP><request-ID><SP><cred-file><SP><subscription><CRLF>groupName<CRLF>vmssName")
            elif(command.upper() == "AZURE_VMSS_STOP"):
                print("E Invalid number of command args. Usage: AZURE_VMSS_STOP<SP><request-ID><SP><cred-file><SP><subscription><CRLF>groupName<CRLF>vmssName")
            if(command.upper() == "AZURE_VMSS_RESTART"):
                print("E Invalid number of command args. Usage: AZURE_VMSS_RESTART<SP><request-ID><SP><cred-file><SP><subscription><CRLF>groupName<CRLF>vmssName")
            continue
        print("S")
    elif(command.upper() == "AZURE_VMSS_SCALE"):
        if (len(parts) < 5) or not agce.handle_command(parts):            
            print("E Invalid number of command args. Usage: AZURE_VMSS_SCALE<SP><request-ID><SP><cred-file><SP><subscription><SP>vmssName<SP>vmssNodeCount<CRLF>")
            continue
        print("S")
    else:
        print("E Unsupported command: To see a list of valid commands use COMMANDS");

##### END MAIN PROGRAM ##############

    
    

    
