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

import com.sun.jna.Pointer;
import com.sun.jna.ptr.PointerByReference;
import com.sun.jna.Memory;

public class Tester {

	public static void main(String[] args){
		/* Make sure the device argument is passed */
		if( args.length == 0){
			System.out.println("Please pass device argument");
			return;
		}

		PointerByReference lkvsdevPtr = new PointerByReference();
		Lkvs.INSTANCE.lkvsdev_create(lkvsdevPtr);
		Pointer lkvsdev = lkvsdevPtr.getValue();
		Lkvs.INSTANCE.lkvsdev_open(lkvsdev, args[0], 0x1);
		String myString = "HelloWorld";
		int devStringSize = myString.length() + 1;
		Pointer putBuf = new Memory(devStringSize);
		Pointer getBuf = new Memory(devStringSize);
		putBuf.setString(0, myString);
		Lkvs.INSTANCE.lkvsdev_put(lkvsdev, "Test",putBuf,devStringSize);
		Lkvs.INSTANCE.lkvsdev_get(lkvsdev, "Test",getBuf,devStringSize);
		String response = getBuf.getString(0);
		System.out.println(response);
		Lkvs.INSTANCE.lkvsdev_destroy(lkvsdev);
	}
}

