# Switchtec-nvme-cli
This project is a fork of the [nvme-cli][0] project with additional support for using NVMe drives in the Switchtec Gen4 PAX fabric.

## Background
Switchtec Gen4 PAX is a variant of Microsemi's Switchtec PCIe switch product which enables sharing SR-IOV devices among multiple hosts. Common SR-IOV devices include NICs, HBAs, IB cards, etc. With the support of a Switchtec Gen4 PAX switch and this utility, an SR-IOV capable NVMe drive can be managed and configured by a host and then shared among multiple hosts.

By default, an NVMe drive in the PAX fabric cannot be enumerated by the host. Using the bind operation provided by the Switchtec Gen4 PAX switch, such a VF can be bound to and enumerated by the host.

Unlike some other SR-IOV devices such as NICs, NVMe drives need to be properly configured before they can be used. Generally, configuration includes creating an namespace and attaching the namespace to a specific VF (NVMe secondary controller). 

The configuration is done through Fabric Manager using MRPC commands. The MRPC interface is a way for a host to send configuration commands to the Switchtec firmware. Switchtec Gen4 PAX provides a special MRPC command to forward NVMe admin commands to the NVMe drives in the PAX fabric. 

Switchtec-nvme-cli is responsible for creating the NVMe admin commands and delivering them to NVMe drives in the PAX fabric, using the special MRPC command. The special MRPC commands can be sent through one of these two channels: the inband PCIe channel, or the Ethernet channel. Switchtec-nvme-cli is currently based on v1.11.1 of the [nvme-cli][0].

Refer to the Switchtec PAX device specification for further information on Switchtec PAX, including MRPC, Fabric Manager and other terminologies used in this document.

## Features
Swichtec-nvme-cli supports all features of nvme-cli for drives connected directly to the host.

Swichtec-nvme-cli supports the following operations for NVMe drives behind PAX:
- Listing all NVMe devices and namespaces
- Creating a namespace
- Deleting a namespace
- Attaching a namespace to requested controller(s)
- Deleting a namespace from the controller
- Sending the NVMe Identify Controller command
- Sending the NVMe Identify Namespace command, displaying structure
- Sending the NVMe Identify List command, displaying structure

## Building and Installation
Switchtec-nvme-cli depends on the [switchtec-user][1] library and [switchtec-kernel][2] driver, among others. Please build and install the switchtec-user library and switchtec-kernel driver before building switchtec-nvme-cli.

To build and install this utility, simply run the following commands:
```
#sudo make
#sudo make install
```

## Examples
Here are some sample commands for managing NVMe drives in a PAX fabric. For the NVMe drives connected directly to the host, all commands from the original nvme-cli are supported without any change.

