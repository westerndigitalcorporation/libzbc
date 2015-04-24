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
#include <string>
#include <fstream>
#include <map>
#include <vector>
#include <cstdio>
#include <cassert>

#include <unistd.h>
#include <endian.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>

// Don't mangle the function names
extern "C" {
	#include <libzbc/zbc.h>
	#include <sha256.h>
}

#include "lkvs.hpp"
#include "lkvs.h"

LkvsDev::LkvsDev()
{
	aligned4kBuf = (char *)memalign(ALIGNMENT, ALIGNMENT);
	memset(aligned4kBuf, 0, ALIGNMENT);
	numZones = 0;
	lastReadZone = 0;
	zDevBlockSize = 0;
	cZones = 0;
	zDev = NULL;
	zDevZones = NULL;
	zoneMeta = NULL;
}

LkvsDev::~LkvsDev()
{
	int i = 0;
	
	// Release all of the zone specific MD buffers
	if ( zoneMeta ) {
		for( i = cZones; i < zDevNumZones; i++){

		  if( zoneMeta[i-cZones].mdBuf ) free( zoneMeta[i-cZones].mdBuf);
		}
		free(zoneMeta);
	}
	if( zDev ) zbc_close( zDev );
	if( zDevZones ) free( zDevZones);
	if (aligned4kBuf) free(aligned4kBuf);
}


int LkvsDev::checkDev(void)
{
	int ret = LKVS_FAILURE;
	unsigned int readBytes, nr_zones, i;
	SuperBlock *sb;

	if(!aligned4kBuf){
		std::cerr << "LKVS Dev 4k Buf allocation failed" << std::endl;
		goto out;
	}
	
	// Read the SB 
	readBytes = zbc_pread(zDev, &zDevZones[0], aligned4kBuf, 
	                      ALIGNMENT / zDevBlockSize, 0);
	if(readBytes != ALIGNMENT / zDevBlockSize ){
		std::cerr << "Error reading super block" << std::endl;
		goto out;
	}

	sb = (SuperBlock *)aligned4kBuf;
	if( sb->magic != LKVS_MAGIC){
		std::cerr << "LKVS MAGIC NOT PRESENT" << std::endl;
		goto out;
	}

	if( sb->devsize !=  devSize){
		std::cerr << "SB dev size does not match target dev size" 
		          << std::endl;
		goto out;
	}

	
	ret = LKVS_SUCCESS;
	
	//std::cerr << "LKVS checkdev success of size: " << devSize << std::endl
	//          << "Found: " << numZones << " zones" << std::endl; 
	
out:
	return (ret);
}

int LkvsDev::formatDev(void)
{
	SuperBlock *sb;
	uint32_t sbSize, written;
	unsigned long long startTime, endTime, offset;
	int ret = LKVS_FAILURE;	

	if( !aligned4kBuf ){
		std::cerr << "Aligned 4k buf not allocated" << std::endl;
		return (ret);
	}
	
	// Initialize buffer that will contain SB info
	memset(aligned4kBuf, 0, ALIGNMENT);
	sb = (SuperBlock *)aligned4kBuf;
	sb->magic = LKVS_MAGIC;
	sb->devsize = devSize; 

	//std::cerr << "SB written at: " << offset << std::endl;
	startTime = getTime();
	written = zbc_pwrite(zDev, &zDevZones[0], aligned4kBuf, 
	                     ALIGNMENT / zDevBlockSize, 0);
	if( written != ALIGNMENT / zDevBlockSize ){
		std::cerr << " Error writing super block" << std::endl;
		return ( ret );
	}
	// Make sure this ends up on the disk
	zbc_flush(zDev);
	endTime = getTime();
	// Set the write pointer on first format
	//wrPointer = ALIGNMENT;
	//std::cerr << "Format wrote: " << ALIGNMENT << " bytes to dev. " 
	//          << std::endl << "us: " << endTime - startTime << std::endl;
	ret = LKVS_SUCCESS;
	return ( ret );
}

