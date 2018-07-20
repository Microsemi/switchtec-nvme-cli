# Switchtec-nvme-cli
This project is a fork of [nvme-cli][0] project with additional support of NVMe drives in the Switchtec Gen3 PAX fabric.

## Background
Switchtec Gen3 PAX is a variant of Microsemi's Switchtec PCIe switch product which enables the SR-IOV device sharing to mutiple hosts. Common SR-IOV devices include NICs, HBAs and IB cards, etc. Around early 2018, SR-IOV capable NVMe drives becomes available on market. With the support Switchtec Gen3 PAX switch, a SR-IOV capable NVMe drive can be managed by host and shared to multiple hosts.

By default, a NVMe drive in the PAX fabric cannot be enumerated by hosts. With the bind operation supported by the Switchtec Gen3 PAX switch, VFs can be bound to the hosts and be enumerated by the hosts.

Unlike some of other SR-IOV devices like NICs, the NVMe drives need to be properly configured before using. Normal configuration include create namespace and attache the namespace to a specific VF (NVMe secondary controller). And the configuration need to be done by the Fabric Manager.

Fabric Manager need a method to talk to the NVMe drives in the PAX fabric. Switchtec Gen3 PAX provides a special MRPC command to forward NVMe admin commands to the NVMe drives in the PAX fabric. The MRPC interface is a mechanism to allow hosts issue commands to Switchtec F/W. Two communication channels are provided between the Fabric Manager and the PAX switch, they are the inband PCIe channel and the MOE channel.

Switchtec-nvme-cli is responsible to create NVMe admin commands and deliver them to NVMe drives in PAX fabric through the special MRPC command, either over the inband PCIe channel or over the MOE channel.

For a better understanding of some terms of Switchtec PAX like MRPC, FABRIC MANAGER, MOE, please refer to Switchtec PAX device specification.

## Features
Swichtec-nvme-cli supports all features of nvme-cli for drives connected directly to host.

Swichtec-nvme-cli supports following features for NVMe drives behind PAX.
- List all NVMe devices and namespaces
- Create namespace
- Delete namespace
- Attaches a namespace to requested controller(s)
- Deletes a namespace from the controller
- Send NVMe Identify Controller
- Send NVMe Identify Namespace, display structure
- Send NVMe Identify List, display structure

## Build and Installation
Switchtec-nvme-cli depends on the [switchtec-user][1] library and [switchtec-kernel][2] driver, among others. Please build and install the switchtec-user library and switchtec-kernel driver before building switchtec-nvme-cli.

To build and install this utility, simply run below commands. 
```
#sudo make
#sudo make install
```

## Examples
Here are some examples of managing the NVMe drives in a PAX fabric. For the NVMe drives connected directly to host, all commands from original nvme-cli are supported without any change.