### Fabric Manager over PCIe channel
1. List all NVMe devices and namespaces
```
#sudo ./switchtec-nvme switchtec list
Node                       SN                   Model                                    Namespace Usage                      Format           FW Rev
-------------------------- -------------------- ---------------------------------------- --------- -------------------------- ---------------- --------
0x3300n1@/dev/switchtec0   SERIALNUMBER         VENDOR MODEL                             1           6.55  MB /   6.55  MB    512   B +  0 B   REVISION
0x3300n2@/dev/switchtec0   SERIALNUMBER         VENDOR MODEL                             2          13.11  MB /  13.11  MB    512   B +  0 B   REVISION
0x3300n3@/dev/switchtec0   SERIALNUMBER         VENDOR MODEL                             3         131.07  MB / 131.07  MB    512   B +  0 B   REVISION
0x3300n4@/dev/switchtec0   SERIALNUMBER         VENDOR MODEL                             4           2.15  GB /   2.15  GB    512   B +  0 B   REVISION
0x3300n5@/dev/switchtec0   SERIALNUMBER         VENDOR MODEL                             5           2.15  GB /   2.15  GB    512   B +  0 B   REVISION

```
2. List controllers of an NVMe drive
```
#sudo ./switchtec-nvme list-ctrl 0x3300@/dev/switchtec0
[   0]:0x1
[   1]:0x2
[   2]:0x3
[   3]:0x4
[   4]:0x5
[   5]:0x6
[   6]:0x7
[   7]:0x8
[   8]:0x9
[   9]:0xa
[  10]:0xb
[  11]:0xc
[  12]:0xd
[  13]:0xe
[  14]:0xf
[  15]:0x21
```
3. List namespaces of an NVMe drive
```
#sudo ./switchtec-nvme list-ns 0x3300@/dev/switchtec0
[   0]:0x1
[   1]:0x2
[   2]:0x3
[   3]:0x4
[   4]:0x5
```
4. Create a 4GB namespace
```
#sudo ./switchtec-nvme  create-ns 0x3300@/dev/switchtec0 -c 1048576 -s 1048576 -f 2
create-ns: Success, created nsid:6
```
5. Attach a namespace to a controller
```
#sudo ./switchtec-nvme attach-ns 0x3300@/dev/switchtec0 -n 6 -c 0x21
attach-ns: Success, nsid:6
```
6. List all NVMe devices and namespaces
```
#sudo ./switchtec-nvme switchtec list
Node                       SN                   Model                                    Namespace Usage                      Format           FW Rev
-------------------------- -------------------- ---------------------------------------- --------- -------------------------- ---------------- --------
0x3300n1@/dev/switchtec0   SERIALNUMBER         VENDOR MODEL                             1           6.55  MB /   6.55  MB    512   B +  0 B   REVISION
0x3300n2@/dev/switchtec0   SERIALNUMBER         VENDOR MODEL                             2          13.11  MB /  13.11  MB    512   B +  0 B   REVISION
0x3300n3@/dev/switchtec0   SERIALNUMBER         VENDOR MODEL                             3         131.07  MB / 131.07  MB    512   B +  0 B   REVISION
0x3300n4@/dev/switchtec0   SERIALNUMBER         VENDOR MODEL                             4           2.15  GB /   2.15  GB    512   B +  0 B   REVISION
0x3300n5@/dev/switchtec0   SERIALNUMBER         VENDOR MODEL                             5           2.15  GB /   2.15  GB    512   B +  0 B   REVISION
0x3300n6@/dev/switchtec0   SERIALNUMBER         VENDOR MODEL                             6           4.29  GB /   4.29  GB      4 KiB +  0 B   REVISION
```
7. Detach a namespace from a controller
```
#sudo ./switchtec-nvme detach-ns 0x3300@/dev/switchtec0 -n 6 -c 0x21
detach-ns: Success, nsid:6
```
8. Delete a namespace
```
#sudo ./switchtec-nvme delete-ns 0x3300@/dev/switchtec0 -n 6
delete-ns: Success, deleted nsid:6
```
9. List all NVMe devices and namespaces
```
#sudo ./switchtec-nvme switchtec list
Node                       SN                   Model                                    Namespace Usage                      Format           FW Rev
-------------------------- -------------------- ---------------------------------------- --------- -------------------------- ---------------- --------
0x3300n1@/dev/switchtec0   SERIALNUMBER         VENDOR MODEL                             1           6.55  MB /   6.55  MB    512   B +  0 B   REVISION
0x3300n2@/dev/switchtec0   SERIALNUMBER         VENDOR MODEL                             2          13.11  MB /  13.11  MB    512   B +  0 B   REVISION
0x3300n3@/dev/switchtec0   SERIALNUMBER         VENDOR MODEL                             3         131.07  MB / 131.07  MB    512   B +  0 B   REVISION
0x3300n4@/dev/switchtec0   SERIALNUMBER         VENDOR MODEL                             4           2.15  GB /   2.15  GB    512   B +  0 B   REVISION
0x3300n5@/dev/switchtec0   SERIALNUMBER         VENDOR MODEL                             5           2.15  GB /   2.15  GB    512   B +  0 B   REVISION
```

