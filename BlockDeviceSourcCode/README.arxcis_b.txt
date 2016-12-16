ArxcisDisk (arxcis_b) Linux block disk driver for Arxcis NVDIMM
v1.0

=================
== Description ==
=================
ArxcisDisk (arxcis_b) was designed to be used in high performing
environments and has been designed with simplicity in mind.
The arxcis_b driver has been designed to use Arxcis NVDIMM memory
as a block disk device (NVRamDisk).  Utilizing a simple command 
line interface, the system administrator is capable of 
dynamically adding new NVRamDisk devices of varying sizes, 
removing existing ones, and listing all existing NVRamDisk 
devices.


==================================
== Building the arxcis_b driver ==
==================================
Change into the project's directory path.

** NOTE: You are required to having either the full kernel source 
   or the kernel headers installed for your current kernel 
   revision.

** Root priviledge is required on some systems to build the 
   driver.  You may prepend each command line with "sudo" to gain 
   root priviledges.

To build arxcis_b from source, type the following on the command line:
	# make


=================================================
== Installing/Uninstalling the arxcis_b driver ==
=================================================
Installing/uninstalling the arxcis_b driver to/from the kernel's 
module directory will allow the user to manually load/unload the 
driver using the modprobe command.

Install the driver only one time after building the driver.

Installing the driver to the kernel's module directory will NOT 
load the driver.  You must use the modprobe command below to 
manually load the driver.

Change into the project's directory path.

** NOTE: Root priviledge is required to load and unload the 
   driver.  You may prepend each command line with "sudo" to gain 
   root priviledges.

To install the arxcis_b driver into the kernel's modules 
directory, type the following on the command line:
	# make install

To uninstall the arxcis_b driver from the kernel's modules 
directory, type the following on the command line:
	# make uninstall


====================================================
== Manually Loading/Unloading the arxcis_b driver ==
====================================================
At this time, automatically loading the driver during system 
reboot is NOT supported.  You must manually load the driver after 
each system reboot using the modprobe command below.

** NOTE: Root priviledge is required to load and unload the 
   driver.  You may prepend each command line with "sudo" to gain 
   root priviledges.

To load the arxcis_b driver:
	# modprobe arxcis_b [optional module parameters]

To unload the arxcis_b driver:
	# modprobe -r arxcis_b


=======================
== Module Parameters ==
=======================
max_devs: Total NVRamDisk devices available for use. 
     (Default = 128 = MAX) (int)

ioremap_mode: Cache attribute NVRamDisk. 1-Nocache, 2-Write 
     Combine, 3-Cache (Default=1) (int)

create_disk: Create 1 NVRamDisk device using entire NVDIMM at 
     driver load.  1-NVRamDisk created, 0-NVRamDisk NOT created 
     (Default = 1) (int)


===========
== Usage ==
===========
By default, the driver automatically creates 1 NVRamDisk device 
(arxcis_b0) utilizing the entire NVDIMM.

To create multiple arxcis_b NVRamDisk devices, load the driver 
using the create_disk=0 command line parameter:
    # modprobe arxcis_b create_disk=0

Then use the following commands to manage arxcis_b NVRamDisk devices:

Attach a new arxcis_b NVRamDisk device by typing:
    # echo "arxcis_b attach 0 0x40000" > /proc/arxcis_b   OR
    # echo "arxcis_b attach 1 8192" > /proc/arxcis_b

Detach a specific arxcis_b NVRamDisk device by typing:
    # echo "arxcis_b detach 0" > /proc/arxcis_b

Detach all arxcis_b NVRamDisk device by typing:
    # echo "arxcis_b detach all" > /proc/arxcis_b

View all arxcis_b NVRamDisk devices by typing:
    # cat /proc/arxcis_b

==============
== Examples ==
==============
# modprobe arxcis_b create_disk=0
# echo "arxcis_b attach 0 0x100000" > /proc/arxcis_b
# cat /proc/arxcis_b
ArxcisDisk (arxcis_b) 1.0

Maximum Number of Attachable Devices: 128
Number of Attached Devices: 1
       Device:   Start    -     End:    # Sectors:   #GB
    arxcis_b0: 0x00000000 - 0x000fffff: 0x00100000   0.5
 Total NVDIMM: 0x00000000 - 0x00800000: 0x00800000   4.0

# echo "arxcis_b attach 1 0x200000" > /proc/arxcis_b
# cat /proc/arxcis_b
ArxcisDisk (arxcis_b) 1.0

Maximum Number of Attachable Devices: 128
Number of Attached Devices: 2
       Device:   Start    -     End:    # Sectors:   #GB
    arxcis_b0: 0x00000000 - 0x000fffff: 0x00100000   0.5
    arxcis_b1: 0x00100000 - 0x002fffff: 0x00200000   1.0
 Total NVDIMM: 0x00000000 - 0x00800000: 0x00800000   4.0

# echo "arxcis_b attach 2 0x400000" > /proc/arxcis_b
# cat /proc/arxcis_b
ArxcisDisk (arxcis_b) 1.0

Maximum Number of Attachable Devices: 128
Number of Attached Devices: 3
       Device:   Start    -     End:    # Sectors:   #GB
    arxcis_b0: 0x00000000 - 0x000fffff: 0x00100000   0.5
    arxcis_b1: 0x00100000 - 0x002fffff: 0x00200000   1.0
    arxcis_b2: 0x00300000 - 0x006fffff: 0x00400000   2.0
 Total NVDIMM: 0x00000000 - 0x00800000: 0x00800000   4.0

# echo "arxcis_b detach 0" > /proc/arxcis_b
# cat /proc/arxcis_b
ArxcisDisk (arxcis_b) 1.0

Maximum Number of Attachable Devices: 128
Number of Attached Devices: 2
       Device:   Start    -     End:    # Sectors:   #GB
    arxcis_b1: 0x00100000 - 0x002fffff: 0x00200000   1.0
    arxcis_b2: 0x00300000 - 0x006fffff: 0x00400000   2.0
 Total NVDIMM: 0x00000000 - 0x00800000: 0x00800000   4.0

# echo "arxcis_b detach all" > /proc/arxcis_b
# cat /proc/arxcis_b
ArxcisDisk (arxcis_b) 1.0

Maximum Number of Attachable Devices: 128
Number of Attached Devices: 0
       Device:   Start    -     End:    # Sectors:   #GB
 Total NVDIMM: 0x00000000 - 0x00800000: 0x00800000   4.0
