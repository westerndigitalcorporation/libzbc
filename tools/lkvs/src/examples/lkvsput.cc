/* This file is part of LKVS.											
 * Copyright (C) 2009-2014, HGST, Inc. This software is distributed
 * under the terms of the GNU General Public License version 3,
 * or any later version, "as is", without technical support, and WITHOUT
 * ANY WARRANTY, without even the implied warranty of MERCHANTIBILITY or 
 * FITNESS FOR A PARTICULAR PURPOSE. You should have received a copy 
 * of the GNU General Public License along with LKVS. If not, 
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors : Adam Manzanares (adam.manzanares@hgst.com)
 *
 */
#include <iostream>
#include <string.h>
#include <malloc.h>

#include <stdlib.h>

#include "liblkvs.hpp"

int main(int argc, char **argv)
{

	int ret = LKVS_FAILURE;
	char *buf = NULL;
	unsigned long long size;

	LkvsDev lkvsDev;

	if( argc != 4 ){
		std::cout << "Usage lkvsput devicePath key size" << std::endl;
		goto out;
	}

	size = atoll(argv[3]);
	buf = (char *)memalign(ALIGNMENT, size);
	if( !buf ){
		std::cout << "Buf allocation fails" << std::endl;
		goto out;
	}
	memset(buf, 'c', size);
	
	if( lkvsDev.openDev(argv[1], LKVS_FLAG_FORMAT)) {
		std::cout << "Open with format fails" << std::endl;
		goto out;
	}
	
	ret = lkvsDev.Put(argv[2], buf, size);
		
out:
	if (buf) free(buf);
	return ( ret );

}

