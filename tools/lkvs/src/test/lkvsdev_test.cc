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

#include "gtest/gtest.h"
#include <cstdlib>
#include <sstream>

#include <string.h>
#include <malloc.h>

#include "lkvs.hpp"

#define BUFSZ 1048576
#define PUTS 1000 // Reset formats a 1GB zone

// LKVSDEV Test fixture
class LkvsDevTest : public ::testing::Test {
	protected:
		virtual void SetUp() {
			int ret;
			// check for the enviroment variable
			
			// Reset the ZBC drive
			ret = system("./zbc_reset.sh");

			// Allocate 4K aligned buffers
			putBuf = (char *)memalign(ALIGNMENT, BUFSZ);
			getBuf = (char *)memalign(ALIGNMENT, BUFSZ);
	
			// Grab the DevFile from the enviroment
			devPath = std::getenv("LKVSDEVFILE");
			ASSERT_TRUE(putBuf);
			ASSERT_TRUE(getBuf);
			ASSERT_TRUE(devPath);
		}

		virtual void TearDown() {
			// Free buffers and reset the drive
			int ret;
			free( putBuf);
			free(getBuf);
			ret = system("./zbc_reset.sh");
		}

		LkvsDev* tester;
		char *putBuf;
		char *getBuf;
		char *devPath;
};

// Open tests
TEST_F(LkvsDevTest, Open){
	
	tester = new LkvsDev();
	// Test that a non existant device fails
	EXPECT_EQ(LKVS_FAILURE, tester->openDev(devPath, 0));
	// Non formatted ZBC drive fails
	EXPECT_EQ(LKVS_FAILURE, tester->openDev(devPath, 0));
	delete tester;

	tester = new LkvsDev();
	// Format of ZBC drive succeeds 
	EXPECT_EQ(LKVS_SUCCESS, tester->openDev(devPath, LKVS_FLAG_FORMAT));
	delete tester;

	tester = new LkvsDev();
	// Test that open succeeds after format
	EXPECT_EQ(LKVS_SUCCESS, tester->openDev(devPath, 0));
	delete tester;


	tester = new LkvsDev();
	// Test that open with format succeeds after format
	EXPECT_EQ(LKVS_SUCCESS, tester->openDev(devPath, 0));
	delete tester;
}

// Put Tests
TEST_F(LkvsDevTest, Put){
	
	memset(putBuf, 'A', BUFSZ);
	tester = new LkvsDev();
	// Put of non open device fails
	EXPECT_EQ(LKVS_FAILURE, tester->Put("test", putBuf, BUFSZ));
	// Format the device
	EXPECT_EQ(LKVS_SUCCESS, tester->openDev(devPath, LKVS_FLAG_FORMAT));
	// Put of size zero fails
	EXPECT_EQ(LKVS_FAILURE, tester->Put("test", putBuf, 0));
	// Insert test key
	EXPECT_EQ(LKVS_SUCCESS, tester->Put("test", putBuf, BUFSZ));
	// Reinsert test key should fail
	EXPECT_EQ(LKVS_FAILURE, tester->Put("test", putBuf, BUFSZ));
	// Get the test key
	EXPECT_EQ(LKVS_SUCCESS, tester->Get("test", getBuf, BUFSZ));
	// Make sure what we put matches what we got
	EXPECT_EQ(0, strncmp(putBuf, getBuf, BUFSZ));
	memset(putBuf, 'B', BUFSZ);
	std::cerr << " Unaligned Put " << std::endl;
	// Non aligned buffer and size not a multiple of 4K
	EXPECT_EQ(LKVS_SUCCESS, tester->Put("test1", putBuf + 1, BUFSZ - 1 ));
	// Get it back 
	std::cerr << " Unaligned Get " << std::endl;
	EXPECT_EQ(LKVS_SUCCESS, tester->Get("test1", getBuf, BUFSZ -1));
	// Compare
	EXPECT_EQ(0, strncmp(putBuf, getBuf, BUFSZ - 1 ));
	delete tester;
}

