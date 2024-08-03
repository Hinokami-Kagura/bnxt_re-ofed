                               README Notes
                    Broadcom bnxt_re Linux RoCE Driver

                            Broadcom Limited
                         5300 California Avenue,
                            Irvine, CA 92617

                   Copyright (c) 2015 - 2016 Broadcom Corporation
                   Copyright (c) 2016 - 2018 Broadcom Limited
                   Copyright (c) 2018 - 2024 Broadcom Inc.
                           All rights reserved


Table of Contents
=================

  Introduction
  BNXT_RE Driver Dependencies
  BNXT_RE Driver compilation
  Configuration Tips
  Limitations
  BNXT_RE Dynamic Debug Messages
  BNXT_RE Compiler Switches
  BNXT_RE Driver Defaults
  DCB Settings
  Congestion Control
  SR-IOV and VF Resource Distribution
  Link Aggregation
  BNXT_RE Driver Statistics
  QP Information in debugfs


Introduction
============

This file describes the bnxt_re Linux RoCE driver for the Broadcom NetXtreme-C
and NetXtreme-E 10/25/40/50/100/200 Gbps Ethernet Network Controllers.

Note: Starting from 219.0 release, driver supports only RoCE v2.
RoCE v1 support is deprecated. Please refer Broadcom Linux RoCE Configuration
Guide for details.

BNXT_RE Driver Dependencies
===========================

The RoCE driver has dependencies on the bnxt_en Ethernet counterpart.

  - It also has dependencies on the IB verbs kernel component
    (Details given below).


BNXT_RE Driver compilation
==========================

bnxt_re driver compilation depends on whether IB stack is available along with
the OS distribution or an external OFED is required.

 => Distros that has IB Stack available along with OS distribution:
    RH7.1/7.2/7.3/6.7/6.8, RH8.x, SLES12SPx,SLES15SPx  and Ubuntu 16.04/14.04 and later.
    All kernels from 4.5.x onwards

To compile bnxt_re:
        - Untar netxtreme-bnxt_en-<version>.tar.gz
	$make

 => Some older distros that doesn't have RoCE v2 support needs upstream OFED

    Please refer OFED release notes from the following link and install
    OFED before compiling bnxt_re driver.
    http://downloads.openfabrics.org/downloads/OFED/release_notes/OFED_3.18-2_release_notes

     To compile bnxt_re:
	$export OFED_VERSION=OFED-4.8
		- For OFEDs other than 4.8, 4.8 needs to be updated
		  with the current OFED version installed on the system
	$make

 => The bnxt_en driver must be built first before building the bnxt_re driver.
    Also, while loading the drivers bnxt_en driver must be loaded first.

Configuration Tips
==================

- It is recommended to use same host OS version on client and server while
  running NFS-RDMA/iSER/NVMEoF tests. Heterogeneous host OS may lead to unexpected
  results. This is due to the incompatible ULP server and Client kernel modules.

- It is recommended to assign at least 3GB RAM to VMs used for memory intensive
  applications like NFSoRDMA, iSER, NVMoF etc.

- When using large number of QPs (close to maximum supported) along with large
  message sizes it is recommended to increase the `max_map_count` kernel parameter
  using sysctl to avoid memory map failures in the application.
  Please refer to https://www.kernel.org/doc/Documentation/sysctl/vm.txt on how to tune
  this kernel parameter.

- When TCP/IP and RoCE traffic are running simultaneously with high work load or
  RoCE traffic with high work load, high CPU utilization can result,
  leading to CPU soft lockup. Hence it is recommended to spread the workload
  across the available CPU cores. This can be achieved by setting the SMP
  affinity of the interrupts and RoCE applications.
  Please refer to OS documentation for setting smp_affinity and specific
  commands like taskset etc.

- To avoid the SQ full reported during iSER stress testing (kernels SLES 12 and later,
  RHEL 7.4 and later), configure the minimum tx depth for the QPs to 4096.
  All connections getting established after setting min_tx_depth uses the user specified values.

  echo 4096 > /sys/kernel/config/bnxt_re/bnxt_re0/ports/1/tunables/min_tx_depth

- For heavy RDMA-READ workloads with large number of active QPs,
  a higher ack-timeout value is recommended.
  For example:
   ib_read_bw with -q 4096 would require ack timeout 18. Ack timeout is
   controlled by option "-u".
   ib_read_bw --report_gbits -F -m 4096 -q 4096 -d bnxt_re2 -x 3 -u 18 -D 60 -s 65536

- For use cases where the adapter QP limit is exercised or active qps are
  close to adapter limits, ack timeout needs to be increased to 24 to avoid
  retransmissions and loss of performance.
  For example:
   For multiple instances of ib_send_bw/ib_read_bw/ib_write_bw, which creates
   total of 64K QPs, specify higher ack timeout in each application instance
   using -u 24.
- "restrict_mrs" module parameter is used to limit the number of MRs supported
  by the driver. For now the parameter applies only to 574xx devices.
  Setting this flag to 1 would reduce the number for MRs supported by the device
  to 64K.

