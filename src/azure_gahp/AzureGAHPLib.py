import sys
import os
import threading
import base64
from azure.common.credentials import ServicePrincipalCredentials
from azure.mgmt.resource import ResourceManagementClient
from azure.mgmt.storage import StorageManagementClient
from azure.mgmt.network import NetworkManagementClient
from azure.mgmt.compute import ComputeManagementClient
from collections import deque


spaceSeparator = " "
singleBackSlashSpaceSeparator = "\\ "
doubleBackSlashSeparator = "\\\\"
singleBackSlashSeparator = "\\"

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
    separator = ' '
    newLineSeparator = "\r\n"
    vmResultSeparator = " "

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
        compute_client = ComputeManagementClient(credentials,
            subscription_id)
        network_client = NetworkManagementClient(credentials,
            subscription_id)
        resource_client = ResourceManagementClient(credentials,
            subscription_id)
        storage_client = StorageManagementClient(credentials,
            subscription_id)

        client_libs = {
            'compute_client': compute_client,
            'network_client': network_client,
            'resource_client': resource_client,
            'storage_client': storage_client}

        self.write_message('creating client libraries: complete' + "\r\n")
        return client_libs


    def create_nic(self, network_client, location, groupName, vnetName, subnetName, nicName, ipConfigName):
        self.write_message('creating vnet' + "\r\n")
        async_vnet_creation = network_client.virtual_networks.create_or_update(groupName,
            vnetName,
            {
                'location': location,
                'address_space': {
                    'address_prefixes': ['10.0.0.0/16']
                }
            })
        async_vnet_creation.wait()
        # Create Subnet
        self.write_message('creating subnet' + "\r\n")
        async_subnet_creation = network_client.subnets.create_or_update(groupName,
            vnetName,
            subnetName,
            {'address_prefix': '10.0.0.0/24'})
        subnet_info = async_subnet_creation.result()
        # Create public ip
        self.write_message('creating public ip' + "\r\n")
        async_public_ip_creation = network_client.public_ip_addresses.create_or_update(groupName,groupName+'pip',{
                    'location': location,
                    'public_ip_allocation_method': 'Dynamic'
                })
        public_ip_info = async_public_ip_creation.result()
        # Create NIC
        self.write_message('Creating NIC' + "\r\n")
        
        async_nic_creation = network_client.network_interfaces.create_or_update(groupName,
            nicName,
            {
                'location': location,
                'ip_configurations': [{
                    'name': ipConfigName,
                    'private_ip_allocation_method': 'Dynamic',
                    'public_ip_address': {
                    'id': public_ip_info.id
                    },
                    'subnet': {
                        'id': subnet_info.id
                    }
                }]
            })
        return async_nic_creation.result()

    def get_ssh_key(self, key_info):
        if not os.path.exists(key_info): 
            return key_info #raw key
        ssh_key_path = os.path.expanduser(key_info)
        # Will raise if file not exists or not enough permission
        with open(ssh_key_path, 'r') as pub_ssh_file_fd:
            return pub_ssh_file_fd.read()

    def get_base64(self, data):
        if not os.path.exists(data): 
            return base64.b64encode(data)
        filePath = os.path.expanduser(data)
        # Will raise if file not exists or not enough permission
        with open(filePath, 'r') as customData:
            return base64.b64encode(bytes(customData.read(), 'utf-8')).decode("utf-8")

    def create_vm_parameters(self,location, vmName, vmSize, storageAccountName, userName, key_info, osDiskName, nic_id, vm_reference,osType,tag,customData,dataDisks):
        customImage = 0        
        if "https://" in vm_reference:
            customImage = 1
            storageAccount = vm_reference.split('/')[2].split('.')[0]        
        key = self.get_ssh_key(key_info)        
        params = {
            'location': location,
            'os_profile': {
                'computer_name': vmName,
                'admin_username': userName
            },
            'hardware_profile': {
                'vm_size': vmSize
            },
            'storage_profile': {               
                'os_disk': {
                    'name': osDiskName,
                    'caching': 'None',
                    'create_option': 'fromImage',
                    'vhd': {
                        'uri': 'https://{}.blob.core.windows.net/vhds/{}.vhd'.format(storageAccountName, vmName)
                    }
                }
            },
            'network_profile': {
                'network_interfaces': [{
                    'id': nic_id,
                }]
            },
        }
        linuxConf = {
                "disable_password_authentication": True,
                "ssh": {
                     "public_keys": [{
                          "path": "/home/{}/.ssh/authorized_keys".format(userName),
                          "key_data": key
                     }]
                }
             }
        tags = {
                "Group": tag
              }

        if customImage == 0:
            imageReference = {
                    'publisher': vm_reference['publisher'],
                    'offer': vm_reference['offer'],
                    'sku': vm_reference['sku'],
                    'version': vm_reference['version']
                }
            params['storage_profile']['image_reference'] = imageReference            
        else:
            params['storage_profile']['os_disk']['caching'] = 'ReadWrite'
            params['storage_profile']['os_disk']['os_type'] = osType
            params['storage_profile']['os_disk']['vhd']['uri'] = 'https://{}.blob.core.windows.net/vhds/{}.vhd'.format(storageAccount, osDiskName)
            imageUri = {
            'uri': vm_reference.strip()
                    }
            params['storage_profile']['os_disk']['image'] = imageUri  

        if os.path.exists(key_info) and osType == 'Linux': 
            params['os_profile']['linux_configuration'] = linuxConf
        else:  
            params['os_profile']['admin_password'] = key

        if tag != "":
            params['tags'] = tags

        if customData != "":
            base64CustomData = self.get_base64(customData)
            params['os_profile']['custom_data'] = base64CustomData
     
        data_Disks = []
        if(dataDisks != ''):
            ddArr = dataDisks.split(',')
            for index,val in enumerate(ddArr):    
                dd = {
                        'name': 'datadisk{}'.format(index),
                        'disk_size_gb': val,
                        'lun': index,
                        'vhd': {
                            'uri' : "http://{}.blob.core.windows.net/vhds/datadisk{}.vhd".format(
                                storageAccountName,index)
                        },
                        'create_option': 'Empty'
                      };
                data_Disks.append(dd)
            params['storage_profile']['data_disks'] = data_Disks

        return params

    def create_vm(self, compute_client, network_client, resource_client, storage_client, location, groupName, vnetName, subnetName, 
osDiskName, storageAccountName, ipConfigName, nicName, userName, key, vmName, vmSize, vmRef,osType,tag,customData,dataDisks):
        #nics = list(network_client.network_interfaces.list(
        #            groupName
        #        ))
        self.write_message('creating resource group' + "\r\n")
        resource_client.resource_groups.create_or_update(groupName, {'location':location})
        self.write_message('creating storage account' + "\r\n")
        storageAccountName = storageAccountName.lower()
        storage_async_operation = storage_client.storage_accounts.create(groupName,
                storageAccountName,
                {
                    'sku': {'name': 'standard_lrs'},
                    'kind': 'storage',
                    'location': location
                })
        storage_async_operation.wait()
        self.write_message('Creating network' + "\r\n")        
        nic = self.create_nic(network_client, location, groupName, vnetName, subnetName, nicName, ipConfigName)
        
        self.write_message('creating vm' + "\r\n")        
        vm_parameters = self.create_vm_parameters(location, vmName, vmSize, storageAccountName, userName, key, osDiskName, nic.id, vmRef,osType,tag,customData,dataDisks)
        async_vm_creation = compute_client.virtual_machines.create_or_update(groupName, vmName, vm_parameters)
        async_vm_creation.wait()
        self.write_message('vm creation complete' + "\r\n")

    def delete_vm(self, compute_client, groupName, vmName):
        self.write_message('deleting vm' + "\r\n")
        async_vm_delete = compute_client.virtual_machines.delete(groupName, vmName)
        async_vm_delete.wait()

    def list_vm(self, compute_client, groupName, vmName):
        vm = compute_client.virtual_machines.get(groupName, vmName, expand='instanceView')        
        statuses = vm.instance_view.statuses
        numStatus = len(statuses)
        str_status_list = []
        
        for i in range(numStatus):
            str_status_list.append(statuses[i].code)
            if(i != (numStatus - 1)):
                str_status_list.append(",")
        
        return (self.Escape(groupName)+ spaceSeparator + self.Escape(''.join(str_status_list))+ spaceSeparator)

    def list_rg(self, compute_client,groupName,tag):
        self.write_message('listing vms in RG: ' + groupName + "\r\n")  
        vms_info_list = []
        vmList = []
        count = 0
        if(groupName != ''):
            vmList = compute_client.virtual_machines.list(groupName)
            for vm in vmList:
                vmInfo = self.list_vm(compute_client, groupName, groupName + "vm")
                vms_info_list.append(vmInfo)
                count = count + 1
        elif(tag != ''):
            for vm in compute_client.virtual_machines.list_all():
                if(vm.tags is not None):
                    for key in vm.tags:
                        if(key == 'Group') and vm.tags['Group']==tag:
                            arr = vm.id.split('/')                                
                            vmInfo = self.list_vm(compute_client, arr[4], vm.name)
                            vms_info_list.append(vmInfo)
                            count = count + 1
        else:
            for vm in compute_client.virtual_machines.list_all():
                arr = vm.id.split('/')                                
                vmInfo = self.list_vm(compute_client, arr[4], vm.name) 
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
            sys.stdout.write("S " + str(len(self.resultQ)) + "\r\n")
            while self.resultQ:
                result = self.resultQ.pop()
                sys.stdout.write(result + "\r\n")
        finally:
            self.resultQLock.release()

    def execute_command(self):
        ci = self.DequeueCommand()
        
        if(ci.command.upper() == "AZURE_PING"):
            try:
                credentials = self.create_credentials_from_file(ci.cred_file)
                if(credentials is None):
                    self.QueueResult(ci.request_id, self.Escape("Error reading or creating credentials"))
                    return
                client_libs = self.create_client_libraries(credentials, ci.subscription)
                if(client_libs is None):
                    self.QueueResult(ci.request_id, self.Escape("Error creating client libraries"))
                    return
                self.QueueResult(ci.request_id, "NULL")
            except Exception as e:
                error = self.Escape(str(e.args[0]))
                self.QueueResult(ci.request_id, error) 
        elif(ci.command.upper() == "AZURE_VM_CREATE"):
            try:
                credentials = self.create_credentials_from_file(ci.cred_file)
                if(credentials is None):
                    self.QueueResult(ci.request_id, self.Escape("Error reading or creating credentials"))
                    return
                client_libs = self.create_client_libraries(credentials, ci.subscription)
                if(client_libs is None):
                    self.QueueResult(ci.request_id, self.Escape("Error creating client libraries"))
                    return
                self.create_vm(client_libs["compute_client"], client_libs["network_client"], client_libs["resource_client"], client_libs["storage_client"], ci.cmdParams["location"], ci.cmdParams["name"], ci.cmdParams["vnetName"], ci.cmdParams["subnetName"], ci.cmdParams["osDiskName"], ci.cmdParams["storageAccountName"], ci.cmdParams["ipConfigName"], ci.cmdParams["nicName"], ci.cmdParams["adminUsername"], ci.cmdParams["key"], ci.cmdParams["vmName"], ci.cmdParams["size"], ci.cmdParams["vmRef"], ci.cmdParams["osType"], ci.cmdParams["tag"],ci.cmdParams["customdata"],ci.cmdParams["datadisks"])
                self.QueueResult(ci.request_id, "NULL")
            except Exception as e:
                error = self.Escape(str(e.args[0]))
                self.QueueResult(ci.request_id, error) #todo: vm_id, ip address
        elif(ci.command.upper() == "AZURE_VM_DELETE"):
            try:
                credentials = self.create_credentials_from_file(ci.cred_file)
                if(credentials is None):
                    self.QueueResult(ci.request_id, self.Escape("Error reading or creating credentials"))
                    return
                client_libs = self.create_client_libraries(credentials, ci.subscription)
                if(client_libs is None):
                    self.QueueResult(ci.request_id, self.Escape("Error creating client libraries"))
                    return
                self.delete_rg(client_libs["resource_client"], ci.cmdParams["vmName"]) # TODO: delete_vm needs 2 parameters: RG and VmName
                self.QueueResult(ci.request_id, "NULL")
            except Exception as e:
                error = self.Escape(str(e.args[0]))
                self.QueueResult(ci.request_id, error) #todo: vm_id, ip address
        elif(ci.command.upper() == "AZURE_VM_LIST"):
            try:
                credentials = self.create_credentials_from_file(ci.cred_file)
                if(credentials is None):
                    self.QueueResult(ci.request_id, self.Escape("Error reading or creating credentials"))
                    return
                client_libs = self.create_client_libraries(credentials, ci.subscription)
                if(client_libs is None):
                    self.QueueResult(ci.request_id, self.Escape("Error creating client libraries"))
                    return
                vms_info_list = self.list_rg(client_libs["compute_client"], ci.cmdParams["vmName"], ci.cmdParams["tag"]) # TODO: list_vm needs 2 parameters: RG and VmName
                result = "NULL" + spaceSeparator + str(len(vms_info_list))+ spaceSeparator + ''.join(vms_info_list)
                self.QueueResult(ci.request_id, result)
            except Exception as e:
                error = self.Escape(str(e.args[0]))
                self.write_message("Error listing VMs: " + error + "\r\n")
                self.QueueResult(ci.request_id,error) #todo: vm_id, ip address sys.exc_info()[0]

    def get_create_vm_args(self, cmdParts):
        vmRef = {
            "linux-ubuntu-latest": {"publisher": "Canonical", "offer": "UbuntuServer", "sku": "16.04.0-LTS", "version": "latest"},
            "windows-server-latest": {"publisher": "MicrosoftWindowsServerEssentials", "offer": "WindowsServerEssentials", "sku": "WindowsServerEssentials","version": "latest"}
        }
        createVMArgNames = ["name","location","image", "size", "dataDisks","adminUsername","key","vnetName","publicIPAddress","customData","tag","ostype"]
        dnary = dict()
        
        #nvpStrings = cmdParts[4].split(separator)
        for index,nvpString in enumerate(cmdParts):
            if(index < 4):
                continue
            nvp = nvpString.split("=")
            if(not nvp):
                self.write_message('Empty args' + "\r\n")
                return None
            if(len(nvp) < 2):
                self.write_message('Insufficient args:' + nvpString + "\r\n")
                return None
            if(nvp[0].upper() not in (val.upper() for val in createVMArgNames)):
                self.write_message('Unrecogonized paramter name: ' + nvp[0] + "\r\n")
                return None
            if(nvp[0].lower() == "ostype"):
                dnary["osType"] = nvp[1].title()             
            if(nvp[0].lower() == "image"):
                if(nvp[1].lower() == "linux-ubuntu-latest"):
                    dnary["vmRef"] = vmRef[nvp[1]]
                    dnary["osType"] = 'Linux'
                elif(nvp[1].lower() == "windows-server-latest"):
                    dnary["vmRef"] = vmRef[nvp[1]]
                    dnary["osType"] = 'Windows'
                elif("https://" in nvp[1].lower()):
                    dnary["vmRef"] = nvp[1].strip()
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

        if(not "tag" in dnary):
            dnary["tag"] = ""

        if(not "customdata" in dnary):
            dnary["customdata"] = ""

        if(not "datadisks" in dnary):
            dnary["datadisks"] = ""

        subnetName = dnary["name"] + "subnet"
        osDiskName = dnary["name"] + "osdisk"
        storageAccountName = dnary["name"] + "sa"
        ipConfigName = dnary["name"] + "ipconfig"
        nicName = dnary["name"] + "nic"

        dnary["subnetName"] = subnetName
        dnary["osDiskName"] = osDiskName
        dnary["storageAccountName"] = storageAccountName
        dnary["ipConfigName"] = ipConfigName
        dnary["nicName"] = nicName
        
        return dnary


    def get_list_vm_args(self, cmdParts):       
        dnary = dict() 
        dnary["vmName"] = ''
        dnary["tag"] = '' 
        if(len(cmdParts) > 4):     
            nvpStrings = cmdParts[4].split('=')
            if(len(nvpStrings) == 1):
                dnary["vmName"] = nvpStrings[0]
            elif(len(nvpStrings) == 2):
                dnary["tag"] = nvpStrings[1]
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
            cmdParams = self.get_list_vm_args(cmdParts)
            #cmdParams["vmName"] = cmdParts[4]
            ci.cmdParams = cmdParams
        elif(ci.command.upper() == "AZURE_PING"):
            ci.cmdParams = None
        else:
            cmdParams = None
        self.QueueCommand(ci)
        t = AzureGAHPCommandExecThread(self)
        t.start()
        return True

    def Escape(self,msg):
        return msg.replace(singleBackSlashSeparator,doubleBackSlashSeparator).replace(spaceSeparator,singleBackSlashSpaceSeparator)

    #### END FUNCTIONS ################################