int LkvsDev::populateMeta(int zoneIndex)
{
	
	MetaData *putMeta;
	KeyContainer key;
	unsigned int metaSize, metaCount = 0, blkCount = 0, blkMDEntries = 0;
	long long metaLocation;
	int ret = LKVS_FAILURE;
	unsigned long long offset, zOffset;
	char *mdPopOffset;
	metaSize = MD_PB_SZ;

	metaLocation = zDevZones[zoneIndex].zbz_write_pointer - 
	               ( ALIGNMENT / zDevBlockSize);
	while( metaLocation > zDevZones[zoneIndex].zbz_start ){
		unsigned int readBytes;
		// Read the Meta 
		memset(aligned4kBuf, 0, ALIGNMENT);
		zOffset = metaLocation - zDevZones[zoneIndex].zbz_start;
		//std::cout << "Reading data at zone: " << zoneIndex << " offset: "
		//          << zOffset << std::endl;
		readBytes = zbc_pread(zDev,&zDevZones[zoneIndex], aligned4kBuf, 
		                      ALIGNMENT / zDevBlockSize, zOffset);
		if( readBytes != ALIGNMENT / zDevBlockSize ){
			std::cerr << "Error reading metadata" << std::endl;
			goto out;
		}
		
		mdPopOffset = aligned4kBuf;
		while( blkMDEntries < (MD_ENTRIES_PER_BLOCK)){

			putMeta = (MetaData *)mdPopOffset;
			if(putMeta->magic != LKVS_META_MAGIC) {
				// Indicates a partial write make this more
				// efficient
				if( blkMDEntries == 0 ) metaLocation -= ALIGNMENT;
				break;
			}
			
			//std::cerr << "Found meta data write. Key: " << putMeta->key0
			//          << " location: " << putMeta->location 
			//          << " size: " << putMeta->size 
			//		  << " dump location: " << putMeta->mddump
			//		  << std::endl;
	
			key.setFromMeta(putMeta);
			try{
				md.insert( std::pair<KeyContainer, MetaData> 
				           (key, *putMeta));
			}catch (std::bad_alloc& ba){
				std::cerr << "Populate MD, Insert failed" << std::endl;
				goto out;
			}

			if( metaCount == 0){
				zoneMeta[zoneIndex - cZones].lastMDump = putMeta->mddump;
			}
			
			if( !blkCount ){
				char *mdBufLocation = zoneMeta[zoneIndex - cZones].mdBuf;
				mdBufLocation += zoneMeta[zoneIndex - cZones].mdEntries * MD_PB_SZ;
				memcpy(mdBufLocation, putMeta, MD_PB_SZ);	
				zoneMeta[zoneIndex - cZones].mdEntries++;
			}
			metaCount++;
			mdPopOffset += MD_PB_SZ;
			metaLocation = putMeta->mddump;
			blkMDEntries++;
		}
		blkCount++;
		blkMDEntries = 0; 
	}

	//std::cerr << "Populate Meta Found " << metaCount 
	//          << " entries. MDBuf offset: " <<  zone.mdEntries 
	//			<< " md blk count: " << blkCount << std::endl; 

	ret = LKVS_SUCCESS;

out:
	return (ret);

}

