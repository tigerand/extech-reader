/*
 * Copyright 2017, Low Power Company, Inc.
 * Copyright 2017, Andrew Sharp
 *
 *	extech - Program for controlling the extech Device
 *	This file was part of PowerTOP
 *
 *      Based on earlier client by Patrick Mochel for Wattsup Pro device
 *	Copyright (c) 2005 Patrick Mochel
 *	Copyright (c) 2006 Intel Corporation
 *	Copyright (c) 2011 Intel Corporation
 *
 *	Authors:
 *	    Patrick Mochel
 *	    Venkatesh Pallipadi
 *	    Arjan van de Ven
 *
 *	Thanks to Rajiv Kapoor for finding out the DTR, RTS line bits issue below
 *	Without that this program would never work.
 *
 *
 *	This program file is free software; you can redistribute it and/or modify it
 *	under the terms of the GNU General Public License as published by the
 *	Free Software Foundation; version 2 of the License.
 *
 *	This program is distributed in the hope that it will be useful, but WITHOUT
 *	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *	FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *	for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program in a file named COPYING; if not, write to the
 *	Free Software Foundation, Inc,
 *	51 Franklin Street, Fifth Floor,
 *	Boston, MA 02110-1301 USA
 *	or just google for it.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <ctype.h>
#include <getopt.h>
#include <time.h>
#include <signal.h>
#include <strings.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "measurement.h"
#include "extech.h"


struct epacket {
	char	buf[256];
	float	watts;
	float	pf;
	float	volts;
	float	amps;
	int		len;
};

static struct power_meter et;
#ifdef DEBUG_PROTO
static FILE *dfile;
#endif

/*
 * these must be defined in the main line or other file
 */
extern struct reading *rsp;
extern int so_rs;
int rs;  /* the index into the rsp array */



 static int
open_device(const char *device_name)
{
	struct stat s;
	int ret;

	ret = stat(device_name, &s);
	if (ret < 0) {
		return -1;
	}

	if (!S_ISCHR(s.st_mode)) {
		return -1;
	}

	ret = access(device_name, R_OK | W_OK);
	if (ret) {
		return -1;
	}

	ret = open(device_name, O_RDWR | O_NONBLOCK | O_NOCTTY);
	if (ret < 0) {
		return -1;
	}

	return ret;
}


 static int
setup_serial_device(int dev_fd)
{
	struct termios t;
	int ret;
	int flgs;

	ret = tcgetattr(dev_fd, &t);
	if (ret) {
		return errno;
	}

	cfmakeraw(&t);
	cfsetispeed(&t, B9600);
	cfsetospeed(&t, B9600);
	tcflush(dev_fd, TCIFLUSH);

	t.c_iflag = IGNPAR;
	t.c_cflag = B9600 | CS8 | CREAD | CLOCAL;
	t.c_oflag = 0;
	t.c_lflag = 0;
	t.c_cc[VMIN] = 2;
	t.c_cc[VTIME] = 0;

	t.c_iflag &= ~(IXON | IXOFF | IXANY);
	t.c_oflag &= ~(IXON | IXOFF | IXANY);

	ret = tcsetattr(dev_fd, TCSANOW, &t);

	if (ret) {
		return errno;
	}

	/*
	 * Root caused by Rajiv Kapoor. Without this extech reads
	 * will fail
	 */

	/*
	 * get DTR and RTS line bits settings
	 */
	ioctl(dev_fd, TIOCMGET, &flgs);
	debugp("DTR '%d' RTS '%d'", flgs & TIOCM_DTR ? 1 : 0,
								flgs & TIOCM_RTS ? 1 : 0);

	/*
	 * set DTR to 1 and RTS to 0
	 */
	flgs |= TIOCM_DTR;
	flgs &= ~TIOCM_RTS;
	ioctl(dev_fd, TIOCMSET, &flgs);

	return 0;
}

 void
print_block(char *b)
{
	fprintf(stderr, "%.2hhx %02hhx %02hhx %02hhx %02hhx\n", b[0], b[1], b[2],
		b[3], b[4]);
}


 static unsigned int
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

 static int
parse_epacket(struct epacket * p)
{
	int i;
	int ret;
	char op[64];
	char *val;

	p->buf[p->len] = '\0';

	/* write out the read data for debugging purposes */
#ifdef DEBUG_PROTO
	fwrite(p->buf, 1, p->len, dfile);
#endif

	/*
	 * First character in 5 character block should be '02'
	 * Fifth character in 5 character block should be '03'
	 */
	for (i = 0; i < 4; i++) {
		if (p->buf[i * 5] != 2 || p->buf[(i * 5) + 4] != 3) {
			fprintf(stderr, "Invalid packet[%d] bookends ", i);
			print_block(&p->buf[i * 5]);
			return -1;
		}
	}

	/*
	 * order of values: watts, amps, volts, pf
	 */
	for (i = 0; i < 4; i++) {
		ret = decode_extech_value(p->buf[(i * 5) + 2], p->buf[(i * 5) + 3],
				&op[i * 10]);
		if (ret) {
			fprintf(stderr, "Invalid packet[%d] failed conversion ", i);
			print_block(&p->buf[i * 5]);
			return -1;
		}
	}
	p->watts = strtof(&op[0], NULL);
	p->amps = strtof(&op[10], NULL);
	p->volts = strtof(&op[20], NULL);
	p->pf = strtof(&op[30], NULL);

	return 0;
}


 static struct epacket *
