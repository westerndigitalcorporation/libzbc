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

from distutils.core import setup
from distutils.extension import Extension
from Cython.Distutils import build_ext

ext_modules = [Extension( "lkvsdev",
						["lkvsdev.pyx"],
						language="c++",
						include_dirs=["../liblkvs"],
						libraries=["lkvs"] )]

setup(
	name = "lkvsdev",
	cmdclass = {"build_ext": build_ext},
	ext_modules = ext_modules)
