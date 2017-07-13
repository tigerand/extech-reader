#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include "extech.h"

 int
decode_extech_value(unsigned char byt3, unsigned char byt4, char *a)
{
	unsigned int input = ((unsigned int)byt4 << 8) + byt3;
	unsigned int i;
	unsigned int idx;
	unsigned char revnum[] = {
				0x0, 0x8, 0x4, 0xc,
				0x2, 0xa, 0x6, 0xe,
				0x1, 0x9, 0x5, 0xd,
				0x3, 0xb, 0x7, 0xf
	};

	unsigned char revdec[] = {0x0, 0x2, 0x1, 0x3};

	unsigned int digit_map[] = {0x2, 0x3c, 0x3c0, 0x3c00};
	unsigned int digit_shift[] = {1, 2, 6, 10};

	unsigned int sign;
	unsigned int decimal;

	/*
	 * this is basically BCD encoded floating point... but kinda weird
	 */

	decimal = (input & 0xc000) >> 14;
	decimal = revdec[decimal];

	sign = input & 0x1;

	idx = 0;
	if (sign) {
		a[idx++] = '+';
	} else {
		a[idx++] = '-';
	}

	/* first digit is only one or zero */
	a[idx] = '0';
	if ((input & digit_map[0]) >> digit_shift[0]) {
		a[idx] += 1;
	}

	idx++;
	/* Reverse the remaining three digits and store in the array */
	for (i = 1; i < 4; i++) {
		int dig = ((input & digit_map[i]) >> digit_shift[i]);
		dig = revnum[dig];
		if (dig > 0xa) {
			goto error_exit;
		}

		a[idx++] = '0' + dig;
	}

	/* Fit the decimal point where appropriate */
	for (i = 0; i < decimal; i++) {
		a[idx - i] = a[idx - i - 1];
	}

	a[idx - decimal] = '.';
	a[++idx] = '0';
	a[++idx] = '\0';

	return 0;
error_exit:
	return -1;
}


main() {
	unsigned char buf[5];
	char out[32];
	unsigned int rc;

	while (read(0, buf, 20)) {
		rc = decode_extech_value(buf[2], buf[3], out);
		if (rc >= 0) {
			printf("%s ", out);
		}
		rc = decode_extech_value(buf[5 + 2], buf[5 + 3], out);
		if (rc >= 0) {
			printf("%s ", out);
		}
		rc = decode_extech_value(buf[10 + 2], buf[10 + 3], out);
		if (rc >= 0) {
			printf("%s ", out);
		}
		rc = decode_extech_value(buf[15 + 2], buf[15 + 3], out);
		if (rc >= 0) {
			printf("%s", out);
		}
		printf("\n");
	}
}
