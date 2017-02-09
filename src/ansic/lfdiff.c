/*
 * lfdiff.c
 *
 *  Created on: 09.02.2017
 *      Author: jh
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
//#include <libgen.h>
#include <strings.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>


#define MAX(a,b)	((a)>(b)?(a):(b))

static const long default_splitsize = 2ul*1024*1024*1024; // 2GB
static const char *default_tmpdir = "/tmp";



const char *mybasename(const char *path) {
    const char *retval = rindex(path, '/');

    return retval? ++retval: path;
}

struct config {
    long splitsize;
    int be_verbose;
    const char *tmpdir;
    const char *outfilename;
    const char *file1;
    const char *file2;
} config = {0};


struct runtime {
    const char *argv0;
} runtime = {0};

void usage(const char *argv0) {

    fprintf(stderr, "usage: %s [-h] [-V] [-v] [-o OUTPUT] [-s SPLITSIZE] INPUT1 INPUT2\n"
	    "\t-h: print this help\n"
	    "\t-V: print version\n"
	    "\t-o: write output to OUTFILE instead of stdout\n"
	    "\t-s: split INPUT* into SPLITSIZE chunks. SPLITSIZE can be appended k,kB,M,MB,G,GB to multiply 1024, 1024², 1024³. (default: %ld byte)\n"
	    "\t-v: be verbose\n"
	    "environment TMPDIR, TMP, TEMP: look in each variable (in this order) for a setting to store files (default: %s)\n"
	    , mybasename(argv0), default_splitsize, default_tmpdir
    );

}


FILE *mypopen(const char *befehl, const char *typ, ...) __attribute__ ((__format__ (__printf__, 1, 3)));
FILE *mypopen(const char *befehl, const char *typ, ...) {
    char *string;
    va_list args;
    va_start(args, typ);
    int retval = vasprintf(&string, befehl, args);
    int myerrno = errno;
    va_end(args);
    if (-1 == retval) {
	fprintf(stderr, "%s error: could not parse command '%s': %s\n", mybasename(runtime.argv0), befehl, strerror(myerrno));
	exit (EXIT_FAILURE);
    }
    FILE *f = popen(string, typ);

    // be nice and exit on error over here
    if (NULL == f) {
	fprintf(stderr, "%s error: could not exec command '%s': %s\n", mybasename(runtime.argv0), string, strerror(errno));
	exit(EXIT_FAILURE);
    }
    free(string);

    return f;
}

int main(int argc, char **argv) {
    int retval;

    runtime.argv0 = argv[0];
    config.splitsize = default_splitsize;

    int opt;

    while ((opt = getopt(argc, argv, "hVvo:s:")) != -1)
    {
	switch (opt)
	{
	case 'h':
	    usage(argv[0]);
	    exit(EXIT_SUCCESS);
	case 'v':
	    config.be_verbose = 1;
	    break;
	case 'V':
//	    print_version();
	    exit(EXIT_SUCCESS);
	case 'o':
	    config.outfilename = optarg;
	    break;
	case 's':
	    config.splitsize = atol(optarg);
	    break;
	default: /* '?' */
	    usage(argv[0]);
	    exit(EXIT_FAILURE);
	}
    }

    if (optind >= argc) {
	fprintf(stderr, "INPUT1 missing\n");
	usage(argv[0]);
	exit(EXIT_FAILURE);
    }
    config.file1 = argv[optind++];

    if (optind >= argc) {
	fprintf(stderr, "INPUT2 missing\n");
	usage(argv[0]);
	exit(EXIT_FAILURE);
    }
    config.file2 = argv[optind++];

    {
	const char *temp;
	if ((temp = getenv("TMPDIR"))) {
	}
	else if ((temp = getenv("TMP"))) {
	}
	else if ((temp = getenv("TEMP"))) {
	}
	else {
	    temp = default_tmpdir;
	}
	config.tmpdir = temp;
    }

    int i;
    {
	struct stat mystat;
	int retval = stat(config.file1, &mystat);
	if (-1 == retval) {
	    fprintf(stderr, "%s error: could not read from file '%s': %s\n", mybasename(argv[0]), config.file1, strerror(errno));
	    exit(EXIT_FAILURE);
	}
	off_t size = mystat.st_size;
	i = size/config.splitsize;

	retval = stat(config.file2, &mystat);
	if (-1 == retval) {
	    fprintf(stderr, "%s error: could not read from file '%s': %s\n", mybasename(argv[0]), config.file2, strerror(errno));
	    exit(EXIT_FAILURE);
	}
	size = mystat.st_size;

	i = MAX(size/config.splitsize, i);
    }
    const int max_i = i+1;

    FILE *tempfile = tmpfile();

    for (i=1; i<=max_i; i++) { // exit loop if both split files return 0 bytes
	int lines1, lines2;
	FILE *splitinput;

	fprintf(stderr, "split input1 %d/%d\n", i, max_i);
	splitinput = mypopen ("split -n l/%d/%d '%s' | wc -l", "r", i, max_i, config.file1);
	retval = fscanf(splitinput, "%d\n", &lines1);
	if (1 > retval) {
	    if (errno) {
		fprintf(stderr, "%s error: reading from 'split -n l/%d/%d '%s' | wc -l': %s\n", mybasename(argv[0]), i, max_i, config.file1, strerror(errno));
	    }
	    else {
		fprintf(stderr, "%s error: no valid input from 'split -n l/%d/%d '%s' | wc -l': %s\n", mybasename(argv[0]), i, max_i, config.file1, strerror(errno));
	    }
	    exit(EXIT_FAILURE);
	}
	pclose(splitinput);

	fprintf(stderr, "split input2 %d/%d\n", i, max_i);
	splitinput = mypopen ("split -n l/%d/%d '%s' | wc -l", "r", i, max_i, config.file2);
	retval = fscanf(splitinput, "%d\n", &lines2);
	if (1 > retval) {
	    if (errno) {
		fprintf(stderr, "%s error: reading from 'split -n l/%d/%d '%s' | wc -l': %s\n", mybasename(argv[0]), i, max_i, config.file1, strerror(errno));
	    }
	    else {
		fprintf(stderr, "%s error: no valid input from 'split -n l/%d/%d '%s' | wc -l': %s\n", mybasename(argv[0]), i, max_i, config.file1, strerror(errno));
	    }
	    exit(EXIT_FAILURE);
	}
	pclose(splitinput);

	if (!lines1 && !lines2)
	    break;

	// use bash for input pipe substitution
	char *line;
	fprintf(stderr, "diff input %d/%d\n", i, max_i);
	splitinput = mypopen ("bash -c \"diff <(split -n l/%d/%d '%s') <(split -n l/%d/%d '%s')\"", "r", i, max_i, config.file1, i, max_i, config.file2);
	while (!feof(splitinput)) {
	    size_t n=0;
	    line = NULL;
	    retval = getline(&line, &n, splitinput);
	    if (1 > retval) {
		if (errno) {
		    fprintf(stderr, "%s error: reading from 'split -n l/%d/%d '%s' | wc -l': %s\n", mybasename(argv[0]), i, max_i, config.file1, strerror(errno));
		}
		else {
		    fprintf(stderr, "%s error: no valid input from 'split -n l/%d/%d '%s' | wc -l': %s\n", mybasename(argv[0]), i, max_i, config.file1, strerror(errno));
		}
		exit(EXIT_FAILURE);
	    }
	    // line includes newline character at the end
	    fprintf(tempfile, "%s", line);


	    free(line);
	}
	pclose(splitinput);
    }

    retval = fseek(tempfile, 0, SEEK_SET);
    size_t n = 0;
    char *line = NULL;
    while (!feof(tempfile)) {
	retval = getline(&line, &n, tempfile);
	if (1 > retval) {
	    if (errno) {
		fprintf(stderr, "%s error: reading from 'split -n l/%d/%d '%s' | wc -l': %s\n", mybasename(argv[0]), i, max_i, config.file1, strerror(errno));
	    }
	    else {
		fprintf(stderr, "%s error: no valid input from 'split -n l/%d/%d '%s' | wc -l': %s\n", mybasename(argv[0]), i, max_i, config.file1, strerror(errno));
	    }
	    exit(EXIT_FAILURE);
	}
	fprintf(stdout, "%s", line);
    }

    return 0;
}