### Fabric Manager over PCIe channel
1. List all NVMe devices and namespaces
```
#sudo ./nvme switchtec list
Node                       SN                   Model                                    Namespace Usage                      Format           FW Rev
-------------------------- -------------------- ---------------------------------------- --------- -------------------------- ---------------- --------
0x3300n1@/dev/switchtec0   SERIALNUMBER         VENDOR MODEL                             1           6.55  MB /   6.55  MB    512   B +  0 B   REVISION
0x3300n2@/dev/switchtec0   SERIALNUMBER         VENDOR MODEL                             2          13.11  MB /  13.11  MB    512   B +  0 B   REVISION
0x3300n3@/dev/switchtec0   SERIALNUMBER         VENDOR MODEL                             3         131.07  MB / 131.07  MB    512   B +  0 B   REVISION
0x3300n4@/dev/switchtec0   SERIALNUMBER         VENDOR MODEL                             4           2.15  GB /   2.15  GB    512   B +  0 B   REVISION
0x3300n5@/dev/switchtec0   SERIALNUMBER         VENDOR MODEL                             5           2.15  GB /   2.15  GB    512   B +  0 B   REVISION

```
2. List controllers of a NVMe drive
```
#sudo ./nvme list-ctrl 0x3300@/dev/switchtec0
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
3. List namespaces of a NVMe drive
```
#sudo ./nvme list-ns 0x3300@/dev/switchtec0
[   0]:0x1
[   1]:0x2
[   2]:0x3
[   3]:0x4
[   4]:0x5
```
4. Create a 4GB namespace
```
#sudo ./nvme  create-ns 0x3300@/dev/switchtec0 -c 1048576 -s 1048576 -f 2
create-ns: Success, created nsid:6
```
5. Attach a namespace to a controller
```
#sudo ./nvme attach-ns 0x3300@/dev/switchtec0 -n 6 -c 0x21
attach-ns: Success, nsid:6
```
6. List all NVMe devices and namespaces
```
#sudo ./nvme switchtec list
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
#sudo ./nvme detach-ns 0x3300@/dev/switchtec0 -n 6 -c 0x21
detach-ns: Success, nsid:6
```
8. Delete a namespace
```
#sudo ./nvme delete-ns 0x3300@/dev/switchtec0 -n 6
delete-ns: Success, deleted nsid:6
```
9. List all NVMe devices and namespaces
```
#sudo ./nvme switchtec list
Node                       SN                   Model                                    Namespace Usage                      Format           FW Rev
-------------------------- -------------------- ---------------------------------------- --------- -------------------------- ---------------- --------
0x3300n1@/dev/switchtec0   SERIALNUMBER         VENDOR MODEL                             1           6.55  MB /   6.55  MB    512   B +  0 B   REVISION
0x3300n2@/dev/switchtec0   SERIALNUMBER         VENDOR MODEL                             2          13.11  MB /  13.11  MB    512   B +  0 B   REVISION
0x3300n3@/dev/switchtec0   SERIALNUMBER         VENDOR MODEL                             3         131.07  MB / 131.07  MB    512   B +  0 B   REVISION
0x3300n4@/dev/switchtec0   SERIALNUMBER         VENDOR MODEL                             4           2.15  GB /   2.15  GB    512   B +  0 B   REVISION
0x3300n5@/dev/switchtec0   SERIALNUMBER         VENDOR MODEL                             5           2.15  GB /   2.15  GB    512   B +  0 B   REVISION
```

### Fabric Manager over MOE channel
1. List all NVMe devices and namespaces
```
#sudo ./nvme switchtec list -r 10.188.117.80:0
Node                       SN                   Model                                    Namespace Usage                      Format           FW Rev
-------------------------- -------------------- ---------------------------------------- --------- -------------------------- ---------------- --------
0x3300n1@10.188.117.80:0   SERIALNUMBER         VENDOR MODEL                             1           6.55  MB /   6.55  MB    512   B +  0 B   REVISION
0x3300n2@10.188.117.80:0   SERIALNUMBER         VENDOR MODEL                             2          13.11  MB /  13.11  MB    512   B +  0 B   REVISION
0x3300n3@10.188.117.80:0   SERIALNUMBER         VENDOR MODEL                             3         131.07  MB / 131.07  MB    512   B +  0 B   REVISION
0x3300n4@10.188.117.80:0   SERIALNUMBER         VENDOR MODEL                             4           2.15  GB /   2.15  GB    512   B +  0 B   REVISION
0x3300n5@10.188.117.80:0   SERIALNUMBER         VENDOR MODEL                             5           2.15  GB /   2.15  GB    512   B +  0 B   REVISION
```
2. List controllers of a NVMe drive
```
#sudo ./nvme list-ctrl 0x3300@10.188.117.80:0
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
3. List namespaces of a NVMe drive
```
#sudo ./nvme list-ns 0x3300@10.188.117.80:0
[   0]:0x1
[   1]:0x2
[   2]:0x3
[   3]:0x4
[   4]:0x5
```
4. Create a 4GB namespace
```
#sudo ./nvme  create-ns 0x3300@10.188.117.80:0 -c 1048576 -s 1048576 -f 2
create-ns: Success, created nsid:6
```
5. Attach a namespace to a controller
```
#sudo ./nvme attach-ns 0x3300@10.188.117.80:0 -n 6 -c 0x21
attach-ns: Success, nsid:6
```
6. List all NVMe devices and namespaces
```
#sudo ./nvme switchtec list -r 10.188.117.80:0
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
#sudo ./nvme detach-ns 0x3300@10.188.117.80:0 -n 6 -c 0x21
detach-ns: Success, nsid:6
```
8. Delete a namespace
```
#sudo ./nvme delete-ns 0x3300@10.188.117.80:0 -n 6
delete-ns: Success, deleted nsid:6
```
9. List all NVMe devices and namespaces
```
#sudo ./nvme switchtec list -r 10.188.117.80:0
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
