#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/sysinfo.h>

#define PROCSTATFILE "/proc/stat"

void eprintf(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

void *emalloc(size_t size) {
	void *p;

	p = malloc(size);
	if (!p)
		eprintf("out of memory\n");
	return p;
}

void *ecalloc(size_t nmemb, size_t size) {
	void *p;

	p = calloc(nmemb, size);
	if (!p)
		eprintf("out of memory\n");
	return p;
}

int getcpucount(void) {
	int i;
	FILE *fp;
	char buf[BUFSIZ];

	fp = fopen(PROCSTATFILE, "r");
	if (!fp)
		eprintf("can't open %s\n", PROCSTATFILE);

	if(!fgets(buf, BUFSIZ, fp)){
        eprintf("error reading %s\n", PROCSTATFILE);
        exit(1);
    }

	for (i = 0; fgets(buf, BUFSIZ, fp); i++)
		if (!!strncmp("cpu", buf, 3))
			break;

	fclose(fp);

	return i;
}

double *getcpuusage(void) {
	int i;
	char buf[BUFSIZ];
	int cpucount;
	unsigned long long total;
	static double *percent;
	FILE *fp;

	struct cpudata {
		int cpuid;
		unsigned long long user;
		unsigned long long nice;
		unsigned long long system;
		unsigned long long idle;
	};

	struct cpudata *cd;
	static struct cpudata *prev;

	cpucount = getcpucount();

	if (!percent)
		percent = ecalloc((cpucount + 1), sizeof(double));

	if (!prev)
		prev = emalloc(cpucount * sizeof(struct cpudata));

	cd = emalloc(cpucount * sizeof(struct cpudata));

	fp = fopen(PROCSTATFILE, "r");
	if (!fp)
		eprintf("can't open %s\n", PROCSTATFILE);
    if (!fgets(buf, BUFSIZ, fp)) {
        eprintf("error reading %s\n", PROCSTATFILE);
        exit(1);
    }

	for (i = 0; i < cpucount; i++) {
		if (prev) {
            if (!fgets(buf, BUFSIZ, fp)) {
                eprintf("error reading %s\n", PROCSTATFILE);
                exit(1);
            }
			sscanf(buf, "cpu%d %llu %llu %llu %llu",
					&cd[i].cpuid, &cd[i].user, &cd[i].nice, &cd[i].system, &cd[i].idle);

			total = 0;
			total += (cd[i].user - prev[i].user);
			total += (cd[i].nice - prev[i].nice);
			total += (cd[i].system - prev[i].system);
			percent[i] = total;
			total += (cd[i].idle - prev[i].idle);
			percent[i] /= total;
			percent[i] *= 100;
		}
	}


	fclose(fp);

	fp = fopen(PROCSTATFILE, "r");
	if (!fp)
		eprintf("can't open %s\n", PROCSTATFILE);
	if(!fgets(buf, BUFSIZ, fp)) {
        eprintf("error reading %s\n", PROCSTATFILE);
        exit(1);
    }

	for (i = 0; i < cpucount; i++) {
		if(!fgets(buf, BUFSIZ, fp)) {
            eprintf("error reading %s\n", PROCSTATFILE);
            exit(1);
        }
		sscanf(buf, "cpu%d %llu %llu %llu %llu",
				&cd[i].cpuid, &prev[i].user, &prev[i].nice, &prev[i].system, &prev[i].idle);
	}

    free(cd);
	fclose(fp);

	return percent;
}
