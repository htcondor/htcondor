#!/usr/bin/env python3 -u

from __future__ import print_function

import platform

## AzureGAHPLib.py ############################
import adal
import copy
import logging
import sys
import os
import threading
import base64
import itertools
import datetime
import json
import time
from collections import deque
from azure.common.credentials import ServicePrincipalCredentials, get_azure_cli_credentials
from azure.graphrbac import GraphRbacManagementClient
from azure.graphrbac.models import *
from azure.mgmt.compute.models import DiskCreateOption
from azure.mgmt.resource import ResourceManagementClient
from azure.mgmt.storage import StorageManagementClient
from azure.mgmt.network import NetworkManagementClient
from azure.mgmt.compute import ComputeManagementClient
from azure.mgmt.scheduler import SchedulerManagementClient
from azure.mgmt.scheduler.models import *
from azure.mgmt.keyvault import KeyVaultManagementClient
from azure.mgmt.keyvault.models import *
from azure.storage import CloudStorageAccount
from azure.storage.blob.models import ContentSettings, PublicAccess
from azure.storage.table import TableService, Entity
from datetime import timedelta

space_separator = " "
single_backslash_space_separator = "\\ "
double_backslash_separator = "\\\\"
single_backslash_separator = "\\"
utf_encoding = "utf-8"
debug = "--debug"
verbose = "--verbose"
double_line_break = "\r\n"
error_creating_keyvault = "Error creating Key Vault:"
error_creating_vm = "Error creating VM:"
error_creating_vmss = "Error creating VMSS:"
error_deleting_vm = "Error deleting VM:"
error_deleting_vmss = "Error deleting VMSS:"
error_listing_vms = "Error listing VMs:"
error_pinging_azure = "Error pinging Azure:"
error_restarting_vmss = "Error restarting VMSS:"
error_scaling_vmss = "Error scaling VMSS:"
error_starting_vmss = "Error starting VMSS:"
error_stoping_vmss = "Error stoping VMSS:"
data_disks_value_format = "The value must be a valid number [between 1 and 4095] separated by comma."
vmss_node_count_value_format = "The value must be a valid number [between 0 and 100]."

#Create log file
log_filename = 'htcondorlog.log'
logging.basicConfig(
    format="%(asctime)s   %(levelname)s   %(message)s", 
    datefmt="%Y-%m-%d %H:%M:%S", filename=log_filename, 
    level=logging.INFO, filemode='w')


########### CLASSES ##################
class AzureGAHPCommandInfo:
    command = ""
    request_id = 0
    cred_file = ""
    subscription = ""
    cmdParams = None

class GahpCommandBuilder:
    def __init__(self):
        self.create_keyvault = "AZURE_KEYVAULT_CREATE"
        self.create_vm = "AZURE_VM_CREATE"
        self.create_vmss = "AZURE_VMSS_CREATE"
        self.delete_vm = "AZURE_VM_DELETE"
        self.delete_vmss = "AZURE_VMSS_DELETE"
        self.list_vm = "AZURE_VM_LIST"
        self.ping_azure = "AZURE_PING"
        self.restart_vmss = "AZURE_VMSS_RESTART"
        self.scale_vmss = "AZURE_VMSS_SCALE"
        self.start_vmss = "AZURE_VMSS_START"
        self.stop_vmss = "AZURE_VMSS_STOP"
        self.commands = "COMMANDS"
        self.quit = "QUIT"
        self.results = "RESULTS"
        self.version = "VERSION"

class GahpClientLibrariesBuilder(object):    
    compute = None
    network = None
    resource = None
    storage = None
    scheduler = None
    keyvault = None
    aad = None
    
    def __init__(self, dnary):
        self.compute = dnary["compute_client"]
        self.network = dnary["network_client"]
        self.resource = dnary["resource_client"]
        self.storage = dnary["storage_client"]
        self.scheduler = dnary["scheduler_client"]
        self.keyvault = dnary["keyvault_client"]
        self.aad = dnary["aad_client"]

class GahpAppSettingsBuilder(object):
    ad_group_name = None
    client_id = None
    secret = None
    tenant_id = None
    max_vm_count_in_thread = 5
    webhook_url = None
    runbook_token = None
    job_group_name = None
    job_collection_name = None
    job_collection_sku = None
    cleaner_job_frequency_type = None
    cleaner_job_interval = None
    shell_script_url = None
    
    def __init__(self, dnary):
        if "ad_group_name" in dnary:
            self.ad_group_name = dnary["ad_group_name"]
        if "job_frequency_type" in dnary:
            self.cleaner_job_frequency_type = dnary["job_frequency_type"]
        if "job_interval" in dnary:
            self.cleaner_job_interval = dnary["job_interval"]
        if "client_id" in dnary:
            self.client_id = dnary["client_id"]
        if "key_vault_setup_script_url" in dnary:
            self.shell_script_url = dnary["key_vault_setup_script_url"]
        if "job_collection" in dnary:
            self.job_collection_name = dnary["job_collection"]
        if "job_collection_sku" in dnary:
            self.job_collection_sku = dnary["job_collection_sku"]
        if "jobs_rg" in dnary:
            self.job_group_name = dnary["jobs_rg"]
        if "max_vm_count_in_thread" in dnary:
            self.max_vm_count_in_thread = dnary["max_vm_count_in_thread"]
        if "token" in dnary:
            self.runbook_token = dnary["token"]
        if "secret" in dnary:
            self.secret = dnary["secret"]
        if "tenant_id" in dnary:
            self.tenant_id = dnary["tenant_id"]
        if "webhook_url" in dnary:
            self.webhook_url = dnary["webhook_url"]

class GahpKeyvaultParametersBuilder(object):    
    group_name = None
    name = None
    location = None
    sku = None
    users = None
    
    def __init__(self, dnary):
        self.group_name = dnary["name"]
        if self.group_name is not None:
            self.name = self.group_name + "kv"
        self.location = dnary["location"].lower()
        self.sku = dnary["sku"].lower()
        self.users = dnary["users"]

class GahpVmCreateParametersBuilder(object):    
    admin_username = "superuser"
    custom_data = None
    data_disks = None
    group_name = None
    key = "Super1234!@#$"
    ip_config_name = None
    location = None
    nic = None
    os_disk_name = None
    os_type = None
    storage_account_name = None
    subnet_name = None
    tag = None
    vm_name = None
    vm_reference = None
    vm_size = None
    vnet_name = None
    vnet_rg_name = None
    
    def __init__(self, dnary):
        self.group_name = dnary["name"]
        self.location = dnary["location"].lower()
        self.vm_reference = dnary["vmref"]
        self.vm_size = dnary["size"]
        if self.group_name is not None:
            self.ip_config_name = self.group_name + "ipconfig"
            self.vm_name = self.group_name + "vm"
            self.nic_name = self.group_name + "nic"
            self.os_disk_name = self.group_name + "osdisk"
            self.storage_account_name = self.group_name + "sa"
        if "adminusername" in dnary:
            self.admin_username = dnary["adminusername"]
        if "customdata" in dnary:
            self.custom_data = dnary["customdata"]
        if "datadisks" in dnary:
            self.data_disks = dnary["datadisks"]
        if "key" in dnary:
            self.key = dnary["key"]
        if "ostype" in dnary:
            self.os_type = dnary["ostype"]
        if "subnetname" in dnary:
            self.subnet_name = dnary["subnetname"]
        if "tag" in dnary:
            self.tag = dnary["tag"]
        if "vnetname" in dnary:
            self.vnet_name = dnary["vnetname"]
        if "vnetrgname" in dnary:
            self.vnet_rg_name = dnary["vnetrgname"]

class GahpVmssCreateParametersBuilder(object):    
    admin_username = "superuser"
    custom_data = None
    data_disks = None
    deletion_job = False
    group_name = None
    key = "Super1234!@#$"
    ip_config_name = None
    keyvault_name = None
    keyvault_rg_name = None
    keyvault_secret_name = None
    location = None
    nic = None
    node_count = 1
    os_disk_name = None
    os_type = None
    schedule = None
    storage_account_name = None
    subnet_name = None
    tag = None
    vmss_name = None
    vmss_reference = None
    vmss_size = None
    vnet_name = None
    vnet_rg_name = None
    
    def __init__(self, dnary):
        self.location = dnary["location"]
        self.vmss_reference = dnary["vmssref"]
        self.vmss_size = dnary["size"]
        self.group_name = dnary["name"]
        if self.group_name is not None:
            self.ip_config_name = self.group_name + "ipconfig"
            self.vmss_name = self.group_name + "vmss"
            self.nic_name = self.group_name + "nic"
            self.os_disk_name = self.group_name + "osdisk"
            self.storage_account_name = self.group_name + "sa"
            self.subnet_name = self.group_name + "subnet"
        if "adminusername" in dnary:
            self.admin_username = dnary["adminusername"]
        if "customdata" in dnary:
            self.custom_data = dnary["customdata"]
        if "datadisks" in dnary:
            self.data_disks = dnary["datadisks"]
        if "deletionjob" in dnary:
            self.deletion_job = dnary["deletionjob"]
        if "key" in dnary:
            self.key = dnary["key"]            
        if "keyvaultname" in dnary:
            self.keyvault_name = dnary["keyvaultname"]
        if "keyvaultrg" in dnary:
            self.keyvault_rg_name = dnary["keyvaultrg"]
        if "vaultkey" in dnary:
            self.keyvault_secret_name = dnary["vaultkey"]
        if "nodecount" in dnary:
            self.node_count = dnary["nodecount"]
        if "ostype" in dnary:
            self.os_type = dnary["ostype"]
        if "schedule" in dnary:
            self.schedule = dnary["schedule"]
        if "tag" in dnary:
            self.tag = dnary["tag"]
        if "vnetname" in dnary:
            self.vnet_name = dnary["vnetname"]
        if "vnetrgname" in dnary:
            self.vnet_rg_name = dnary["vnetrgname"]

class AzureGAHPCommandExecThread(threading.Thread):
    def __init__(self, ce):
        threading.Thread.__init__(self)
        self.ce = ce

    def run(self):
        self.ce.execute_command()

