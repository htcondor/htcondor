import sys
import platform

import AzureGAHPLib

double_line_break = "{}"

class GahpMinParamsCountBuilder():
    def __init__(self):
        self.create_keyvault = 8
        self.create_vm = 5
        self.create_vmss = 5
        self.delete_vm = 5
        self.delete_vmss = 5
        self.list_vm = 4
        self.ping_azure = 4
        self.restart_vmss = 6
        self.scale_vmss = 7
        self.start_vmss = 6
        self.stop_vmss = 6

class GahpCommandUsageBuilder():
    def __init__(self):
        self.create_keyvault = "<SP>name=<Resource group name> location=<Region> sku=<SKU> users=<user_id0>,<user_id1>,..."
        self.ping_azure = ""
        self.create_vmss = "<SP>name=vmssName<SP>location=region<SP>size=vmssSize<SP>image=osImage<SP>[dataDisks=disk1,disk2,...<SP>adminUsername=userName<SP>key=sshKeyOrPassword<SP>vnetName=azureVnetName<SP>publicIPAddress=vmPublicIpAddress<SP>customData=textOrScriptFile<SP>tag=vmTagName]<SP>nodecount=VmNodes"
        self.delete_vmss = "<SP>vmssName"
        self.restart_vmss = "<SP>groupName<SP>vmssName"
        self.scale_vmss = "<SP>vmssName<SP>requiredVmssNodeCount[Between 0 and 100]"
        self.start_vmss = "<SP>groupName<SP>vmssName"
        self.stop_vmss = "<SP>groupName<SP>vmssName"
        self.create_vm = "<SP>name=vmName<SP>location=region<SP>size=vmSize<SP>image=osImage<SP>[dataDisks=disk1,disk2,...<SP>adminUsername=userName<SP>key=sshKeyOrPassword<SP>vnetName=azureVnetName<SP>publicIPAddress=vmPublicIpAddress<SP>customData=textOrScriptFile<SP>tag=vmTagName]"
        self.delete_vm = "<SP>vmName"
        self.list_vm = ""

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
    cmds = AzureGAHPLib.GahpCommandBuilder()
    cmds_usage = GahpCommandUsageBuilder()
    
    for i in range(len(input_command_params)):
        input_command_params[i] = input_command_params[i].replace(
            unique_str, AzureGAHPLib.space_separator)

    input_command = input_command_params[0]
    if(input_command.upper() == cmds.commands):
        all_commands = vars(cmds)
        sys.stdout.write("{} ".format(s_alphabet))
        for command in all_commands.values():
            sys.stdout.write("{} ".format(command))
        sys.stdout.write("{}".format(double_line_break))
        continue
    elif(input_command.upper() == cmds.version):
        print("{} {}".format(s_alphabet, version))
        continue
    elif(input_command.upper() == cmds.results):
        agce.deque_all_results_and_print()
        continue
    elif(input_command.upper() == cmds.quit):
        print(s_alphabet)
        sys.exit()
    elif(input_command.upper() == cmds.create_keyvault):
        usage = "{}{}{}<CRLF>".format(
            cmds.create_keyvault, common_paramters, 
            cmds_usage.create_keyvault)
        if (is_error_with_input_command_params(
                input_command_params, usage, 
                min_params_count.create_keyvault)
            ):
            continue
        print(s_alphabet)
    elif(input_command.upper() == cmds.ping_azure):
        usage = "{}{}{}<CRLF>".format(
            cmds.ping_azure, common_paramters, 
            cmds_usage.ping_azure)
        if (is_error_with_input_command_params(
                input_command_params, usage, 
                min_params_count.ping_azure)
            ):
            continue
        print(s_alphabet)
    elif(input_command.upper() == cmds.create_vmss):
        usage = "{}{}{}<CRLF>".format(
            cmds.create_vmss, common_paramters, 
            cmds_usage.create_vmss)
        if (is_error_with_input_command_params(
                input_command_params, usage, 
                min_params_count.create_vmss)
            ):
            continue
        print(s_alphabet)
    elif(input_command.upper() == cmds.delete_vmss):
        usage = "{}{}{}<CRLF>".format(
            cmds.delete_vmss, common_paramters, 
            cmds_usage.delete_vmss)
        if (is_error_with_input_command_params(
                input_command_params, usage, 
                min_params_count.delete_vmss)
            ):
            continue
        print(s_alphabet)
    elif(input_command.upper() == cmds.scale_vmss):
        usage = "{}{}{}<CRLF>".format(
            cmds.scale_vmss, common_paramters, 
            cmds_usage.scale_vmss)
        if (is_error_with_input_command_params(
                input_command_params, usage, 
                min_params_count.scale_vmss)
            ):
            continue
        print(s_alphabet)
    elif(input_command.upper() == cmds.start_vmss 
         or input_command.upper() == cmds.stop_vmss
         or input_command.upper() == cmds.restart_vmss):
        usage = "{}{}{}<CRLF>".format(
            cmds.start_vmss, common_paramters, 
            cmds_usage.start_vmss)
        if (is_error_with_input_command_params(
                input_command_params, usage, 
                min_params_count.start_vmss)
            ):
            continue
        print(s_alphabet)
    elif(input_command.upper() == cmds.create_vm):
        usage = "{}{}{}<CRLF>".format(
            cmds.create_vm, common_paramters, 
            cmds_usage.create_vm)
        if (is_error_with_input_command_params(
                input_command_params, usage, 
                min_params_count.create_vm)
            ):
            continue
        print(s_alphabet)
    elif(input_command.upper() == cmds.delete_vm):
        usage = "{}{}{}<CRLF>".format(
            cmds.delete_vm, common_paramters, 
            cmds_usage.delete_vm)
        if (is_error_with_input_command_params(
                input_command_params, usage, 
                min_params_count.delete_vm)
            ):
            continue
        print(s_alphabet)
    elif(input_command.upper() == cmds.list_vm):
        usage = "{}{}{}<CRLF>".format(
            cmds.list_vm, common_paramters, 
            cmds_usage.list_vm)
        if (is_error_with_input_command_params(
                input_command_params, usage, 
                min_params_count.list_vm)
            ):
            continue
        print(s_alphabet)
    else:
        print("E Unsupported command: To see a list of valid commands use COMMANDS");

##### END MAIN PROGRAM ##############
