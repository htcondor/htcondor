import sys
import platform

import AzureGAHPLib

class GahpCommandBuilder():
    def __init__(self):
        self.azure_keyvault_create = "AZURE_KEYVAULT_CREATE"
        self.azure_ping = "AZURE_PING"
        self.azure_vmss_create = "AZURE_VMSS_CREATE"
        self.azure_vmss_delete = "AZURE_VMSS_DELETE"
        self.azure_vmss_restart = "AZURE_VMSS_RESTART"
        self.azure_vmss_scale = "AZURE_VMSS_SCALE"
        self.azure_vmss_start = "AZURE_VMSS_START"
        self.azure_vmss_stop = "AZURE_VMSS_STOP"
        self.azure_vm_create = "AZURE_VM_CREATE"
        self.azure_vm_delete = "AZURE_VM_DELETE"
        self.azure_vm_list = "AZURE_VM_LIST"
        self.commands = "COMMANDS"
        self.quit = "QUIT"
        self.results = "RESULTS"
        self.version = "VERSION"

class GahpMinParamsCountBuilder():
    def __init__(self):
        self.azure_keyvault_create = 8
        self.azure_ping = 4
        self.azure_vmss_create = 5
        self.azure_vmss_delete = 5
        self.azure_vmss_restart = 5
        self.azure_vmss_scale = 5
        self.azure_vmss_start = 5
        self.azure_vmss_stop = 5
        self.azure_vm_create = 5
        self.azure_vm_delete = 5
        self.azure_vm_list = 4

class GahpCommandUsageBuilder():
    def __init__(self):
        self.azure_keyvault_create = "<SP>name=<Resource group name> location=<Region> sku=<SKU> users={{‘<user_id0>’, ‘<user_id1>’, …}}"
        self.azure_ping = ""
        self.azure_vmss_create = "<SP>name=vmssName<SP>location=region<SP>size=vmssSize<SP>image=osImage<SP>[dataDisks=disk1,disk2,...<SP>adminUsername=userName<SP>adminSshKey=sshKey<SP>vnetName=azureVnetName<SP>publicIPAddress=vmPublicIpAddress<SP>customData=textOrScriptFile<SP>tag=vmTagName]<SP>nodecount=VmNodes"
        self.azure_vmss_delete = "<SP>vmssName"
        self.azure_vmss_restart = "<SP>groupName<SP>vmssName"
        self.azure_vmss_scale = "<SP>vmssName<SP>vmssNodeCount"
        self.azure_vmss_start = "<SP>groupName<SP>vmssName"
        self.azure_vmss_stop = "<SP>groupName<SP>vmssName"
        self.azure_vm_create = "<SP>name=vmName<SP>location=region<SP>size=vmSize<SP>image=osImage<SP>[dataDisks=disk1,disk2,...<SP>adminUsername=userName<SP>adminSshKey=sshKey<SP>vnetName=azureVnetName<SP>publicIPAddress=vmPublicIpAddress<SP>customData=textOrScriptFile<SP>tag=vmTagName]"
        self.azure_vm_delete = "<SP>vmName"
        self.azure_vm_list = ""