class AzureGAHPCommandExec():
    command_queue = None
    result_queue = None
    command_queue_lock = None
    result_queue_lock = None
    max_vm_count_in_thread = None
    

    def __init__(self):
        self.command_queue = deque()  
        self.result_queue = deque()
        self.command_queue_lock = threading.Lock()
        self.result_queue_lock = threading.Lock()
    
    # Check if the programming is running in debug mode
    def check_mode(self,input):
        if debug in input or verbose in input:
            self.debug_mode = True
        else:
            self.debug_mode = False

    # Read credentails from file
    def read_app_settings_from_file(self, file_name):
        dnary = dict()
        with open(file_name, "r") as f:
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

    # Read credentails from file
    def read_app_settings_file(self, file_name):
        dnary = dict()
        if file_name != "NULL":
            with open(file_name, "r") as f:
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

        app_settings = GahpAppSettingsBuilder(dnary)
        return app_settings

    # Write log in file
    def write_log(self,request_id, msg, msgType=logging.INFO):
        if msgType == logging.INFO :   
            logging.info("{} {}\n".format(request_id,msg))
        else:
            logging.error("{} {}\n".format(request_id,msg))
    
    # Print log on console
    def write_message(self,request_id, msg, msgType=logging.INFO):        
        self.write_log(request_id, msg, msgType) 
        if self.debug_mode:
             sys.stderr.write("{} {}\n".format(request_id,msg))
        else: 
            return
    
    # Read max vm count from app settings
    def set_vm_count_in_thread(self, app_settings):
        if("max_vm_count_in_thread" in app_settings):
            self.max_vm_count_in_thread = int(
                app_settings["max_vm_count_in_thread"])
        else:
           self.max_vm_count_in_thread = 5 

    # Calculate thread count to get vm list
    def calculate_thread_Count(self, vm_list):
        total_vm = len(vm_list)
        rem = total_vm % self.max_vm_count_in_thread
        count = total_vm / self.max_vm_count_in_thread
        if rem > 0 :
            count = total_vm / self.max_vm_count_in_thread + 1
        return int(count)

    # Load credentials from file
    def create_credentials_from_file(self, request_id, file_name):
        dnary = dict()
        cred = None
        if file_name == "NULL":
            cred, _ = get_azure_cli_credentials()
        else:
            dnary = self.read_app_settings_from_file(file_name)
            cred = self.create_service_credentials(request_id, dnary)
        self.set_vm_count_in_thread(dnary)
        self.write_message(
            request_id, 
            "Credentials created {}".format(double_line_break)
            )
        return cred

    # Create service principal credentials using 
    # client id, secret and tenant id
    def create_service_credentials(self, request_id, dnary):
        self.write_message(
            request_id, 
            "Creating credentials {}".format(double_line_break)
            )
        credentials = ServicePrincipalCredentials(
            client_id = dnary["client_id"], 
            secret = dnary["secret"], 
            tenant = dnary["tenant_id"],
            )
        return credentials

    # Create client libraries using 
    # service credentails and subscription id
    def create_client_libraries(
            self, request_id, credentials, subscription_id, 
            app_settings):
        self.write_message(
            request_id, 
            "Creating client libraries {} {}".format(
                subscription_id, double_line_break
                )
            )
        compute_client = ComputeManagementClient(
            credentials, subscription_id)
        network_client = NetworkManagementClient(
            credentials, subscription_id)
        resource_client = ResourceManagementClient(
            credentials, subscription_id)
        storage_client = StorageManagementClient(
            credentials, subscription_id)
        scheduler_client = SchedulerManagementClient(
            credentials, subscription_id)
        keyvault_client = KeyVaultManagementClient(
            credentials, subscription_id)
        resource_client.providers.register('Microsoft.Scheduler')
        resource_client.providers.register('Microsoft.KeyVault')

        aad_client = None
        # Get token for Azure AD
        # TODO This Active Directory Authentication Library code only
        #   works with a service account principal at the moment.
        #   We skip it if we're using the default CLI credentials.
        #   This code is only used for the AZURE_VMSS_CREATE and
        #   AZURE_KEYVAULT_CREATE commands, which we don't currently
        #   use for the grid universe.
        #   This deficiency should be fixed at some point.
        if app_settings.tenant_id is not None:
            resource_url = "https://login.microsoftonline.com/{}".format(
                app_settings.tenant_id)
            context = adal.AuthenticationContext(resource_url)
            GRAPH_RESOURCE = '00000002-0000-0000-c000-000000000000' #AAD graph resource
            aad_token = context.acquire_token_with_client_credentials(
                GRAPH_RESOURCE,
                app_settings.client_id,
                app_settings.secret)

            # Update access token in credentials for Azure AD client
            aad_credentials = copy.deepcopy(credentials)
            aad_credentials.token['access_token'] = aad_token['accessToken']
            aad_client = GraphRbacManagementClient(
                aad_credentials, app_settings.tenant_id)

        dnary = {
            "compute_client": compute_client,
            "network_client": network_client,
            "resource_client": resource_client,
            "storage_client": storage_client,
            "scheduler_client": scheduler_client,
            "keyvault_client": keyvault_client,
            "aad_client": aad_client
            }
        client_libs = GahpClientLibrariesBuilder(dnary)
        self.write_message(
            request_id, 
            "Creating client libraries: complete {}".format(
                double_line_break)
            )
        return client_libs

    # Return ssh key
    def get_ssh_key(self, key_info):
        if not os.path.exists(key_info): 
            return key_info #raw key
        ssh_key_path = os.path.expanduser(key_info)
        # Will raise if file not exists or not enough permission
        with open(ssh_key_path, "r") as pub_ssh_file_fd:
            return pub_ssh_file_fd.read()
    
    # Return base64 string
    def get_base64_string(self, data):
        if not os.path.exists(data): 
            return base64.b64encode(bytes(
                data, utf_encoding
                )).decode(utf_encoding)#raw data
        file_path = os.path.expanduser(data)
        # Will raise if file not exists or not enough permission
        with open(file_path, "r") as custom_data:
            return base64.b64encode(bytes(
                custom_data.read(), 
                utf_encoding
                )).decode(utf_encoding)
    
    # Get specific VMs on the basis of index
    def take(self, index, compute_client, vm_list):
        result_list = []
        start = index * self.max_vm_count_in_thread
        end = int((index + 1) * self.max_vm_count_in_thread)
        count = 0
        for vm in vm_list:
          if count >= start and count < end :
                result_list.append(vm)
          count = count + 1
        return result_list

    # Get vm power state
    def get_vminfo_list(
            self, request_id, compute_client, group_name, 
            network_client, vmlist, queue):
        vms_info_list = []
        for vm in vmlist:
            try:
                arr = vm.id.split("/") 
                vm_info = self.list_vm(
                    compute_client, arr[4], vm.name) 
                vms_info_list.append(vm_info)                    
            except Exception as e:
                vm_info = self.get_vm_info(
                    request_id, compute_client, network_client, 
                    group_name, vm.name)
                self.show_vm_info_error(request_id, vm_info, e)
        self.queue_list_result(queue, vms_info_list)

    # Get vm power state by vm tag
    def get_vminfo_list_by_tag(
            self, request_id, compute_client, group_name, 
            network_client, tag, vmlist, queue): 
        vms_info_list = []
        for vm in vmlist:
            try:
                if(vm.tags is not None):
                    for key in vm.tags:
                        if key == "Group" and vm.tags["Group"] == tag:
                            arr = vm.id.split("/") 
                            vm_info = self.list_vm(
                                compute_client, arr[4], vm.name) 
                            vms_info_list.append(vm_info)                    
            except Exception as e:
                vm_info = self.get_vm_info(
                    request_id, compute_client, network_client, 
                    group_name, vm.name)
                self.show_vm_info_error(request_id, vm_info, e)
        self.queue_list_result(queue, vms_info_list)

    # Return vm list based on tag having "key=Group"
    def list_vms_by_tag(
            self, request_id, compute_client, network_client, tag):
        self.write_message(
            request_id, 
            "Listing VMs by tag: {} {}".format(
                tag, double_line_break)
            )
        vms_info_list = []
        vm_list = compute_client.virtual_machines.list_all()
        vms_info_list = self.run_vm_list_thread(
            request_id, compute_client, network_client, 
            vm_list, tag)
        return vms_info_list 

    def run_vm_list_thread(
            self, request_id, compute_client, network_client, 
            vm_list, tag=None): 
        list_queue = deque() 
        threads = []
        temp_vm_list = []
        for vm in vm_list:
            temp_vm_list.append(vm)
        count = self.calculate_thread_Count(temp_vm_list)
        for x in range(count):  
            vm_filter_list = self.take(
                x, compute_client, temp_vm_list)
            if tag is not None: 
                t = threading.Thread(
                        target = self.get_vminfo_list_by_tag, 
                        args=(request_id, compute_client, "", 
                              network_client, tag, vm_filter_list, 
                              list_queue)
                        )
            else:              
                t = threading.Thread(
                        target = self.get_vminfo_list, 
                        args=(request_id, compute_client, "", 
                              network_client, vm_filter_list, 
                              list_queue)
                        )
            t.start()
            threads.append(t)
        for t in threads:
            t.join()  
        return self.deque_all_list_results(list_queue, list_queue)

    # Create nic parameters
    def create_nic_parameters(
            self, location, ip_config_name, public_ip_info, 
            subnet_id):
        params = {
                "location": location,
                "ip_configurations": [{
                    "name": ip_config_name,
                    "private_ip_allocation_method": "Dynamic",
                    "subnet": {
                        "id": subnet_id
                    }
                }]
            }
        if public_ip_info is not None:
            pip = {
                "id" : public_ip_info.id
            }
            params["ip_configurations"][0]["public_ip_address"] = pip
        return params

    # Create resource group
    def create_resource_group(
            self, request_id, resource_client, group_name, 
            location):
        message = "Creating resource group"
        self.write_message(
            request_id, 
            "{} '{}' {}".format(
                message, group_name, double_line_break)
            )
        resource_client.resource_groups.create_or_update(
            group_name, {'location':location})

        self.write_message(
            request_id, 
            "{}: complete {}".format(message, double_line_break))

    # Create public ip
    def create_public_ip(
            self, request_id, network_client, group_name, location, 
            public_ip_name, pip_alloc_method):
        try:
            self.write_message(
                request_id, 
                "Creating public ip '{}'{}".format(
                    public_ip_name, double_line_break)
                )
            pip_client = network_client.public_ip_addresses
            async_public_ip_creation = pip_client.create_or_update(
                group_name, 
                public_ip_name,
                {
                    'location': location,
                    'public_ip_allocation_method': pip_alloc_method
                })
            return async_public_ip_creation.result()
        except Exception as e:
            error = self.escape(str(e.args[0]))
            self.write_message(
                request_id, 
                "Error creating Public IP: {} {}".format(
                    error, double_line_break), 
                logging.ERROR)
            return None

    # Create virtual network
    def create_virtual_network(
            self, request_id, network_client, location, 
            vnet_rg_name, vnet_name):
        self.write_message(
            request_id, 
            "Creating vnet '{}' {}".format(
                vnet_name, double_line_break)
            )
        vnet_client = network_client.virtual_networks
        async_vnet_creation = vnet_client.create_or_update(
            vnet_rg_name,
            vnet_name,
            {
                "location": location,
                "address_space": {
                    "address_prefixes": ["10.0.0.0/16"]
                }
            })
        async_vnet_creation.wait()

    # Create subnet
    def create_subnet(
            self, request_id, network_client, vnet_rg_name, 
            vnet_name, subnet_name):
        self.write_message(
            request_id, 
            "Creating subnet '{}' {}".format(
                subnet_name, double_line_break)
            )
        subnet_client = network_client.subnets
        async_subnet_creation = subnet_client.create_or_update(
            vnet_rg_name,
            vnet_name,
            subnet_name,
            {"address_prefix": "10.0.0.0/24"})
        return async_subnet_creation.result()

    # Method will create virtual network, subnet, public ip and 
    # use all of them for nic creation
    def create_nic(
            self, request_id, network_client, cmd_params, subnet):
        
        #Create Virtual Network
        if (cmd_params.vnet_name is not None
            and cmd_params.vnet_rg_name is not None ):

            self.write_message(
                request_id, 
                "Using existing vnet '{}' {}".format(
                    cmd_params.vnet_name, double_line_break)
                )
            vnet = network_client.virtual_networks.get(
                cmd_params.vnet_rg_name, cmd_params.vnet_name)
            if(subnet is None):
                subnets_of_vnet = vnet.subnets
                if len(subnets_of_vnet) > 0:
                    # use first subnet of the vnet
                    subnet = subnets_of_vnet[0]
                    self.write_message(
                        request_id, 
                        "Using existing subnet '{}' {}".format(
                            subnet.name, double_line_break)
                        )
                else:
                    # Create Subnet
                    subnet_name = cmd_params.group_name + "subnet"
                    self.write_message(
                        request_id, 
                        "Creating subnet: {} {}".format(
                            subnet_name, double_line_break)
                        )
                    prefix = vnet.address_space.address_prefixes[0]
                    subnet_client = network_client.subnets
                    async_subnet = subnet_client.create_or_update(
                        cmd_params.vnet_rg_name,
                        cmd_params.vnet_name,
                        subnet_name,
                        { "address_prefix": prefix }
                        )
                    subnet = async_subnet.result()
            else:
                self.write_message(
                    request_id, 
                    "Using existing subnet '{}' {}".format(
                        subnet.name, double_line_break)
                    )
        else:
            vnet_rg_name = cmd_params.group_name
            vnet_name = cmd_params.group_name + "vnet"
            subnet_name = cmd_params.group_name + "subnet"
            self.create_virtual_network(
                request_id, network_client, cmd_params.location, 
                vnet_rg_name, vnet_name)
            subnet = self.create_subnet(
                request_id, network_client, vnet_rg_name, 
                vnet_name, subnet_name)

        # Create public ip
        public_ip_name = cmd_params.group_name + "pip"
        public_ip_info = self.create_public_ip(
            request_id, network_client, cmd_params.group_name, 
            cmd_params.location, public_ip_name, "Dynamic")

        # Create NIC
        self.write_message(
            request_id, 
            "Creating NIC '{}'{}".format(
                cmd_params.nic_name, double_line_break)
            )
        nic_params = self.create_nic_parameters(
            cmd_params.location, cmd_params.ip_config_name, 
            public_ip_info, subnet.id)
        async_nic = network_client.network_interfaces.create_or_update(
            cmd_params.group_name, cmd_params.nic_name, nic_params)
        return async_nic.result()

    # Create public ip and load balancer
    def create_load_balancers(
            self, request_id, network_client, group_name, location, 
            public_ip_name, frontend_ip_name, addr_pool_name, 
            probe_name, lb_name): 
        # Create PublicIP
        public_ip_info = self.create_public_ip(
            request_id, network_client, group_name, location, 
            public_ip_name, "Dynamic")
        
        # Retrieve subscription id from public ip
        subscription_id = public_ip_info.id.split('/')[2]        
        
        # Create front end, back end and probe id
        front_end_id = ('/subscriptions/{}'
            '/resourceGroups/{}'
            '/providers/Microsoft.Network'
            '/loadBalancers/{}'
            '/frontendIPConfigurations/{}').format(
                subscription_id,
                group_name,
                lb_name,
                frontend_ip_name)
        back_end_id = ('/subscriptions/{}/resourceGroups/{}'
            '/providers/Microsoft.Network'
            '/loadBalancers/{}'
            '/backendAddressPools/{}').format(
                subscription_id,
                group_name,
                lb_name,
                addr_pool_name)
        probe_id = ('/subscriptions/{}'
            '/resourceGroups/{}'
            '/providers/Microsoft.Network'
            '/loadBalancers/{}'
            '/probes/{}').format(
                subscription_id,
                group_name,
                lb_name,
                probe_name)

        # Building a FrontEndIpPool
        frontend_ip_configurations = [{
            'name': frontend_ip_name,
            'private_ip_allocation_method': 'Dynamic',
            'public_ip_address': {
                'id': public_ip_info.id
            }
        }]

        # Building a BackEnd adress pool
        backend_address_pools = [{
            'name': addr_pool_name
        }]

        # Building a HealthProbe
        probes = [{
            'name': probe_name,
            'protocol': 'Http',
            'port': 80,
            'interval_in_seconds': 15,
            'number_of_probes': 4,
            'request_path': 'healthprobe.aspx'
        }]

        # Create inbound nat pool
        inbound_nat_pool = [{
            'name':  lb_name + "natrulelinux",
            'protocol': 'tcp',
            'frontend_port_range_start': 50000,
            'frontend_port_range_end': 52000,
            'backend_port': 22,
            'frontend_ip_configuration': {
                'id': front_end_id
            }
        }]
        inbound_nat_pool.append({
            'name':  lb_name + "natrulewindows",
            'protocol': 'tcp',
            'frontend_port_range_start': 52001,
            'frontend_port_range_end': 54000,
            'backend_port': 3389,
            'frontend_ip_configuration': {
                'id': front_end_id
            }
        })

        # Creating Load Balancer
        self.write_message(
            request_id, 
            "Creating load balancer '{}'{}".format(
                lb_name, double_line_break)
            )
        lb_async_creation = network_client.load_balancers.create_or_update(
            group_name,
            lb_name,
            {
                'location': location,
                'frontend_ip_configurations': frontend_ip_configurations,
                'backend_address_pools': backend_address_pools,
                'probes': probes,
                'inbound_nat_pools' :inbound_nat_pool
            })
        return lb_async_creation.result()  

    # Create image from vhd
    def create_image_from_vhd(
            self, request_id, compute_client, group_name, 
            image_name, location, os_type, blob_uri):       
        self.write_message(
            request_id, 
            "Creating image '{}'{}".format(
                image_name, double_line_break)
            )
        async_create_image = compute_client.images.create_or_update(
            group_name,
            image_name,
            {
                "location": location,
                "storage_profile": {
                    "os_disk": {
                        "os_type": os_type,
                        "os_state": "Generalized",
                        "blob_uri": blob_uri,
                        "caching": "ReadWrite"
                    }
                }
            })
        return async_create_image.result()

    # Delete image
    def delete_image(
            self, request_id, compute_client, group_name, 
            image_name):
        self.write_message(
            request_id, 
            "Deleting image '{}'{}".format(
                image_name, double_line_break)
            )
        async_delete_image = compute_client.images.delete(
            group_name, image_name)
        async_delete_image.wait()
          
    # Get vm public ip and vm id
    def print_vm_info(
            self, request_id, network_client, vm_id, public_ip, 
            group_name):
        if public_ip is not None :
            public_ip = network_client.public_ip_addresses.get(
                group_name, group_name + "pip")
            public_ip_address = public_ip.ip_address
        else:
            public_ip_address = "NULL"

        vm_id_msg = "VM Id {}{}".format(
            self.escape(vm_id), double_line_break)
        public_ip_msg = "Public IP {}{}".format(
            self.escape(public_ip_address), double_line_break)

        self.write_message(request_id, vm_id_msg)
        self.write_message(request_id, public_ip_msg)

        vm_info = {
                    "vm_id":vm_id,
                    "public_ip":public_ip_address
                  }
        return vm_info

    # Create vm parameters
    # check whether vm_reference contains custom image url
    def create_vm_parameters(self, cmd_params, nic_id, image_id):
        is_custom_image = False
        if "https://" in cmd_params.vm_reference:            
            storage_account = cmd_params.vm_reference.split("/")[2].split(".")[0]   
            is_custom_image = True     
        key = self.get_ssh_key(cmd_params.key)
        params = {
            "location": cmd_params.location,
            "os_profile": {
                "computer_name": cmd_params.vm_name,
                "admin_username": cmd_params.admin_username
            },
            "hardware_profile": {
                "vm_size": cmd_params.vm_size
            },
            "storage_profile": { 
                "image_reference": {}  
            },
            "network_profile": {
                "network_interfaces": [{
                    "id": nic_id,
                }]
            },
        }

        linux_conf = {
            "disable_password_authentication": True,
            "ssh": {
                "public_keys": [{
                    "path": "/home/{}/.ssh/authorized_keys".format(
                        cmd_params.admin_username),
                    "key_data": key
                    }]
                }
            }

        tags = {
            "Group": cmd_params.tag
            }
        
        if is_custom_image:
            params["storage_profile"]["image_reference"]["id"] = image_id
        else:            
            image_reference = {
                    "publisher": cmd_params.vm_reference["publisher"],
                    "offer": cmd_params.vm_reference["offer"],
                    "sku": cmd_params.vm_reference["sku"],
                    "version": cmd_params.vm_reference["version"]
                }
            params["storage_profile"]["image_reference"] = image_reference            

      
        if os.path.exists(cmd_params.key) and cmd_params.os_type.lower() == "linux": 
            params["os_profile"]["linux_configuration"] = linux_conf
        else:  
            params["os_profile"]["admin_password"] = key

        # Handle tags related configuration
        if cmd_params.tag is not None:
            params["tags"] = tags
        # Handle custom data related configuration
        if cmd_params.custom_data is not None:
            base64_custom_data = self.get_base64_string(cmd_params.custom_data)
            params["os_profile"]["custom_data"] = base64_custom_data
     
        # Handle data disk configuration
        data_disks_arr = []
        if(cmd_params.data_disks is not None):
            dd_arr = cmd_params.data_disks.split(",")
            for index,val in enumerate(dd_arr):    
                dd = {
                        "name": "datadisk{}".format(index),
                        "disk_size_gb": val,
                        "lun": index,
                        "create_option": "Empty"
                      }
                data_disks_arr.append(dd)
            params["storage_profile"]["data_disks"] = data_disks_arr
        # return vm parameters
        return params

    def check_existing_vnet_and_subnet(
            self, request_id, network_client, vnet_rg_name, 
            vnet_name, subnet_name):
        existing_subnet = None
        subnets_of_vnet = None
        error = None

        vnet = network_client.virtual_networks.get(
            vnet_rg_name, vnet_name)
        subnets_of_vnet = vnet.subnets
        if len(subnets_of_vnet) > 0:
            if subnet_name is not None:
                for subnet in subnets_of_vnet:
                    if subnet_name.lower() == subnet.name.lower():
                        existing_subnet = subnet
                if existing_subnet is None:
                    error = "{} '{}' subnet is not found in '{}' vnet".format(
                        error_creating_vm, subnet_name, vnet_name)
                    self.write_message(
                        request_id, 
                        "{}{}".format(error, double_line_break), 
                        logging.ERROR)
            else:
                try:
                    existing_subnet = subnets_of_vnet[0]
                except Exception as e:
                    error = e.args[0]
        return existing_subnet, error

    # Create virtual machine based on input parameters
    def create_vm(self, request_id, client_libs, cmd_params):
        
        location = cmd_params.location
        compute_client = client_libs.compute
        network_client = client_libs.network
        resource_client = client_libs.resource

        existing_subnet = None
        # Check for existing Virtual Network and subnet
        if (cmd_params.vnet_name is not None
            and cmd_params.vnet_rg_name is not None):

            existing_subnet, error = self.check_existing_vnet_and_subnet(
                request_id, network_client, cmd_params.vnet_rg_name, 
                cmd_params.vnet_name, cmd_params.subnet_name)
            if(error is not None):
                return error

        # Create resource group
        self.create_resource_group(
            request_id, resource_client, cmd_params.group_name, 
            cmd_params.location)
        
        # Create Nic
        self.write_message(
            request_id, 
            "Creating network {}".format(double_line_break)
            )        
        nic = self.create_nic(
            request_id, network_client, cmd_params, existing_subnet)
        
        image_id = None
        if "https://" in cmd_params.vm_reference:
            image = self.create_image_from_vhd(
                request_id, compute_client, cmd_params.group_name, 
                cmd_params.group_name + "image", cmd_params.location, 
                cmd_params.os_type, cmd_params.vm_reference)
            image_id = image.id

        # Create VM
        self.write_message(
            request_id, 
            "Creating VM '{}'{}".format(
                cmd_params.vm_name, double_line_break)
            )
        vm_parameters = self.create_vm_parameters(cmd_params, nic.id, image_id)
        
        async_vm_creation = compute_client.virtual_machines.create_or_update(
            cmd_params.group_name, cmd_params.vm_name, vm_parameters)
        vm_info = async_vm_creation.result()
        
        if "https://" in cmd_params.vm_reference: 
            self.delete_image(
                request_id, compute_client, cmd_params.group_name, 
                cmd_params.group_name + "image")

        self.write_message(
            request_id, 
            "VM creation completed {}".format(double_line_break)
            )
        vm_result = self.print_vm_info(
            request_id, network_client, vm_info.vm_id, 
            nic.ip_configurations[0].public_ip_address, 
            cmd_params.group_name)

        return "NULL {} {}".format(
            self.escape(vm_result["vm_id"]), 
            self.escape(vm_result["public_ip"]))

    # Create VMSS parameters
    def create_vmss_parameters(
            self, cmd_params, subnet_id, image_id, 
            load_balancer_info):
        back_end_address_pool_id = load_balancer_info.backend_address_pools[0].id
        inbound_nat_pool_id = load_balancer_info.inbound_nat_pools[0].id 
        inbound_nat_pool_windows_id = load_balancer_info.inbound_nat_pools[1].id        
        is_custom_image = False
        # check whether vm_reference contains custom image url
        if "https://" in cmd_params.vmss_reference:            
            storage_account = cmd_params.vmss_reference.split("/")[2].split(".")[0]   
            is_custom_image = True     
        key = self.get_ssh_key(cmd_params.key)          
        params = {
            "location": cmd_params.location,
            "upgrade_policy": {
                "mode": "Manual"
            },
            "sku": {
                "name": cmd_params.vmss_size,
                "tier": "Standard",
                "capacity": cmd_params.node_count
              },
           "identity": {
                "type": "systemAssigned"
                },
            "virtual_machine_profile":
            {
                "os_profile": {
                    "computer_name_prefix": cmd_params.vmss_name,
                    "admin_username": cmd_params.admin_username
                },
                "storage_profile": {
                     "image_reference": {
                    }
                },
                "network_profile": {
                    "network_interface_configurations" : [{
                        "name": cmd_params.vmss_name + "nic",
                        "primary": True,
                        "ip_configurations": [{
                            "name": cmd_params.vmss_name + "ipconfig",
                            "subnet": {
                                "id": subnet_id
                            },
                            "load_balancer_backend_address_pools":[{
                                "id":back_end_address_pool_id
                            }],
                            "load_balancer_inbound_nat_pools":[{
                                "id":inbound_nat_pool_id
                            },
                            {
                                "id":inbound_nat_pool_windows_id
                            }]
                        }]
                    }]
                }
            },
        }        
        linux_conf = {
            "disable_password_authentication": True,
            "ssh": {
                "public_keys": [{
                    "path": "/home/{}/.ssh/authorized_keys".format(
                        cmd_params.admin_username),
                    "key_data": key
                    }]
                }
             }
        tags = {
            "Group": cmd_params.tag
            }

        if is_custom_image:
            params["virtual_machine_profile"]["storage_profile"]["image_reference"]["id"] = image_id
        else:            
            image_reference = {
                    "publisher": cmd_params.vmss_reference["publisher"],
                    "offer": cmd_params.vmss_reference["offer"],
                    "sku": cmd_params.vmss_reference["sku"],
                    "version": cmd_params.vmss_reference["version"]
                }
            params["virtual_machine_profile"]["storage_profile"]["image_reference"] = image_reference

        if os.path.exists(cmd_params.key) and cmd_params.os_type.lower() == "linux": 
            params["virtual_machine_profile"]["os_profile"]["linux_configuration"] = linux_conf
        else:  
            params["virtual_machine_profile"]["os_profile"]["admin_password"] = key

        if cmd_params.tag is not None:
            params["tags"] = tags
        
        base64_custom_data = None
        if cmd_params.custom_data is not None:
            base64_custom_data = self.get_base64_string(cmd_params.custom_data)
        
        if(base64_custom_data is not None):
            params["virtual_machine_profile"]["os_profile"]["custom_data"] = base64_custom_data
     
        dd_arr = []
        if(cmd_params.data_disks is not None):
            arr = cmd_params.data_disks.split(",")
            for index,val in enumerate(arr):    
                dd = {
                    "disk_size_gb": val,
                    "lun": index,
                    "create_option": "Empty"
                    }
                dd_arr.append(dd)
            params["virtual_machine_profile"]["storage_profile"]["data_disks"] = dd_arr
   
     # Install Linux MSI extension
        if (cmd_params.keyvault_name is not None
            and cmd_params.keyvault_rg_name is not None
            and cmd_params.keyvault_secret_name is not None ):
            extension_profile = {
                "extensions":[{
                    "name": cmd_params.vmss_name + "linuxmsiext",
                    "publisher": "Microsoft.ManagedIdentity",
                    "type": "ManagedIdentityExtensionForLinux",
                    "type_handler_version": "1.0",
                    "settings": {"port": 50342}
                    }]
                }
            params["virtual_machine_profile"]["extension_profile"] = extension_profile
        return params

    def create_rg_deletion_job(
            self, request_id, resource_client, scheduler_client, 
            group_name, location, schedule, app_settings):

        if(schedule.lower() == "now"):
           start_time = datetime.datetime.now() - datetime.timedelta(
               days = 1)
        else:
           start_time = datetime.datetime.strptime(
               schedule, "%Y%m%d%H%M")

        # Create job collection
        self.create_scheduler_job_collection(
            request_id, resource_client, scheduler_client, 
            app_settings.job_group_name, location, 
            app_settings.job_collection_name, 
            self.get_job_collection_sku(
                app_settings.job_collection_sku)
            )   

        # Create cleaner job to delete all the expired job
        self.create_cleaner_job(
            request_id, scheduler_client, 
            location, app_settings)

        # Create deletion job
        job_name = "Dj" + group_name
        self.write_message(
            request_id, 
            "Creating job '{}' in '{}' job collection.{}".format(
                job_name, app_settings.job_collection_name, 
                double_line_break)
            )

        rg_delete_job_body = {
            "ResourceGroupName": group_name, 
            "SecureToken": app_settings.runbook_token,
            "ExecutionMode": "DeleteResource"
            }

        # Create Deletion Job properties
        prop = self.create_scheduler_job_properties(
            start_time, app_settings.webhook_url, rg_delete_job_body)        

        job_async = scheduler_client.jobs.create_or_update(
            app_settings.job_group_name, 
            app_settings.job_collection_name, job_name, 
            properties=prop)

        self.write_message(
            request_id, 
            "Creating job: complete{}".format(double_line_break)
            )

        self.write_message(
            request_id, 
            "Scheduled VMSS deletion: {} UTC{}".format(
                start_time, double_line_break)
            )

        if(schedule.lower() == "now"):
            scheduler_client.jobs.run(
                group_name, app_settings.job_collection_name, 
                job_name)
            self.write_message(
                request_id, 
                "Started '{}' job.{}".format(
                    job_name, double_line_break)
                )

    # Create Azure key vault with access policy
    def create_keyvault(
            self, request_id, client_libs, keyvault_params, 
            app_settings):

        self.create_resource_group(
            request_id, client_libs.resource, 
            keyvault_params.group_name, keyvault_params.location)
        
        ad_group_object_id = self.get_existing_ad_group_object_id(
            client_libs.aad, app_settings.ad_group_name)
        if(ad_group_object_id is None):
            ad_group_object_id = self.create_ad_group(
                request_id, client_libs.aad, app_settings.ad_group_name)
        else:
            message = "Using existing Azure AD group:"
            self.write_message(
                request_id, 
                "{} {}{}".format(
                    message, app_settings.ad_group_name, 
                    double_line_break)
                )

        keyvault_users_object_ids = keyvault_params.users.split(',')
        keyvault_users_object_ids.append(ad_group_object_id)

        self.write_message(
            request_id, 
            "Creating keyvault '{}' {}".format(
                keyvault_params.name, double_line_break)
            )
        tenant_id = app_settings.tenant_id
        access_policies = []
        for object_id in keyvault_users_object_ids:
            access_policies.append(
                self.create_access_policy_entry(
                    tenant_id, object_id)
                )
        param = {
            'location': keyvault_params.location,
            'properties': {
                'sku': {
                    'name': keyvault_params.sku
                    },
                'tenant_id': tenant_id,
                'access_policies': access_policies
                }
            }
        keyvaults = client_libs.keyvault.vaults.list_by_resource_group(
            keyvault_params.group_name)
        for keyvault in keyvaults:
            keyvault_name = keyvault_params.name
            if keyvault.name.lower() == keyvault_name.lower():
                self.write_message(
                    request_id, 
                    "{} '{}' keyvault already exists {}".format(
                        error_creating_keyvault, keyvault_name, 
                        double_line_break)
                    )
                return message
        
        keyvault = client_libs.keyvault.vaults.create_or_update(
            keyvault_params.group_name, keyvault_params.name, param)
        
        self.write_message(
            request_id, 
            "Keyvault creation completed {}".format(
                double_line_break)
            )
        return "NULL"

    def create_access_policy_entry(
            self, tenant_id, object_id):
        return AccessPolicyEntry(
            tenant_id, object_id, Permissions(
                keys=['all'], secrets=['all'], certificates=['all'])
            )

    def get_existing_keyvault(
            self, keyvault_client, app_settings, group_name, 
            keyvault_name):
        keyvaults = keyvault_client.vaults.list_by_resource_group(
            group_name)
        for keyvault in keyvaults:
            if keyvault.name.lower() == keyvault_name.lower():
                return keyvault
        return None

    # Create virtual machine scale set based on input parameters
    def create_vmss(
            self, request_id, client_libs, app_settings, 
            cmd_params):

        aad_client = client_libs.aad
        compute_client = client_libs.compute
        network_client = client_libs.network
        resource_client = client_libs.resource

        ad_group_object_id = None
        keyvault = None

        if(cmd_params.keyvault_name is not None
           and cmd_params.keyvault_rg_name is not None
           and cmd_params.keyvault_secret_name is not None):
            keyvault = self.get_existing_keyvault(
                client_libs.keyvault, app_settings,
                cmd_params.keyvault_rg_name, 
                cmd_params.keyvault_name)
            if(keyvault is None):
                message = "{} '{}' key vault in '{}' resource group doesn't exist".format(
                    error_creating_vmss, cmd_params.keyvault_name, 
                    cmd_params.keyvault_rg_name)
                self.write_message(
                    request_id, "{}{}".format(
                        message, double_line_break))
                return message
            ad_group_object_id = self.get_existing_ad_group_object_id(
                aad_client, app_settings.ad_group_name)
            if(ad_group_object_id is None):
                message = "{} '{}' Azure AD group doesn't exist".format(
                    error_creating_vmss, app_settings.ad_group_name)
                self.write_message(
                    request_id, "{}{}".format(
                        message, double_line_break))
                return message

        # Create deletion job
        if(cmd_params.deletion_job):
              self.create_rg_deletion_job(
                  request_id, resource_client, 
                  client_libs.scheduler, cmd_params.group_name, 
                  cmd_params.location, cmd_params.schedule, 
                  app_settings)

        self.create_resource_group(
            request_id, resource_client, cmd_params.group_name, 
            cmd_params.location)
        #Create Virtual Network
        if (cmd_params.vnet_name is not None
                and cmd_params.vnet_rg_name is not None):
            self.write_message(
                request_id, 
                "Using existing vnet '{}'{}".format(
                    cmd_params.vnet_name, double_line_break)
                )
            vnet = network_client.virtual_networks.get(
                cmd_params.vnet_rg_name, cmd_params.vnet_name)
            if len(vnet.subnets) > 0 :
                subnet_info = vnet.subnets[0]
            else:
                # Create Subnet
                self.write_message(
                    request_id, 
                    "Creating subnet{}".format(double_line_break)
                    )
                address_prefix_ip = vnet.address_space.address_prefixes[0]
                async_subnet_creation = network_client.subnets.create_or_update(
                    cmd_params.vnet_rg_name, cmd_params.vnet_name, 
                    cmd_params.subnet_name,
                    {"address_prefix": "{}".format(address_prefix_ip)})
                subnet_info = async_subnet_creation.result()
        else:
            vnet_rg_name = cmd_params.group_name
            vnet_name = cmd_params.group_name + "vnet"
            self.create_virtual_network(
                request_id, network_client, cmd_params.location, 
                vnet_rg_name, vnet_name)
            # Create Subnet
            subnet_info = self.create_subnet(
                request_id, network_client, vnet_rg_name, 
                vnet_name, cmd_params.subnet_name)

        # Create load balancer
        public_ip_name = cmd_params.group_name + "pip"
        lb_name = cmd_params.group_name + "lb"
        lb_front_ip = cmd_params.group_name + "lbfrontip"
        lb_addr_pool = cmd_params.group_name + "loadaddrpool"
        lb_prob = cmd_params.group_name + "loadprob"
        load_balancer_info = self.create_load_balancers(
            request_id, network_client, cmd_params.group_name, 
            cmd_params.location, public_ip_name, lb_front_ip, 
            lb_addr_pool, lb_prob, lb_name)        

        # Create image from VHD
        image_id = None
        image_name = cmd_params.group_name + "image"
        if "https://" in cmd_params.vmss_reference:
            image = self.create_image_from_vhd(
                request_id, compute_client, cmd_params.group_name, 
                image_name, cmd_params.location, cmd_params.os_type, 
                cmd_params.vmss_reference)
            image_id = image.id 

        # Create VMSS
        self.write_message(
            request_id, "Creating vmss '{}' {}".format(
                cmd_params.vmss_name, double_line_break)
            )
        vmss_parameters = self.create_vmss_parameters(
            cmd_params, subnet_info.id, image_id, load_balancer_info)
        async_vmss_creation = compute_client.virtual_machine_scale_sets.create_or_update(
            cmd_params.group_name, cmd_params.vmss_name, 
            vmss_parameters)
        vmss_info = async_vmss_creation.result()

        if(vmss_info.identity is not None):
            vmss_principal_id = vmss_info.identity.principal_id

        # Download secret from Azure keyvault
        if (app_settings.shell_script_url is not None
            and cmd_params.keyvault_name is not None
            and cmd_params.keyvault_rg_name is not None
            and cmd_params.keyvault_secret_name is not None):
            
            # Add VMSS to AAD group
            base_url = "https://graph.windows.net"
            vmss_url = "{}/{}/directoryObjects/{}".format(
                base_url, app_settings.tenant_id, vmss_principal_id)

            message = "Adding VMSS in Azure AD security group:"
            self.write_message(
                request_id, 
                "{} {}{}".format(
                    message, app_settings.ad_group_name, 
                    double_line_break)
                )
            aad_client.groups.add_member(
                ad_group_object_id, vmss_url)
            self.write_message(
                request_id, 
                "{} complete{}".format(message, double_line_break)
                )

            # Create extension to run shell script
            self.write_message(
                request_id, 
                "Creating extension for shell script{}".format(
                    double_line_break)
                )
            
            self.waiting_for_succeeded_status(
                compute_client.virtual_machine_scale_set_vms, 
                cmd_params.group_name, cmd_params.vmss_name)

            extension = self.install_download_secret_extension(
                compute_client.virtual_machine_scale_set_extensions, 
                cmd_params.group_name, cmd_params.vmss_name, 
                app_settings.shell_script_url, cmd_params.keyvault_name, 
                cmd_params.keyvault_secret_name, app_settings.tenant_id)
            self.write_message(
                request_id, 
                "Creating extension: complete{}".format(
                    double_line_break)
                )

            # Update the VMSS instances to latest model
            self.write_message(
                request_id, 
                "Updating VMSS instances to latest model {}".format(
                    double_line_break)
                )
            vmss_vms = compute_client.virtual_machine_scale_set_vms
            vmss_instances = vmss_vms.list(
                cmd_params.group_name, cmd_params.vmss_name)

            updated_vmss_instances = self.update_vmss_instances(
                compute_client.virtual_machine_scale_sets, 
                vmss_instances, cmd_params.group_name, 
                cmd_params.vmss_name)
            
            self.write_message(
                request_id, 
                "Updating VMSS instances: complete {}".format(
                    double_line_break)
                )
        message = "VMSS creation completed"
        self.write_message(
            request_id, "{}{}".format(
                message, double_line_break))
        return "NULL"

    # Shows error for missing information
    # in AppSettings.txt file
    def show_missing_info_error(self, request_id, missing_info_name):
        missing_info_error = "Missing information in AppSettings.txt:"
        self.write_message(
            request_id, 
            "{} {} {}".format(
                missing_info_error, missing_info_name, 
                double_line_break)
            )

    def create_ad_group(
            self, request_id, aad_client, ad_group_name):
        message = "Creating Azure AD security group:"
        self.write_message(
            request_id, 
            "{} {}{}".format(
                message, ad_group_name, double_line_break)
            )
        ad_group = aad_client.groups.create(
            ad_group_name, ad_group_name + "mail")
        self.write_message(
            request_id, 
            "{} complete{}".format(
                message, double_line_break)
            )
        return ad_group.object_id

    def get_existing_ad_group_object_id(
            self, aad_client, ad_group_name):
        filter_value = "displayName eq '{}'".format(ad_group_name)
        ad_groups = aad_client.groups.list(filter=filter_value)
        for ad_group in ad_groups:
            return ad_group.object_id
        return None

    # Create cleaner job to delete all the expired job
    def create_cleaner_job(
            self, request_id, scheduler_client, 
            location, app_settings):
        
        self.write_message(
            request_id, 
            "Creating cleaner job {}".format(
                double_line_break)
            )
        cleaner_job_name = "CleanerJob"
        cleaner_job_body = {
            "JobResourceGroupName": app_settings.job_group_name, 
            "JobCollection": app_settings.job_collection_name, 
            "SecureToken": app_settings.runbook_token,
            "ExecutionMode": "DeleteExpiredJobs"
            }
        start_time = datetime.datetime.utcnow() + datetime.timedelta(minutes = 5)
        prop = self.create_scheduler_job_properties(
            start_time, app_settings.webhook_url, cleaner_job_body, 
            app_settings.cleaner_job_frequency_type, 
            app_settings.cleaner_job_interval)
        async_cleaner_job = scheduler_client.jobs.create_or_update(
            app_settings.job_group_name, 
            app_settings.job_collection_name, 
            cleaner_job_name, 
            properties=prop)

        self.write_message(
            request_id, 
            "Creating cleaner job: complete {}".format(
                double_line_break)
            )

    def create_scheduler_job_properties(
            self, start_time, webhook_url, job_body, 
            clean_job_frequency=None, clean_job_interval=None):
        prop = {
            "start_time": start_time,
            "action": {
                "type": "https",                         
                "request": {
                    "method": "POST",
                    "uri": webhook_url,
                    "headers":{
                        "content-type": "text/plain"
                        },
                    "body": json.dumps(job_body)
                    },
                }
            }
        if (clean_job_frequency is not None
            and clean_job_interval is not None):
            recurrence_value = {
                "frequency": self.get_recurrence_frequency(
                    clean_job_frequency),
                "interval": clean_job_interval
            }
            prop["recurrence"] = recurrence_value

        return prop
    
    # Create extension to run shell script to 
    # download secret from Azure key vault
    def install_download_secret_extension(
            self, vmss_extensions, group_name, vmss_name, 
            script_url, keyvault_name, secret_name, tenant_id):        
        extension = self.get_download_secret_ext_params(
                vmss_name, script_url, keyvault_name, 
                secret_name, tenant_id
                )
        extension_name = "{}_downloadsecret".format(vmss_name)
        async_extension_creation = vmss_extensions.create_or_update(
            group_name, vmss_name, extension_name, extension)
        return async_extension_creation.result()
    
    # Creates parameters for custom shell script extension
    def get_download_secret_ext_params(
            self, vmss_name, script_url, keyvault_name, 
            secret_name, tenant_id):
        pieces = script_url.split("/")
        length = len(pieces)
        filename = pieces[length - 1]
        properties = {
            "publisher": "Microsoft.Azure.Extensions",
            "type": "CustomScript",
            "type_handler_version": "2.0",
            "settings": {
                "fileUris": [script_url],
                "commandToExecute": (
                    "bash {} {} {} {} >> script-execution.log".format(
                        filename, keyvault_name, 
                        secret_name, tenant_id
                        )
                    )
                }
            }
        return properties

    # Wait until the provisioning of all the VMSS nodes is succeeded
    def waiting_for_succeeded_status(
            self, virtual_machine_scale_set_vms, group_name, 
            vmss_name):
        flag = True
        while(flag):
            time.sleep(10)
            vmss_instances = virtual_machine_scale_set_vms.list(
                group_name, vmss_name)
            flag = self.is_provisioning_succeeded(
                virtual_machine_scale_set_vms, group_name, 
                vmss_name, vmss_instances)
    
    # Sends the True when VMSS nodes provisioning is succeeded
    def is_provisioning_succeeded(
            self, virtual_machine_scale_set_vms, group_name, 
            vmss_name, vmss_instances):
        flag = False
        for instance in vmss_instances:
            id = instance.instance_id
            vmss_node = virtual_machine_scale_set_vms.get_instance_view(
                group_name, vmss_name, id)
            statuses = vmss_node.statuses
            num_status = len(statuses)
        
            for i in range(num_status):
                if("SUCCEEDED" not in statuses[i].code.upper()
                    and "RUNNING" not in statuses[i].code.upper()):
                    flag =  True        
        return flag
        
    # Update the VMSS instances to latest model
    def update_vmss_instances(
            self, vmss, vmss_instances, group_name, vmss_name):
        instance_ids = []
        for instance in vmss_instances:
            instance_ids.append(instance.instance_id)
        async_vmss_instances_update = vmss.update_instances(
            group_name, vmss_name, instance_ids)
        return async_vmss_instances_update.result()

    def create_scheduler_job_collection(
            self, request_id, resource_client, 
            scheduler_client, job_group_name, location, 
            job_collection_name, sku_name):

        # Create resource group
        self.create_resource_group(
            request_id, resource_client, job_group_name, location)

        # Create job collection
        message = "Creating job collection:"
        self.write_message(
            request_id, 
            "{} {}{}".format(
                message, job_collection_name, double_line_break)
            )
        param = {
            "location": "Central India",
            "properties": {
                "sku": {
                    "name": sku_name
                    },
                "state": "Enabled"
                }
            }
        scheduler_client.job_collections.create_or_update(
            job_group_name, job_collection_name, param)
        self.write_message(
            request_id, 
            "{} complete{}".format(
                message, double_line_break)
            )

    def get_recurrence_frequency(self, frequency):
        result = ""
        if (frequency.upper() in RecurrenceFrequency.minute.value.upper()):
            result = RecurrenceFrequency.minute.value
        elif (frequency.upper() in RecurrenceFrequency.hour.value.upper()):
            result = RecurrenceFrequency.hour.value
        elif (frequency.upper() in RecurrenceFrequency.day.value.upper()):
            result = RecurrenceFrequency.day.value
        elif (frequency.upper() in RecurrenceFrequency.week.value.upper()):
            result = RecurrenceFrequency.week.value
        elif (frequency.upper() in RecurrenceFrequency.month.value.upper()):
            result = RecurrenceFrequency.month.value
        return result

    def get_job_collection_sku(self, sku):
        result = None
        if (sku.upper() in SkuDefinition.free.value.upper()):
            result = SkuDefinition.free.value
        elif (sku.upper() in SkuDefinition.standard.value.upper()):
            result = SkuDefinition.standard.value
        elif (sku.upper() in SkuDefinition.p10_premium.value.upper()):
            result = SkuDefinition.p10_premium.value
        elif (sku.upper() in SkuDefinition.p20_premium.value.upper()):
            result = SkuDefinition.p20_premium.value
        return result

    # Delete VMs
    def delete_vms(
            self, request_id, resource_client, compute_client, 
            group_name, vm_name):
        if(group_name != "" and vm_name != ""):
            self.delete_vm(request_id, compute_client, group_name, vm_name)
        elif(group_name != "" and vm_name == ""):
            self.delete_rg(request_id, resource_client, group_name)
    
    # Delete vmSS
    def delete_vmss(
            self, request_id, client_libs, 
            group_name, vmss_name):

        if(group_name != "" and vmss_name != ""):
            self.delete_vmss_only(
                request_id, client_libs.compute, group_name, vmss_name)
        elif(group_name != "" and vmss_name == ""):
            self.delete_rg(request_id, client_libs.resource, group_name)

    # Delete vm by resource group and vm name
    def delete_vm(
            self, request_id, compute_client, group_name, vm_name):
        self.write_message(
            request_id, 
            "Deleting VM '{}'{}".format(vm_name, double_line_break)
            )
        async_vm_delete = compute_client.virtual_machines.delete(
            group_name, vm_name)
        async_vm_delete.wait()
        self.write_message(
            request_id, 
            "VM '{}' deleted{}".format(vm_name, double_line_break)
            )

    # Delete all artifacts under resource group
    def delete_rg(self, request_id, resource_client, group_name):
        self.write_message(
            request_id, 
            "Deleting resource group '{}'{}".format(
                group_name, double_line_break)
            )
        delete_async_operation = resource_client.resource_groups.delete(
            group_name)
        delete_async_operation.wait()
        self.write_message(
            request_id, 
            "Resource group '{}' deleted{}".format(
                group_name, double_line_break)
            )

    # Delete vmss by resource group and vmss name
    def delete_vmss_only(
            self, request_id, compute_client, group_name, 
            vmss_name):
        self.write_message(
            request_id, 
            "Deleting VMSS '{}'{}".format(
                vmss_name, double_line_break)
            )
        async_vm_delete = compute_client.virtual_machine_scale_sets.delete(
            group_name, vmss_name)
        async_vm_delete.wait()
        self.write_message(
            request_id, 
            "VMSS '{}' deleted{}".format(
                vmss_name, double_line_break)
            )

    # Return vm properties based on resource group name and vm name
    def list_vm(self, compute_client, group_name, vm_name):

        vm = compute_client.virtual_machines.get(
            group_name, vm_name, expand="instanceView")    
        # vm status i.e Running/Provisioning/Deallocating
        statuses = vm.instance_view.statuses
        num_status = len(statuses)
        str_status_list = []
        
        for i in range(num_status):
            str_status_list.append(statuses[i].code)
            if(i != (num_status - 1)):
                str_status_list.append(",")
        return "{}{}{}{}".format(
            space_separator, self.escape(group_name), 
            space_separator, self.escape(''.join(str_status_list))
            )
    
    # Return vm based on resource group and vm name
    def list_vms_by_rg_vm(
            self, request_id, compute_client, network_client, 
            group_name, vm_name):
        self.write_message(
            request_id, 
            "Listing information of VM: {} in Resource Group: {}{}".format(
                vm_name, group_name, double_line_break)
            )
        vms_info_list = []
        try:
            vm_info = self.list_vm(compute_client, group_name, vm_name) 
            vms_info_list.append(vm_info)                    
        except Exception as e:
            vm_info = self.get_vm_info(
                request_id, compute_client, network_client, 
                group_name, vm.name)
            self.show_vm_info_error(request_id, vm_info, e)
        return vms_info_list

    # Return vm list based on resource group
    def list_vms_by_rg(
            self, request_id, compute_client, network_client, 
            group_name):
        self.write_message(
            request_id, 
            "Listing VMs in resource group: {}{}".format(
                group_name, double_line_break)
            )
        vms_info_list = []
        vm_list = compute_client.virtual_machines.list(group_name)
        vms_info_list = self.run_vm_list_thread(
            request_id, compute_client, network_client, vm_list)
        return vms_info_list 

    # Return vm list based under current subscription
    def list_all_vms(
            self, request_id, compute_client, network_client):
        self.write_message(
            request_id, 
            "Listing all VMs {}".format(
                double_line_break)
            )
        #for vm in compute_client.virtual_machines.list_all():
        #    print(vm.name)
        vm_list = compute_client.virtual_machines.list_all()
        return self.run_vm_list_thread(
            request_id, compute_client, network_client, vm_list)
     
    # List VM by options
    def list_rg(
            self, request_id, compute_client, network_client, 
            group_name, vm_name, tag):
        vms_info_list = []
        if(group_name != "" and vm_name != ""): # return vm based on resource group and vm name
            vms_info_list = self.list_vms_by_rg_vm(
                request_id, compute_client, network_client, 
                group_name, vm_name)
        elif(group_name != "" and vm_name == ""): # return vm based on resource group
            vms_info_list = self.list_vms_by_rg(
                request_id, compute_client, network_client, 
                group_name)
        elif(tag != ""): # return vm based on tag having "key=Group"
            vms_info_list = self.list_vms_by_tag(
                request_id, compute_client, network_client, 
                tag)
        else: # return vm based under current subscription
            vms_info_list = self.list_all_vms(
                request_id, compute_client, network_client)
        return vms_info_list

     # Restart vmss by resource group and vmss name
    def restart_vmss(
            self, request_id, compute_client, group_name, vmss_name):
        self.write_message(
            request_id, 
            "Restarting VMSS '{}'{}".format(
                vmss_name, double_line_break)
            )
        async_vm_restart = compute_client.virtual_machine_scale_sets.restart(
            group_name, vmss_name)
        async_vm_restart.wait()
        self.write_message(
            request_id, 
            "VMSS '{}' restarted{}".format(
                vmss_name, double_line_break)
            )

    # Start vmss by resource group and vmss name
    def start_vmss(
            self, request_id, compute_client, group_name, vmss_name):
        self.write_message(
            request_id, 
            "Starting VMSS '{}'{}".format(
                vmss_name, double_line_break)
            )
        async_vm_deallocate = compute_client.virtual_machine_scale_sets.start(
            group_name, vmss_name)
        async_vm_deallocate.wait()
        self.write_message(
            request_id, 
            "VMSS '{}' started{}".format(
                vmss_name, double_line_break)
            )

    # Deallocate(stop) vmss by resource group and vmss name
    def stop_vmss(
            self, request_id, compute_client, group_name, 
            vmss_name):
        self.write_message(
            request_id, 
            "Deallocating VMSS '{}'{}".format(
                vmss_name, double_line_break)
            )
        async_vm_deallocate = compute_client.virtual_machine_scale_sets.deallocate(
            group_name, vmss_name)
        async_vm_deallocate.wait()
        self.write_message(
            request_id, 
            "VMSS '{}' deallocated{}".format(
                vmss_name, double_line_break)
            )

    # Deallocate(stop) vmss by resource group and vmss name
    def scale_vmss(
            self, request_id, compute_client, group_name, vmss_name, 
            node_count):
        vmss = compute_client.virtual_machine_scale_sets.get(
            group_name, vmss_name)
        capacity = vmss.sku.capacity   
        self.write_message(
            request_id, 
            "Scaling VMSS '{}' instances from {} to {}{}".format(
                vmss_name, capacity, node_count, double_line_break)
            )
        vmss.sku.capacity = node_count     
        async_vmss_scale = compute_client.virtual_machine_scale_sets.create_or_update(
            group_name, vmss_name, vmss)
        async_vmss_scale.wait()
        self.write_message(
            request_id, 
            "VMSS '{}' scaled from instances {} to {}{}".format(
                vmss_name, capacity, node_count, double_line_break)
            )

    # Add command into queue
    def queue_command(self, ci):
        self.command_queue_lock.acquire()
        try:
            self.command_queue.appendleft(ci)
        finally:
            self.command_queue_lock.release()

    # Remove command from queue
    def dequeue_command(self):
        self.command_queue_lock.acquire()
        try:
            ci = self.command_queue.pop()
        finally:    
            self.command_queue_lock.release()
        return ci

    # Queue results
    def queue_result(self, reqId, message):
        self.result_queue_lock.acquire()
        try:
            self.result_queue.appendleft("" + str(reqId) + " " + message)
        finally:    
            self.result_queue_lock.release()

    # Display all results from queue and print results
    def deque_all_results_and_print(self):
        if not self.result_queue:
            sys.stdout.write("S 0{}".format(double_line_break))
            return

        self.result_queue_lock.acquire()
        try:
            sys.stdout.write(
                "S {}{}".format(
                    str(len(self.result_queue)), double_line_break)
                )
            while self.result_queue:
                result = self.result_queue.pop()
                sys.stdout.write(
                    "{}{}".format(result, double_line_break))
        finally:
            self.result_queue_lock.release()

    # Queue list results
    def queue_list_result(self, queue, result):
        self.result_queue_lock.acquire()
        try:
            queue.appendleft(result)
        finally:    
            self.result_queue_lock.release()

    # Display all results from queue
    def deque_all_list_results(self, queue, list_queue):
        vms_info_list = []
        self.result_queue_lock.acquire()
        try:            
            while list_queue:
                result = list_queue.pop()
                vms_info_list.extend(result)
        finally:
            self.result_queue_lock.release()
        return vms_info_list

    # Execute current command from queue
    def execute_command(self):
        ci = self.dequeue_command()
        cmds = GahpCommandBuilder()
        try:
            app_settings = self.read_app_settings_file(ci.cred_file)
            credentials = self.create_credentials_from_file(
                ci.request_id, ci.cred_file)
            if(credentials is None):
                self.queue_result(
                    ci.request_id, self.escape(
                        "Error reading or creating credentials")
                    )
                return
            client_libs = self.create_client_libraries(
                ci.request_id, credentials, ci.subscription, 
                app_settings)
            if(client_libs is None):
                self.queue_result(
                    ci.request_id, self.escape(
                        "Error creating client libraries")
                    )
                return
        except Exception as e:
            error = self.escape(str(e.args[0]))
            self.write_message(
                ci.request_id, 
                "Error before executing command: {}{}".format(
                    error, double_line_break), 
                logging.ERROR)
            self.queue_result(ci.request_id, error)
            return
        if(ci.command.upper() == cmds.create_keyvault):
            try:
                result = self.create_keyvault(
                    ci.request_id, client_libs, ci.cmdParams, 
                    app_settings)
                self.queue_result(ci.request_id, result)
            except Exception as e:
                self.process_error(
                    ci.request_id, e, error_creating_keyvault)
        elif(ci.command.upper() == cmds.ping_azure):
            try:
                self.queue_result(ci.request_id, "NULL")
            except Exception as e:
                self.process_error(ci.request_id, e, error_pinging_azure)
        elif(ci.command.upper() == cmds.create_vm):
            try:               
                result = self.create_vm(
                    ci.request_id, client_libs, ci.cmdParams)
                self.queue_result(ci.request_id, result)
            except Exception as e:
                self.process_error(
                    ci.request_id, e, error_creating_vm)
        elif(ci.command.upper() == cmds.delete_vm):
            try:
                self.delete_vms(
                    ci.request_id, client_libs.resource, 
                    client_libs.compute, ci.cmdParams["rgName"], 
                    ci.cmdParams["vmName"])
                self.queue_result(ci.request_id, "NULL")
            except Exception as e:
                self.process_error(
                    ci.request_id, e, error_deleting_vm)
        elif(ci.command.upper() == cmds.list_vm):
            try:
                vms_info_list = self.list_rg(
                    ci.request_id, client_libs.compute, 
                    client_libs.network, ci.cmdParams["rgName"], 
                    ci.cmdParams["vmName"], ci.cmdParams["tag"])
                result = "NULL {}{}".format(
                    str(len(vms_info_list)), ''.join(vms_info_list))
                self.queue_result(ci.request_id, result)
                self.write_message(
                    ci.request_id, 
                    "VM list result queued{}".format(
                        double_line_break)
                    )
            except Exception as e:
                self.process_error(
                    ci.request_id, e, error_listing_vms)
        elif(ci.command.upper() == cmds.create_vmss):
            try:
                result = self.create_vmss(
                    ci.request_id, client_libs, app_settings, 
                    ci.cmdParams)
                self.queue_result(ci.request_id, result)
            except Exception as e:
                self.process_error(
                    ci.request_id, e, error_creating_vmss)
        elif(ci.command.upper() == cmds.delete_vmss):
            try:
                self.delete_vmss(
                    ci.request_id, client_libs,
                    ci.cmdParams["rgName"], ci.cmdParams["vmssName"])
                self.queue_result(ci.request_id, "NULL")
            except Exception as e:
                self.process_error(
                    ci.request_id, e, error_deleting_vmss)   
        elif(ci.command.upper() == cmds.start_vmss):
            try:
                self.start_vmss(
                    ci.request_id, client_libs.compute, 
                    ci.cmdParams["rgName"], ci.cmdParams["vmssName"])
                self.queue_result(ci.request_id, "NULL")
            except Exception as e:
                self.process_error(
                    ci.request_id, e, error_starting_vmss)
        elif(ci.command.upper() == cmds.stop_vmss):
            try:
                self.stop_vmss(
                    ci.request_id, client_libs.compute, 
                    ci.cmdParams["rgName"], ci.cmdParams["vmssName"])
                self.queue_result(ci.request_id, "NULL")
            except Exception as e:
                self.process_error(
                    ci.request_id, e, error_stoping_vmss)
        elif(ci.command.upper() == cmds.restart_vmss):
            try:
                self.restart_vmss(
                    ci.request_id, client_libs.compute, 
                    ci.cmdParams["rgName"], ci.cmdParams["vmssName"])
                self.queue_result(ci.request_id, "NULL")
            except Exception as e:
                self.process_error(
                    ci.request_id, e, error_restarting_vmss)
        elif(ci.command.upper() == cmds.scale_vmss):
            try:
                self.scale_vmss(
                    ci.request_id, client_libs.compute, 
                    ci.cmdParams["rgName"], ci.cmdParams["vmssName"], 
                    ci.cmdParams["nodeCount"])
                self.queue_result(ci.request_id, "NULL")
            except Exception as e:
                self.process_error(
                    ci.request_id, e, error_scaling_vmss)

    def process_error(self, request_id, e, error_message):
        message = "{}\\ {}".format(
            self.escape(error_message), self.escape(str(e.args[0])))
        self.write_message(request_id, message, logging.ERROR)
        self.queue_result(request_id, message)
    
    # Create vm arguments based on input parameters
    def get_create_keyvault_args(self, request_id, cmd_parts):

        create_keyvault_arg_names = ["name", "location", "sku", 
                                     "users"]
        dnary = dict()
        
        for index, nvp_string in enumerate(cmd_parts):
            if(index < 4):
                continue
            nvp = nvp_string.split("=")
            if(not nvp):
                self.write_message(
                    request_id, 
                    "Empty args{}".format(double_line_break))
                return None
            if(len(nvp) < 2):
                self.write_message(
                    request_id, 
                    "Insufficient args: {}{}".format(
                        nvp_string, double_line_break)
                    )
                return None
            if(nvp[0].lower() not in (val.lower() for val in create_keyvault_arg_names)):
                self.write_message(
                    request_id, 
                    "Unrecognized parameter name: {}{}".format(
                        nvp[0], double_line_break)
                    )
                return None
            dnary[nvp[0]] = nvp[1]

        missing_arg = None
        if dnary["name"] == "":
            missing_arg = "name"
        elif dnary["location"] == "":
            missing_arg = "location"
        elif dnary["sku"] == "":
            missing_arg = "sku"
        elif dnary["users"] == "":
            missing_arg = "users"

        if missing_arg is not None:            
            self.write_message(
                request_id, 
                "Missing mandatory argument value: {}{}".format(
                    missing_arg, double_line_break)
                )
            return None

        return dnary
    
    # Create vm arguments based on input parameters
    def get_create_vm_args(self, request_id, cmd_parts):
        vm_ref = {
            "linux-ubuntu-latest": {
                "publisher": "Canonical", 
                "offer": "UbuntuServer", 
                "sku": "16.04.0-LTS", 
                "version": "latest"
                },
            "windows-server-latest": {
                "publisher": "MicrosoftWindowsServer", 
                "offer": "WindowsServer", 
                "sku": "2012-R2-Datacenter", 
                "version": "latest"
                }
        }
        create_vm_arg_names = [
            "name", "location", "image", 
            "size", "datadisks", "adminusername", 
            "key", "vnetname", "vnetrgname", 
            "publicipaddress", "customdata", "tag", 
            "ostype", "subnetname"
            ]
        dnary = dict()
        
        for index,nvp_string in enumerate(cmd_parts):
            if(index < 4):
                continue
            nvp = nvp_string.split("=")
            if(not nvp):
                self.write_message(
                    request_id, 
                    "Empty args{}".format(double_line_break))
                return None
            if(len(nvp) < 2):
                self.write_message(
                    request_id, 
                    "Insufficient args: {}{}".format(
                        nvp_string, double_line_break)
                    )
                return None
            if(nvp[0].lower() not in (val.lower() for val in create_vm_arg_names)):
                self.write_message(
                    request_id, 
                    "Unrecognized parameter name: {}{}".format(
                        nvp[0], double_line_break)
                    )
                return None
            if(nvp[0].lower() == "ostype"):
                dnary["ostype"] = nvp[1].title()             
            if(nvp[0].lower() == "image"):
                dnary["image"] = nvp[1]
                if(nvp[1].lower() == "linux-ubuntu-latest"):
                    dnary["vmref"] = vm_ref[nvp[1].lower()]
                    dnary["ostype"] = "Linux"
                elif(nvp[1].lower() == "windows-server-latest"):
                    dnary["vmref"] = vm_ref[nvp[1].lower()]
                    dnary["ostype"] = "Windows"
                elif("https://" in nvp[1].lower()):
                    dnary["vmref"] = nvp[1].strip()
                else:
                    self.write_message(
                    request_id, 
                    "Unrecognized image: {}{}".format(
                        nvp[1], double_line_break)
                    )
                    return None
            else:
                dnary[nvp[0].lower()] = nvp[1]

        missing_arg = None
        if dnary["name"] == "":
            missing_arg = "name"
        elif dnary["location"] == "":
            missing_arg = "location"
        elif dnary["size"] == "":
            missing_arg = "size"
        elif dnary["image"] == "":
            missing_arg = "image"

        if missing_arg is not None:            
            self.write_message(
                request_id, 
                "Missing mandatory argument value: {}{}".format(
                    missing_arg, double_line_break)
                )
            return None

        if "datadisks" in dnary:
            is_invalid_data_disks = False
            try:
                dds_arg = dnary["datadisks"]
                dds = dds_arg.split(",")
                for dd in dds:
                    each_dd_value = int(dd)
                    if(each_dd_value < 1 or each_dd_value > 4095):
                        is_invalid_data_disks = True
                        break
            except:
                is_invalid_data_disks = True

            if is_invalid_data_disks:
                self.write_message(
                    request_id, 
                    "Invalid value for VM data disks: '{}'.{}".format(
                        dnary["datadisks"], data_disks_value_format, 
                        double_line_break)
                    )
                return None

        return dnary        

    # Create vm arguments based on input parameters
    def get_create_vmss_args(self, request_id, cmd_parts):
        vm_ref = {
            "linux-ubuntu-latest": {
                "publisher": "Canonical", 
                "offer": "UbuntuServer", 
                "sku": "16.04.0-LTS", 
                "version": "latest"
                },
            "windows-server-latest": {
                "publisher": "MicrosoftWindowsServer", 
                "offer": "WindowsServer", 
                "sku": "2012-R2-Datacenter", 
                "version": "latest"
                }
            }
        create_vmss_arg_names = [
            "deletionjob", "name", "location", "image", "size", 
            "datadisks", "adminusername", "key", "vnetname", 
            "vnetrgname", "publicipaddress", "customdata", "tag", 
            "ostype", "nodecount", "schedule", "keyvaultrg", 
            "keyvaultname", "vaultkey"]
        dnary = dict()
        
        for index,nvp_string in enumerate(cmd_parts):
            if(index < 4):
                continue
            nvp = nvp_string.split("=")
            if(nvp[0].lower() == "deletionjob"):
                dnary["deletionjob"] = True                
                continue
            if(not nvp):
                self.write_message(
                    request_id, 
                    "Empty args{}".format(double_line_break))
                return None
            if(len(nvp) < 2):
                self.write_message(
                    request_id, 
                    "Insufficient args: {}{}".format(
                        nvp_string, double_line_break)
                    )
                return None
            if(nvp[0].lower() not in (val.lower() for val in create_vmss_arg_names)):
                self.write_message(
                    request_id, 
                    "Unrecognized parameter name: {}{}".format(
                        nvp[0], double_line_break)
                    )
                return None
            if(nvp[0].lower() == "ostype"):
                dnary["ostype"] = nvp[1].title()             
            if(nvp[0].lower() == "image"):
                dnary["image"] = nvp[1]
                if(nvp[1].lower() == "linux-ubuntu-latest"):
                    dnary["vmssref"] = vm_ref[nvp[1]]
                    dnary["ostype"] = "Linux"
                elif(nvp[1].lower() == "windows-server-latest"):
                    dnary["vmssref"] = vm_ref[nvp[1]]
                    dnary["ostype"] = "Windows"
                elif("https://" in nvp[1].lower()):
                    dnary["vmssref"] = nvp[1].strip()
                else:
                    self.write_message(
                    request_id, 
                    "Unrecognized image: {}{}".format(
                        nvp[1], double_line_break)
                    )
                    return None
            else:
                dnary[nvp[0].lower()] = nvp[1]

        missing_arg = None
        if dnary["name"] == "":
            missing_arg = "name"
        elif dnary["location"] == "":
            missing_arg = "location"
        elif dnary["size"] == "":
            missing_arg = "size"
        elif dnary["image"] == "":
            missing_arg = "image"

        if missing_arg is not None:            
            self.write_message(
                request_id, 
                "Missing mandatory argument value: {}{}".format(
                    missing_arg, double_line_break)
                )
            return None

        if "nodecount" in dnary:
            is_invalid_node_count = False
            try:
                node_count = int(dnary["nodecount"])
                if(node_count < 0):
                    is_invalid_node_count = True
            except:
                is_invalid_node_count = True

            if is_invalid_node_count:
                self.write_message(
                    request_id, 
                    "Invalid value for VMSS node count: '{}'.{}{}".format(
                        dnary["nodecount"], vmss_node_count_value_format, 
                        double_line_break)
                    )
                return None

        if "datadisks" in dnary:
            is_invalid_data_disks = False
            try:
                dds_arg = dnary["datadisks"]
                dds = dds_arg.split(",")
                for dd in dds:
                    each_dd_value = int(dd)
                    if(each_dd_value < 1 or each_dd_value > 4095):
                        is_invalid_data_disks = True
                        break
            except:
                is_invalid_data_disks = True

            if is_invalid_data_disks:
                self.write_message(
                    request_id, 
                    "Invalid value for VMSS data disks: '{}'.{}{}".format(
                        dnary["datadisks"], data_disks_value_format, 
                        double_line_break)
                    )
                return None

        if "deletionjob" in dnary:
            arg = "schedule"
            if arg in dnary:
                if dnary["schedule"] == "":
                    self.write_message(
                    request_id, 
                    ("Missing mandatory argument value for deletion job"
                    " functionality: {}{}").format(
                        arg, double_line_break)
                    )
                    return None
                else:
                    if len(dnary["schedule"]) != 12:
                        self.write_message(
                            request_id,
                            "Unrecognized schedule. Required format: {}=YYYYmmddHHMM {}".format(
                                arg, double_line_break)
                            )
                        return None
            else:
                self.write_message(
                    request_id,
                    ("Missing mandatory argument for deletion job"
                    " functionality: {}{}").format(
                        arg, double_line_break)
                    )
                return None
        
        return dnary

    # Return arguments for list vm command
    def get_list_vm_args(self, cmd_parts):       
        dnary = dict() 
        dnary["rgName"] = ""
        dnary["vmName"] = ""
        dnary["tag"] = "" 

        if(len(cmd_parts) > 5):
            dnary["rgName"] = cmd_parts[4]
            dnary["vmName"] = cmd_parts[5]
        elif(len(cmd_parts) > 4):
            nvp_strings = cmd_parts[4].split("=")
            if(len(nvp_strings) > 1):
                dnary["tag"] = nvp_strings[1]
            else:
                dnary["rgName"] = cmd_parts[4]
        return dnary

    # Return arguments for delete vm command
    def get_delete_vm_args(self, cmd_parts):       
        dnary = dict() 
        dnary["rgName"] = ""
        dnary["vmName"] = ""
        dnary["tag"] = "" 

        if(len(cmd_parts) > 5):
            dnary["rgName"] = cmd_parts[4]
            dnary["vmName"] = cmd_parts[5]
        elif(len(cmd_parts) > 4):
            nvp_strings = cmd_parts[4].split("=")
            if(len(nvp_strings) > 1):
                dnary["tag"] = nvp_strings[1]
            else:
                dnary["rgName"] = cmd_parts[4]
        else:
            self.write_message(
                request_id, 
                "Insufficient parameters{}".format(
                    double_line_break)
                )
            return None
        return dnary

    # Return arguments for delete vmss commands
    def get_delete_vmss_args(self, request_id, cmd_parts):       
        dnary = dict()
        dnary["rgName"] = ""
        dnary["vmssName"] = ""

        vmss_arg_names = ["rgName","vmssName"]
        
        if(len(cmd_parts) > 5):
            dnary["rgName"] = cmd_parts[4]
            dnary["vmssName"] = cmd_parts[5]
        elif(len(cmd_parts) > 4):
            dnary["rgName"] = cmd_parts[4]
            dnary["vmssName"] = ""
        else:
            self.write_message(
                request_id, 
                "Insufficient args{}".format(
                    double_line_break)
                )
            return None
        return dnary

    # Return arguments for start, stop and restart vmss commands
    def get_vmss_args(self, request_id, cmd_parts):       
        dnary = dict()
        dnary["rgName"] = ""
        dnary["vmssName"] = ""

        vmss_arg_names = ["rgName","vmssName"]
        
        if(len(cmd_parts) > 5):
            dnary["rgName"] = cmd_parts[4]
            dnary["vmssName"] = cmd_parts[5]
        else:
            self.write_message(
                request_id, 
                "Insufficient args {}".format(
                    double_line_break)
                )
            return None
        return dnary

    # Return arguments for vmss commands
    def get_scale_vmss_args(self, request_id, cmd_parts):
        dnary = dict() 
        dnary["rgName"] = ""
        dnary["vmssName"] = ""
        dnary["nodeCount"] = "" 

        is_invalid_node_count = False
        if(len(cmd_parts) > 6):
            dnary["rgName"] = cmd_parts[4]
            dnary["vmssName"] = cmd_parts[5]
            try:
                node_count = int(cmd_parts[6])
                if(node_count >= 0):
                    dnary["nodeCount"] = node_count
                else:
                    is_invalid_node_count = True
            except:
                is_invalid_node_count = True

            if is_invalid_node_count:
                self.write_message(
                    request_id, 
                    "Invalid value for VMSS node count: '{}'.{}{}".format(
                        cmd_parts[6], vmss_node_count_value_format, 
                        double_line_break)
                    )
                return None
        else:
            self.write_message(
                request_id, 
                "Insufficient args {}".format(
                    double_line_break)
                )
            return None
        return dnary

    # Handle command and validate input parameters
    def handle_command(self, cmd_parts):
        ci = AzureGAHPCommandInfo()
        cmds = GahpCommandBuilder()
        ci.command = cmd_parts[0]
        ci.request_id = cmd_parts[1]
        ci.cred_file = cmd_parts[2]
        ci.subscription = cmd_parts[3]
        if(ci.command.upper() == cmds.create_keyvault):
            cmdParams = self.get_create_keyvault_args(
                ci.request_id, cmd_parts)
            if(cmdParams is None):
                return False
            ci.cmdParams = GahpKeyvaultParametersBuilder(cmdParams)
        elif(ci.command.upper() == cmds.create_vm):
            cmdParams = self.get_create_vm_args(
                ci.request_id, cmd_parts)
            if(cmdParams is None):
                return False
            ci.cmdParams = GahpVmCreateParametersBuilder(cmdParams)
            #ci.cmdParams = cmdParams
        elif(ci.command.upper() == cmds.delete_vm):
            cmdParams = self.get_delete_vm_args(cmd_parts)
            if(cmdParams is None):
                return False
            ci.cmdParams = cmdParams
        elif(ci.command.upper() == cmds.list_vm):
            cmdParams = dict()
            cmdParams = self.get_list_vm_args(cmd_parts)
            if(cmdParams is None):
                return False           
            ci.cmdParams = cmdParams
        elif(ci.command.upper() == cmds.create_vmss):
            cmdParams = self.get_create_vmss_args(
                ci.request_id, cmd_parts)
            if(cmdParams is None):
                return False            
            ci.cmdParams = GahpVmssCreateParametersBuilder(cmdParams)
        elif(ci.command.upper() == cmds.delete_vmss):
            cmdParams = self.get_delete_vmss_args(
                ci.request_id, cmd_parts)
            if(cmdParams is None):
                return False
            ci.cmdParams = cmdParams
        elif(ci.command.upper() == cmds.restart_vmss 
             or ci.command.upper() == cmds.start_vmss 
             or ci.command.upper() == cmds.stop_vmss):
            cmdParams = self.get_vmss_args(
                ci.request_id, cmd_parts)
            if(cmdParams is None):
                return False
            ci.cmdParams = cmdParams
        elif(ci.command.upper() == cmds.scale_vmss):
            cmdParams = self.get_scale_vmss_args(
                ci.request_id, cmd_parts)
            if(cmdParams is None):
                return False
            ci.cmdParams = cmdParams
        elif(ci.command.upper() == cmds.ping_azure):
            ci.cmdParams = None
        else:
            cmdParams = None
        self.queue_command(ci)
        t = AzureGAHPCommandExecThread(self)
        t.start()
        return True

    # Handle escaping of space and backslash
    def escape(self, msg):
        return msg.replace(
            single_backslash_separator, 
            double_backslash_separator).replace(
                space_separator, 
                single_backslash_space_separator)

    # Method will return vm related info to showcase in error log
    def get_vm_info(self, request_id, compute_client, network_client, resource_group, vm_name):
        vm_info = dict()
        try:
            # read virtual machine based on group and vm name
            result = compute_client.virtual_machines.get(resource_group, vm_name)
            # read vm id
            vm_info["vm_id"] = result.vm_id
            # get network interface refernce
            ni_reference = result.network_profile.network_interfaces[0]
            ni_reference = ni_reference.id.split("/")
            ni_group = ni_reference[4]
            ni_name = ni_reference[8]
            # get network interface
            net_interface = network_client.network_interfaces.get(ni_group, ni_name)
            ip_reference = net_interface.ip_configurations[0].public_ip_address
            ip_reference = ip_reference.id.split("/")
            ip_group = ip_reference[4]
            ip_name = ip_reference[8]
            # get public ip info
            public_ip = network_client.public_ip_addresses.get(ip_group, ip_name)
            public_ip_add = public_ip.ip_address
            vm_info["public_ip"] = public_ip_add
        except Exception as e :
            error = self.escape(str(e.args[0]))
            self.write_message(
                request_id, 
                "Error getting VM info: {}{}".format(
                    error, double_line_break), 
                logging.ERROR)        
        return vm_info

    # Write the error on console
    def show_vm_info_error(self, vm_info, e):
        error = self.escape(("{} {} {}").format(
                    vm_info["vm_id"], 
                    self.escape(vm_info["public_ip"]), 
                    self.escape(str(e.args[0]))
                    ))
        self.queue_result(request_id, error)
        self.write_message(
            request_id,
            "{} {}".format(error, double_line_break)
            )
    
