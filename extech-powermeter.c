#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <ctype.h>
#include <time.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include "../../../../../software/perrno/perrno.h"
#include "extech.h"


char **argvec;
int scroll_opt = 0; /* means scroll the output rather than updating one line */
int helpout = 0;    /* output basic help text */

struct option ep_opts[] = {
	{
		"scroll",
		no_argument,
		&scroll_opt,
		1
	},
	{
		"help",
		no_argument,
		&helpout,
		1
	},
	{}
};

char *main_helptxt =
"This program turns your terminal into a display for the extech 380801/380803\n"
"line of power meters.  So you don't have to hover over the meter to watch\n"
"readings on the meter.  It gets the readings from the serial port on the\n"
"meter.";

char *opt_help[] = {
"	This option makes the output scroll, rather than updating one line\n"
"	of display continuously.",

"	Output this help text.",

	NULL,
};

 void
usage(int argn, char *estring)
{
	char *argstr;
	int x;

	if (argn > 0) {
		printf("%s: for option %s\n", estring, ep_opts[argn].name);
	} else if (argn == 0) {
		/* if both arguments are 0, then usage can be used for basic help */
		if (estring) {
			printf("invalid option '%s'\n", estring);
		}
	}
	printf("\n");
	printf("usage: %s [<options>] <serial-port-device>\n\n", argvec[0]);
	printf(main_helptxt);
	printf("\n\n");
	printf("The valid options are:\n\n");
	for (x = 0; ep_opts[x].name; x++) {
		switch (ep_opts[x].has_arg) {
			case required_argument:
				argstr = "=<arg>";
				break;
			case optional_argument:
				argstr = "[=<arg>]";
				break;
			default:
				argstr = "";
				break;
		}
		printf("--%s%s\n", ep_opts[x].name, argstr);
		printf(opt_help[x]);
		printf("\n");
	}
}


/*
 * see if a file is a device node
 */
 static int
is_dev(char *dname)
{
	struct stat st;

	stat(dname, &st);
	/*
	 * should we really allow block devices?  i guess so, i mean, whatever
	 */
	if (S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode) || S_ISFIFO(st.st_mode)
		|| S_ISSOCK(st.st_mode)) {
		return 1;
	}

	return 0;
}


 int
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


 int
open_file(const char *fname)
{
	struct stat s;
	int ret;

	ret = stat(fname, &s);
	if (ret < 0) {
		return -1;
	}

	ret = access(fname, R_OK);
	if (ret) {
		return -1;
	}

	ret = open(fname, O_RDONLY | O_NONBLOCK); /* not sure about the nonblock */
	if (ret < 0) {
		return -1;
	}

	return ret;
}


 int
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
//printf("DTR '%d' RTS '%d'\n", flgs & TIOCM_DTR ? 1 : 0, flgs & TIOCM_RTS ? 1 : 0);

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
		//if (dig > 0xa) {
			//goto error_exit;
		//}

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


int is_dfile = 0; /* set to 1 if "serial device" is actually a device file */
int is_rfile = 0; /* set to 1 if "serial device" is a regular file */


 int