int LkvsDev::openDev(const char *dev, int flags)
{
	unsigned long long lba = 0;
	enum zbc_reporting_options ro = ZBC_RO_ALL;
	int ret = LKVS_FAILURE, i;
	// Open the target device
	targetDev = dev;
	zbc_device_info_t *zDevInfo;
	
	if( zDev ){
		std::cerr << "Device already open" << std::endl;
		goto out;
	}
	
	ret = zbc_open(targetDev.c_str(), O_RDWR, &zDev);
	if( ret != 0) {
		std::cerr << "ZBC open fails" << std::endl;
		ret = LKVS_FAILURE;
		goto out;
	}

	ret = zbc_list_zones(zDev, lba, ro, &zDevZones, &zDevNumZones);
	if( ret != 0){
		std::cout << "ZBC List Zones Fails" << std::endl;
		ret = LKVS_FAILURE;
		goto out;
	}

	cZones = 0;
	// Determine how many zones that are not sequential only
	for( i = 0; i < zDevNumZones; i++)
	{
		// Make sure that the conventional zones are in a contiguous
		// LBA range. May vary, but this works for our prototype drive.
		if(cZones > 0 && zbc_zone_conventional(&zDevZones[i])){
			assert(zbc_zone_conventional(&zDevZones[i-1]));
			cZones++;
		}else if(zbc_zone_conventional(&zDevZones[i])){
			cZones++;
		}
	}

	lastZoneAlloc = cZones;
	zDevInfo = (zbc_device_info_t *)malloc(sizeof(zbc_device_info_t));
	if(!zDevInfo){
		std::cerr << "Lkvs Dev info allocation failed" << std::endl;
		ret = LKVS_FAILURE;
		goto out;
	}
	
	zbc_get_device_info(zDev, zDevInfo);

	devSize = zDevInfo->zbd_logical_blocks; 
	zDevBlockSize = zDevInfo->zbd_logical_block_size;
	free(zDevInfo);

	if( flags == LKVS_FLAG_FORMAT ){
		if( formatDev() ){
			std::cerr << "formatDev failed" << std::endl;
			ret = LKVS_FAILURE;
			if( zDev ) zbc_close( zDev );
			goto out;
		}
	}

	// Check for magic and simple checks 
	if ( checkDev() ) {
		std::cerr << "checkdev failed" << std::endl;
		ret = LKVS_FAILURE;
		if( zDev ) zbc_close(zDev);
		zDev = NULL;
		goto out;
	}

	// Alocate memory for zone MetaData
	zoneMeta = (LkvsZone*)malloc(sizeof(LkvsZone) * zDevNumZones - cZones);	
	if( !zoneMeta ) {
		std::cerr << " ZoneMeta alloc fails" << std::endl;
		ret = LKVS_FAILURE;
		goto out;
	}
	memset(zoneMeta, 0, sizeof(LkvsZone) * (zDevNumZones - cZones));
	// Iterate over sequential only zones and grab metadata from 
	// zones that have data
	for( i = cZones; i < zDevNumZones; i++)
	{
		zoneMeta[ i - cZones].mdBuf = (char *)memalign(ALIGNMENT, ALIGNMENT);
		if( ! zoneMeta[i - cZones].mdBuf ){
			std::cerr << "MdBuf alloc fails" << std::endl;
			ret = LKVS_FAILURE;
			goto out;
		}
		memset(zoneMeta[i- cZones].mdBuf, 0, ALIGNMENT);
		zoneMeta[i - cZones].lastMDump = 0;
		zoneMeta[i - cZones].mdEntries = 0;

		if(zDevZones[i].zbz_write_pointer > zDevZones[i].zbz_start )
		{
			if( populateMeta(i) ){

				std::cerr << "Error in populate MD for zone: " 
			              << i << std::endl;
			}
		}
	}
	
	//std::cerr << "Made it to open dev without closing the fd" << std::endl;
	ret = LKVS_SUCCESS;

out:
	return (ret);
}

