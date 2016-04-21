// data file in natural layout (array of structs)

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/time.h>


typedef struct dtype dtype;

struct dtype {
	float	a;
	float	b;
	float	c;
};

int chkread;
int arraycount;
int chunkcount;

float *a;
float *b;
float *c;
dtype *da;

dtype *dachunk;

unsigned long long
timeus()
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return tv.tv_sec*1000000ULL + tv.tv_usec;
}

float
aval(int n)
{
	return 1.6432093e4 + n*4.232473e3;
}

float
bval(int n)
{
	return 2.293873e12 + n*3.92384057e11;
}

float
cval(int n)
{
	return 4.8394275e9 + n*8.94398752e8;
}

int
write_seq(int fd)
{
	unsigned long long arraysz;

	arraysz = arraycount * sizeof(dtype);
	if (pwrite64(fd, da, arraysz, 0) < 0) 
		return -1;

	return 0;
}

int
write_stride(int fd)
{
	unsigned long long i, n, m, arraysz, chunksz;

	arraysz = arraycount * sizeof(dtype);
	chunksz = chunkcount * sizeof(dtype);
	for(n = 0; n < arraycount; ) {
		m = chunkcount;
		if (n + m > arraycount)
			m = arraycount - n;

		for(i = 0; i < m; i++) {
			dachunk[i].a = a[i+n];
			dachunk[i].b = b[i+n];
			dachunk[i].c = c[i+n];
		}

		if (pwrite64(fd, dachunk, m * sizeof(*dachunk), n * sizeof(*dachunk)) < 0)
			return -1;

		n += m;
	}

	return 0;
}

int
read_seq(int fd)
{
	unsigned long long arraysz, n;

	arraysz = arraycount * sizeof(dtype);
	n = pread64(fd, da, arraysz, 0);
	if (n<0 || n!=arraysz) {
		return -1;
	}

	return 0;
}

int
read_stride(int fd)
{
	unsigned long long i, l, n, m, arraysz, chunksz;

	arraysz = arraycount * sizeof(dtype);
	chunksz = chunkcount * sizeof(dtype);
	for(n = 0; n < arraycount; ) {
		m = chunkcount;
		if (n + m > arraycount)
			m = arraycount - n;

		l = pread64(fd, dachunk, m * sizeof(*dachunk), n * sizeof(*dachunk));
		if (l<0 || l!=m*sizeof(*dachunk)) {
			return -1;
		}

		for(i = 0; i < m; i++) {
			a[i+n] = dachunk[i].a;
			b[i+n] = dachunk[i].b;
			c[i+n] = dachunk[i].c;
		}

		n += m;
	}

	return 0;
}

void
usage()
{
	fprintf(stderr, "Usage: atest -r -s array-size -c chunk-size -f file-name ops\n");
	exit(-1);
}

int
checkread()
{
	int i;
	float aa, bb, cc;

	for(i = 0; i < arraycount; i++) {
		aa = aval(i);
		bb = bval(i);
		cc = cval(i);

		if (a[i]!=aa || da[i].a!=aa) {
			fprintf(stderr, "i %d aa %f a %f da %f\n", i, aa, a[i], da[i].a);
			return -1;
		}

		if (b[i]!=bb || da[i].b!=bb) {
			fprintf(stderr, "i %d bb %f b %f da %f\n", i, bb, b[i], da[i].b);
			return -1;
		}

		if (c[i]!=cc || da[i].c!=cc) {
			fprintf(stderr, "i %d cc %f c %f da %f\n", i, cc, c[i], da[i].c);
			return -1;
		}
	}

	return 0;
}

int
runops(int fd, char *ops)
{
	char op;
	int err, i;
	unsigned long long st, et;

	for(op = *ops; op!='\0'; op = *(++ops)) {
		if (chkread && (op=='r' || op=='R')) {
			if (op=='r') {
				for(i = 0; i < arraycount; i++) {
					a[i] = 0.0;
					b[i] = 0.0;
					c[i] = 0.0;
				}
			} else {
				for(i = 0; i < arraycount; i++) {
					da[i].a = 0.0;
					da[i].b = 0.0;
					da[i].c = 0.0;
				}
			}
		}

		st = timeus();
		switch (op) {
		case 'r':
			err = read_seq(fd);
			break;

		case 'R':
			err = read_stride(fd);
			break;

		case 'w':
			err = write_seq(fd);
			break;

		case 'W':
			err = write_stride(fd);
			break;

		default:
			usage();
		}

		et = timeus();

		if (err<0) 
			return -1;

		if (chkread && (op=='r' || op=='R')) {
			if (checkread() != 0) {
				fprintf(stderr, "read data incorrect\n");
				return -1;
			}
		}

		printf("%c %llu %llu\n", op, 3*sizeof(float)*((unsigned long long) arraycount), et - st);
	}

	return 0;
}

int
main(int argc, char **argv)
{
	int i, p, fd, frachunk;
	char *fname, *ops, *s;

	frachunk = 0;
	fname = "atest.data";
	while ((p = getopt(argc, argv, "c:f:rs:")) != -1) {
		switch (p) {
		case 'c':
			if (*optarg=='/')
				frachunk = strtol(&optarg[1], &s, 10);
			else
				chunkcount = strtol(optarg, &s, 10);
				
			if (*s != '\0')
				usage();
			break;

		case 'f':
			fname = optarg;
			break;

		case 's':
			arraycount = strtol(optarg, &s, 10);
			if (*s != '\0')
				usage();
			break;

		case 'r':
			chkread++;
			break;

		default:
			usage();
		}
	}

	if (frachunk != 0) {
		chunkcount = arraycount / frachunk;
	}

	if (arraycount==0 || chunkcount==0)
		usage();

	if (argc - optind < 1)
		usage();

	ops = argv[optind];
	fd = open(fname, O_RDWR|O_CREAT, 0666);
	if (fd < 0) {
		perror("open");
		return -1;
	}

	a = calloc(arraycount, sizeof(*a));
	if (chkread) {
		for(i = 0; i < arraycount; i++)
			a[i] = aval(i);
	}

	b = calloc(arraycount, sizeof(*b));		
	if (chkread) {
		for(i = 0; i < arraycount; i++)
			b[i] = bval(i);
	}

	c = calloc(arraycount, sizeof(*c));
	if (chkread) {
		for(i = 0; i < arraycount; i++)
			c[i] = cval(i);
	}

	da = calloc(arraycount, sizeof(*da));
	if (chkread) {
		for(i = 0; i < arraycount; i++) {
			da[i].a = aval(i);
			da[i].b = bval(i);
			da[i].c = cval(i);
		}
	}

	dachunk = calloc(chunkcount, sizeof(*dachunk));

	if (runops(fd, ops) < 0) {
		perror("error while running the ops");
		exit(-1);
	}

	return 0;
}