def is_error_with_input_command_params(
        input_command_params, usage, min_params_count):
        
    invalid_args_error = "E Invalid command args. Usage:"
    invalid_number_of_args_error = "E Invalid number of command args. Usage:"
    if (len(input_command_params) < min_params_count):
        print("{} {}".format(
            invalid_number_of_args_error, usage)
                )
        return True
    if not agce.handle_command(input_command_params):
        print("{} {}".format(
            invalid_args_error, usage)
                )
        return True
    return False

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
    input_command_params = input_line.split(
        AzureGAHPLib.space_separator)
    # Validate input parameters
    if (len(input_command_params) < 1):
        print("E Invalid command")
        continue
    
    s_alphabet = "S"
    common_paramters = "<SP><request-ID><SP><cred-file><SP><subscription>"

    min_params_count = GahpMinParamsCountBuilder()
    gahp_commands = GahpCommandBuilder()
    gahp_commands_usage = GahpCommandUsageBuilder()
    
    for i in range(len(input_command_params)):
        input_command_params[i] = input_command_params[i].replace(
            unique_str, AzureGAHPLib.space_separator)

    input_command = input_command_params[0]
    if(input_command.upper() == gahp_commands.commands):
        print("{}".format(s_alphabet))
        all_commands = vars(gahp_commands)
        for command in all_commands.values():
            print("{} ".format(command))
        continue
    elif(input_command.upper() == gahp_commands.version):
        print("{} {}".format(s_alphabet, version))
        continue
    elif(input_command.upper() == gahp_commands.results):
        agce.deque_all_results_and_print()
        continue
    elif(input_command.upper() == gahp_commands.quit):
        print(s_alphabet)
        sys.exit()
    elif(input_command.upper() == gahp_commands.azure_keyvault_create):
        usage = "{}{}{}<CRLF>".format(
            gahp_commands.azure_keyvault_create, common_paramters, 
            gahp_commands_usage.azure_keyvault_create)
        if (is_error_with_input_command_params(
                input_command_params, usage, 
                min_params_count.azure_keyvault_create)
            ):
            continue
        print(s_alphabet)
    elif(input_command.upper() == gahp_commands.azure_ping):
        usage = "{}{}{}<CRLF>".format(
            gahp_commands.azure_ping, common_paramters, 
            gahp_commands_usage.azure_ping)
        if (is_error_with_input_command_params(
                input_command_params, usage, 
                min_params_count.azure_ping)
            ):
            continue
        print(s_alphabet)
    elif(input_command.upper() == gahp_commands.azure_vmss_create):
        usage = "{}{}{}<CRLF>".format(
            gahp_commands.azure_vmss_create, common_paramters, 
            gahp_commands_usage.azure_vmss_create)
        if (is_error_with_input_command_params(
                input_command_params, usage, 
                min_params_count.azure_vmss_create)
            ):
            continue
        print(s_alphabet)
    elif(input_command.upper() == gahp_commands.azure_vmss_delete):
        usage = "{}{}{}<CRLF>".format(
            gahp_commands.azure_vmss_delete, common_paramters, 
            gahp_commands_usage.azure_vmss_delete)
        if (is_error_with_input_command_params(
                input_command_params, usage, 
                min_params_count.azure_vmss_delete)
            ):
            continue
        print(s_alphabet)
    elif(input_command.upper() == gahp_commands.azure_vmss_scale):
        usage = "{}{}{}<CRLF>".format(
            gahp_commands.azure_vmss_scale, common_paramters, 
            gahp_commands_usage.azure_vmss_scale)
        if (is_error_with_input_command_params(
                input_command_params, usage, 
                min_params_count.azure_vmss_scale)
            ):
            continue
        print(s_alphabet)
    elif(input_command.upper() == gahp_commands.azure_vmss_start 
         or input_command.upper() == gahp_commands.azure_vmss_stop
         or input_command.upper() == gahp_commands.azure_vmss_restart):
        usage = "{}{}{}<CRLF>".format(
            gahp_commands.azure_vmss_start, common_paramters, 
            gahp_commands_usage.azure_vmss_start)
        if (is_error_with_input_command_params(
                input_command_params, usage, 
                min_params_count.azure_vmss_start)
            ):
            continue
        print(s_alphabet)
    elif(input_command.upper() == gahp_commands.azure_vm_create):
        usage = "{}{}{}<CRLF>".format(
            gahp_commands.azure_vm_create, common_paramters, 
            gahp_commands_usage.azure_vm_create)
        if (is_error_with_input_command_params(
                input_command_params, usage, 
                min_params_count.azure_vm_create)
            ):
            continue
        print(s_alphabet)
    elif(input_command.upper() == gahp_commands.azure_vm_delete):
        usage = "{}{}{}<CRLF>".format(
            gahp_commands.azure_vm_delete, common_paramters, 
            gahp_commands_usage.azure_vm_delete)
        if (is_error_with_input_command_params(
                input_command_params, usage, 
                min_params_count.azure_vm_delete)
            ):
            continue
        print(s_alphabet)
    elif(input_command.upper() == gahp_commands.azure_vm_list):
        usage = "{}{}{}<CRLF>".format(
            gahp_commands.azure_vm_list, common_paramters, 
            gahp_commands_usage.azure_vm_list)
        if (is_error_with_input_command_params(
                input_command_params, usage, 
                min_params_count.azure_vm_list)
            ):
            continue
        print(s_alphabet)
    else:
        print("E Unsupported command: To see a list of valid commands use COMMANDS");

##### END MAIN PROGRAM ##############