int LkvsDev::Put(const char *key, void *buf, size_t size)
{
	MetaData *putMeta;
	KeyContainer keyContainer;
	int ret = LKVS_FAILURE;
	unsigned int chunks, wrPointerOffset;
	unsigned long long reqSize, origReqSize;
	unsigned long long xferStart, xferEnd, offset;
	size_t written = 0, slack = 0;
	char *mdBufLocation, *alignedcBuf;
	char *cBuf = (char *)buf; 
	bool bufAligned = true, sizeAligned = true;
	zbc_zone_t * curZone = NULL;

	// Make sure the device is open
	if( !zDev ){
		std::cerr << "Device not opened" << std::endl;
		goto out;
	}
	
	// Size of zero do nothing, we don't want to use 4K to store a 
	// 256 bit hash of a string
	if( !size ){
		std::cerr << "Put of size zero is not supported" << std::endl;
		goto out;
	}

	// Check if the buffer is aligned
	if( (int)((size_t)buf & 0xFFF) ) {
		bufAligned = false;
	}
	
	// Check if the size is a 4K multiple
	if( (int)((size_t)size & 0xFFF) ) {
		sizeAligned = false;
	}
	
	reqSize = size;
	
	//std::cerr << "Put Request Key0: " << key << " Size: " 
	//          << reqSize << " LastZone: " << lastZoneAlloc << std::endl;

	// Build metadata entry from request
	keyContainer.setFromChar(key);
	// Check if the key is already present
	if (md.find(keyContainer) != md.end()){
		std::cerr << "Key already present in store. Aborting request" 
		          << std::endl;
		goto out;
	}

	// Find a zone to write this entry in
	if ( reserve(lastZoneAlloc, size + ALIGNMENT) ){
		//std::cerr << "Unable to reserve in prev zone, searching" << std::endl;
		// Need to search for a zone to alloc
		if( searchForZone(size + ALIGNMENT) ){
			std::cerr << "No space available for current request" 
			          << std::endl;
			goto out;
		}
	}

	//std::cerr << "Writing Request to Zone: " << lastZoneAlloc << std::endl;
	curZone = &zDevZones[lastZoneAlloc];
	// Align write pointer to 4k boundary
	//wrPointerOffset = curZone->zbz_write_pointer % 8;
	//if( wrPointerOffset ) curZone->wrPointer += 8 - wrPointerOffset;
	
	if(!bufAligned || !sizeAligned){
		alignedcBuf = (char *)memalign(ALIGNMENT, MAX_IO_REQ);
		if(!alignedcBuf){
			std::cerr << "Malloc of aligned put buf fails" << std::endl;
			goto out;
		}
		memset(alignedcBuf, 0, MAX_IO_REQ);
	}

	xferStart = getTime();
	// Code to deal with max_hw_segment && non aligned io
	while(written < size ){
		size_t curWritesz;
		size_t curWritten;
		char *toWrite = cBuf;  
		
		if((size - written) > MAX_IO_REQ){
			curWritesz = MAX_IO_REQ;
		}else{
			curWritesz = size - written;
		}
		// Can't copy beyond the passed in pointer so perfrom
		// memcpy before slack calculation
		if( !bufAligned || (!sizeAligned && curWritesz < MAX_IO_REQ)){
			memcpy(alignedcBuf, cBuf, curWritesz);
			toWrite = alignedcBuf;
		}
		// Make sure the writes to the disk are 4K aligned
		// Should only be set to > 0 at most once
		if( curWritesz % ALIGNMENT ) slack = ALIGNMENT - (curWritesz % ALIGNMENT);
		curWritesz += slack;
		//std::cerr << "Writing: " << curWritesz / zDevBlockSize 
		//          << " at logical block: " << curZone->zbz_write_pointer 
		//		  << std::endl;
		curWritten = zbc_write(zDev, curZone, toWrite, 
		                       curWritesz / zDevBlockSize);
		if( curWritten != curWritesz / zDevBlockSize)  break;
		written += curWritten * zDevBlockSize;
		cBuf += curWritten * zDevBlockSize;
		// zbc_write now updates the zone write pointer
		//curZone->zbz_write_pointer += curWritten;
	}
	xferEnd = getTime();

	if( (!bufAligned || !sizeAligned) && alignedcBuf) free(alignedcBuf);

	if( written != size + slack){
		std::cerr << " DATA Wanted to write: " << size 
		          << " bytes but wrote: " << written 
			      << " bytes." << std::endl;
		std::cerr << strerror(errno);
		goto out;
	}
		
	mdBufLocation = zoneMeta[lastZoneAlloc - cZones].mdBuf;
	if( zoneMeta[lastZoneAlloc - cZones].mdEntries < MD_ENTRIES_PER_BLOCK){
		mdBufLocation += (MD_PB_SZ * zoneMeta[lastZoneAlloc - cZones].mdEntries);
		zoneMeta[lastZoneAlloc - cZones].mdEntries++;
	}else{
		//std::cerr << "MdEntries: " << curZone->mdEntries 
		//          << ". Going to reset" << std::endl;
		memset(zoneMeta[lastZoneAlloc - cZones].mdBuf, 0, ALIGNMENT);
		zoneMeta[lastZoneAlloc - cZones ].mdEntries = 1;
	}

	putMeta = (MetaData *)mdBufLocation;
	// Set up and Write out the metadata for this write
	putMeta->size = size;
	putMeta->location = curZone->zbz_write_pointer - 
	                    ((size + slack) / zDevBlockSize);
	keyContainer.metaKeySet(putMeta);
	// Check the logic of this across zones
	putMeta->mddump = zoneMeta[lastZoneAlloc - cZones].lastMDump;
	putMeta->magic = LKVS_META_MAGIC;

	// Write the MD

	written = zbc_write(zDev, curZone, zoneMeta[lastZoneAlloc - cZones].mdBuf, 
	                     ALIGNMENT / zDevBlockSize );
	if( written != ALIGNMENT / zDevBlockSize){
		std::cerr << "MD Wanted to write: " << ALIGNMENT / zDevBlockSize 
		          << " bytes but wrote: " 
		          << written << " bytes." << std::endl;
		std::cerr << strerror(errno);
		zoneMeta[lastZoneAlloc - cZones].mdEntries--;
		goto out;
	}
	// zbc_write now updates the zone write pointer
	//curZone->zbz_write_pointer += written;
	// Update last offset 
	if( zoneMeta[lastZoneAlloc - cZones ].mdEntries == MD_ENTRIES_PER_BLOCK ){
		zoneMeta[lastZoneAlloc - cZones ].lastMDump = 
		  curZone->zbz_write_pointer - ALIGNMENT / zDevBlockSize;  
	}
	zbc_flush(zDev);
	try{
		md.insert( std::pair<KeyContainer, MetaData>
		           (keyContainer, *putMeta));
	}catch (std::bad_alloc& ba){
		std::cerr << "Insert of Put MD fails" << std::endl;
		goto out;
	}
	//std::cerr << "Put request complete. Xfer us: "
	//          << xferEnd - xferStart << ". Wrpointer: " 
	//		  << curZone->wrPointer << std::endl;
	ret = LKVS_SUCCESS;
out:
	return ( ret );
}

