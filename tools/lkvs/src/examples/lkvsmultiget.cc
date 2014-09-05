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
#include <sstream>
#include <cstdlib>

#include <malloc.h>
#include <sys/time.h>
#include <string.h>

#include "liblkvs.hpp"


unsigned long long getTime(void);

int main(int argc, char **argv)
{

	int ret = LKVS_FAILURE, numPut, i;
	std::ostringstream converter;
	unsigned long long startTime, endTime, elapsed;
	size_t size;
	char *buf = NULL, *verifyBuf = NULL;

	LkvsDev lkvs;

	if( argc != 4 ){
		std::cout << "Usage lkvsget device numPuts size" << std::endl;
		goto out;
	}

	numPut = atoi(argv[2]);
	size = atol(argv[3]);

	if( posix_memalign((void **)&buf, ALIGNMENT, size))
	{
		std::cout << "Allocation of buffer failed" << std::endl;
		goto out;
	}

	verifyBuf = (char *)malloc(size);
	if( !verifyBuf ){
		std::cout << "Allocation of verify buffer failed" << std::endl;
		goto out;
	}

	if( lkvs.openDev(argv[1], 0 ) ) goto out;
	
	startTime = getTime();
	for(i = 0; i < numPut; i++){
		converter << i;
		if( lkvs.Get(converter.str().c_str(), buf, size)) {
			std::cout << "Get of key: " << converter.str() 
			          << ". Failed" << std::endl;
			goto out;
		}
		memset(verifyBuf, (char)i, size);
		if( strncmp(buf, verifyBuf, size) ){
			std::cout << "Key: " << converter.str() << "data mismatch" 
			          << std::endl;
		}
		converter.str(std::string());
	}
	endTime = getTime();
	elapsed = endTime - startTime;
	std::cout << "Get ops took: " << elapsed << " us." << std::endl;
	std::cout << "Average op time: " << elapsed/numPut << " us." << std::endl;
	ret = LKVS_SUCCESS;
out:
	if(buf) free(buf);
	if(verifyBuf) free(verifyBuf);
	return ( ret );

}

unsigned long long getTime(void)
{
    struct timeval now;
    gettimeofday( &now, NULL);
    return now.tv_usec + (unsigned long long)now.tv_sec * 1000000;
}

