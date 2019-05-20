/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (C) 2014-2019 Intel Corporation
 */
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>

#include "meiuuid.h"

int mei_uuid_parse(const char *str, uuid_le *uuid)
{
	const char *p = "00000000-0000-0000-0000-000000000000";
	const size_t len = strlen(p);
	uint32_t a;
	uint16_t b, c;
	uint8_t d[2], e[6];
	char buf[3];
	int i;

	if (strlen(str) != len)
		return -1;

	for (i = 0; i < len; i++) {
		if (str[i] == '-') {
			if (p[i] == '-')
				continue;
			else
				return -1;
		} else if (!isxdigit(str[i])) {
			return -1;
		}
	}

	a = strtoul(str +  0, NULL, 16);
	b = strtoul(str +  9, NULL, 16);
	c = strtoul(str + 14, NULL, 16);

	buf[2] = 0;
	for (i = 0; i < 2; i++) {
		buf[0] = str[19 + i * 2];
		buf[1] = str[19 + i * 2 + 1];
		d[i] = strtoul(buf, NULL, 16);
	}

	for (i = 0; i < 6; i++) {
		buf[0] = str[24 + i * 2];
		buf[1] = str[24 + i * 2 + 1];
		e[i] = strtoul(buf, NULL, 16);
	}

	*uuid = UUID_LE(a, b, c, d[0], d[1], e[0], e[1], e[2], e[3], e[4], e[5]);

	return 0;
}

