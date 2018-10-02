Copyright (C) 2009-2014, HGST, Inc. <br>
Copyright (C) 2016, Western Digital.


# <p align="center">libzbc</p>


## I. Introduction

### I.1. Overview

libzbc is a simple library  providing functions for manipulating disks
supporting the Zoned Block Command  (ZBC) and Zoned-device ATA command
set (ZAC)  disks.  libzbc  implemention is  compliant with  the latest
drafts  of the  ZBC  and  ZAC standards  defined  by INCITS  technical
committee T10 and T13 (respectively).

In addition to supporting ZBC and ZAC disks, libzbc also implements an
emulation mode allowing emulating the behavior of a host-managed zoned
disk using a regular file or a standard block device as backing store.

Several  example applications  using  libzbc are  available under  the
tools directory.

### I.2. Library version

libzbc current major version is 5.  Due  to interface  changes,  this
version  is  not compatible  with  previous  libzbc versions  (version
4.x). Overall, the library operation does not change, but applications
written for  previous libzbc versions must  be updated to use  the new
API.

### I.3. ZBC/ZAC Standards Versions Supported

The  "master"  code  branch  implements libzbc  v5.0.0  which  provide
supports for the latest ZBC and ZAC standards (Rev 05).

Support  for the  older draft  standards are  available with  previous
releases and pre-releases (see https://github.com/hgst/libzbc/releases
for details).

### I.4. License

libzbc  is distributed  under the  terms of  the of  the BSD  2-clause
license ("Simplified  BSD License"  or "FreeBSD  License"). A  copy of
this  license  with  the  library   copyright  can  be  found  in  the
COPYING.BSD file.

All  example applications  under the  tools directory  are distributed
under the terms of the GNU Lesser General Public License version 3, or
any later version. A copy of version 3 of this license can be found in
the COPYING.LESSER file.

libzbc  and all  its  example applications  are  distributed "as  is,"
without technical support, and WITHOUT  ANY WARRANTY, without even the
implied  warranty  of  MERCHANTABILITY  or FITNESS  FOR  A  PARTICULAR
PURPOSE.  Along  with libzbc, you should  have received a copy  of the
BSD 2-clause license and of the GNU Lesser General Public License.  If
not,  please   see  <http://opensource.org/licenses/BSD-2-Clause>  and
<http://www.gnu.org/licenses/>.

### I.5. Contact and Bug Reports

To report problems, please contact:
* Damien Le Moal  (damien.lemoal@wdc.com)
* Dmitry Fomichev (dmitry.fomichev@wdc.com)
* David Butterfield (david.butterfield@wdc.com)

PLEASE DO NOT SUBMIT CONFIDENTIAL INFORMATION OR INFORMATION SPECIFIC
TO DRIVES THAT ARE VENDOR SAMPLES OR NOT PUBLICLY AVAILABLE.

## II. Compilation and installation

### II.1. Requiremensts

libzbc requires  that the  autoconf, automake and  libtool development
packages be installed on the host used for compilation.

The GTK3 and GTK3 development packages are necessary to compile of the
gzbc application. Installing these  packages will automatically enable
the compilation of gzbc.

### II.2. Compilation

To compile  the library and  all example applications under  the tools
directory, execute the following commands.

	> sh ./autogen.sh
	> ./configure
	> make

### II.3. Installation

To install the library and all example applications compiled under the
tools directory, as root, execute the following command.

	> make install

The  library  files  are  by  default  installed  under  /usr/lib  (or
/usr/lib64).    The   library   header    files   are   installed   in
/usr/include/libzbc. The executable files for the example applications
are installed under /usr/bin.  These defaults can be changed using the
configure script.


Executing the following command displays the options used to control
the installation pathes.

	> ./configure --help

### II.4. Compilation for device tests

The test directory contains several test programs and scripts allowing
testing the compatibility of libzbc with a particular device. That is,
testing if a  device follows the same standard  as currently supported
by libzbc (see section I.2). The compilation of these test programs is
disabled by default.

To compile the test programs, libzbc must be configured as follows.

	> ./configure --with-test

The test  programs and scripts  are not  affected by the  execution of
"make install". All  defined tests must be executed  directly form the
test  directory  using the  zbc_test.sh  script.  To test  the  device
/dev/<SG node>, the following can be executed.

	> cd test
	> sudo ./zbc_test.sh /dev/<SG node>