#### END CLASSES ################################
## AzureGAHPLib.py ############################

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

agce = AzureGAHPCommandExec()
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
    input_line = input_line.replace(double_backslash_separator, single_backslash_separator)

    # Spec: In the event that a string argument needs to contain a <SP> within
    # the string itself, it must be escaped by placing a backslash ("\")
    input_line = input_line.replace(single_backslash_space_separator, unique_str)

    # Replace "\ " with unqiue string with chars is not allowed so that the

    #set running mode
    agce.check_mode(input_line)
    #remove debug and verbose string
    input_line = input_line.replace(debug,'').replace(verbose,'').strip()

    # rest of the string can be split by space
    input_command_params = input_line.split(
        space_separator)
    # Validate input parameters
    if (len(input_command_params) < 1):
        print("E Invalid command")
        continue
    
    s_alphabet = "S"
    common_paramters = "<SP><request-ID><SP><cred-file><SP><subscription>"

    min_params_count = GahpMinParamsCountBuilder()
    cmds = GahpCommandBuilder()
    cmds_usage = GahpCommandUsageBuilder()
    
    for i in range(len(input_command_params)):
        input_command_params[i] = input_command_params[i].replace(
            unique_str, space_separator)

    input_command = input_command_params[0]
    if(input_command.upper() == cmds.commands):
        all_commands = vars(cmds)
        sys.stdout.write("{}".format(s_alphabet))
        for command in all_commands.values():
            sys.stdout.write(" {}".format(command))
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
        print("E Unsupported command: To see a list of valid commands use COMMANDS")

##### END MAIN PROGRAM ##############
