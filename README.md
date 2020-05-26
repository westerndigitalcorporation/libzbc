Copyright (C) 2016, Western Digital.<br>
Copyright (C) 2020 Western Digital Corporation or its affiliates.


# libzbc

*libzbc* is a simple library providing functions for manipulating SCSI and ATA
devices supporting the Zoned Block Command (ZBC) and Zoned-device ATA command
set (ZAC) specifications.

*libzbc* implementation is compliant with the latest drafts of the ZBC and ZAC
standards defined by INCITS technical committee T10 and T13 (respectively).

In addition to supporting ZBC and ZAC disks, *libzbc* also implements an
emulation mode allowing to imitate the behavior of a host managed zoned disk
using a regular file or a standard block device as backing store.

Several example applications using *libzbc* are available under the tools
directory.

### Library version

*libzbc* current major version is 5. Due to interface changes, this version is
not compatible with previous *libzbc* versions (version 4.x). Overall, the
library operation does not change, but applications written for previous
*libzbc* versions must be updated to use the new API.

### ZBC and ZAC Standards Versions Supported

*libzbc* latest version is implements ZBC and ZAC standards revision 05. Support
for the older draft standards are available with [previous releases and
pre-releases](https://github.com/hgst/libzbc/releases).

### License

*libzbc* source code is distributed under the terms of the BSD 2-clause
license ("Simplified BSD License" or "FreeBSD License", SPDX: *BSD-2-Clause*)
and under the terms of the GNU Lesser General Public License version 3, or any
later version (SPDX: *LGPL-3.0-or-later*).
A copy of these licenses with *libzbc* copyright can be found in the files
[LICENSES/BSD-2-Clause.txt] and [COPYING.BSD] for the BSD 2-clause license and
[LICENSES/LGPL-3.0-or-later.txt] and [COPYING.LESSER] for the LGPL-v3 license.
If not, please see
http://opensource.org/licenses/BSD-2-Clause and http://www.gnu.org/licenses/.

All example applications under the tools directory are distributed under the
terms of the GNU Lesser General Public License version 3, or any later version
(SPDX: *LGPL-3.0-or-later*).

*libzbc* and all its example applications are distributed "as is," without
technical support, and WITHOUT ANY WARRANTY, without even the implied warranty
of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

All source files in *libzbc* contain the BSD 2-clause and LGPL v3 license SPDX
short identifiers in place of the full license text.

```
SPDX-License-Identifier: BSD-2-Clause
SPDX-License-Identifier: LGPL-3.0-or-later
```

Some files such as the `.gitignore` file are public domain specified by the
CC0 1.0 Universal (CC0 1.0) Public Domain Dedication. These files are
identified with the following SPDX header.

```
SPDX-License-Identifier: CC0-1.0
```

See [LICENSES/CC0-1.0.txt] for the full text of this license.

### Contributions and Bug Reports

Contributions are accepted as github pull requests. Any problem may also be
reported through github issue page or by contacting:

* Damien Le Moal (damien.lemoal@wdc.com)
* Dmitry Fomichev (dmitry.fomichev@wdc.com)

PLEASE DO NOT SUBMIT CONFIDENTIAL INFORMATION OR INFORMATION SPECIFIC TO DRIVES
THAT ARE VENDOR SAMPLES OR NOT PUBLICLY AVAILABLE.

## Compilation and installation

*libzbc* requires the following packages for compilation:

* autoconf
* autoconf-archive
* automake
* libtool

The GTK3 and GTK3 development packages must be installed to automatically enable
compiling the *gzbc* and *gzviewer* applications.

To compile the library and all example applications under the tools directory,
execute the following commands.

```
$ sh ./autogen.sh
$ ./configure
$ make
```

To install the library and all example applications compiled under the tools
directory, execute the following command.

```
$ sudo make install
```

The library file is by default installed under `/usr/lib` (or `/usr/lib64`).
The library header file is installed in `/usr/include/libzbc`. The executable
files for the example applications are installed under `/usr/bin`.

These default installation locations can be changed using the configure script.
Executing the following command displays the options used to control the
installation paths.

```
$ ./configure --help
```

## Compilation with GUI tools

The *gzbc* and *gzviewer* tools implement a graphical user interface (GUI) using
the GTK3 toolkit. The configure script will automatically detect the presence of
GTK3 and its development header files and compile these tools if the header
files are found. This behavior can be manually changed and the compilation of
*gzbc* and *gzviewer* disabled using the `--disable-gui` configuration option.

```
$ ./configure --disable-gui
```

## Compilation for device tests

The test directory contains several test programs and scripts allowing testing
the compatibility of *libzbc* with a particular device. That is, testing if a
device follows the same standard as currently supported by *libzbc*. The
compilation of these test programs is disabled by default.

To compile the test programs, *libzbc* must be configured as follows.

```
$ ./configure --with-test
```

The test programs and scripts are not affected by the execution of "make
install". All defined tests must be executed directly from the test directory
using the *zbc_test.sh* script. To test the device `/dev/<SG node>`, the
following can be executed.

```
$ cd test
$ sudo ./zbc_test.sh /dev/<SG node>
```

By default, the script will run through all the test cases. Detailed control
of the test execution is possible using the `-e` (execute) and `-s`(skip)
options. Run `zbc_test.sh --help` for details.

*libzbc* tests check the detailed error output from the device for invalid
commands. This detailed error output cannot be obtained for a device being
accessed using the block device backend driver. Specifying a block device file
for the tests is thus not allowed.

Each test outputs a log file in the `test/log` directory. These files can be
consulted in case of a failed test to identify the reason for the test failure.

## Building rpm packages

The following command will build redistributable rpm packages.

```
$ make rpm
```

Three rpm packages are built: a binary package providing the library and
executable tools, a development package providing *libzbc* header files and a
source package. The source package can be used to build the binary and
development rpm packages outside of *libzbc* source tree using the following
command.

```
$ rpmbuild --rebuild libzbc-<version>.src.rpm
```

## Library Overview

*libzbc* functions operate using a device handle obtained by executing the
*zbc_open()* function. The path specified to identify a device can point to a
regular file, a block device file (*/dev/sdX*)or an SG node device file
(*/dev/sgY*).

As host-aware devices are backward-compatible with regular block device files
(device type/signature 0x00), a host-aware device will always be accessible
either through a block device file as well as its SG node device file.

For host-managed devices, the different device type and device signature require
kernel support for block device files to be enabled. Kernel support for
host-managed devices has been added with Linux kernel 4.10. For older kernels,
host-managed devices will be accessible only through their SG node device files.
Kernel support for the ZAC host-managed device signature (0xabcd) was introduced
in kernel 3.18. Any kernel older than this version will not create an SG node
device file for ZAC host-managed devices connected to an AHCI SATA port on the
target host.

Regular files and block device files for regular devices can be used to operate
*libzbc* in emulation mode. This will enable exposing the target file or block
device as a host-managed zoned device.

### Library Functions

*libzbc* provides functions for discovering the zone configuration of a zoned
device and for accessing the device. Accesses to the device may result in
changes to the device zones condition, attributes and state (such as a
sequential zone write pointer location). These changes are not internally
tracked by *libzbc*. The functions provided to obtain the device zone
information only provide a snapshot of the zone condition and state when
executed. It is the responsability of an application to implement tracking of
the device zone changes (such as increment to a sequential zone write pointer as
writes to the zone are executed) if necessary.

All *libzbc* functions since version 5.0.0 use 512B sector unit for reporting
zone information and as the addressing unit for device accesses, regardless of
the actual device logical block size. This unification in the unit used by all
API functions can simplify application development by hiding potential
differences in logical block sizes between devices. However, application
programmers must be careful to always implement write accesses to sequential
write required zones of the device in multiple of the physical block size.

The main functions provided by *libzbc* are as follows.

Function                 | Description
-------------------------|---------------------------------------------
*zbc_open()*             | Open a zoned device
*zbc_close()*            | Close a zoned device
*zbc_get_device_info()*  | Get device information
*zbc_report_nr_zones()*  | Get the number of zones of the device
*zbc_report_zones()* <br> *zbc_list_zones()* | Get zone information
*zbc_zone_operation()*   | Execute a zone operation
*zbc_open_zone()*        | Explicitely open a zone
*zbc_close_zone()*       | Close an open zone
*zbc_finish_zone()*      | Finish a zone
*zbc_reset_zone()*       | Reset a zone write pointer
*zbc_pread()*            | Read data from a zone
*zbc_preadv()*           | Read data from a zone using vectored buffer
*zbc_pwrite()*           | Write data to a zone
*zbc_pwritev()*          | Write data to a zone using vectored buffer
*zbc_flush()*            | Flush data to disk

Additionally, the following functions are also provided to facilitate
application development and tests.

Function                   | Description
---------------------------|---------------------------------------------------
*zbc_map_iov()*            | Map a vectored buffer using a single buffer
*zbc_set_log_level()*      | Set the logging level of the library functions
*zbc_device_is_zoned()*    | Test if a device is a zoned block device
*zbc_print_device_info()*  | Print device information to a file (stream)
*zbc_device_type_str()*    | Get a string description of a device type
*zbc_device_model_str()*   | Get a string description of a device model
*zbc_zone_type_str()*      | Get a string description of a zone type
*zbc_zone_condition_str()* | Get a string description of a zone condition
*zbc_errno()*              | Get the sense key and code of the last function call
*zbc_sk_str()*             | Get a string description of a sense key
*zbc_asc_ascq_str()*       | Get a string description of a sense code

*libzbc* does not implement any synchronization mechanism for multiple threads
or processes to safely operate simultaneously on the same zone. In particular,
concurrent write operations by multiple threads to the same zone may result in
write errors without write ordering control by the application. The
*zbc_errno()* function is the only exception to this rule. This function is
thread safe and does not require serialized execution by the application.

### Native Operation Mode

Linux kernels older than version 4.10 do not create a block device file for
host-managed ZBC and ZAC devices. As a result, these devices can only be
accessed through their associated SG node (/dev/sgx device file). For these
older kernels, opening a ZBC or ZAC host managed disk with *libzbc* must thus
be done using the device SG node. For kernel versions 4.10 and beyond compiled
with zoned block device support, the device will be exposed also through a block
device file which can be used with *libzbc* to identify the device.

For host-aware devices, a block device file and an SG node file will exist and
can both be used to open the device.

Once the device is open, accesses to the device are done transparently using the
device handle returned by the *zbc_open()* function. Operations such as report
zones, reset zone write pointer, etc. only need the device handle.

### Emulation Mode

*libzbc* can emulate host-managed disks operation using a regular file or a
legacy standard block device file (regular disk or loopback device). The use of
the library in such case is identical to the native mode case, assuming that
the emulated device is first configured by executing the *zbc_set_zones tool*.
For an emulated zoned block device setup using a regular block device, the block
device file of the backend device must always be used. Using the backend device
SG node file will not work.

### Functions Documentation

More detailed information on *libzbc* functions and data types is available
through the comments in the file `include/libzbc/zbc.h`. This file has comments
formatted with the doxygen convention. HTML files documenting *libzbc* API can
be generated using the doxygen project file documentation/libzbc.doxygen.

```
$ cd documentation
$ doxygen libzbc.doxygen
```

## Tools

Under the tools directory, several simple applications are available as
examples. These appliations are as follows.

* **zbc_info** This application tests if a device file points to a physical
  zoned device supporting ZBC or ZAC features. This excludes the emulation mode
  implemented by *libzbc*. If the device is identified as a zoned device, some
  information about the device are displayed (e.g. the device type, capacity,
  sector size, etc).

* **zbc_report_zones** This application illustrates the use of the zone
  reporting functions *zbc_report_zones()*, *zbc_report_nr_zones()* and
  *zbc_list_zones()*. *zbc_report_zones* obtains the zone information of a
  device and displays it in readable form on the standard output.

* **zbc_open_zone** This application illustrates the use of the
  *zbc_open_zone()* function allowing opening a zone.

* **zbc_close_zone** This application illustrates the use of the
  *zbc_close_zone()* function allowing closing a zone.

* **zbc_finish_zone** This application illustrates the use of the
  *zbc_finish_zone()* function allowing finishing a zone.

* **zbc_reset_zone** This application illustrates the use of the
  *zbc_reset_zone()* function allowing resetting the write pointer of a zone to
  the first sector of the zone.

* **zbc_read_zone** This application reads data from a zone, up to the zone
  write pointer location and either sends the read data to the standard output
  or copies the data to a regular file. Its implementation illustrates the use
  of the functions *zbc_pread()* and *zbc_preadv()*.

* **zbc_write_zone** This application illustrates the use of the functions
  *zbc_pwrite()* and *zbc_pwritev()* to write data to a zone at the zone write
  pointer location.

* **zbc_set_zones** This application can be used to initialize the ZBC emulation
  mode for a regular file or a raw standard block device.

* **zbc_set_write_ptr** This application can be used to set the write pointer of
  a zone of an emulated ZBC device to any LBA value (within the range of the
  specified zone). It is intended for testing purposes only and is not valid for
  native ZBC devices.

* **gzbc** provides a graphical user interface showing zone information of a
  zoned device. It also displays the write status (write pointer position) of
  zones graphically using color coding (red for written space and green for
  unwritten space). Some operations on zones can also be executed directly from
  the interface (reset zone write pointer, open zone, close zone, etc).

* **gzviewer** provides a simple graphical user interface showing the write
  pointer position and zone state of zones of a zoned device. Similar color
  coding as *gzbc* is used.
