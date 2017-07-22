#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "extech.h"

#undef debugp
#define debugp(A, B...) fprintf(stdout, A, B)

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

 void
print_block(char *b, int bcount)
{
	if ((bcount == 0) || (bcount > 4)) {
		debugp("%.2hhx %02hhx %02hhx %02hhx %02hhx", b[0], b[1], b[2], b[3],
			b[4]);
	} else if (bcount == 1) {
		debugp("%.2hhx", b[0]);
	} else if (bcount == 2) {
		debugp("%.2hhx %02hhx", b[0], b[1]);
	} else if (bcount == 3) {
		debugp("%.2hhx %02hhx %02hhx", b[0], b[1], b[2]);
	} else if (bcount == 4) {
		debugp("%.2hhx %02hhx %02hhx %02hhx", b[0], b[1], b[2], b[3]);
	}
}


main(int argc, char **argv) {
	unsigned char buf[64];
	char out[32];
	unsigned int rc;
	int hexout;

	hexout = 0;
	if (argv[1] && (!strncmp("-x", argv[1], 2))) {
		hexout = 1;
	}

	/* output in watts,pf,volts,amps order */
	while (rc = read(0, buf, 20)) {
printf("read returned %d  ", rc);
		if (rc < 20) {
			if (rc == 1) {
				printf("'%hhx'\n", buf[0]);
			}
			break;
		}
		if (hexout) {
			print_block(&buf[0], 5);
			printf("|");
			print_block(&buf[15], 5);
			printf("|");
			print_block(&buf[10], 5);
			printf("|");
			print_block(&buf[5], 5);
			printf("\t");
		}
		rc = decode_extech_value(buf[2], buf[3], out);
		if (rc >= 0) {
			printf("%s ", out);
		}
		rc = decode_extech_value(buf[15 + 2], buf[15 + 3], out);
		if (rc >= 0) {
			printf("%s ", out);
		}
		rc = decode_extech_value(buf[10 + 2], buf[10 + 3], out);
		if (rc >= 0) {
			printf("%s ", out);
		}
		rc = decode_extech_value(buf[5 + 2], buf[5 + 3], out);
		if (rc >= 0) {
			printf("%s", out);
		}
		printf("\n");
	}
}
