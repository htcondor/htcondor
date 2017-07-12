import sys
import AzureGAHPLib

agce = AzureGAHPLib.AzureGAHPCommandExec()
#### MAIN PROGRAM ####
print ("$GahpVersion 2.0.0 Mar 9 2017 HTCondor\ GAHP $")
#resource_client.providers.register('Microsoft.Compute')
#resource_client.providers.register('Microsoft.Network')
UNIQUE_STR = "##**//,,@@$$"
while(True):
    inputLine = input().strip()
    if not inputLine:
        print("E Invalid command")
        continue
    # Spec: If a string argument contains a backslash character, it must be escaped by preceding it with another backslash character
    inputLine = inputLine.replace("\\\\", "\\")
    # Spec: In the event that a string argument needs to contain a <SP> within the string itself, it must be escaped by placing a backslash ("\")
    inputLine = inputLine.replace("\\ ", UNIQUE_STR)
    # Replace "\ " with unqiue string with chars is not allowed so that the rest of the string can be split by space
    parts = inputLine.split(" ")
    
    if (len(parts) < 1):
        print("E Invalid command")
        continue

    for i in range(len(parts)):
        parts[i] = parts[i].replace(UNIQUE_STR, " ")

    command = parts[0]
    if(command.upper() == "COMMANDS"):
        print("S COMMANDS QUIT RESULTS VERSION AZURE_PING AZURE_VM_CREATE AZURE_VM_DELETE AZURE_VM_LIST")
        continue
    elif(command.upper() == "VERSION"):
        print("S $GahpVersion 2.0.0 Mar 9 2017 HTCondor\ GAHP $")
        continue
    elif(command.upper() == "RESULTS"):
        agce.DequeAllResultsAndPrint()
        continue
    elif(command.upper() == "QUIT"):
        print("S")
        sys.exit()
    elif(command.upper() == "AZURE_PING"):
        if (len(parts) < 4):
            print("E Invalid number of command args. Usage: AZURE_PING<SP><request-ID><SP><cred-file><SP><subscription><CRLF>")
            continue
        if not agce.HandleCommand(parts):
            print("E Invalid command args. Usage: AZURE_PING<SP><request-ID><SP><cred-file><SP><subscription><CRLF>")
            continue
        print("S")
    elif(command.upper() == "AZURE_VM_CREATE"):
        if (len(parts) < 5):
            print("E Insufficient number of command args. Usage: AZURE_VM_CREATE<SP><request-ID><SP><cred-file><SP><subscription><SP>name=vmName<SP>location=region<SP>size=vmSize<SP>image=osImage<SP>[dataDisks=disk1,disk2,...<SP>adminUsername=userName<SP>adminSshKey=sshKey<SP>vnetName=azureVnetName<SP>publicIPAddress=vmPublicIpAddress<SP>customData=textOrScriptFile<SP>tag=vmTagName]<CRLF>")
            continue
        if not agce.HandleCommand(parts):
            print("E Invalid command args. Usage: AZURE_VM_CREATE<SP><request-ID><SP><cred-file><SP><subscription><SP>name=vmName,location=region,size=vmSize,image=osImage,[dataDisks=disk1,disk2,...,adminUsername=userName,adminSshKey=sshKey,vnetName=azureVnetName,publicIPAddress=vmPublicIpAddress,customData=textOrScriptFile,tag=vmTagName]<CRLF>")
            continue
        print("S")
    elif(command.upper() == "AZURE_VM_DELETE"):
        if (len(parts) < 5):
            print("E Invalid number of command args. Usage: AZURE_VM_DELETE<SP><request-ID><SP><cred-file><SP><subscription><CRLF>vmName")
            continue
        if not agce.HandleCommand(parts):
            print("E Invalid command args. Usage: AZURE_VM_DELETE<SP><request-ID><SP><cred-file><SP><subscription><CRLF>vmName")
            continue
        print("S")
    elif(command.upper() == "AZURE_VM_LIST"):
        if (len(parts) < 5):
            print("E Invalid number of command args. Usage: AZURE_VM_LIST<SP><request-ID><SP><cred-file><SP><subscription><CRLF>")
            continue
        if not agce.HandleCommand(parts):
            print("E Invalid command args. Usage: AZURE_VM_LIST<SP><request-ID><SP><cred-file><SP><subscription><CRLF>")
            continue
        print("S")
    else:
        print("E Unsupported command: To see a list of valid commands use COMMANDS");
# TODO: What about invalid command strings?

##### END MAIN PROGRAM ##############

    
    

    
