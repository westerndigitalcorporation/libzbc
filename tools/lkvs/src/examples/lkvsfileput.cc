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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>

#include "lkvs.hpp"

int main(int argc, char **argv)
{

	int ret;
	char *buf;
	LkvsDev lkvsDev;
	int dataFilefd;
	off_t dataFileSize;
	ssize_t readBytes;

	if( argc != 3 ){
		std::cout << "Usage lkvsput devicePath filepath" << std::endl;
		ret = -1;
		return ( ret ) ;
	}

	dataFilefd = open(argv[2], O_RDONLY );
	if( dataFilefd < 0 ){
		std::cout << "Error opening: " << argv[2] << std::endl;
		ret = -1;
		return (ret);
	}
	// Get file size
	dataFileSize = lseek(dataFilefd, 0, SEEK_END);	
	lseek(dataFilefd, 0, SEEK_SET);
	
	buf = (char *)memalign(ALIGNMENT, dataFileSize);
	if( !buf ){
		std::cout << "Malloc of buffer fails" << std::endl;
		ret = -1;
		return (ret);

	}

	// Read the contents of the file into the buffer
	readBytes = read( dataFilefd, buf, dataFileSize);
	if( readBytes != dataFileSize){
		std::cout << "Read of file returned fewere bytes than expected" 
		          << std::endl;
		ret = -1;
		return (ret);
	}
	
	if( lkvsDev.openDev(argv[1], LKVS_FLAG_FORMAT)) return 1;
	

	ret = lkvsDev.Put(basename(argv[2]), buf, dataFileSize);
		
	close(dataFilefd);
	free(buf);
	return ( ret );

}