int LkvsDev::Get(const char *key, void *buf, size_t size)
{
	std::map<KeyContainer, MetaData>::iterator valueIter;
	int ret = LKVS_FAILURE;
	unsigned long long  keyLocation, zOffset; 
	KeyContainer keyContainer;
	ssize_t readBytes = 0, reqSize, slack = 0; 
	int fail = 0;
	char *cBuf = (char *)buf;
	bool bufAligned = true, sizeAligned = true;
	char *alignedcBuf;
	unsigned int zoneIndex;
	
	//std::cerr << "Get request begin servicing" << std::endl; 
	
	// Make sure the device is open
	if( !zDev ){
		std::cerr << "Device not opened" << std::endl;
		goto out;
	}

	// Check if the buffer is aligned
	if( (int)((size_t)buf & 0xFFF) ) {
		bufAligned = false;
	}
	
	// Check if the size is a 4K multiple
	if( (int)((size_t)size & 0xFFF) ) {
		sizeAligned = false;
	}

	keyContainer.setFromChar(key);
	valueIter = md.find(keyContainer);
	if (valueIter == md.end()) {
		std::cerr << "Get Key: " << key << ". Not found in metadata." 
		          << std::endl;
		goto out;
	}
	
	reqSize = valueIter->second.size;
	keyLocation = valueIter->second.location;
	
	if( reqSize != size){
		std::cerr << "Requested size does not match key size" << std::endl;
		goto out;
	}

	zoneIndex = blockToZone(keyLocation);
	//std::cerr << "Get Key: " << key << " Size: " << reqSize 
	//          << " Location: " << keyLocation << std::endl;
	
	
	if(!bufAligned || !sizeAligned){
		alignedcBuf = (char *)memalign(ALIGNMENT, MAX_IO_REQ);
		if(!alignedcBuf){
			std::cerr << "Malloc of aligned put buf fails" << std::endl;
			goto out;
		}
		memset(alignedcBuf, 0, MAX_IO_REQ);
	}

	// Code to deal with max_hw_segment
	while( readBytes < size ){
		size_t curReadsz;
		size_t curRead;
		char *toRead = cBuf;

		if((size - readBytes) > MAX_IO_REQ){
			curReadsz = MAX_IO_REQ;
		}else{
			curReadsz = size - readBytes;
		}
		
		if(!bufAligned || (!sizeAligned && curReadsz < MAX_IO_REQ)) { 
			toRead = alignedcBuf;
		}
		
		if( curReadsz % ALIGNMENT ) slack = ALIGNMENT - (curReadsz % ALIGNMENT);
		curReadsz += slack;
	
		zOffset = ( keyLocation + (readBytes / zDevBlockSize) ) - 
		          zDevZones[zoneIndex].zbz_start;
		curRead = zbc_pread(zDev, &zDevZones[zoneIndex], toRead, 
		                    curReadsz / zDevBlockSize, zOffset);
		if( curRead != curReadsz / zDevBlockSize) break;
		
		if(!bufAligned || (!sizeAligned && ((curReadsz - slack)  < MAX_IO_REQ))) { 
			memcpy(cBuf, toRead, curReadsz - slack);
		}
		readBytes += curRead * zDevBlockSize;
		cBuf += (curRead * zDevBlockSize) - slack;
	}

	if((!bufAligned || !sizeAligned) && alignedcBuf ) free(alignedcBuf);

	if( readBytes != size + slack){
		std::cerr << "Read: " << readBytes << " bytes, but asked for: "
			      << size << " bytes." << std::endl;
		goto out;
	}

	//std::cout << "Get Request finshed" << std::endl;
	ret = LKVS_SUCCESS;
out:
	return ( ret );
}