By default, all  test cases will be executed. Detailed  control of the
test execution is  possible using the options "-e"  (execute) and "-s"
(skip). Execute zbc_test.sh --help for details.

libzbc  tests check  the detailed  error  output from  the device  for
invalid commands.  This detailed error output cannot be obtained for a
device    being   accessed    using   the    block   device    backend
driver. Specifying a block device file  for the tests will thus result
in many failed  tests. For block device files, tests  must be executed
using the SG node  of the block device. The SG node  of a block device
can be easily  identified with the command "lsscsi -g".  Note that for
an emulated device, a block device file can be specified.

Each test executed outputs a log file in the test/log directory. These
files can be  consulted in case of failed test  to identify the reason
for the test failure.

## III. Usage

### III.1 Kernel Version

libzbc functions  operate using device  handles which are  obtained by
executing  the zbc_open  function. The  path specified  to identify  a
device can point to a regular file,  a block device file or an SG node
device file (/dev/sg<x>).

As  host-aware devices  are background  compatible with  regular block
device files  (device type/signature  0x00), a host-aware  device will
always be  accessible either  through a  block device  file as well as
its SG node device file.

For host-managed devices, the different device type/signature requires
kernel  support for  block device  files to  be created.  Kernel level
support for host-managed block devices  has been added to Linux kernel
4.10. For older kernels, host-managed  devices will be accessible only
through  their  SG node  device  files.  Kernel  support for  the  ZAC
host-managed  device  signature  (0xabcd)  was  introduced  in  kernel
3.19. Any  kernel older than this  version will not create  an SG node
device file for  ZAC host-managed devices connected to a  SATA port on
the target host.

Regular files and  block device files for regular devices  can be used
to operate  libzbc in  emulation mode. This  will enable  exposing the
target file or block device as a host-managed zoned block device.

### III.2 Library Functions

libzbc provides functions for discovering  the zone configuration of a
zoned device and for accessing the  device. Accesses to the device may
result in changes  to the device zones condition,  attributes or state
(such as a sequential zone  write pointer location). These changes are
not internally tracked by libzbc. The functions provided to obtain the
device  zone  information  only  provide  a  "snapshot"  of  the  zone
condition and  state when  executed.  It is  the responsability  of an
application to implement tracking of  the device zone changes (such as
increment to a sequential zone write pointer as writes to the zone are
executed) if necessary.

All libzbc  functions, since version  5.0.0, use 512B sector  unit for
reporting  zone information  and  as the  addressing  unit for  device
accesses, independently of the actual  device logical block size. This
unification  in  the unit  used  by  all  API functions  can  simplify
application  development by  hiding potential  differences in  logical
block sizes between devices.  However, application programmers must be
careful to always implement accesses (read  or write) to the device in
multiple  of  the  logical  block  size  for  reading  and  writing  a
zone.  Furthermore,  on  host-managed  devices,  write  operations  to
sequential zones must be aligned on  a multiple of the device physical
block size.

The main functions provided are as follows.

Function               | Description 
-----------------------|----------------------------
zbc_open()             | Open a zoned device
zbc_close()            | Close a zoned device
zbc_get_device_info()  | Get device information
zbc_report_nr_zones()  | Get the number of zones
zbc_report_zones()<br>zbc_list_zones() | Get zone information
zbc_zone_operation()   | Execute a zone operation
zbc_open_zone()        | Explicitely open a zone
zbc_close_zone()       | Close an open zone
zbc_finish_zone()      | Finish a zone
zbc_reset_zone()       | Reset a zone write pointer
zbc_pread()            | Read data from a zone
zbc_pwrite()           | Write data to a zone
zbc_flush()            | Flush data to disk

The current implementation  of these functions is NOT  thread safe. In
particular,  concurrent write  operations by  multiple threads  to the
same zone may result in write errors without write ordering control by
the application.

Additionally, the following functions  are also provided to facilitate
application development and tests.

Function                 | Description 
-------------------------|----------------------------
zbc_set_log_level()      | Set log level of the library functions
zbc_device_is_zoned()    | Test if a device is a zoned block device
zbc_print_device_info()  | Print to a file (stream) a device information
zbc_device_type_str()    | Get a string description of a device type
zbc_device_model_str()   | Get a string description of a device model
zbc_zone_type_str()      | Get a string description of a zone type
zbc_zone_condition_str() | Get a string description of a zone condition
zbc_errno()              | Return sense key and sense code of the last command executed
zbc_sk_str()             | Get a string description of a sense key
zbc_asc_ascq_str()       | Get a string description of a sense code