Limitations
===========
- GIDs corresponding to IPv4 and IPv6 addresses maybe missing after
  device creation sequences such as  driver load or device error recovery.

  e.g. when RoCE v1 and RoCE v2 are enabled on the adapter,
  ibv_devinfo -d <device> -vvv
  Shows:
	GID[  0]: fe80:0000:0000:0000:5e6f:69ff:fe1e:2f3e, RoCE v1
	GID[  1]: fe80::5e6f:69ff:fe1e:2f3e, RoCE v2

  Should show:
	GID[  0]: fe80:0000:0000:0000:5e6f:69ff:fe1e:2f3e, RoCE v1
	GID[  1]: fe80::5e6f:69ff:fe1e:2f3e, RoCE v2
	GID[  2]: 0000:0000:0000:0000:0000:ffff:c0a8:0033, RoCE v1
	GID[  3]: ::ffff:192.168.0.51, RoCE v2
	GID[  4]: 2001:0000:0000:0000:0000:0000:0000:0051, RoCE v1
	GID[  5]: 2001::51, RoCE v2
  This is due to device creation sequence from netdev event context.
  The design change to avoid these failures will be available in future
  releases.  As a workaround, bring down the L2 interface (ifconfig down)
  and bring it up (ifconfg up). This will force stack to add the GIDs again.

- Stack traces seen with following message during link
  down and other administrative events like PFC enable/disable.
  "task ib_write_bw:406494 blocked for more than 120 seconds"
  This is because of the un-graceful destroy of the resources and FW
  can take more time to destroy these resources. RoCE driver can
  wait upto 240 seconds before hitting the timeout. These error messages
  will stop once all resources are destroyed.

- When the applications are run simultaneously, there is a chance of commands
  getting failed with an error message "send failed - retries:2000". This is due
  to the CMD Queue is getting full because applications are creating/destroying
  resources simultaneously. This is also observed, when the applications are
  ungracefully killed and if restarted before the active resources are cleaned
  up when killed. In any case if this issue is seen, restart the application with
  delay between the applications.

- For error recovery to succeed, the interface should be in the ifup state
  with no disruptions during the process that might reconfigure the device.
  In other words, for reliable error recovery, it is recommended to not run
  any configuration changes (such as unloading the RoCE Driver, bonding inerface,
  ethtool self-tests etc) while error recovery is in progress.
  If at all changes are done, and recovery does not succeed, try the below
  actions to recover:
   -  Unload and reload both the drivers roce and L2
   -  Unbind and rebind the PCIe function using sysfs

- Error messages seen during RoCE driver load after a live FW update/FW reset,
  if L2 interface down during the reset.
  To avoid errors bring up all ethernet interfaces up before loading RoCE driver.

- When remote directories are mounted using NFS-RDMA, unloading bnxt_re shall
  cause system hang and the system needs a reboot for normal operations.
  Always unmount all active NFS mounts over bnxt_re interface, before unloading
  bnxt_re driver.

- Using same interface MTU on both client and server is recommended.  User can
  see unexpected results if there is a mismatch in interface MTUs on Client
  and Server.

- Changing MAC address of the interface while bnxt_re is loaded can trigger failure
  during GID deletion. Unload bnxt_re driver before changing the interface MAC address.

- The legacy FMR Pool is not supported yet.

- Raw Ethertype QP is not supported yet.

- Tunnel is not supported yet.

- On SLES11 SP4 default kernel(3.0.101-63-default), tc command to map the
  priority to traffic class throws error and hence ETS b/w will not get
  honored when NIC + RoCE traffic is run together.
  This issue is fixed in 3.0.101-91-default. Users are advised to upgrade to
  this kernel while testing ETS.

- iscsi ping timeout reported in dmesg during 128 VF testing over 8
  RHEL 8.3 VMs. Some of the connections report recovery timeout
  during scale testing. Reduce the number of VFs to 64 and use
  lesser number of VMs per host to avoid the recovery failures.

- When RoCE VFs are created, destroying the VFs may take a longer time to complete.
   For example, 64 VFs destroy may take up to 20 sec.

- Avoid running ethtool offline selftest when QPs are active.

- Avoid performing PCI reset when QPs are active. Driver has no way to know
  about this reset and eventually causes PCI Fatal errors and a system crash when
  QPs are active and Doorbell recovery/pacing enabled.

- On AMD64 chipset (recently noticed on AMD EPYC 9554) with IOMMU enabled systems,
  users can notice the below error string from bnxt_re driver.

  infiniband bnxt_re0: bnxt_re_build_reg_wqe: bnxt_re_mr 0xff211d4fb9eaa800  len (65536 > 4096)
  infiniband bnxt_re0: bnxt_re_build_reg_wqe: build_reg_wqe page[0] = 0xffffffffffff0000
  infiniband bnxt_re0: bad_wr seen with opcode = 0x20

  Primary issue is AMD IOMMU is providing iova reaching to max U64 and that is
  not expected. Contact Broadcom support for additional information.

- Driver is no longer supporting max_msix_vec module parameter.
  The num_comp_vectors in the output of "ibv_devinfo -v" is controlled
  by the L2 driver ring counts before loading bnxt_re driver. If users
  want more completion vectors for RoCE (ie. upto 64 or num_cpus), unload
  bnxt_re driver, reduce the L2 rings using ethtool and then load RoCE driver.

BNXT_RE Dynamic Debug Messages
==============================
The bnxt_re driver supports the Linux dynamic debug feature.

