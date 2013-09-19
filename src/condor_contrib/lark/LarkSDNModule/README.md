htcondor-sdn-module
===================

Developing SDN module to support job scheduling at network layer

1. Overview of System Structure
-------------------------------

The sdn module has four components: network setup script, network cleanup script,
htcondor module and pox controller component.

For each job in Lark, before the network configuration is finished, a callout 
network setup script is called. The inputs to the setup script are the job 
and machine ClassAds. 

The setup script sents the ClassAds to a htcondor module that runs at the same 
remote host as the openflow controller (in our case, a pox controller) does. 
The htcondor module performs like a TCP socket server, which can handle the 
request from multiple jobs. Whenever a job sents the ClassAd to the htcondor 
module, the module parses the ClassAds and gather useful info such like 
the username of submitter, the IP address of the machine where the job will 
execute. HTcondor module store all these information in a dictionary and the 
IP address of that job is the key.

A pox controller component is also implemented, aiming to utilize the parsed 
info from the htcondor module to make network scheduling. The controller sends 
out corresponding openflow action messages and install openflow rules  to the 
openvswitch or higher-level openflow switch to determine how the switches should 
handle the incoming packets according to the predefined network policies.

After each job finishes executation, the network cleanup script would be invoked 
and the inputs to the clenaup script are also the job and machien ClassAds. It 
sends the ClassAds to the htcondor module to let htcondor module know the classad 
info stored for IP address associated with this specific job should be deleted.

2. Network Policy Examples
-------------------------

Right now, there are some example polices applied on the openflow controller. The 
owners of submitted jobs are divided into three groups. Group 1 is for blocked users.
Users fall in this group are blocked to communicate with anywhere. As long as the 
openflow controller see the traffic coming from this user, it would drop the packet. 
Group 2 is for users that are blocked to communicate with outside network. At the same 
time, it cannot communicate with jobs from other users. Group 3 has most freedom, it 
can communicate with outside netowrk. The only constraint is that it cannot talk to 
jobs from other users either. We want to isolate jobs from different users by doing this.

3. Usage
--------

To use this software, please make sure you have pox controller source code. Source code 
can be found at github: http://github.com/noxrepo/pox. Also openvswitch is required to 
install on worker node if you want to use this openflow controller to controll the virtual 
switch on each worker node.

At each worker node, first set the openvswitch fail mode to be standalone. In this case, 
if the controller does not work well or the connection to openflow controller is broken, 
openvswitch can go back to standalone mode to perform like a regular l2 virtual switch. 

$ sudo ovs-vsctl set-fail-mode your_bridge_name standalone

Then, set the openflow controller host and port that openvswitch should listen to:

$ sudo ovs-vsctl set-controller your_bridge_name tcp:$(host):$(port)

The host and port are corresponding to the IP address and port openflow controller is bind to.

At the controller host, just put two files: job_aware_switch.py and htcondor_module.py under 
directory ~/pox/ext/ and make sure you python version is 2.7. Go to pox top level directory do

$ ./pox.py log.level --DEBUG job_aware_switch htcondor_module

This will make openflow controller listen on all the interface (0.0.0.0) and port 6633 by 
default. If you want to change the port, you can also indicate that by:

$ ./pox.py OpenFlow.of_01 --address=xxx.xxx.xxx.xxx --port=1234 job_aware_switch htcondor_module

4. Condor Related Configuration
-------------------------------

To use this module, serveral macros need to be configured in condor config files.
BLOCKED_USERS: condor users who are blocked to communicate with anywhere, e.g. BLOCKED_USERS = zzhang, bbockelman

BLOCKED_USERS_OUTSIDE: condor users who are blocked to communicate with outside network, e.g. BLOCKED_USERS_OUTSIDE = gattebury

HTCONDOR_MODULE_HOST: IP address of the host where htcondor module and sdn controller is running

HTCONDOR_MODULE_PORT: Port of the host where htcondor module binds to

WHITE_LIST_IP: the white list of IP addresses that job can comminicate with

NETWORK_NAMESPACE_CREATE_SCRIPT = /path/to/lark_setup_script.py

NETWORK_NAMESPACE_DELETE_SCRIPT = /path/to/lark_cleanup_script.py