int LkvsDev::searchForZone(size_t size){

	unsigned int curZonePos;

	for(curZonePos = cZones; curZonePos < zDevNumZones; curZonePos++){
		if( !reserve(curZonePos, size) ){
			lastZoneAlloc = curZonePos;
			return LKVS_SUCCESS;
		}
	}

	return LKVS_FAILURE;
}

unsigned long long LkvsDev::getTime(void)
{
	struct timeval now;
	gettimeofday( &now, NULL);
	return now.tv_usec + (unsigned long long)now.tv_sec * 1000000;
}

int LkvsDev::reserve(int zoneIndex, size_t size){

	if( (zDevZones[zoneIndex].zbz_write_pointer + size / zDevBlockSize) <= 
		(zDevZones[zoneIndex].zbz_start + zDevZones[zoneIndex].zbz_length))
	{
		return LKVS_SUCCESS;
	}

	return LKVS_FAILURE;
}

unsigned int LkvsDev::blockToZone(uint64_t blockNum)
{
	unsigned int i;

	if( blockNum >= zDevZones[lastReadZone].zbz_start &&
		blockNum < zDevZones[lastReadZone].zbz_start + 
		            zDevZones[lastReadZone].zbz_length){
		return lastReadZone;
	}else{
		for(i = 0; i < zDevNumZones; i++){
			if( blockNum >= zDevZones[i].zbz_start &&
			    blockNum < zDevZones[i].zbz_start + 
			    zDevZones[i].zbz_length){
				
					lastReadZone = i;
					return lastReadZone;
				}
		}
	}
	return i;
}

void KeyContainer::setFromChar(const char *in)
{
	sha256_state md;
	sha256_init(&md);
	sha256_process(&md, (unsigned char *)in, (unsigned long)strlen(in));
	sha256_done(&md, (unsigned char *)&key);
}

void KeyContainer::setFromMeta(MetaData *md)
{
	key[0] = md->key0;
	key[1] = md->key1;
	key[2] = md->key2;
	key[3] = md->key3;
}

void KeyContainer::metaKeySet(MetaData *md)
{
	md->key0 = key[0];
	md->key1 = key[1];
	md->key2 = key[2];
	md->key3 = key[3];
}

bool KeyContainer::operator<(const KeyContainer &other) const
{
	if( key[0] == other.key[0]){
		
		if(key[1] == other.key[1]){
			
			if(key[2] == other.key[2]){
				
				return key[3] < other.key[3];
			}
			
			return key[2] < other.key[2];
		}
		
		return key[1] < other.key[1];
	}
	return key[0] < other.key[0]; 
}

// C API
extern "C" int lkvsdev_create(lkvsdev_t *lkvsdev){
	// Make sure this allocation succeeds
	LkvsDev *lkvsdevp;
	lkvsdevp = new LkvsDev();
	*lkvsdev = (void *)lkvsdevp;
	return LKVS_SUCCESS;
}

extern "C" int lkvsdev_open(lkvsdev_t lkvsdev, const char *devFile, int flags){
	LkvsDev *lkvsdevp = (LkvsDev *)lkvsdev;
	return lkvsdevp->openDev(devFile, flags);
}

extern "C" int lkvsdev_get(lkvsdev_t lkvsdev, const char *key, void *buf, size_t size){
	LkvsDev *lkvsdevp = (LkvsDev *)lkvsdev;
	return lkvsdevp->Get(key, buf, size);
}

extern "C" int lkvsdev_put(lkvsdev_t lkvsdev, const char *key, void *buf, size_t size){
	LkvsDev *lkvsdevp = (LkvsDev *)lkvsdev;
	return lkvsdevp->Put(key, buf, size);
}

extern "C" void lkvsdev_destroy(lkvsdev_t lkvsdev){
	// Make sure this allocation succeeds
	LkvsDev *lkvsdevp = (LkvsDev *)lkvsdev;
	delete lkvsdevp;
}