All error, warning and info messages are logged by default.
Any debug messages if needed, could be enabled by writing to
the standard <debugfs>/dynamic_debug/control file.
Debug messages can be enabled/disabled at various granularities
like - module, file, function, a range of line numbers or a
specific line number.

The following kernel document describes this in detail with examples:
https://www.kernel.org/doc/Documentation/dynamic-debug-howto.txt

A few examples on how to use this with bnxt_re driver:

1) To check the debug messages that are available in bnxt_re:
# cat /sys/kernel/debug/dynamic_debug/control | grep bnxt_re

2) To enable all debug messages in bnxt_re during load time:
# insmod bnxt_re.ko  dyndbg==p

3) To enable all debug messages in bnxt_re after loading:
# echo "module bnxt_re +p" > /sys/kernel/debug/dynamic_debug/control

4) To disable all debug messages in bnxt_re after loading:
# echo "module bnxt_re -p" > /sys/kernel/debug/dynamic_debug/control

5) To enable a debug message at a specific line number in a file:
# echo -n "file qplib_fp.c line 2554 +p" > /sys/kernel/debug/dynamic_debug/control


BNXT_RE Compiler Switches
=========================

ENABLE_DEBUGFS - Enable debugFS operation

ENABLE_RE_FP_SPINLOCK - Enable spinlocks on the fast path bnxt_re_qp queue
			resources

ENABLE_FP_SPINLOCAK - Enable spinlocks on the fast path bnxt_qplib queue
		      resources

ENABLE_DEBUG_SGE - Enable the dumping of SGE info to the journal log

BNXT_RE Driver Defaults
=======================
Driver enables #3 traffic classes (L2, RoCE and CNP) during the load.
Driver configures the default RoCE and CNP priorities and DSCP values and
enable PFC and CC by default. No other configuration required on the host
if the switches are configured with default values.

Following are the default CC/QoS values.

Default priority and DSCP
-------------------------
RoCE Traffic Priority: 3
RoCE Traffic DSCP: 26
CNP Traffic Priority: 7
CNP Traffic DSCP: 48

Default traffic classes
----------------------
TC0: L2 traffic
TC1: RoCE Traffic
TC2: CNP Traffic

Default PFC priority
--------------------
Enabled for priority 3

Default ETS Configuration
-------------------------
Assigned bandwidth to L2 traffic: 50%
Assigned bandwidth to RoCE traffic: 50%

DCB Settings
============

The users can change the above default values using bnxt_setupcc.sh or
other tools. PFC settings can be configured with bnxtqos or lldptool
While adding new settings, please make sure that the default settings
(say, app TLVs) are removed, before programming the new values.
Refer bnxt_setupcc.sh for usage of lldptool/bnxtqos and configfs.

Note: Since bnxt_re driver enables PFC on ROCE Priority during driver load,
      PFC must be disabled using bnxtqos/lldptool before changing any TC
      mapping. This ensures proper mapping between the user traffic class
      to HW Queues.

Note: Unloading bnxt_re driver would change the current DCBx settings on
      the adapter. This might include some of the settings done by users
      after loading bnxt_re. If bnxt_re is unloaded after the user changes
      settings, please clear all DCBx settings before loading bnxt_re again.

      To confirm the dcbx settings,  use bnxtqos or lldptool as following.

	$lldptool get-tlv -n -i <eth x>
	or
	$ bnxtqos -dev=p6p1 get_qos


Example usages of bnxtqos and lldptool is given below.

bnxtqos
-------
#Set APP TLV for RoCE v2 traffic with RoCE priority
 bnxtqos -dev=<ethx> set_apptlv app=<roce_prio>,3,4791
E.g. To set roce_prio=5, selector=3 and protocol=4791.
    bnxtqos -dev=eth0 set_apptlv app=5,3,4791

#Enable PFC on RoCE priority
 bnxtqos -dev=<ethx> set_pfc enabled=<roce_prio>
E.g. To enable PFC for roce_prio=5
    bnxtqos -dev=eth0 set_pfc enabled=5

#Set APP TLV for RoCE dscp and priority
 bnxtqos -dev=<ethx> set_apptlv app=<roce_prio>,5,<roce_dscp>
 echo <roce_dscp> > /sys/kernel/config/bnxt_re/<bnxt_reX>/ports/1/cc/roce_dscp
E.g. To set roce_prio=5 and roce_dscp=28
    bnxtqos -dev=eth0 set_apptlv app=5,5,28
    echo 0x1c > /sys/kernel/config/bnxt_re/bnxt_re2/ports/1/cc/roce_dscp

#Set CNP priority and dscp
#For this command to execute, make sure "service prof type" is supported.
#Following is the command to know the "service prof type" is supported:
#cat /sys/kernel/debug/bnxt_re/<Device name>/info | grep fw_service_prof_type_sup
 bnxtqos -dev=<ethx> set_apptlv app=<cnp_prio>,5,<cnp_dscp>
 echo <cnp_dscp> > /sys/kernel/config/bnxt_re/<bnxt_reX>/ports/1/cc/cnp_dscp
E.g. To set cnp_prio=4 and cnp_dscp=40 for selector=5
    bnxtqos -dev=eth0 set_apptlv app=4,5,40
    echo 0x28 > /sys/kernel/config/bnxt_re/bnxt_re2/ports/1/cc/cnp_dscp

