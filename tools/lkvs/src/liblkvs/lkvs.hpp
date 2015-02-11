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

/** @file */
#include <string>
#include <vector>
#include <map>
#include <cstdio>
#include <stdint.h>

// Don't mangle the function names from C code
extern "C" { 
    #include <libzbc/zbc.h> 
    #include <sha256.h> 
}


/**
 * On disk superblock 
 * 
 * Used to verify that the device is set up as a LKVS Device
 * This super block is not not updated after the first format. 
 * Adding a CRC to this block can help verify integrity of the
 * super block. 
 *
 * magic: Identifies that this is a LkvsDevice
 * devSize: Number of logical blocks on the device
 */
typedef struct
{
	uint32_t magic;
	uint64_t devsize;
}SuperBlock;

/** 
 * On disk and in-memory metadata
 *
 * This metadata is placed on disk after each put operation and is also
 * stored in memory to prevent duplicate updates. There is a 4k MD write
 * after each put operation. The first MD write contains only one MD entry
 * and each subsequent MD write contains one more entry until we fill up 
 * a 4k buffer. When the buffer is filled the buffer is reset and this process
 * is repeated. This code is assuming that the backing device has a block size 
 * of 4K. 
 *
 * magic: Identifies this block as being a MD block, can be placed only at the
 *        start of the 4k block as an optimization.
 * key0-3: Sha256 value of the key
 * size: Size of the value stored
 * location: block address where the value is stored
 * mddump: block location of previous full 4K MD write
           speeds up start up.
 */
typedef struct
{
	uint32_t magic;
	uint64_t key0;
	uint64_t key1;
	uint64_t key2;
	uint64_t key3;
	uint64_t size;
	uint64_t location;
	uint64_t mddump;
}MetaData;

/** 
 * Lkvs Per Zone In-Memory MD
 *
 * lastMDump: block location of the last full 4K MD dump in this zone
 * mdEntries: the number of MD entries in the MD buffer
 * mdBuf:     MD buffer for the zone
 */
typedef struct{
	unsigned long long lastMDump;
	unsigned int mdEntries;
	char *mdBuf;
}LkvsZone;

// Forward declarations 
class KeyContainer;

/**
 * @defgroup LKVS_DEV LKVS Device 
 *
 * @{
 */

#define LKVS_SUCCESS 0
#define LKVS_FAILURE 1

/// Size, in bytes, of serialized Metadata entry
#define MD_PB_SZ sizeof(MetaData)
/// Size, in bytes, of serialized Super Block
#define SB_PB_SZ sizeof(SuperBlock)

/// File and Memory Alignment in Bytes
#define ALIGNMENT 4096
/// Fing max_hw_sector
#define MAX_IO_REQ 131072
/// Number of MD Entries Per 4k BLOCK
#define MD_ENTRIES_PER_BLOCK (ALIGNMENT/MD_PB_SZ)

#define LKVS_MAGIC \
((((int)'L') << 24) | (((int)'K') << 16) | (((int)'V') << 8) | ((int)'S'))

#define LKVS_META_MAGIC \
((((int)'M') << 24) | (((int)'E') << 16) | (((int)'T') << 8) | ((int)'A'))


#define LKVS_FLAG_FORMAT 0x1

/** @} */

/** LkvsDev
 * 
 * In-memory representation of a running Linear Key/Value Store
 *
 * Currently there is only one instance of a LkvsDev per backing store.
 * This class has no explicit multi-threading for request handling. 4K
 * puts are slow because we don't buffer anything. 
 */
class LkvsDev{

	public:
		/// Constructor
		LkvsDev();
		
		/// Destructor
		~LkvsDev();
		int openDev(const char *dev, int flags);
		/// Put handler.
		int Put(const char *key, void *buf, size_t size);
		/// Get Handler
		int Get(const char *key, void *buf, size_t size);
	private:
		std::string targetDev;
		struct zbc_device *zDev;
		zbc_zone_t *zDevZones;
		std::map<KeyContainer, MetaData> md;
		std::vector<LkvsZone> zones;
		unsigned long long devSize;
		unsigned int numZones, lastZoneAlloc, lastReadZone; 
		unsigned int zDevBlockSize;
		unsigned int zDevNumZones, cZones;
		char *aligned4kBuf;
		LkvsZone *zoneMeta;

		/** Read the metadata at the start of the zone to determine if 
		  * LKVS dev has been run on the target device previously. 
		  */
		int checkDev(void);
	    /** Format the target device with the Lkvs SB in order to 
		 *  determine that an LKVS store is present on the target device
		 */
		int formatDev(void);
		/** If the write pointer is past the end of the Lkvs SB this function
		 *  is invoked to read in the MD of all writes that occured before 
		 *  the write pointer. I need to add MD dumps. 
		 */
		int populateMeta(int zoneIndex);
		// Find a zone for a put of a given size
		int searchForZone(size_t size);
		// Give the zoneIndex determine is zone has required capacity
		int reserve(int zoneIndex, size_t size);
		unsigned int blockToZone(uint64_t blockNum);
		unsigned long long getTime(void);
};


/** KeyContainer
 *
 *  Key Container holds 256 bit hashed key values
 *
 *  This class holds a 256 bit key, which is a hash of the 
 *  4K key that is supplied by the user using liblkvs. I am 
 *  not currently saving ths 4K key on the disk, and want to 
 *  leave this as an option to the user, who may wish to save 
 *  the user supplied key external to the ZBC drive. This class
 *  is used to map keys to metadata containers in a stl map
 *  that is held by the LkvsDev class
 */
class KeyContainer{
	public:
		/// KeyContainer Constructor
		KeyContainer() {};
		/// KeyContainer Destructor
		~KeyContainer() {};
		/// Initialize key container from given MD entry
		void setFromMeta(MetaData *);
		/// Copy key within keyContainer into given MD entry
		void metaKeySet(MetaData *);
		/// Set the KeyContainer using supplied string
		void setFromChar(const char *in);
		/// Comparator for the stl::map
		bool operator<(const KeyContainer &other) const;
	private:
		/// Array of 4 64 bit values that represent the SHA 256 of the key
		unsigned long long key[4];
};

