#!/bin/bash

# This file is part of LKVS.											
# Copyright (C) 2009-2014, HGST, Inc. This software is distributed
# under the terms of the GNU General Public License version 3,
# or any later version, "as is", without technical support, and WITHOUT
# ANY WARRANTY, without even the implied warranty of MERCHANTIBILITY or 
# FITNESS FOR A PARTICULAR PURPOSE. You should have received a copy 
# of the GNU General Public License along with LKVS. If not, 
# see <http://www.gnu.org/licenses/>.
#
# Authors : Adam Manzanares (adam.manzanares@hgst.com)
#
#

dd if=/dev/zero of=$LKVSDEVFILE count=1 bs=4096 oflag=sync conv=notrunc
zbc_set_zones $LKVSDEVFILE set_sz 2097152 2097152