#Set the ETS and Priority to Traffic Class mapping
 bnxtqos -dev=<ethx> set_ets tsa=0:ets,1:ets,2:strict,3:strict,4:strict,5:strict,6:strict,7:strict priority2tc=<pri2tc> tcbw=<L2_BW>,<ROCE_BW>
E.g.
    bnxtqos -dev=<ethx> set_ets tsa=0:ets,1:ets priority2tc=0:0,1:0,2:0,3:0,4:0,5:1,6:0,7:0 tcbw=20,80

#Dump existing settings
bnxtqos -dev=<ethx> get_qos

#Delete the existing app tlvs
 bnxtqos -dev=<ethx> set_apptlv -d app=<priority,selector,protocol>
E.g. To delete roce_prio=5, selector=3 and protocol=4791
    bnxtqos -dev=eth0 set_apptlv -d app=5,3,4791

#Disable PFC
 bnxtqos -dev=<ethx> set_pfc enabled=none

lldptool
--------
Note: If the switches are capable of handling RoCE TLVs, the following
settings are not required as adapter will override local settings, if any,
with the switch settings.

Following steps are recommended to configure
the local adapter to set DCB parameters, in case switches are not capable
of DCB negotiations.

# Load L2 driver and make sure port and Link are  UP
 service lldpad start
 lldptool -L -i p6p1 adminStatus=rxtx
#Disable PFC
lldptool -T -i <ethx> -V PFC enabled=none
#Delete the existing app TLVs. For eg:
lldptool -T -i <ethx> -V APP -d app=3,5,26
lldptool -T -i <ethx> -V APP -d app=3,3,4791
#For RoCE-V2 protocol with Priority-5
 lldptool -T -i p6p1 -V APP app=5,3,4791
 lldptool -T -i p6p1 -V ETS-CFG tsa=0:ets,1:ets,2:strict,3:strict,4:strict,5:strict,6:strict,7:strict \
    up2tc=0:0,1:0,2:0,3:0,4:0,5:1,6:0,7:0  tcbw=10,90,0,0,0,0,0,0
 lldptool -T -i p6p1 -V PFC enabled=5
 service lldpad restart

Note: Please refer man pages of lldptool, lldptool-app,
lldptool-ets, lldptool-pfc, etc. for more details

Note: VF inherits the PFC settings of the PF. VF doesn't have privilege to
set DCB parameters using lldptool. No need of running lldpad service on the VM.

Note: The driver supports only one priority for RoCE traffic.

Note: The driver by default supports Priority VLAN Tagging i.e it adds a NULL
VLAN tag if a priority is configured for RoCE Traffic, without VLANs being
configured. However, for customers who are interested only in PFC via DSCP,
driver provides a knob to disable the auto VLAN 0 tag insertion.

echo 1 > /sys/kernel/config/bnxt_re/bnxt_re0/ports/1/cc/disable_prio_vlan_tx

Guidelines for changing DCB settings
------------------------------------

The current software requires the following Traffic Class mapping.

TC0: L2 traffic
TC1: RoCE Traffic
TC2: CNP Traffic and L2 Traffic
TC3 – TC7: L2 traffic

Each TC can be mapped to different priority. So while mapping priority to traffic
class, make sure that TC1 is mapped for RoCE priority and TC2 is mapped for CNP priority.
RoCE traffic class support only one DSCP value programmed through the DSCP App TLV. Since
the CNP Traffic class (TC2) is shared between CNP and L2 traffic, multiple DSCP values are
supported for this traffic class.  The current solution requires that the DSCP App TLV
for CNP should be programmed at the end, after programming other App TLVs.

Sample programming for multiple DSCP values for TC2.
TC1 RoCE pri – 5
TC1 RoCE dscp – 59
TC2 CNP pri – 6
TC2 CNP dscp – 49
TC2 L2 dscp – 55
TC2 L2 dscp – 54
All other priorities are mapped to remaining traffic classes.

# Map the priority to Traffic class and enable PFC on priority 5
bnxtqos -dev=enp37s0f0np0 set_ets tsa=0:ets,1:ets,2:strict,3:ets,4:ets,5:ets,6:ets,7:strict \
			priority2tc=0:0,1:3,2:4,3:5,4:6,5:1,6:2,7:7 tcbw=10,50,2,3,2,33
bnxtqos -dev=enp37s0f0np0 set_pfc enabled=5

#Set up RoCE v2 packet based TLV (dest port 4791)
bnxtqos -dev=enp37s0f0np0 set_apptlv app=5,3,4791
#Setup RoCE DSCP (59) App TLV
bnxtqos -dev=enp37s0f0np0 set_apptlv app=5,5,59
#TC2 mapped for priority 6. Setup L2 DSCP values (54,55) first
#and then program CNP DSCP value (49)
bnxtqos -dev=enp37s0f0np0 set_apptlv app=6,5,54
bnxtqos -dev=enp37s0f0np0 set_apptlv app=6,5,55
bnxtqos -dev=enp37s0f0np0 set_apptlv app=6,5,49

Congestion Control
===================

