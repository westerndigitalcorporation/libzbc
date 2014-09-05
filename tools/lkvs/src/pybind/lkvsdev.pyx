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
# distutils: language = c++
# distutils: sources = liblkvs.cpp

from ctypes import *

cdef extern from "liblkvs.hpp":
	cdef cppclass LkvsDev:
		LkvsDev() except +
		int openDev(const char *, int)
		int Put(const char *, char *, size_t)
		int Get(const char *, char *, size_t)

cdef class PyLkvsDev:
	cdef LkvsDev* thisptr
	def __cinit__(self):
		self.thisptr = new LkvsDev()
	def __dealloc_(self):
		del self.thisptr
	def openDev(self, device, flags):
		if not isinstance(device, str):
			raise TypeError('device must be a string')
		if not isinstance(flags, int):
			raise TypeError('flags must be a int')
		cdef char *p = device
		return self.thisptr.openDev(p, flags)
	def Put(self, key, value):
		if not isinstance(key, str):
			raise TypeError('key must be a string')
		if not isinstance(value, str):
			raise TypeError('value must be a string')
		cdef char *pkey = key
		cdef char *pval = value
		size = len(value)
		self.thisptr.Put(pkey, pval, size)
	def Get(self, key, size):
		if not isinstance(key, str):
			raise TypeError('key must be a string')
		if not isinstance(size, int):
			raise TypeError('size must be a int')
		cdef char *pkey = key
		value = bytearray(size)
		cdef char *pval = value
		result = self.thisptr.Get(pkey, pval, size)
		return value
