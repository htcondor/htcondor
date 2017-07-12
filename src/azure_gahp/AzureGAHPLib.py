import sys
import os
import threading
from azure.common.credentials import ServicePrincipalCredentials
from azure.mgmt.resource import ResourceManagementClient
from azure.mgmt.storage import StorageManagementClient
from azure.mgmt.network import NetworkManagementClient
from azure.mgmt.compute import ComputeManagementClient
from collections import deque

########### CLASSES ##################
class AzureGAHPCommandInfo:
    command = ""
    request_id = 0
    cred_file = ""
    subscription = ""
    cmdParams = None

class AzureGAHPCommandExecThread(threading.Thread):
    def __init__(self, ce):
        threading.Thread.__init__(self)
        self.ce = ce

    def run(self):
        self.ce.execute_command()

class AzureGAHPCommandExec():
    commandQ = None
    resultQ = None
    commandQLock = None
    resultQLock = None

    def __init__(self):
        self.commandQ = deque()  
        self.resultQ = deque()
        self.commandQLock = threading.Lock()
        self.resultQLock = threading.Lock()

    def read_credentials_from_file(self, fileName):
        dnary = dict()
        with open(fileName, 'r') as f:
            for line in f:
                if not line.strip():
                   continue
                nvp = line.split(" ")
                if(not nvp):
                    return None
                if(len(nvp) < 2):
                    return None
                nvp[0] = nvp[0].strip()
                nvp[1] = nvp[1].strip()
                if((len(nvp[0]) < 1) or (len(nvp[1]) < 1)):
                    return None
                dnary[nvp[0]] = nvp[1]
        return dnary        

    def write_message(self, msg):
        return
        #sys.stderr.write(msg + "\n")

    def create_credentials_from_file(self, fileName):
        dnary = self.read_credentials_from_file(fileName)
        cred = self.create_service_credentials(dnary)
        self.write_message("credentials created\r\n")
        return cred

    def create_service_credentials(self, dnary):
        self.write_message('creating credentials\r\n')
        credentials = ServicePrincipalCredentials(client_id = dnary["client_id"], secret = dnary["secret"], tenant = dnary["tenant_id"])
        return credentials

    def create_client_libraries(self, credentials, subscription_id):
        self.write_message('creating client libraries ' + subscription_id + "\r\n")
        compute_client = ComputeManagementClient(
            credentials,
            subscription_id
        )
        network_client = NetworkManagementClient(
            credentials,
            subscription_id
        )
        resource_client = ResourceManagementClient(
            credentials,
            subscription_id
        )
        storage_client = StorageManagementClient(
            credentials,
            subscription_id
        )

        client_libs = {
            'compute_client': compute_client,
            'network_client': network_client,
            'resource_client': resource_client,
            'storage_client': storage_client}

        self.write_message('creating client libraries: complete' + "\r\n")
        return client_libs


    def create_nic(self, network_client, location, groupName, vnetName, subnetName, nicName, ipConfigName):
        self.write_message('creating vnet' + "\r\n")
        async_vnet_creation = network_client.virtual_networks.create_or_update(
            groupName,
            vnetName,
            {
                'location': location,
                'address_space': {
                    'address_prefixes': ['10.0.0.0/16']
                }
            }
        )
        async_vnet_creation.wait()
        # Create Subnet
        self.write_message('creating subnet' + "\r\n")
        async_subnet_creation = network_client.subnets.create_or_update(
            groupName,
            vnetName,
            subnetName,
            {'address_prefix': '10.0.0.0/24'}
        )
        subnet_info = async_subnet_creation.result()
        # Create NIC
        self.write_message('Creating NIC' + "\r\n")
        async_nic_creation = network_client.network_interfaces.create_or_update(
            groupName,
            nicName,
            {
                'location': location,
                'ip_configurations': [{
                    'name': ipConfigName,
                    'subnet': {
                        'id': subnet_info.id
                    }
                }]
            }
        )
        return async_nic_creation.result()

    def get_ssh_key(self, key_info):
        if not os.path.exists(key_info): 
            return key_info #raw key
        ssh_key_path = os.path.expanduser(key_info)
        # Will raise if file not exists or not enough permission
        with open(ssh_key_path, 'r') as pub_ssh_file_fd:
            return pub_ssh_file_fd.read()

    def create_vm_parameters(self, location, vmName, vmSize, storageAccountName, userName, key_info, osDiskName, nic_id, vm_reference):
        key = self.get_ssh_key(key_info)
        return {
            'location': location,
            'os_profile': {
                'computer_name': vmName,
                'admin_username': userName,
                'admin_password': key
                #'sshKeyData': key
            },
            'hardware_profile': {
                'vm_size': vmSize
            },

            'storage_profile': {
                'image_reference': {
                    'publisher': vm_reference['publisher'],
                    'offer': vm_reference['offer'],
                    'sku': vm_reference['sku'],
                    'version': vm_reference['version']
                },
                'os_disk': {
                    'name': osDiskName,
                    'caching': 'None',
                    'create_option': 'fromImage',
                    'vhd': {
                        'uri': 'https://{}.blob.core.windows.net/vhds/{}.vhd'.format(
                            storageAccountName, vmName)
                    }
                },
            },
            'network_profile': {
                'network_interfaces': [{
                    'id': nic_id,
                }]
            },
        }

    def create_vm(self, compute_client, network_client, resource_client, storage_client, location, groupName, vnetName, subnetName, 
osDiskName, storageAccountName, ipConfigName, nicName, userName, key, vmName, vmSize, vmRef):
        self.write_message('creating resource group' + "\r\n")
        resource_client.resource_groups.create_or_update(groupName, {'location':location})
        self.write_message('creating storage account' + "\r\n")
        storage_async_operation = storage_client.storage_accounts.create(
                groupName,
                storageAccountName,
                {
                    'sku': {'name': 'standard_lrs'},
                    'kind': 'storage',
                    'location': location
                }
            )
        storage_async_operation.wait()
        self.write_message('Creating network' + "\r\n")
        nic = self.create_nic(network_client, location, groupName, vnetName, subnetName, nicName, ipConfigName)
        self.write_message('creating vm' + "\r\n")
        vm_parameters = self.create_vm_parameters(location, vmName, vmSize, storageAccountName, userName, key, osDiskName, nic.id, vmRef)
        async_vm_creation = compute_client.virtual_machines.create_or_update(groupName, vmName, vm_parameters)
        async_vm_creation.wait()
        self.write_message('vm creation complete' + "\r\n")

    def delete_vm(self, compute_client, groupName, vmName):
        self.write_message('deleting vm' + "\r\n")
        async_vm_delete = compute_client.virtual_machines.delete(groupName, vmName)
        async_vm_delete.wait()

    def list_vm(self, compute_client, groupName, vmName):
        #compute_client.virtual_machines.get(groupName, groupName+"vm", expand='instanceView')
        #compute_client.virtual_machines.get_with_instance_view(groupName, groupName+"vm")
        vm = compute_client.virtual_machines.get(groupName, groupName+"vm", expand='instanceView')
        statuses = vm.instance_view.statuses
        numStatus = len(statuses)
        str_status_list = []
        
        for i in range(numStatus):
            str_status_list.append(statuses[i].code)
            if(i != (numStatus - 1)):
                str_status_list.append(",")
        
        return (vm.name + " " + ''.join(str_status_list))
            
    def list_rg(self, compute_client, groupName):
        self.write_message('listing vms in RG: ' + groupName + "\r\n")  
        vms_info_list = []
        count = 0
        for vm in compute_client.virtual_machines.list(groupName):
            vmInfo = self.list_vm(compute_client, groupName, vm.name)
            if(count != 0):
                vms_info_list.append("\r\n")
            vms_info_list.append(vmInfo)
            count = count + 1
        
        return vms_info_list

    def delete_rg(self, resource_client, groupName):
        self.write_message('deleting resource group' + "\r\n")
        delete_async_operation = resource_client.resource_groups.delete(groupName)
        delete_async_operation.wait()
        self.write_message('resource group deleted' + "\r\n")

    def QueueCommand(self, ci):
        self.commandQLock.acquire()
        try:
            self.commandQ.appendleft(ci)
        finally:
            self.commandQLock.release()

    def DequeueCommand(self):
        self.commandQLock.acquire()
        try:
            ci = self.commandQ.pop()
        finally:    
            self.commandQLock.release()
        return ci
   
    def QueueResult(self, reqId, message):
        self.resultQLock.acquire()
        try:
            self.resultQ.appendleft("" + str(reqId) + " " + message)
        finally:    
            self.resultQLock.release()

    def DequeAllResultsAndPrint(self):
        if not self.resultQ:
            sys.stdout.write("S 0\r\n")
            return

        self.resultQLock.acquire()
        try:
            sys.stdout.write("S " + str(len(self.resultQ)) +"\r\n" )
            while self.resultQ:
                result = self.resultQ.pop()
                sys.stdout.write(result + "\r\n")
        finally:
            self.resultQLock.release()

    def execute_command(self):
        ci = self.DequeueCommand()
        
        if(ci.command.upper() == "AZURE_PING"):
            try:
                #sample_create_vm()
                credentials = self.create_credentials_from_file(ci.cred_file)
                if(credentials is None):
                    self.QueueResult(ci.request_id, "Error reading or creating credentials")
                    return
                client_libs = self.create_client_libraries(credentials, ci.subscription)
                if(client_libs is None):
                    self.QueueResult(ci.request_id, "Error creating client libraries")
                    return
                self.QueueResult(ci.request_id, "NULL")
            except Exception as e:
                self.QueueResult(ci.request_id, str(e.args[0])) 
        elif(ci.command.upper() == "AZURE_VM_CREATE"):
            try:
                #sample_create_vm()
                credentials = self.create_credentials_from_file(ci.cred_file)
                if(credentials is None):
                    self.QueueResult(ci.request_id, "Error reading or creating credentials")
                    return
                client_libs = self.create_client_libraries(credentials, ci.subscription)
                if(client_libs is None):
                    self.QueueResult(ci.request_id, "Error creating client libraries")
                    return
                self.create_vm(client_libs["compute_client"], client_libs["network_client"], client_libs["resource_client"], client_libs["storage_client"], ci.cmdParams["location"], ci.cmdParams["name"], ci.cmdParams["vnetName"], ci.cmdParams["subnetName"], ci.cmdParams["osDiskName"], ci.cmdParams["storageAccountName"], ci.cmdParams["ipConfigName"], ci.cmdParams["nicName"], ci.cmdParams["adminUsername"], ci.cmdParams["key"], ci.cmdParams["vmName"], ci.cmdParams["size"], ci.cmdParams["vmRef"])
                self.QueueResult(ci.request_id, "NULL")
            except Exception as e:
                self.QueueResult(ci.request_id, str(e.args[0])) #todo: vm_id, ip address
        elif(ci.command.upper() == "AZURE_VM_DELETE"):
            try:
                credentials = self.create_credentials_from_file(ci.cred_file)
                if(credentials is None):
                    self.QueueResult(ci.request_id, "Error reading or creating credentials")
                    return
                client_libs = self.create_client_libraries(credentials, ci.subscription)
                if(client_libs is None):
                    self.QueueResult(ci.request_id, "Error creating client libraries")
                    return
                self.delete_rg(client_libs["resource_client"], ci.cmdParams["vmName"]) # TODO: delete_vm needs 2 parameters: RG and VmName
                self.QueueResult(ci.request_id, "NULL")
            except Exception as e:
                self.QueueResult(ci.request_id, str(e.args[0])) #todo: vm_id, ip address 
        elif(ci.command.upper() == "AZURE_VM_LIST"):
            try:
                credentials = self.create_credentials_from_file(ci.cred_file)
                if(credentials is None):
                    self.QueueResult(ci.request_id, "Error reading or creating credentials")
                    return
                client_libs = self.create_client_libraries(credentials, ci.subscription)
                if(client_libs is None):
                    self.QueueResult(ci.request_id, "Error creating client libraries")
                    return
                vms_info_list = self.list_rg(client_libs["compute_client"], ci.cmdParams["vmName"]) # TODO: list_vm needs 2 parameters: RG and VmName
                result = "NULL " + str(len(vms_info_list)) + "\r\n" + ''.join(vms_info_list) 
                self.QueueResult(ci.request_id, result)
            except Exception as e:
                self.write_message("Error listing VMs: " + str(e.args[0]) + "\r\n")
                self.QueueResult(ci.request_id, str(e.args[0])) #todo: vm_id, ip address sys.exc_info()[0]

    def get_create_vm_args(self, cmdParts):
        vmRef = {
            "linux-ubuntu-latest": {"publisher": "Canonical", "offer": "UbuntuServer", "sku": "16.04.0-LTS", "version": "latest"},
            "windows-server-latest": {"publisher": "MicrosoftWindowsServerEssentials", "offer": "WindowsServerEssentials", "sku": "WindowsServerEssentials","version": "latest"}
        }
        createVMArgNames = ["name","location","image", "size", "dataDisks","adminUsername","key","vnetName","publicIPAddress","customData","tag"]
        dnary  = dict()
        
        nvpStrings = cmdParts[4].split(",")
        for nvpString in nvpStrings:
            nvp = nvpString.split("=")
            if( not nvp):
                self.write_message('Empty args' + "\r\n")
                return None
            if(len(nvp) < 2):
                self.write_message('Insufficient args:' + nvpString + "\r\n")
                return None
            if(nvp[0].upper() not in (val.upper() for val in createVMArgNames)):
                self.write_message('Unrecogonized paramter name: ' + nvp[0] + "\r\n")
                return None
            if(nvp[0].lower() == "image"):
                if(nvp[1].lower() == "linux-ubuntu-latest"):
                    dnary["vmRef"] = vmRef[nvp[1]]
                elif(nvp[1].lower() == "windows-server-latest"):
                    dnary["vmRef"] = vmRef[nvp[1]]
                else:
                    self.write_message('Unrecognized image: ' + nvp[1] + "\r\n")
                    return None
            else:
                dnary[nvp[0]] = nvp[1]
        
        if((not "name" in dnary) or (not "location" in dnary) or (not "size" in dnary) or (not "vmRef" in dnary)):
            self.write_message('Missing mandatory arg' + "\r\n")
            return None

        dnary["vmName"] = dnary["name"] + "vm"

        if(not "adminUsername" in dnary):
            dnary["adminUsername"] = "superuser"

        if(not "key" in dnary):
            dnary["key"] = "Super1234!@#$"

        if(not "vnetName" in dnary):
            dnary["vnetName"] = dnary["name"] + "vnet"

        subnetName =  dnary["name"] + "subnet" #"aa1rangassubnet"
        osDiskName = dnary["name"] + "osdisk" #"aa1rangasosdisk"
        storageAccountName = dnary["name"] + "sa" #"aa1rangassa"
        ipConfigName = dnary["name"] + "ipconfig" #"aa1rangasipconfig"
        nicName = dnary["name"] + "nic" #"aa1rangasnic"

        dnary["subnetName"] = subnetName
        dnary["osDiskName"] = osDiskName
        dnary["storageAccountName"] = storageAccountName
        dnary["ipConfigName"] = ipConfigName
        dnary["nicName"] = nicName
        
        return dnary

    def HandleCommand(self, cmdParts):
        ci = AzureGAHPCommandInfo()
        ci.command = cmdParts[0]
        ci.request_id = cmdParts[1]
        ci.cred_file = cmdParts[2]
        ci.subscription = cmdParts[3]
        if(ci.command.upper() == "AZURE_VM_CREATE"):
            cmdParams = self.get_create_vm_args(cmdParts)
            if(cmdParams is None):
                return False
            ci.cmdParams = cmdParams
        elif(ci.command.upper() == "AZURE_VM_DELETE"):
            cmdParams = dict()
            cmdParams["vmName"] = cmdParts[4]
            ci.cmdParams = cmdParams
        elif(ci.command.upper() == "AZURE_VM_LIST"):
            cmdParams = dict()
            cmdParams["vmName"] = cmdParts[4]
            ci.cmdParams = cmdParams
        elif(ci.command.upper() == "AZURE_PING"):
            ci.cmdParams = None
        else:
            cmdParams = None
        self.QueueCommand(ci)
        t = AzureGAHPCommandExecThread(self)
        t.start()
        return True
    #### END FUNCTIONS ################################
