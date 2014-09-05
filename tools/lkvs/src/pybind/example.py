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

import lkvsdev 
import sys

if len(sys.argv) != 2:
	print "Not enough arguments. Exiting"
	exit()

devFile = sys.argv[1]
x = lkvsdev.PyLkvsDev()
x.openDev(devFile, 1)
data = "Hello World"
x.Put("test", data)
response = x.Get("test", len(data))
print response
