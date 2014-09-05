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

import com.sun.jna.Library;
import com.sun.jna.Native;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.PointerByReference;

public interface Lkvs extends Library {
	Lkvs INSTANCE = (Lkvs) Native.loadLibrary("lkvs", Lkvs.class);
	
	int lkvsdev_create(PointerByReference lkvsdevice);
	int lkvsdev_open(Pointer lkvsdevice, 
	                 String devFile, int flag);
	int lkvsdev_put(Pointer lkvsdevice, 
	                 String key, Pointer buf, int size);
	int lkvsdev_get(Pointer lkvsdevice, 
	                 String key, Pointer buf, int size);
	void lkvsdev_destroy(Pointer lkvsdevice);
}