// Get Tests
TEST_F(LkvsDevTest, Get){

	memset(putBuf, 'B', BUFSZ);

	tester = new LkvsDev();
	// Get of non open device fails
	EXPECT_EQ(LKVS_FAILURE, tester->Get("test", getBuf, BUFSZ));
	// Format the device
	EXPECT_EQ(LKVS_SUCCESS, tester->openDev(devPath, LKVS_FLAG_FORMAT));
	// Get of non existent key fails
	EXPECT_EQ(LKVS_FAILURE, tester->Get("test", getBuf, BUFSZ));
	// Put Succeeds 
	EXPECT_EQ(LKVS_SUCCESS, tester->Put("test", putBuf, BUFSZ));
	// Get size != Put Size Fails
	EXPECT_EQ(LKVS_FAILURE, tester->Get("test", getBuf, BUFSZ * 2 ));
	// Get succeeds 
	EXPECT_EQ(LKVS_SUCCESS, tester->Get("test", getBuf, BUFSZ));
	// Data is correct
	EXPECT_EQ(0, strncmp(putBuf, getBuf, BUFSZ));
	// Clean up
	delete tester;
}

// Test puts that span multiple zones
TEST_F(LkvsDevTest, MultiZone) {
	
	std::ostringstream converter;
	int i, puts = 1000;
	
	
	tester = new LkvsDev();
	
	// Open and Format The Device
	EXPECT_EQ( LKVS_SUCCESS, tester->openDev(devPath, LKVS_FLAG_FORMAT));

	// Fill up one zone and write to second zone
	for( i = 0; i < PUTS; i++){

		converter << i;
		memset(putBuf, (int)i, BUFSZ);
		EXPECT_EQ( LKVS_SUCCESS, tester->Put(converter.str().c_str(), 
		           putBuf, BUFSZ) );
		converter.str(std::string());
	}
	delete tester;
	
	// Reopen the device and add some new values
	tester = new LkvsDev();
	EXPECT_EQ( LKVS_SUCCESS, tester->openDev(devPath, 0));
	memset(putBuf,'A', BUFSZ);
	EXPECT_EQ( LKVS_SUCCESS, tester->Put("testA", putBuf, BUFSZ));
	memset(putBuf,'B', BUFSZ);
	EXPECT_EQ( LKVS_SUCCESS, tester->Put("testB", putBuf, BUFSZ));
	memset(putBuf,'C', BUFSZ);
	EXPECT_EQ( LKVS_SUCCESS, tester->Put("testC", putBuf, BUFSZ));
	delete tester;
	
	// Open one more time and check everything
	tester = new LkvsDev();	
	EXPECT_EQ( LKVS_SUCCESS, tester->openDev(devPath, 0));
	for( i = 0; i < PUTS; i++){
		converter << i;
		memset(putBuf, (int)i, BUFSZ);
		EXPECT_EQ( LKVS_SUCCESS, tester->Get(converter.str().c_str(), 
		           getBuf, BUFSZ) );
		EXPECT_EQ(0, strncmp(putBuf, getBuf, BUFSZ));
		converter.str(std::string());
	}
	
	memset(putBuf,'A', BUFSZ);
	EXPECT_EQ( LKVS_SUCCESS, tester->Get("testA", getBuf, BUFSZ));
	EXPECT_EQ(0, strncmp(putBuf, getBuf, BUFSZ));
	memset(putBuf,'B', BUFSZ);
	EXPECT_EQ( LKVS_SUCCESS, tester->Get("testB", getBuf, BUFSZ));
	EXPECT_EQ(0, strncmp(putBuf, getBuf, BUFSZ));
	memset(putBuf,'C', BUFSZ);
	EXPECT_EQ( LKVS_SUCCESS, tester->Get("testC", getBuf, BUFSZ));
	EXPECT_EQ(0, strncmp(putBuf, getBuf, BUFSZ));

	delete tester;
}