### III.3 Native Mode Operation

Linux kernel older than version 4.10 do not create a block device file
for host-managed ZBC and ZAC devices.   As a result, these devices can
only be  accessed through  their associated  SG node  (/dev/sgx device
file).  For  these older kernels,  opening a  ZBC or ZAC  host managed
disk  with libzbc  must thus  be done  using the  device SG  node. For
kernel  versions 4.10  and  beyond compiled  with  zoned block  device
support, a block device file will be created for host-managed devices.

For host-aware  devices, the device block  device file or its  SG node
can both be used to open the device.

Once the device is open, accesses to the device are done transparantly
using    the   device    handle    returned    by   libzbc    zbc_open
function. Operations such  as report zones, reset  zone write pointer,
etc. only need the device handle.

### III.4 Emulation Mode Operation

libzbc can emulate  host-managed disks operation using  a regular file
or  a legacy  standard block  device  file (regular  disk or  loopback
device).  The use  of the  library in  such case  is identical  to the
native  mode  case,  assuming  that   the  emulated  device  is  first
configured by executing the zbc_set_zones  tool. For an emulated zoned
block device setup using a regular block device, the block device file
of the backend device must always be used. Using the backend device SG
node file will not work.

### III.5 Documentation

More  detailed  information on  libzbc  functions  and data  types  is
available through the comments  in the file include/libzbc/zbc.h. This
file  has comments formatted with  the doxygen convention.  HTML files
documenting libzbc API can be generated using the doxygen project file
documentation/libzbc.doxygen.

	> cd documentation
	> doxygen libzbc.doxygen

## IV. Example Applications

Under the  tools directory, several simple  applications are available
as examples.  These appliations are as follows.

### IV.1. gzbc (tools/gui)

gzbc provides a graphical user interface showing zone information of a
zoned  device.   It also  displays  the  write status  (write  pointer
position) of  zones graphically  using color  coding (red  for written
space and  green for  unwritten space). Some  operations on  zones can
also  be  executed  directly  from the  interface  (reset  zone  write
pointer, open zone, close zone, etc).

### IV.2. zbc_report_zones (tools/report_zones/)

This application illustrates  the use of the  zone reporting functions
(zbc_report_zones,  zbc_report_nr_zones, zbc_list_zones).   It obtains
the zone information  of a device and displays it  in readable form on
the standard output.

### IV.3. zbc_open_zone (tools/open_zone/)

This  application illustrates  the use  of the  zbc_open_zone function
allowing opening a zone.

### IV.4. zbc_close_zone (tools/close_zone/)

This application  illustrates the  use of the  zbc_close_zone function
allowing closing a zone.

### IV.5. zbc_finish_zone (tools/finish_zone/)

This application  illustrates the use of  the zbc_finish_zone function
allowing finishing a zone.

### IV.6. zbc_reset_zone (tools/reset_zone/)

This application  illustrates the  use of  the zbc_reset_write_pointer
function allowing resetting  the write pointer of a zone  to the start
LBA of the zone.

### IV.7. zbc_read_zone (tools/read_zone/)

This application reads data from a  zone, up to the zone write pointer
location and either send the read  data to the standard output or copy
the  data to  a  regular  file. It  implementation  uses the  function
zbc_pread.

### IV.8. zbc_write_zone (tools/write_zone/)

This application illustrates the use of the zbc_pwrite function which
write data to a zone at the zone write pointer location.

### IV.9. zbc_set_zones (tools/set_zones/)

This application can be used to initialize the ZBC emulation mode for
a regular file or a raw standard block device.

### IV.10. zbc_set_write_ptr (tools/set_write_ptr/)

This application can be used to set  the write pointer of a zone of an
emulated  ZBC  device to  any  LBA  value  (within  the range  of  the
specified zone). It  is intended for testing purposes only  and is not
valid for native ZBC devices.

### IV.11. zbc_info (tools/info/)

This  application tests  if a  device file  points to  a physical  SMR
device supporting either ZBC or  ZAC. This excludes the emulation mode
implemented  by  libzbc on  top  of  regular  files or  regular  block
devices.  If the  device is identified as SMR,  some information about
the device are displayed (device type, capacity, sector size, etc).

