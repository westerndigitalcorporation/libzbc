// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
 * Copyright (C) 2016, Western Digital. All rights reserved.
 * Copyright (C) 2020 Western Digital Corporation or its affiliates.
 *
 * Author: Damien Le Moal (damien.lemoal@wdc.com)
 *         Christophe Louargant (christophe.louargant@wdc.com)
 */
#define _GNU_SOURCE     /* O_LARGEFILE & O_DIRECT */

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
#include <libgen.h>

#include <libzbc/zbc.h>

static int zbc_write_zone_abort = 0;

static inline unsigned long long zbc_write_zone_usec(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return (unsigned long long) tv.tv_sec * 1000000LL +
		(unsigned long long) tv.tv_usec;
}

static int zbc_write_zone_usage(FILE *out, char *prog)
{
	fprintf(out,
		"Usage: %s [options] <dev> <zone no> <I/O size (B)>\n"
		"  Write to a zone from the zone write pointer, until\n"
		"  the zone is full or until the specified number of I/Os\n"
		"  are all executed.\n"
		"Options:\n"
		"  -h | --help  : Display this help message and exit\n"
		"  -v           : Verbose mode\n"
		"  -scsi        : Force the use of SCSI passthrough commands\n"
		"  -ata         : Force the use of ATA passthrough commands\n"
		"  -s           : Run zbc_flush after writing (equivalent to\n"
		"                 executing sync())\n"
		"  -dio         : Use direct I/Os\n"
		"  -vio <num>   : Use vectored I/Os with <num> buffers of\n"
		"                 <I/O size> bytes, resulting in an actual I/O\n"
		"                 size of <num> x <I/O size> bytes.\n"
		"  -p <num>     : Set the byte pattern to write. If this option\n"
		"                 is omitted, zeroes are written.\n"
		"  -nio <num>   : Limit the number of I/O executed to <num>\n"
		"  -f <file>    : Write the content of <file>\n"
		"  -loop        : If a file is specified, repeatedly write the\n"
		"                 file content to the zone until the zone is full\n"
		"  -ofst <ofst> : Write the zone starting form the sector offset\n"
		"                 <ofst> instead of from the zone start sector.\n"
		"                 This option should be used only with\n"
		"                 conventional zones.\n",
		basename(prog));
	return 1;
}

static void zbc_write_zone_sigcatcher(int sig)
{
	zbc_write_zone_abort = 1;
}