extech_read(int er_fd)
{
	static struct epacket p;
	fd_set read_fd;
	struct timeval tv;
	int ret;

	if (er_fd < 0) {
		return NULL;
	}

	FD_ZERO(&read_fd);
	FD_SET(er_fd, &read_fd);

	bzero(&p, sizeof(p));

	/*
	 * half a second timeout
	 */
	tv.tv_sec = 0;
	tv.tv_usec = 500000;

	ret = select(er_fd + 1, &read_fd, NULL, NULL, &tv);
	if (ret <= 0) {
		return NULL;
	}

	ret = read(er_fd, &p.buf, 200);  /* why 200?  why not 20? */
	debugp("serial read returned %d", ret);
	if (ret < 20) {
		return NULL;
	}
	p.len = ret;

	if (parse_epacket(&p) == 0) {
		/* success */
		return &p;
	}

	return NULL;
}

 int
extech_power_meter(const char *extech_name)
{
	int ret;
	char date[32];
	time_t t;
	struct tm *timem;

	et.rate = 0.0;
	strncpy(et.dev_name, extech_name, sizeof(et.dev_name));

	et.fd = open_device(et.dev_name);
	if (et.fd < 0) {
		return errno;
	}

	ret = setup_serial_device(et.fd);
	if (ret) {
		/*
		 * ret is 0 on success, errno on failure
		 */
		close(et.fd);
		et.fd = -1;
		return ret;
	}

	t = time(NULL);
	timem = localtime(&t);
	strftime(date, sizeof(date), "%D", timem);
#ifdef DEBUG_PROTO
	dfile = fopen("extech-debug.dat", "a");
	fprintf(dfile, "date %s\n", date);
#endif

	return 0;
}


 void
measure(void)
{
	struct epacket *mp;

	/* trigger the extech to send data */
	if (write(et.fd, " ", 1) == -1) {
		 printf("write error device '%s': %s\n", et.dev_name, strerror(errno));
	}

	mp = extech_read(et.fd);
	if (ISPOINTER(mp)) {
		et.rate = (double)mp->watts;
	} else {
		et.rate = 0.;
	}
}




/*
 * store the readings for later inclusion into a database or whatever
 */
 void
store_reading(struct epacket *ep)
{
	struct timespec res;
	extern struct timespec startclk;

 	if (rs == 0) {
		/*
		 * nacent code used to determine the coarse clock resolution
		 clock_getres(CLOCK_REALTIME_COARSE, &res);
		 printf("\nresolution of CLOCK_REALTIME_COARSE is %ld secs %ld (%#lx) nsecs\n", res.tv_sec, res.tv_nsec, res.tv_nsec);
		 */
		/* which apparently is 4000000 nsecs (4 msecs) */

		clock_gettime(CLOCK_REALTIME_COARSE, &startclk);
	}

	if (rs >= so_rs) {
		//malloc(second store block);
		//rsp = new readings store address;
		//continue on
		//the above isn't implemented yet, so for now we punt.  malloc
		//latency will have to be addressed.
		return;
	}

	clock_gettime(CLOCK_MONOTONIC_COARSE, &rsp[rs].tstamp);
	rsp[rs].watts = ep->watts;
	rsp[rs].pf = ep->pf;
	rsp[rs].volts = ep->volts;
	rsp[rs].amps = ep->amps;
	rs++;
}


 void
sample(void)
{
	ssize_t ret;
	struct timespec tv;
	float wattsread;
	struct epacket rp, *pp;

	/*
	 * take a reading 2.5 times a second
	 */
	tv.tv_sec = 0;
	tv.tv_nsec = 400000000;
	while (!et.end_thread) {
		nanosleep(&tv, NULL);
		/* trigger the extech to send data */
		ret = write(et.fd, " ", 1);
		if (ret < 0) {
			continue;
		}

		wattsread = 0.;

		pp = extech_read(et.fd);
		if (pp) {
			rp = *pp;
			wattsread = rp.watts;
			et.sum += (double)wattsread;
			et.samples++;
			store_reading(&rp);
		}
	}
}

 void *
thread_proc(void *arg)
{
	//class extech_power_meter *parent;

	//parent = (class extech_power_meter *)arg;
	//parent->et.sample();
	sample();
	return 0;
}

 void
end_measurement(void)
{
	et.end_thread = 1;
	pthread_join(et.thread, NULL);
	if (et.samples) {
		et.rate = et.sum / et.samples;
	} else {
		measure();
	}

#ifdef DEBUG_PROTO
	fprintf(dfile, "\n");
	fclose(dfile);
#endif

	printf("number of readings saved: %d\n", rs);
}

 void
start_measurement(void)
{
	et.end_thread = 0;
	et.sum = et.samples = 0;

	if (pthread_create(&et.thread, NULL, thread_proc, "GO")) {
		fprintf(stderr, "ERROR: extech measurement thread creation failed\n");
	}
}


 double
ex_joules_consumed(void)
{
	return et.rate;
}