main(int argc, char **argv) {
	unsigned char buf[256];
	char out[32];
	unsigned int rc;
	struct timespec tv;
	int ser_port_fd;
	struct termios ti; /* termios to set */
	struct termios ts; /* place to store/save termios */
	int flgs;
	int argx;


	argvec = argv;

	/*
	 * process args
	 */
	do {
		rc = getopt_long(argc, argv, ":h", &ep_opts[0], &argx);
		switch (rc) {
			case ':':
				usage(argx, "missing required argument");
				exit(1);
			case '?':
				usage(0, argv[optind - 1]);
				exit(1);
			case 'h':
				helpout = 1;
				break;
			case 0:
				/*
				 * check which option this is, and process option args if needed
				 */
				break;
		}
	} while (rc != -1);

	if (helpout) {
		usage(0, NULL);
		exit(0);
	}

	if ((argv[optind] == NULL) || (strlen(argv[optind]) == 0)) {
		printf("error: first argument must be serial port device file\n");
		usage(0, NULL);
		exit(1);
	}

	/*
	 * open the device and then screw with all the settings
	 */
	if (is_dev(argv[optind])) {
		ser_port_fd = open_device(argv[optind]);
		setup_serial_device(ser_port_fd);
		is_dfile = 1;
	} else {
		ser_port_fd = open_file(argv[optind]);
		is_rfile = 1;
	}
	if (ser_port_fd < 0) {
		fprintf(stderr, "failed to open '%s' - errno %d\n", argv[optind],
			errno);
		exit(1);
	}

	/*
	 * set stdin to non blocking and no ints
	 * so we can check for a 'q' w/o getting stuck
	 */
	rc = tcgetattr(0, &ti);
	ts = ti;

	tcflush(0, TCIFLUSH); /* discard any unread characters */

	ti.c_lflag |= (~ECHO | ~ISIG);  /* turn of echo and interrupt generation */

	/* min 1 character.
	 * could be zero, really, since we set non_block
	 */
	ti.c_cc[VMIN] = 1;
	ti.c_cc[VTIME] = 0; /* no min time to wait */

	rc = tcsetattr(0, TCSANOW, &ti); /* set settings immediately TCSAFLUSH */

	rc = fcntl(0, F_SETFL, O_NONBLOCK); /* set non-blocking i/o on stdin */
	if (rc) {
		fprintf(stderr, "fcntl of stdin failed with errno %d\n", errno);
		rc = tcsetattr(0, TCSANOW, &ts);
		exit(1);
	}

	/*
	 * take a reading 2.5 times a second
	 */
	tv.tv_sec = 0;
	tv.tv_nsec = 400000000;

	if (is_dfile) {
		/*
		 * clear out any residual values in power meter output queue
		 */
		read(ser_port_fd, buf, 200);
		nanosleep(&tv, NULL);
	}
	printf("\n");

	do {
		if (is_dfile) {
			/*
			 * wait just a tad
			 */
			nanosleep(&tv, NULL);
			/* send the "give me a reading" cmd char to powermeter */
			write(ser_port_fd, " ", 1);
		}

		rc = read(ser_port_fd, buf, 40);

		if ((rc >= 0) && (rc < 20)) {
			//fprintf(stderr, "\nread returned %d\n", rc);
			/*
			 * short read?  get out of the car, walk around it,
			 * get back in, and try again
			 */
			continue;
		} else if (rc == -1) {
			if (errno == EAGAIN) {
				if (is_rfile) {
					/* eof */
					break;
				}
				continue;
			}
			fprintf(stderr, "\nread returned %d, errno = %d(%s)\n", rc, errno,
				perrno(errno) ?: "NULL");
			continue;
		}

		/*
		 * watts
		 */
//print_block(buf);
		rc = decode_extech_value(buf[2], buf[3], out);
		if (rc >= 0) {
			printf("watts: %s ", out);
		}
		fflush(stdout);
		/*
		 * power factor
		 */
//print_block(buf+15);
		rc = decode_extech_value(buf[15 + 2], buf[15 + 3], out);
		if (rc >= 0) {
			printf("pf: %s ", out);
		}
		fflush(stdout);
		/*
		 * volts
		 */
//print_block(buf+10);
		rc = decode_extech_value(buf[10 + 2], buf[10 + 3], out);
		if (rc >= 0) {
			printf("volts: %s ", out);
		}
		fflush(stdout);
		/*
		 * amps
		 */
//print_block(buf+5);
		rc = decode_extech_value(buf[5 + 2], buf[5 + 3], out);
		if (rc >= 0) {
			printf("amps: %s", out);
		}
		fflush(stdout);

		/*
		 * bust out of here if user typed 'q'
		 */
		if (read(0, buf, 1) == 1) {
			if ((buf[0] == 'q') || (buf[0] == 0x3)) {
				break;
			}
		}

		/*
		 * move cursor to beginning of line and clear line
		 */
		if (!scroll_opt) {
			printf("\r\e[K");
		} else {
			printf("\n");
		}
	} while (1);

	close(ser_port_fd);

	tcflush(0, TCIFLUSH); /* discard any unread characters from stdin */

	/*
	 * set stdin back to normal
	 */
	rc = tcsetattr(0, TCSANOW, &ts);
	printf("\n");
}
