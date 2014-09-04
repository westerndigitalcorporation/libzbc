/*
 * This file is part of libzbc.
 * 
 * Copyright (C) 2009-2014, HGST, Inc.  This software is distributed
 * under the terms of the GNU Lesser General Public License version 3,
 * or any later version, "as is," without technical support, and WITHOUT
 * ANY WARRANTY, without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  You should have received a copy
 * of the GNU Lesser General Public License along with libzbc.  If not,
 * see <http://www.gnu.org/licenses/>.
 * 
 * Authors: Damien Le Moal (damien.lemoal@hgst.com)
 *          Christophe Louargant (christophe.louargant@hgst.com)
 */

/***** Including files *****/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <libzbc/zbc.h>

/***** Main *****/

int main(int argc,
         char **argv)
{
    struct zbc_device_info info;
    struct zbc_device *dev;
    unsigned long long cap;
    int i, ret = 1;
    uint8_t *buf;
    char *path;

    /* Check command line */
    if ( argc < 2 ) {
usage:
        printf("Usage: %s [options] <dev>\n"
               "Options:\n"
               "    -v : Verbose mode\n",
               argv[0]);
        return( 1 );
    }

    /* Parse options */
    for(i = 1; i < (argc - 1); i++) {

        if ( strcmp(argv[i], "-v") == 0 ) {

            zbc_set_log_level("debug");

        } else if ( argv[i][0] == '-' ) {
            
            printf("Unknown option \"%s\"\n",
                   argv[i]);
            goto usage;

        } else {

            break;

        }

    }

    /* Open device */
    path = argv[i];
    ret = zbc_open(path, O_RDONLY, &dev);
    if ( ret < 0 ) {
        fprintf(stderr,
                "zbc_open failed\n");
        return( 1 );
    }

    /* Get device info */
    ret = zbc_get_device_info(dev, &info);
    if ( ret < 0 ) {
        fprintf(stderr,
                "zbc_get_device_info failed\n");
        return( 1 );
    }

    /* Run INQUIRY */
    ret = zbc_inquiry(dev, &buf);
    if ( ret != 0 ) {
        fprintf(stderr,
                "zbc_inquiry failed\n");
        ret = 1;
        goto out;
    }

    /* Output buffer format:
     * +=============================================================================+
     * |  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
     * |Byte |        |        |        |        |        |        |        |        |
     * |=====+==========================+============================================|
     * | 0   | Peripheral Qualifier     |           Peripheral Device Type           |
     * |-----+-----------------------------------------------------------------------|
     * | 1   |  RMB   |                  Device-Type Modifier                        |
     * |-----+-----------------------------------------------------------------------|
     * | 2   |   ISO Version   |       ECMA Version       |  ANSI-Approved Version   |
     * |-----+-----------------+-----------------------------------------------------|
     * | 3   |  AENC  | TrmIOP |     Reserved    |         Response Data Format      |
     * |-----+-----------------------------------------------------------------------|
     * | 4   |                           Additional Length (n-4)                     |
     * |-----+-----------------------------------------------------------------------|
     * | 5   |                           Reserved                                    |
     * |-----+-----------------------------------------------------------------------|
     * | 6   |                           Reserved                                    |
     * |-----+-----------------------------------------------------------------------|
     * | 7   | RelAdr | WBus32 | WBus16 |  Sync  | Linked |Reserved| CmdQue | SftRe  |
     * |-----+-----------------------------------------------------------------------|
     * | 8   | (MSB)                                                                 |
     * |- - -+---                        Vendor Identification                    ---|
     * | 15  |                                                                 (LSB) |
     * |-----+-----------------------------------------------------------------------|
     * | 16  | (MSB)                                                                 |
     * |- - -+---                        Product Identification                   ---|
     * | 31  |                                                                 (LSB) |
     * |-----+-----------------------------------------------------------------------|
     * | 32  | (MSB)                                                                 |
     * |- - -+---                        Product Revision Level                   ---|
     * | 35  |                                                                 (LSB) |
     * |-----+-----------------------------------------------------------------------|
     * | 36  |                                                                       |
     * |- - -+---                        Vendor Specific                          ---|
     * | 55  |                                                                       |
     * |-----+-----------------------------------------------------------------------|
     * | 56  |                                                                       |
     * |- - -+---                        Reserved                                 ---|
     * | 95  |                                                                       |
     * |=====+=======================================================================|
     * |     |                       Vendor-Specific Parameters                      |
     * |=====+=======================================================================|
     * | 96  |                                                                       |
     * |- - -+---                        Vendor Specific                          ---|
     * | n   |                                                                       |
     * +=============================================================================+
     */

    /* Let's display the Vendor ID, the Product ID and Revision Level*/
    printf("Device %s:\n",
           path);
    printf("    Vendor ID: %.8s\n", (char *) (buf + 8));
    printf("    Product ID: %.16s\n", (char *) (buf + 16));
    printf("    Product Revision Level: %.4s\n", (char *) (buf + 32));
    printf("    Device type: %xh\n", (int)(buf[0] & 0x1f));

    printf("    Interface: %s\n"
           "    Model:     %s\n",
           zbc_disk_type_str(info.zbd_type),
           zbc_disk_model_str(info.zbd_model));

    cap = info.zbd_physical_block_size * info.zbd_physical_blocks;
    printf("Capacity: %llu.%03llu GB\n",
           cap / 1000000000,
           (cap / 1000000) % 1000);
    printf("    Logical blocks: %llu blocks of %u B\n",
           (unsigned long long) info.zbd_logical_blocks,
           (unsigned int) info.zbd_logical_block_size);
    printf("    Physical blocks: %llu blocks of %u B\n",
           (unsigned long long) info.zbd_physical_blocks,
           (unsigned int) info.zbd_physical_block_size);

    free(buf);
    
out:

    /* Close device file */
    zbc_close(dev);

    return( ret );

}

