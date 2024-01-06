I am working on pcap analysis, looking at network traffic, currently from tcpdump captures.
I am getting sucked in to (willingly) more and more  site problems in this area.

Part of this is a process called a connectivity test that looks at the collection of config files both modbus and dnp3 to determine the expected system topology.
This could easily be extended to include ssh and web connections if required.
 
If requested at run time , this system analysis will try both a ping and a port connect.

A ping failure does not mean a network failure . A port connect failure is more serious.
Note that a port connect attempt may, briefly, disrupt old dnp3 system connections. 

While running these connectivity tests I intend to automatically run tcpdump captures.

The pcap analysis of the network traffic during these connection attempts will contain far more details than the simple ping test.

So, while ping is useful, I think the complete pcap analysis will provide more details about the system state.
It will not matter if we have ping or not.

This connectivity test will publish results using fims so that it can then interface with any system UI components.

Indeed, I propose that the system connectivity test can be initiated from a UI control.

Still in my first week of study on this but I already have about 70% of the modbus traffic analysis working including detecting   connection attempts, retries, heartbeats.

Alongside this system proposal is an encapsulation of the continuous tcpdump monitor we have deployed in the past.
 
This was using "run by hand" shell scripts.

I have moved this process to a python tool that can be controlled by systemctl, once again possibly under UI control.
 
This monitoring takes overlapping snapshots of the network traffic and now includes an initial inspection of the results.

Any snapshots that are deemed to be free of important errors can be placed in temp storage and deleted after a short retention period.

Other snapshots that may contain details of network problems can be archived either locally or sent to a historian.
With this system, a fims output is also included to report the system status.

The concept here is that this system can be turned on as part of a maintenance check , or enabled to run for longer periods if systems are experiencing connectivity problems.
We'll capture the data and save both the raw data and any analysis of that data.

It looks like that a full pcap analysis tool would be a bigger project and we may use third party tools to do that.
The intent or scope of this project is to provide a first look or initial scan of the data to see if we can make the initial determination of the need for further inspection.
   
I intend to demo the config file scan (for modbus) and the initial system connectivity test Friday 01_12_2024.
I'll also show the continuous capture package and an initial package scanner for modbus tcp data. 

When given a tcpdump to "look at" I am already using my pcap_modbus tool as a first glance.

This is all done using python3 but , if needed, I can port the system to C++  or Go.
 