Explicit Congestion Notification (ECN) is a congestion avoidance mechanism.
In this protocol a Congestion Notification Packet(CNP) signals the existence
of congestion to the remote transmitter. Reacting to CNP, the transmitter reduces
the transmit rate on a transmit-flow for a given time quanta. CNP is generated
by the receiver when it detects congestion in the receive processing pipe.

To export the tuning parameters RoCE driver uses configfs support from linux
kernel. Following are the steps to configure congestion control parameters.

	1. Pre-requisites
	   ===============
		1.a Host base lldpad is configured for RoCE-v2 protocol
		    and a valid priority is assigned to RoCE-v2.
			ref: "lldptool" section of this document.

	2. Mount per-port-configfs interface
	   ===================================
		2.a Load RoCE driver
		2.b ls /sys/kernel/config should list directory "bnxt_re"
		2.c Create a directory in configfs-path with the RoCE device name.
		    E.g. for bnxt_re1 use following:
			mkdir -p /sys/kernel/config/bnxt_re/bnxt_re1
		2.d ls /sys/kernel/config/bnxt_re/bnxt_re1
			ls /sys/kernel/config/bnxt_re/bnxt_re1/ports/1/cc/
				cnp_dscp  cnp_prio  apply  cc_mode roce_prio
				ecn_enable  g  inact_cp  init_cr  init_tr
				nph_per_state  rtt  tcp_cp  roce_dscp  ecn_marking
		2.e To enable CC, set 1 to ecn_enable, To disable, set 0
			E.g.
			    echo -n 0x1 > ecn_marking
			    echo -n 0x0 > ecn_enable
			Note: There are other tunables under same directory. Use these fields as
			      needed.
			...
		2.f Check for "service prof type" is supported, by using the
			following command:-
			cat /sys/kernel/debug/bnxt_re/<Device name>/info | grep fw_service_prof_type_sup
			E.g. for bnxt_re0 use following:
			cat /sys/kernel/debug/bnxt_re/bnxt_re0/info | grep fw_service_prof_type_sup

			If "service prof type" is supported, refer to
			"DCB settings" section of this document.
			If "service prof type" is *not* supported, follow the
			steps below.
		2.g Change the value of a specific parameter
			echo <roce_prio> > roce_prio
			echo <cnp_prio> > cnp_prio
			echo <roce_dscp> > roce_dscp
			echo <cnp_dscp> > cnp_dscp
			E.g.
			    echo -n 0x05 > roce_dscp
		2.h Apply the changes to hardware
			echo -n 0x01 > apply
		Note: Any changes will not take effect unless step 2.h is
		      carried out.

		2.i Read back a specific parameter
			cat roce_dscp
			...

	3. Unmount per-port-configfs interface
	   ====================================
		3.a remove all per-port-configfs mounts as following:
			rmdir /sys/kernel/config/bnxt_re/bnxt_re1
			rmdir /sys/kernel/config/bnxt_re/bnxt_re0
			...

		Note: If configfs is mounted rmmod bnxt_re will fail.
		      It is must to perform step 3.a before issuing
		      rmmod bnxt_re.

SR-IOV and VF Resource Distribution
===================================
RDMA SR-IOV is supported on BCM575xx devices only, with NPAR disabled.

Note: Before enabling the VFs, both bnxt_en and bnxt_re drivers should be loaded.
      Loading bnxt_re driver after creating VFs is not supported. Removal of bond
      interface while VFs present is also not supported, as removal of bond
      interface creates the RoCE base interfaces which is similar to loading
      bnxt_re driver.

      In distros that support auto loading of bnxt_re based on udev rules,
      (ie. having an entry  ENV{ID_NET_DRIVER}=="bnxt_en", RUN{builtin}+="kmod load bnxt_re"
      in udev rules file 90-rdma-hw-modules.rules)
      Note: The location of the file is distro specific.
            RHEL: /usr/lib/udev/rules.d/90-rdma-hw-modules.rules
            UBUNTU: /lib/udev/rules.d/90-rdma-hw-modules.rules
      If the bnxt_re driver is unloaded before creating VFs, vf creation loads bnxt_re
      driver. This operation throws error in dmesg as this is considered as loading
      driver after creating VFs. Disable RoCE on the adapter if RoCE feature is not
      required or disable this udev rule to prevent auto loading of the bnxt_re driver.

If SR-IOV is supported on the adapter, QPs, SRQs, CQs and MRs are distributed
across VF by the bnxt_re driver.

Driver allocates 64K of QPs, SRQs and CQs for the PF pool. It creates 256K MRs
for the PF pool.
For VFs, the driver is restricting the total number of resources as follows

Max QPs - 6144
Max MRs - 6144
Max CQs - 6144
Max SRQs - 4096

For eg: Active number of VFs can be obtained from the following command.
	$cat /sys/class/net/p6p1/device/sriov_numvfs

If sriov_numvfs is 2, half of the above values will be supported by each
VF.

Note: Since PF is in privileged mode, it is allowed to use the
entire PF pool resources. But VFs are restricted to create max configured
by the above calculation. User must ensure that total resources created by
PF and its VFs shall be less than Max configured (64K for QPs/SRQs/CQs and 256K for MRs).

Use following command to get the active resource count.
$cat /sys/kernel/debug/bnxt_re/<Device name>/info

