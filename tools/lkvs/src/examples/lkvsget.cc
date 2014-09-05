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

#include "lkvs.hpp"

int main(int argc, char **argv)
{

	int ret = LKVS_FAILURE, curPos = 0;
	char *buf = NULL;
	unsigned long long size = 0;
	ssize_t n_written = 0;
	LkvsDev lkvs;


	if( argc != 4 ){
		std::cout << "Usage lkvsget devicePath key size" << std::endl;
		goto out;
	}

	size = atoll(argv[3]);

	if( size == 0 ){
		std::cout << "Size zero doing nothing" << std::endl;
		goto out;
	}
	
	buf = (char *)memalign(ALIGNMENT, size);

	if( !buf ){
		std::cout << "Allocation of buffer failed" << std::endl;
		goto out;
	}

	if( lkvs.openDev(argv[1], 0 )) goto out;
	
	ret = lkvs.Get(argv[2], buf, size);
	
	n_written = fwrite(buf, 1, size, stdout);
	if( n_written != size ){
		std::cout << "Unable to write buf to stdout" << std::endl;
	}
	ret = LKVS_SUCCESS;
out:
	if( buf ) free(buf);
	return ( ret );
}

