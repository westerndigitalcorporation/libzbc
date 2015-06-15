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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

#include <libzbc/zbc.h>

/***** Main *****/

int main(int argc,
         char **argv)
{
    struct zbc_device_info info;
    struct zbc_device *dev = NULL;
    int i, ret = 1;
    size_t iosize;
    void *iobuf = NULL;
    uint32_t lba_count;
    struct zbc_zone *zones = NULL;
    struct zbc_zone *iozone = NULL;
    unsigned int nr_zones;
    char *path;
    long long lba = 0;

    /* Check command line */
    if ( argc < 3 ) {
usage:
        printf("Usage: %s [options] <dev> <lba>\n"
               "  Read a zone up to the current write pointer\n"
               "  or the number of I/O specified is executed\n"
               "Options:\n"
               "    -v         : Verbose mode\n"
               "    -lba       : lba offset from the starting lba of the zone <zone no>.\n",
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

    if ( i != (argc - 2) ) {
        goto usage;
    }

    /* Get parameters */
    path = argv[i];
    lba = atoll(argv[i+1]);

    /* Open device */
    ret = zbc_open(path, O_RDONLY|ZBC_FORCED_ATA_RW, &dev);
    if ( ret != 0 ) {
        fprintf(stderr,
                "[TEST][ERROR],can't open device\n");
        return( 1 );
    }

    ret = zbc_get_device_info(dev, &info);
    if ( ret < 0 ) {
        fprintf(stderr,
                "[TEST][ERROR],zbc_get_device_info failed\n");
        goto out;
    }

    /* Get zone list */
    ret = zbc_list_zones(dev, 0, ZBC_RO_ALL, &zones, &nr_zones);
    if ( ret != 0 ) {
        fprintf(stderr,
                "[TEST][ERROR],zbc_list_zones failed\n");
        ret = 1;
        goto out;
    }

    /* Search target zone */
    for ( i = 0; i < nr_zones; i++ ) {
        if ( lba < (zones[i].zbz_start + zones[i].zbz_length) ) {
            iozone = &zones[i];
            break;
        }
    }

    if ( iozone == NULL ) {
        fprintf(stderr,
                "[TEST][ERROR],Target zone not found\n");
        ret = 1;
        goto out;
    }

    iosize = 2 * info.zbd_logical_block_size;
    ret = posix_memalign((void **) &iobuf, info.zbd_logical_block_size, iosize);
    if ( ret != 0 ) {
        fprintf(stderr,
                "[TEST][ERROR],No memory for I/O buffer (%zu B)\n",
                iosize);
        ret = 1;
        goto out;
    }

    lba_count = iosize / info.zbd_logical_block_size;

    ret = zbc_pread(dev, iozone, iobuf, lba_count, lba - iozone->zbz_start);
    if ( ret <= 0 ) {
        fprintf(stderr,
                "[TEST][ERROR],zbc_read_zone failed\n");

        {
            zbc_errno_t zbc_err;
            const char *sk_name;
            const char *ascq_name;

            zbc_errno(dev, &zbc_err);
            sk_name = zbc_sk_str(zbc_err.sk);
            ascq_name = zbc_asc_ascq_str(zbc_err.asc_ascq);

            printf("[TEST][ERROR][SENSE_KEY],%s\n", sk_name);
            printf("[TEST][ERROR][ASC_ASCQ],%s\n", ascq_name);
        }

        ret = 1;
    }

out:

    if ( iobuf ) {
        free(iobuf);
    }


    zbc_close(dev);

    return( ret );

}

