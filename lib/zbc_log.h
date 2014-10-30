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
 * Author: Damien Le Moal (damien.lemoal@hgst.com)
 */

#ifndef __ZBC_LOG_H__
#define __ZBC_LOG_H__

/***** Including Files *****/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

/***** Macro definitions *****/

/**
 * Log levels.
 */
enum {
    ZBC_LOG_NONE = 0,
    ZBC_LOG_ERROR,
    ZBC_LOG_INFO,
    ZBC_LOG_DEBUG,
    ZBC_LOG_VDEBUG,
    ZBC_LOG_MAX
};

/**
 * Library log level.
 */
extern int zbc_log_level;

/***** Macro functions *****/

#define zbc_print(stream,format,args...)        \
    do {                                        \
        fprintf((stream), format, ## args);     \
        fflush(stream);                         \
    } while(0)

/**
 * Log level controlled messages.
 */
#define zbc_print_level(l,stream,format,args...)        \
    do {                                                \
        if ( (l) <= zbc_log_level ) {                   \
            zbc_print((stream), "(libzbc) " format,     \
                      ## args);                         \
        }                                               \
    } while( 0 )

#define zbc_info(format,args...)                \
    zbc_print_level(ZBC_LOG_INFO,               \
                    stdout,                     \
                    format,                     \
                    ##args)

#define zbc_error(format,args...)               \
    zbc_print_level(ZBC_LOG_ERROR,              \
                    stderr,                     \
                    "[ERROR] " format,          \
                    ##args)

#define zbc_debug(format,args...)               \
    zbc_print_level(ZBC_LOG_DEBUG,              \
                    stdout,                     \
                    format,                     \
                    ##args)

#define zbc_vdebug(format,args...)              \
    zbc_print_level(ZBC_LOG_VDEBUG,             \
                    stdout,                     \
                    format,                     \
                    ##args)

#define zbc_panic(format,args...)               \
    do {                                        \
        zbc_print_level(ZBC_LOG_ERROR,          \
                        stderr,                 \
                        "[PANIC] " format,      \
                        ##args);                \
        assert(0);                              \
    } while( 0 )

#define zbc_assert(cond)                        \
    do {                                        \
        if ( ! (cond) ) {                       \
            zbc_panic("Condition %s failed\n",  \
                       # cond);                 \
        }                                       \
    } while( 0 )

#endif /* __ZBC_LOG_H__ */
