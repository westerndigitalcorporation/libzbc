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

#ifndef LIBLKVS_H_
#define LIBLKVS_H_

#include <cstdlib>


extern "C" {
	
	typedef void *lkvsdev_t;
	int lkvsdev_create(lkvsdev_t *lkvsdevice);
	int lkvsdev_open(lkvsdev_t lksvsdevice, const char *devFile, int flag);
	int lkvsdev_put(lkvsdev_t lkvsdevice, const char *key, void *buf, 
	                size_t size);
	int lkvsdev_get(lkvsdev_t lkvsdevice, const char *key, void *buf, 
	                size_t size);
	void lkvsdev_destroy(lkvsdev_t lkvsdevice);

}

#endif
