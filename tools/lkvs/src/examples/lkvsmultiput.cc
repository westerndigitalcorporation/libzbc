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

#include <sys/time.h>
#include <malloc.h>
#include <string.h>

#include "lkvs.hpp"

unsigned long long getTime(void);

int main(int argc, char **argv)
{

	int ret = LKVS_FAILURE , numPut, i;
	std::ostringstream converter;
	unsigned long long startTime, endTime, elapsed;
	size_t size;
	char *buf = NULL;

	LkvsDev lkvs;

	if( argc != 4 ){
		std::cout << "Usage lkvsput dev numPuts Size" << std::endl;
		goto out;
	}

	numPut = atoi(argv[2]);
	size = atol(argv[3]);

	if(lkvs.openDev(argv[1], LKVS_FLAG_FORMAT)) {
		goto out;
	}
	

	if(posix_memalign((void **)&buf, ALIGNMENT, size))
	{
		std::cout << "Malloc of data buffer failed " << std::cout;
		goto out;
	}

	startTime = getTime();
	for(i = 0; i < numPut; i++){
		converter << i;
		memset(buf, (char)i, size);
		if( lkvs.Put(converter.str().c_str(), buf, size)) {
			std::cout << "Put of: " << converter.str() << "failed" 
			          << std::endl;
			goto out;
		}
		converter.str(std::string());
	}
	endTime = getTime();
	elapsed = endTime - startTime;
	std::cout << "Put ops took: " << elapsed << " us." << std::endl;
	std::cout << "Average op time: " << elapsed/numPut << " us." << std::endl;
	ret = LKVS_SUCCESS;

out:
	if( buf ) free(buf);
	return ( ret );

}

unsigned long long getTime(void)
{
    struct timeval now;
    gettimeofday( &now, NULL);
    return now.tv_usec + (unsigned long long)now.tv_sec * 1000000;
}

