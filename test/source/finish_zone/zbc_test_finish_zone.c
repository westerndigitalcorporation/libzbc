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
#include <string.h>
#include <errno.h>

#include <libzbc/zbc.h>

/***** Main *****/

int main(int argc,
         char **argv)
{
    long long z;
    struct zbc_device *dev;
    int i, ret = 1;
    zbc_zone_t *zones = NULL;
    char *path;

    /* Check command line */
    if ( argc < 2 ) {
usage:
        printf("Usage: %s [option] <dev> <lba>\n"
               "   lba -1 is to set all bit flag\n"
               "Options:\n"
               "    -v   : Verbose mode\n",
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

    /* Open device */
    path = argv[i];
    z = atoll(argv[i+1]);

    ret = zbc_open(path, O_RDONLY, &dev);
    if ( ret != 0 ) {
        fprintf(stderr,
                "[TEST][ERROR],can't open device\n");
        return( 1 );
    }

    /* Finish zone */
    ret = zbc_finish_zone(dev, (uint64_t)z);
    if ( ret != 0 ) {
        fprintf(stderr,
                "[TEST][ERROR],zbc_test_finish_zone failed\n");

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

    if ( zones ) {
        free(zones);
    }

    /* Close device file */
    zbc_close(dev);

    return( ret );

}