Presence of active RoCE traffic on the VF undergoing Function Level Reset (FLR)
or on any other PFs/VFs impacts the function initialization time
of the VF undergoing FLR. Function initialization time scales linearly as
the cumulative active QP count across all PFs and VFs increases.
The increased function initialization time may lead to VF probe failures
and periodic HWRM timeouts when the cumulative active QP count is greater than 6K QPs.


Link Aggregation
================
Link aggregation is a common technique that is used to provide
additional aggregate bandwidth and high availability for logical
interfaces that aggregate multiple physical interfaces. Additional
aggregate bandwidth can be achieved by balancing the traffic load
across multiple physical interfaces. High availability can be achieved
by reconfiguring the loads across the active links when one of the
physical links fails.
The concepts of link aggregation can be applied to RoCE also.

The current solution allows a link aggregation only if all of the
following conditions are met:

-> The netdev associated with each RDMA interface is a
   part of an upper level device.
-> The two netdev interfaces part of  same bond device.
-> Two netdevs on the same physical device are added to the bond.
-> The link aggregate cannot span separate physical devices.
-> The bond interface has exactly two non-NPAR physical interfaces.
-> The bond mode is one of the following modes:
   round-robin (mode0), active-backup (mode 1), xor (mode 2),
   or 8023ad (mode 4).

Note: mode 0, 2 and 4 will be handled as active-active mode in HW.

When a LAG is created roce device interface is visible
with name bnxt_re_bond0.

Note: RoCE LAG is not supported on multi host or multi root configs.
Note: If VFs are created on any of the functions of the bond, RoCE Bond device
      will not be created. If RoCE bond is created before VF creation, RoCE bond
      will continue to work on the PFs. But VF RoCE devices will not be supported.
Note: If the adapter has more than 2 RoCE enabled functions (4 port adapter, etc.),
      RoCE bond device will not be created.
      There should be exactly two RoCE devices from an adapter when bond is
      created. If L2 bond is enabled on this adapter, RoCE doesn't work on
      the bnxt_re devices created for the physical interfaces.
Note: RoCE Bond is created only if there are two ethernet functions added
      to the bond and the ethernet devices are from the same physical
       adapter. Multiple adapters are not supported.
Note: When LAG is enabled, driver creates all QPs on PF0 and firmware
      does the load balancing between the 2 LAG ports. In the current
      algorithm, firmware will do load balancing on a per DPI (application)
      basis. If we have 100 applications creating 1 QP each then all the
      QPs will get created on the same port. Similarly if we have 100
      applications each creating odd number of QPs then the QP count
      difference between the ports can be up to 100. Only when all the
      applications are creating even number of QPs does the firmware
      guarantee that the difference in QP count between both ports
      is <= 2.
Note: On BCM9574xx devices to enable entropy for RoCE V2 UDP source port
      firmware limits the number of GIDs available to 8 across all PFs on
      Performance NIC and to 128 on Smart NIC. If host tries to create more
      GID entries than these limits then firmware will fail the GID add
      command and as a result QP data traffic will fail.
Note: RoCE LAG solution involves a HW pipeline configuration that enables
      RoCE traffic to be directed to the right port using an internal GID
      to port mapping logic. However, the HW transmit queues and ring
      shapers used for RoCE traffic are associated only with port 0.
      The GID to port mapping enables re-direction to the correct port as
      port status changes.

      To enable transmit endpoint shaping with RoCE LAG, even for an
      active-backup mode, the transmit endpoint shapers associated with
      port 0 always need to be enabled.

      For example, in active-backup mode if the following command was ran
      where port 0 and port 1 was linked at 100Gpbs

         bnxtqos -dev=<interface name> tx_endpoint_ratelimit port_idx=0 ep0 max=40

      The TX traffic out of port 0 would be 40Gpbs when port 0 is active.
      And when port 1 became active, the TX traffic out also would be 40Gpbs.
      This is because the shapers are associated with port 0 in active-backup
      mode. Please note in the example above if port_idx was set to 1
      in active-backup mode, the setting for port 1 will be set but not used.

      Another example, in active-backup mode when port 0 goes down, port 1 becomes
      active, transmit per COS statistics will not reflect the current active
      port stats. RoCE statistics available from debugfs interface are updated
      accurately and can be used.
Note: When the L2 bond is created and the RoCE LAG is not created by the driver
      due to the RoCE LAG not supported in the device, error messages are seen
      in the dmesg for GID add/delete.

=> Instructions to create/Destroy RoCE LAG

   - Load bnxt_en and bnxt_re driver
   - Follow the distro specific commands to create L2 bond. RoCE bond will be
     created in the background
   - ibv_devices shows bnxt_re_bond0 device once the L2 bond is created.

Note: If stable name is set by udev rule, the RoCE bond device name will point to the
device name of the first child device of the bond.

Known Issues with Link aggregation:

-> Supports only on distros RH 7.2 and later, SLES12 and later.
-> bnxt_re and bnxt_en drivers need to be loaded before creating bond interface.
-> Changing bond mode when RoCE driver is in use can cause system hang.
   E.g. changing the bonding mode while running a user application,
   can cause a system hang.
   Please make sure that no reference to bnxt_re is taken while changing the bond mode.
   Use the following command to check the module usage count
	#lsmod|grep bnxt_re
   For proper removal of bnxt_re devices or update the bond state:
   1. Unmount all active NFS RDMA mounts.
   2. Stop the ibacm service (or any similar service) on systems where OFED is
      installed using the command:
	# service ibacm stop
   3. Stop all user space RoCE applications.

