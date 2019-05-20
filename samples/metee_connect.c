/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2014-2019 Intel Corporation
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <ctype.h>
#include <linux/mei.h>
#include <metee.h>
#include "meiuuid.h"

struct params {
	bool verbose;
	uuid_le uuid;
	int iterations;
};

static int work(struct params *p)
{
	TEEHANDLE cl;
	const unsigned char cmd[] = "AB";
	const ssize_t sz = sizeof(cmd);
	unsigned char *buf;
	size_t rsz, wsz;
	int i;
	TEESTATUS status;

	status = TeeInit(&cl, &p->uuid, NULL);
	//rc = mei_init(&cl, "/dev/mei", &p->uuid, 0, p->verbose);
	if (status != TEE_SUCCESS)
		goto out;

	status = TeeConnect(&cl);
	if (status != TEE_SUCCESS)
		goto out;

	rsz = cl.maxMsgLen;
	buf = (unsigned char *)malloc(rsz);
	memset(buf, 0, rsz);
	if (buf == NULL)
		goto out;

	for (i = 0; i < p->iterations; i++) {

		status = TeeWrite(&cl, cmd , sz, &wsz, 0);
		if (status != TEE_SUCCESS)
			goto out;

		if (wsz != sz) {
			status = TEE_UNABLE_TO_COMPLETE_OPERATION;
			goto out;
		}

		status = TeeRead(&cl, buf, rsz, NULL, 10000);
		if (status != TEE_SUCCESS)
			goto out;
	}
out:
	TeeDisconnect(&cl);
	return status;
}



static void usage(const char *p)
{
	fprintf(stdout, "%s: -u <uuid> [-i <iterations>] [-h]\n", p);
}

#define BIT(_x) (1<<(_x))
#define UUID_BIT  BIT(1)
#define ITER_BIT  BIT(2)

static int mei_getopt(int argc, char *argv[], struct params *p)
{

	unsigned long required = UUID_BIT;
	unsigned long present  = 0;

	extern char *optarg;
	extern int optind, opterr, optopt;
	int opt;

	while ((opt = getopt(argc, argv, "hvu:i:")) != -1) {
		switch (opt) {
		case 'v':
			p->verbose = true;
			break;
		case 'u':
			present |= UUID_BIT;
			if (mei_uuid_parse(optarg, &p->uuid) < 0)
				return -1;
			break;
		case 'i':
			present |= ITER_BIT;
			p->iterations = strtoul(optarg, NULL, 10);
			break;
		case 'h':
		case '?':
			usage(argv[0]);
			break;
		}
	}

	if ((required & present) != required) {
		usage(argv[0]);
		return -1;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	struct params p;
	int rc;

	p.verbose = false;
	p.uuid = NULL_UUID_LE;
	p.iterations = 1;

#ifdef _WIN32
	//p.uuid = ;
#else
	rc = mei_getopt(argc, argv, &p);
	if (rc) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
#endif /* _WIN32 */

	rc = work(&p);
	exit(rc);
}