int main(int argc, char **argv)
{
	struct zbc_device_info info;
	struct zbc_device *dev = NULL;
	unsigned long long elapsed;
	unsigned long long bcount = 0;
	unsigned long long fsize, brate;
	struct stat st;
	int zidx, fd = -1, i;
	ssize_t ret = 1;
	size_t bufsize, iosize, ioalign;
	void *iobuf = NULL;
	ssize_t sector_count = 0;
	unsigned long long iocount = 0, ionum = 0;
	struct zbc_zone *zones = NULL;
	struct zbc_zone *iozone = NULL;
	unsigned int nr_zones;
	char *path, *file = NULL, *end;
	long long sector_ofst = 0;
	long long sector_max = 0;
	long long zone_ofst = 0;
	bool flush = false, floop = false, vio = false;
	unsigned long pattern = 0;
	int flags = O_WRONLY;
	int oflags = 0;
	struct iovec *iov = NULL;
	int iovcnt = 1, n;

	/* Check command line */
	if (argc < 4)
		return zbc_write_zone_usage(stderr, argv[0]);

	/* Parse options */
	for (i = 1; i < (argc - 1); i++) {

		if (strcmp(argv[i], "-h") == 0 ||
		    strcmp(argv[i], "--help") == 0)
			return zbc_write_zone_usage(stdout, argv[0]);

		if (strcmp(argv[i], "-v") == 0) {

			zbc_set_log_level("debug");

		} else if (strcmp(argv[i], "-scsi") == 0) {

			oflags = ZBC_O_DRV_SCSI;

		} else if (strcmp(argv[i], "-ata") == 0) {

			oflags = ZBC_O_DRV_ATA;

		} else if (strcmp(argv[i], "-dio") == 0) {

			flags |= O_DIRECT;

		} else if (strcmp(argv[i], "-s") == 0) {

			flush = true;

		} else if (strcmp(argv[i], "-p") == 0) {

			if (i >= (argc - 1))
				goto err;
			i++;

			pattern = strtol(argv[i], &end, 0);
			if (*end != '\0' || errno != 0) {
				fprintf(stderr,
					"Invalid data pattern value \"%s\"\n",
					argv[i]);
				return 1;
			}
			if (pattern > 0xff) {
				fprintf(stderr,
					"Not a single-byte pattern:\"%s\"\n",
					argv[i]);
				return 1;
			}

		} else if (strcmp(argv[i], "-vio") == 0) {

			if (i >= (argc - 1))
				goto err;
			i++;

			iovcnt = atoi(argv[i]);
			if (iovcnt <= 0) {
				fprintf(stderr,
					"Invalid number of IO buffers\n");
				return 1;
			}
			vio = true;

		} else if (strcmp(argv[i], "-nio") == 0) {

			if (i >= (argc - 1))
				goto err;
			i++;

			ionum = atoi(argv[i]);
			if (ionum <= 0) {
				fprintf(stderr, "Invalid number of I/Os\n");
				return 1;
			}

		} else if (strcmp(argv[i], "-f") == 0) {

			if (i >= (argc - 1))
				goto err;
			i++;

			file = argv[i];

		} else if (strcmp(argv[i], "-loop") == 0) {

			floop = true;

		} else if (strcmp(argv[i], "-ofst") == 0) {

			if (i >= (argc - 1))
				goto err;
			i++;

			zone_ofst = atoll(argv[i]);
			if (zone_ofst < 0) {
				fprintf(stderr, "Invalid zone sector offset\n");
				return 1;
			}

		} else if (argv[i][0] == '-') {

			fprintf(stderr, "Unknown option \"%s\"\n", argv[i]);
			return 1;

		} else {

			break;

		}

	}

	if (i != (argc - 3))
		goto err;

	/* Get parameters */
	path = argv[i];

	if (oflags & ZBC_O_DRV_SCSI && oflags & ZBC_O_DRV_ATA) {
		fprintf(stderr,
			"-scsi and -ata options are mutually exclusive\n");
		return 1;
	}

	zidx = atoi(argv[i + 1]);
	if (zidx < 0) {
		fprintf(stderr, "Invalid zone number %s\n",
			argv[i + 1]);
		return 1;
	}

	bufsize = atol(argv[i + 2]);
	if (!bufsize) {
		fprintf(stderr, "Invalid I/O size %s\n", argv[i + 2]);
		return 1;
	}

	/* Setup signal handler */
	signal(SIGQUIT, zbc_write_zone_sigcatcher);
	signal(SIGINT, zbc_write_zone_sigcatcher);
	signal(SIGTERM, zbc_write_zone_sigcatcher);

	/* Open device */
	ret = zbc_open(path, oflags | flags, &dev);
	if (ret != 0) {
		if (ret == -ENODEV)
			fprintf(stderr,
				"Open %s failed (not a zoned block device)\n",
				path);
		else
			fprintf(stderr, "Open %s failed (%s)\n",
				path, strerror(-ret));
		return 1;
	}

	zbc_get_device_info(dev, &info);

	printf("Device %s:\n", path);
	zbc_print_device_info(&info, stdout);

	/* Get zone list */
	ret = zbc_list_zones(dev, 0, ZBC_RO_ALL, &zones, &nr_zones);
	if (ret != 0) {
		fprintf(stderr, "zbc_list_zones failed\n");
		ret = 1;
		goto out;
	}

	/* Get target zone */
	if (zidx >= (int)nr_zones) {
		fprintf(stderr, "Target zone not found\n");
		ret = 1;
		goto out;
	}
	iozone = &zones[zidx];

	if (zbc_zone_conventional(iozone))
		printf("Target zone: Conventional zone %d / %d, "
		       "sector %llu, %llu sectors\n",
		       zidx,
		       nr_zones,
		       zbc_zone_start(iozone),
		       zbc_zone_length(iozone));
	else
		printf("Target zone: Zone %d / %d, type 0x%x (%s), "
		       "cond 0x%x (%s), rwp %d, non_seq %d, "
		       "sector %llu, %llu sectors, wp %llu\n",
		       zidx,
		       nr_zones,
		       zbc_zone_type(iozone),
		       zbc_zone_type_str(zbc_zone_type(iozone)),
		       zbc_zone_condition(iozone),
		       zbc_zone_condition_str(zbc_zone_condition(iozone)),
		       zbc_zone_rwp_recommended(iozone),
		       zbc_zone_non_seq(iozone),
		       zbc_zone_start(iozone),
		       zbc_zone_length(iozone),
		       zbc_zone_wp(iozone));

	/* Check I/O alignment and get an I/O buffer */
	if (zbc_zone_sequential(iozone))
		ioalign = info.zbd_pblock_size;
	else
		ioalign = info.zbd_lblock_size;
	if (bufsize % ioalign) {
		fprintf(stderr,
			"Invalid I/O size %zu (must be aligned on %zu)\n",
			bufsize, ioalign);
		ret = 1;
		goto out;
	}

	if (vio) {
		iov = calloc(iovcnt, sizeof(struct iovec));
		if (!iov) {
			fprintf(stderr, "No memory for I/O vector\n");
			ret = -ENOMEM;
			goto out;
		}
	}

	iosize = bufsize * iovcnt;
	ret = posix_memalign((void **) &iobuf, sysconf(_SC_PAGESIZE), iosize);
	if (ret != 0) {
		fprintf(stderr, "No memory for I/O buffer (%zu B)\n", iosize);
		ret = 1;
		goto out;
	}

	memset(iobuf, pattern, iosize);

	/* Open the file to read, if any */
	if (file) {

		fd = open(file, O_LARGEFILE | O_RDONLY);
		if (fd < 0) {
			fprintf(stderr, "Open file \"%s\" failed %d (%s)\n",
				file,
				errno, strerror(errno));
			ret = 1;
			goto out;
		}

		ret = fstat(fd, &st);
		if (ret != 0) {
			fprintf(stderr, "Stat file \"%s\" failed %d (%s)\n",
				file,
				errno, strerror(errno));
			ret = 1;
			goto out;
		}

		if (S_ISREG(st.st_mode)) {
			fsize = st.st_size;
		} else if (S_ISBLK(st.st_mode)) {
			ret = ioctl(fd, BLKGETSIZE64, &fsize);
			if (ret != 0) {
				fprintf(stderr,
					"ioctl BLKGETSIZE64 block device \"%s\" failed %d (%s)\n",
					file,
					errno, strerror(errno));
				ret = 1;
				goto out;
			}
		} else {
			fprintf(stderr, "Unsupported file \"%s\" type\n", file);
			ret = 1;
			goto out;
		}

		printf("Writing file \"%s\" (%llu B) to target zone %d, %zu B I/Os\n",
		       file, fsize, zidx, iosize);

	} else if (!ionum) {

		printf("Filling target zone %d, %zu B I/Os\n",
		       zidx, iosize);

	} else {

		printf("Writing to target zone %d, %llu I/Os of %zu B\n",
		       zidx, ionum, iosize);

	}

	sector_max = zbc_zone_length(iozone);
	if (zbc_zone_sequential_req(iozone)) {
		if (zbc_zone_full(iozone))
			sector_max = 0;
		else if (zbc_zone_wp(iozone) > zbc_zone_start(iozone))
			sector_max =
				zbc_zone_wp(iozone) - zbc_zone_start(iozone);
	}

	elapsed = zbc_write_zone_usec();

	while (!zbc_write_zone_abort) {

		if (file) {

			size_t ios;

			/* Read file */
			ret = read(fd, iobuf, iosize);
			if (ret < 0) {
				fprintf(stderr, "Read file \"%s\" failed %d (%s)\n",
					file,
					errno,
					strerror(errno));
				ret = 1;
				break;
			}

			ios = ret;
			if (ios < iosize) {
				if (floop) {
					/* Rewind and read remaining of buffer */
					lseek(fd, 0, SEEK_SET);
					ret = read(fd, iobuf + ios, iosize - ios);
					if (ret < 0) {
						fprintf(stderr, "Read file \"%s\" failed %d (%s)\n",
							file,
							errno, strerror(errno));
						ret = 1;
						break;
					}
					ios += ret;
				} else if (ios) {
					/* Clear end of buffer */
					memset(iobuf + ios, 0, iosize - ios);
				}
			}

			if (!ios)
				/* EOF */
				break;

		}

		/* Do not exceed the end of the zone */
		if (zbc_zone_sequential(iozone) && zbc_zone_full(iozone))
			sector_count = 0;
		else
			sector_count = iosize >> 9;
		if (zone_ofst + sector_count > sector_max)
			sector_count = sector_max - zone_ofst;
		if (!sector_count)
			break;
		sector_ofst = zbc_zone_start(iozone) + zone_ofst;

		/* Write to zone */
		if (vio) {
			n = zbc_map_iov(iobuf, sector_count,
					iov, iovcnt, bufsize >> 9);
			if (n < 0) {
				fprintf(stderr, "iov map failed %d (%s)\n",
					-n, strerror(-n));
				ret = 1;
				goto out;
			}
			ret = zbc_pwritev(dev, iov, n, sector_ofst);
		} else {
			ret = zbc_pwrite(dev, iobuf, sector_count, sector_ofst);
		}
		if (ret <= 0) {
			fprintf(stderr, "%s failed %zd (%s)\n",
				vio ? "zbc_pwritev" : "zbc_pwrite",
				-ret, strerror(-ret));
			ret = 1;
			goto out;
		}

		zone_ofst += ret;
		bcount += ret << 9;
		iocount++;

		if (ionum > 0 && iocount >= ionum)
			break;
	}

	if (flush) {
		printf("Flushing device...\n");
		ret = zbc_flush(dev);
		if (ret != 0) {
			fprintf(stderr, "zbc_flush failed %zd (%s)\n",
				-ret, strerror(-ret));
			ret = 1;
		}
	}

	elapsed = zbc_write_zone_usec() - elapsed;

	if (elapsed) {
		printf("Wrote %llu B (%llu I/Os) in %llu.%03llu sec\n",
		       bcount,
		       iocount,
		       elapsed / 1000000,
		       (elapsed % 1000000) / 1000);
		printf("  IOPS %llu\n",
		       iocount * 1000000 / elapsed);
		brate = bcount * 1000000 / elapsed;
		printf("  BW %llu.%03llu MB/s\n",
		       brate / 1000000,
		       (brate % 1000000) / 1000);
	} else {
		printf("Wrote %llu B (%llu I/Os)\n",
		       bcount,
		       iocount);
	}

out:
	if (fd > 0)
		close(fd);
	zbc_close(dev);
	free(iobuf);
	free(zones);
	free(iov);

	return ret;

err:
	printf("Invalid command line\n");

	return 1;
}