-> User has to delete the configfs entry created for the bond device before
   a slave is removed from the bond. Without that, user would see error messages
   on the terminal and may cause a hang.
-> Create / destroy bond in a loop:
   Make sure that enough delay is provided (i.e. 5-10 sec) after create and destroy
   of the bond. This is to avoid hang and call traces related to the rtnl_lock usage.
-> When there is a link toggle, bnxt_re driver communicates that to the fw to switch over.
   If there are parallel outstanding FW cmds, it can take time for the fail over command
   to reach the FW. The QP timeout value should be high enough to accommodate this.
   It is recommended to use a timeout value 19.

- If the error recovery process fails for some reasons when the LAG is created,
  any subsequent administrative operations like de-slaving interfaces, unloading
  the bonding driver and bringing up base interfaces would cause unexpected
  behavior (can be a system crash).

BNXT_RE Driver Statistics
=======================

The bnxt_re driver supports debugFS which allows statistics and debug parameters be accessed.
To access this information, read the /sys/kernel/debug/bnxt_re/bnxt_re<x>/info file. Each port will be
listed with associated state. The available statistics will vary based on hardware capability, eg:

# cat /sys/kernel/debug/bnxt_re/bnxt_re0/info

bnxt_re debug info:
=====[ IBDEV bnxt_re0 ]=============================
	link state: UP
	Max QP: 0xff7f
	Max SRQ: 0xffff
	Max CQ: 0xffff
	Max MR: 0x10000
	Max MW: 0x10000
	Active QP: 0x2
	Active SRQ: 0x0
	Active CQ: 0x21
	Active MR: 0x4
	Active MW: 0x0
...


Field Explanation:

Device resource limits:
Max QP 		Max number of QP limit
Max SRQ		Max number of SRQ limit
Max CQ		Max number of CQs limit
Max MR		Max number of memory region limit
Max MW		Max number of memory window limit

Active Resources:
Active QP	Number of active QPs
Active SRQ	Number of active SRQs
Active CQ	Number of active CQs
Active MR	Number of active Memory Regions
Active MW	Number of active Memory Windows
Active RC QP	Number of active RC QPs
Active UD QP	Number of active UD QPs

Note: HW uses the same resource pages for MR and MW.
 So the total number of  Active MR and Active MW should
 be less than or equal to Max MR/MW.

Resource Watermarks:
QP Watermark	Max QPs active after driver load
SRQ Watermark   Max SRQs active after driver load
CQ Watermark    Max CQs active after driver load
MR Watermark    Max MRs active after driver load
MW Watermark    Max MWs active after driver load
AH Watermark    Max AHs active after driver load
PD Watermark    Max PDs active after driver load
RC QP Watermark	Max RC QPs active after driver load
UD QP Watermark	Max UD QPs active after driver load

Byte and Packet Counters:
Rx Pkts	 	Number of RoCE packets received
Rx Bytes	Number of RoCE bytes received
Tx Pkts		Number of RoCE packets transmitted
Tx Bytes	Number of RoCE bytes transmitted

Congestion Notification Counters:
CNP Tx Pkts	Number of RoCE CNP packets received
CNP Tx Bytes	Number of RoCE CNP bytes received
CNP Rx Pkts	Number of RoCE CNP packets transmitted
CNP Rx Bytes	Number of RoCE CNP bytes transmitted

RDMA operation Counters:
tx_atomic_req	Number of atomic requests transmitted
rx_atomic_req	Number of atomic requests received
tx_read_req	Number of read requests transmitted
tx_read_resp	Number of read responses transmitted
rx_read_req	Number of read requests received
rx_read_resp	Number of read responses received
tx_write_req	Number of write requests transmitted
rx_write_req	Number of write request received
tx_send_req	Number of send requests transmitted
rx_send_req	Number of send requests received

Driver Debug counters:
Resize CQ count		 Debug counter for CQ resize ops after driver load
num_irq_started		 Debug counter for IRQs started after device creation
num_irq_stopped		 Debug counter for IRQs stopped after device creation
poll_in_intr_en  	 Debug counter for indicating control path polling when
			 interrupt enabled
poll_in_intr_dis 	 Debug counter for indicating control path polling when
			 interrupt are disabled
cmdq_full_dbg_cnt 	 Debug counter to indicate control path CMDQ full
fw_service_prof_type_sup Debug info to indicate the current service profile config
dbq_int_recv		 Debug counter to indicate the DBQ interrupt received
dbq_int_en		 Debug counter to indicate the number of iterations dbq
                         interrupt is enabled
dbq_pacing_resched	 Debug counter to indicate the number of times pacing thread
			 rescheduled
dbq_pacing_complete	 Debug counter to indicate the count where the pacing thread
			 completed
dbq_pacing_alerts	 Debug counter to indicate the number of times userlibs alerted
			 the driver for onset congestion