### Fabric Manager over Ethernet channel
1. List all NVMe devices and namespaces
```
#sudo ./switchtec-nvme switchtec list -r 10.188.117.80:0
Node                       SN                   Model                                    Namespace Usage                      Format           FW Rev
-------------------------- -------------------- ---------------------------------------- --------- -------------------------- ---------------- --------
0x3300n1@10.188.117.80:0   SERIALNUMBER         VENDOR MODEL                             1           6.55  MB /   6.55  MB    512   B +  0 B   REVISION
0x3300n2@10.188.117.80:0   SERIALNUMBER         VENDOR MODEL                             2          13.11  MB /  13.11  MB    512   B +  0 B   REVISION
0x3300n3@10.188.117.80:0   SERIALNUMBER         VENDOR MODEL                             3         131.07  MB / 131.07  MB    512   B +  0 B   REVISION
0x3300n4@10.188.117.80:0   SERIALNUMBER         VENDOR MODEL                             4           2.15  GB /   2.15  GB    512   B +  0 B   REVISION
0x3300n5@10.188.117.80:0   SERIALNUMBER         VENDOR MODEL                             5           2.15  GB /   2.15  GB    512   B +  0 B   REVISION
```
2. List controllers of an NVMe drive
```
#sudo ./switchtec-nvme list-ctrl 0x3300@10.188.117.80:0
[   0]:0x1
[   1]:0x2
[   2]:0x3
[   3]:0x4
[   4]:0x5
[   5]:0x6
[   6]:0x7
[   7]:0x8
[   8]:0x9
[   9]:0xa
[  10]:0xb
[  11]:0xc
[  12]:0xd
[  13]:0xe
[  14]:0xf
[  15]:0x21
```
3. List namespaces of an NVMe drive
```
#sudo ./switchtec-nvme list-ns 0x3300@10.188.117.80:0
[   0]:0x1
[   1]:0x2
[   2]:0x3
[   3]:0x4
[   4]:0x5
```
4. Create a 4GB namespace
```
#sudo ./switchtec-nvme  create-ns 0x3300@10.188.117.80:0 -c 1048576 -s 1048576 -f 2
create-ns: Success, created nsid:6
```
5. Attach a namespace to a controller
```
#sudo ./switchtec-nvme attach-ns 0x3300@10.188.117.80:0 -n 6 -c 0x21
attach-ns: Success, nsid:6
```
6. List all NVMe devices and namespaces
```
#sudo ./switchtec-nvme switchtec list -r 10.188.117.80:0
Node                       SN                   Model                                    Namespace Usage                      Format           FW Rev
-------------------------- -------------------- ---------------------------------------- --------- -------------------------- ---------------- --------
0x3300n1@10.188.117.80:0   SERIALNUMBER         VENDOR MODEL                             1           6.55  MB /   6.55  MB    512   B +  0 B   REVISION
0x3300n2@10.188.117.80:0   SERIALNUMBER         VENDOR MODEL                             2          13.11  MB /  13.11  MB    512   B +  0 B   REVISION
0x3300n3@10.188.117.80:0   SERIALNUMBER         VENDOR MODEL                             3         131.07  MB / 131.07  MB    512   B +  0 B   REVISION
0x3300n4@10.188.117.80:0   SERIALNUMBER         VENDOR MODEL                             4           2.15  GB /   2.15  GB    512   B +  0 B   REVISION
0x3300n5@10.188.117.80:0   SERIALNUMBER         VENDOR MODEL                             5           2.15  GB /   2.15  GB    512   B +  0 B   REVISION
0x3300n6@10.188.117.80:0   SERIALNUMBER         VENDOR MODEL                             6           4.29  GB /   4.29  GB      4 KiB +  0 B   REVISION
```
7. Detach a namespace from a controller
```
#sudo ./switchtec-nvme detach-ns 0x3300@10.188.117.80:0 -n 6 -c 0x21
detach-ns: Success, nsid:6
```
8. Delete a namespace
```
#sudo ./switchtec-nvme delete-ns 0x3300@10.188.117.80:0 -n 6
delete-ns: Success, deleted nsid:6
```
9. List all NVMe devices and namespaces
```
#sudo ./switchtec-nvme switchtec list -r 10.188.117.80:0
Node                       SN                   Model                                    Namespace Usage                      Format           FW Rev
-------------------------- -------------------- ---------------------------------------- --------- -------------------------- ---------------- --------
0x3300n1@10.188.117.80:0   SERIALNUMBER         VENDOR MODEL                             1           6.55  MB /   6.55  MB    512   B +  0 B   REVISION
0x3300n2@10.188.117.80:0   SERIALNUMBER         VENDOR MODEL                             2          13.11  MB /  13.11  MB    512   B +  0 B   REVISION
0x3300n3@10.188.117.80:0   SERIALNUMBER         VENDOR MODEL                             3         131.07  MB / 131.07  MB    512   B +  0 B   REVISION
0x3300n4@10.188.117.80:0   SERIALNUMBER         VENDOR MODEL                             4           2.15  GB /   2.15  GB    512   B +  0 B   REVISION
0x3300n5@10.188.117.80:0   SERIALNUMBER         VENDOR MODEL                             5           2.15  GB /   2.15  GB    512   B +  0 B   REVISION
```

[0]: https://github.com/linux-nvme/nvme-cli
[1]: https://github.com/Microsemi/switchtec-user
[2]: https://github.com/Microsemi/switchtec-kernel