dbq_dbr_fifo_reg	 Debug counter to monitor the HW FIFO reg
dbr_drop_recov_epoch		Debug counter to indicate epoch of latest DBR drop event
dbr_drop_recov_events		Debug counter to indicate the number of DBR drop events
dbr_drop_recov_timeouts		Debug counter to indicate the DBR drop events scheduled to the
				user space and failed to complete within the timeout.
dbr_drop_recov_timeout_users	Debug counter to indicate the number of user instances that
				experienced timeout when driver finishes the recovery thread.
dbr_drop_recov_event_skips	Debug counter to indicate the number of DBR drop events ignored
				(skipped) by the driver because of one or more outstanding event.
latency_slab		Each slab is of 1 second granularity. The Counters of each slab represent
			the total number of rcfw commands completed in that range.
			Upto 128 seconds latency is tracked.
rx_dcn_payload_cut	Number of received DCN payload cut packets.
te_bypassed		Number of transmitted packets that bypassed the transmit engine.

Recoverable Errors:
Recoverable Errors	Number of recoverable errors detected.  Recoverable errors are
	    		detected by the HW.  HW instructs FW to initiate the recovery
			process.  RC connection does not teardown as a result of these errors.
to_retransmits		Number of retransmission requests
rnr_naks_rcvd		Number of RNR (Receiver-Not-Ready) NAKs received.
dup_req			Number of duplicated requests detected.
missing_resp		Number of responses missing
seq_err_naks_rcvd	Number of PSN sequencing error NAKs received
res_oob_drop_count	Number of packets dropped because of no host buffers
res_oos_drop_count	Number of  out of sequence packets received
rx_roce_discard_pkts	Number of discard packets received
rx_roce_error_pkts	Number of error packets received


Fatal Errors:
max_retry_exceeded	Number of retransmission requests exceeded the max
unrecoverable_err	Number of unrecoverable errors detected
bad_resp_err		Number of bad response errors detected
local_qp_op_err		Number of QP local operation errors detected
local_protection_err	Number of local protection errors detected
mem_mgmt_op_err		Number of times HW detected an error because of illegal bind/fast
			register/invalidate attempted by the driver
remote_invalid_req_err	Number of invalid request received from the remote rdma initiator.
remote_access_err	Number of times H/W received a REMOTE ACCESS ERROR NAK from the peer.
remote_op_err		Number of times HW received a REMOTE OPERATIONAL ERROR NAK from the peer.

Responder errors:
res_exceed_max		Number of times HW detected incoming Send, RDMA write or RDMA read
			messages which exceed the maximum transfer length.
res_length_mismatch	Number of times HW detected incoming RDMA write message payload
			size does not match write length in the RETH.
res_exceeds_wqe		Number of times HW detected Send payload exceeds RQ/SRQ RQE buffer capacity.
res_opcode_err		Number of times HW detected First, Only, Middle, Last packets for
			incoming requests are improperly ordered with respect to the previous packet.
res_rx_invalid_rkey	Number of times HW detected a incoming request with an R_KEY that
			did not reference a valid MR/MW.
res_rx_domain_err	Number of times HW detected a incoming request with an R_KEY that
			referenced a MR/MW that was not in the same PD as the QP on which the
			request arrived.
res_rx_no_perm		Number of times HW detected a incoming RDMA write request with an
			R_KEY that referenced a MR/MW which did not have the access permission
			needed for the operation.
res_rx_range_err	Number of times HW detected an incoming RDMA write request that had
			a combination of R_KEY, VA and length that was out of bounds of the
			associated MR/MW.
res_tx_invalid_rkey	Number of times HW detected a R_KEY that did not reference a valid
			MR/MW while processing incoming read request.
res_tx_domain_err	Number of times HW detected a incoming request with an R_KEY that
			referenced a MR/MW that was not in the same PD as the QP on which
			the RDMA read request is received.
res_tx_no_perm		Number of times HW detected a incoming RDMA read request with an R_KEY
			that referenced a MR/MW which did not have the access permission needed
			for the operation.
res_tx_range_err	Number of times HW detected an incoming RDMA read request that had a
			combination of R_KEY, VA and length that was out of bounds of the associated MR/MW.
res_irrq_oflow		Number of times HW detected that peer sent us more RDMA read or atomic
			requests that the negotiated maximum
res_unsup_opcode	Number of times HW detected that peer sent us a request with an opcode
			for a request type that is not supported on this QP.
res_unaligned_atomic	Number of times HW detected that VA of an atomic request is on a memory
			boundary that prevents atomic execution.
res_rem_inv_err		Number of times HW detected a incoming send with invalidate request in
			which the R_KEY to invalidate did not MR/MW which could be invalidated.
res_mem_error64		Number of times HW detected a RQ/SRQ SGE which points to an inaccessible memory.
res_srq_err		Number of times HW detected a QP moving to error state because the associated
			SRQ is in error.
res_cmp_err		Number of time HW detected that there is no CQE space available on CQ or
			CQ is not in valid state.
res_invalid_dup_rkey	Number of times HW detected invalid R_KEY while re-sending responses to
			duplicate read requests.
res_wqe_format_err	Number of times HW detected error in the format of the WQE in the RQ/SRQ.
res_cq_load_err		Number of times HW detected error while attempting to load the CQ context.
res_srq_load_err	Number of times HW detected error while attempting to load the SRQ context.

Note: When a LAG is created, all the statistics are reported on function 0 of the device.
