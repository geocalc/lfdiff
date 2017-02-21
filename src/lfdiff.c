/*
 * lfdiff.c
 *
 *  Created on: 09.02.2017
 *      Author: jh
 */

/*
    Main module of lfdiff

    Copyright (C) 2017  Jörg Habenicht (jh@mwerk.net)

    This file is part of lfdiff

    lfdiff is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    lfdiff is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#define _GNU_SOURCE
#include "diffmanager.h"
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <strings.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <regex.h>
#include <limits.h>


#define MIN(a,b)	((a)<(b)?(a):(b))
#define MAX(a,b)	((a)>(b)?(a):(b))

#define REGEX_MATCHBUFFER_LEN	6

#define PRINT_VERBOSE(stream, text, ...) \
    do { \
	if (config.be_verbose) \
	    fprintf((stream), (text), ##__VA_ARGS__); \
    } while (0)


static const long default_splitsize = 2l*1024*1024*1024; // 2GB



const char *mybasename(const char *path) {
    const char *retval = rindex(path, '/');

    return retval? ++retval: path;
}

struct config {
    long splitsize;
    int be_verbose;
    const char *outfilename;
    const char *filename1;
    const char *filename2;
} config = {0};


struct runtime {
    const char *argv0;
    unsigned long currentlineFileA;
    unsigned long currentlineFileB;
    unsigned long lineOffsetFileA;
    unsigned long lineOffsetFileB;
    struct diffmanager_s *diffmanager;
} runtime = {0};

void usage(const char *argv0) {

    fprintf(stderr, "usage: %s [-h] [-V] [-v] [-o OUTPUT] [-s SPLITSIZE] INPUT1 INPUT2\n"
	    "\t-h: print this help\n"
	    "\t-V: print version\n"
	    "\t-o: write output to OUTFILE instead of stdout\n"
	    "\t-s: split INPUT* into SPLITSIZE chunks. SPLITSIZE can be appended k,kB,M,MB,G,GB to multiply 1024, 1024², 1024³. (default: %ld byte)\n"
	    "\t-v: be verbose\n"
	    , mybasename(argv0), default_splitsize
    );

}

void print_version() {

    fprintf(stderr, "%s\n", VERSION);
}

char *mystrlcpy(char *dest, const char *src, size_t n) {
    assert(dest);
    assert(src);
    assert(n>=0);

    strncpy(dest, src, n - 1);
    if (n > 0)
	dest[n - 1]= '\0';

    return dest;
}

void myregexbuffercpy(char *dest, const char *src, int start, int end, int bufferlen) {
    assert(dest);
    assert(src);
    assert(start>=0);
    assert(end>=start);
    assert(bufferlen>=0);

    mystrlcpy(dest, src+start, MIN(bufferlen,end-start+1));
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
    regex_t regex;

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
	    print_version();
	    exit(EXIT_SUCCESS);
	case 'o':
	    config.outfilename = optarg;
	    break;
	case 's':
	{
	    retval = regcomp(&regex, "^([0-9]+)([kMG]?)B?$",  REG_EXTENDED/*|REG_NEWLINE*/);
	    if( retval ) {
		size_t len = regerror(retval, &regex, NULL, 0);
		char *buffer = malloc(len);
		assert(buffer);
		(void) regerror (retval, &regex, buffer, len);
		fprintf(stderr, "Could not compile regular expression: %s", buffer);
		free(buffer);
		exit(EXIT_FAILURE);
	    }

	    regmatch_t matchptr[3];
	    retval = regexec(&regex, optarg, 3, matchptr, 0);
	    if( !retval )
	    {
		// Match
		static const int bufferlen = 32;
		char buffer[bufferlen];

		// extract data
		myregexbuffercpy(buffer, optarg, matchptr[1].rm_so, matchptr[1].rm_eo, bufferlen);
		config.splitsize = atol(buffer);
		if (matchptr[2].rm_so != matchptr[2].rm_eo) {
		    myregexbuffercpy(buffer, optarg, matchptr[2].rm_so, matchptr[2].rm_eo, bufferlen);
		    switch (*buffer) {
		    case 'G':
			if (config.splitsize >= LONG_MAX/1024) {
			    fprintf(stderr, "Integer overflow error parsing option -s '%s'\n", optarg);
			    exit(EXIT_FAILURE);
			}
			config.splitsize *= 1024;
			// no break, fall through
		    case 'M':
			if (config.splitsize >= LONG_MAX/1024) {
			    fprintf(stderr, "Integer overflow error parsing option -s '%s'\n", optarg);
			    exit(EXIT_FAILURE);
			}
			config.splitsize *= 1024;
			// no break, fall through
		    case 'k':
			if (config.splitsize >= LONG_MAX/1024) {
			    fprintf(stderr, "Integer overflow error parsing option -s '%s'\n", optarg);
			    exit(EXIT_FAILURE);
			}
			config.splitsize *= 1024;
			break;

		    default:
			fprintf(stderr, "Program error parsing '%s'", optarg);
			exit(EXIT_FAILURE);
		    }
		}
	    }
	    else if( retval == REG_NOMATCH )
	    {
		// No match on diff header
		fprintf(stderr, "Invalid argument to option '-s': %s\n", optarg);
		usage(argv[0]);
		exit(EXIT_FAILURE);
	    }
	    else
	    {
		size_t len = regerror(retval, &regex, NULL, 0);
		char *buffer = malloc(len);
		assert(buffer);
		(void) regerror (retval, &regex, buffer, len);
		fprintf(stderr, "Could not compile regular expression: %s", buffer);
		free(buffer);
		exit(EXIT_FAILURE);
	    }
	    regfree(&regex);
	}
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
    config.filename1 = argv[optind++];

    if (optind >= argc) {
	fprintf(stderr, "INPUT2 missing\n");
	usage(argv[0]);
	exit(EXIT_FAILURE);
    }
    config.filename2 = argv[optind++];


    int i;
    {
	struct stat mystat;
	int retval = stat(config.filename1, &mystat);
	if (-1 == retval) {
	    fprintf(stderr, "%s error: could not read from file '%s': %s\n", mybasename(argv[0]), config.filename1, strerror(errno));
	    exit(EXIT_FAILURE);
	}
	off_t size = mystat.st_size;
	i = size/config.splitsize;

	retval = stat(config.filename2, &mystat);
	if (-1 == retval) {
	    fprintf(stderr, "%s error: could not read from file '%s': %s\n", mybasename(argv[0]), config.filename2, strerror(errno));
	    exit(EXIT_FAILURE);
	}
	size = mystat.st_size;

	i = MAX(size/config.splitsize, i);
    }
    const int max_i = i+1;

    runtime.diffmanager = diffmanager_new();

    char *line = NULL;	// buffer holding one line of diff
    size_t n = 0;	// size of the buffer

    // prepare regular expression
    retval = regcomp(&regex, "^([0-9]+),?([0-9]*)([acd])([0-9]+),?([0-9]*)\n$",  REG_EXTENDED/*|REG_NEWLINE*/);
    if( retval ) {
	size_t len = regerror(retval, &regex, NULL, 0);
	char *buffer = malloc(len);
	assert(buffer);
	(void) regerror (retval, &regex, buffer, len);
	fprintf(stderr, "Could not compile regular expression: %s", buffer);
	free(buffer);
	exit(EXIT_FAILURE);
    }

    FILE *outfile = config.outfilename?fopen(config.outfilename, "w"):stdout;
    if (NULL == outfile) {
	fprintf(stderr, "error: could not open output file '%s': %s\n", config.outfilename, strerror(errno));
	exit(EXIT_FAILURE);
    }

    for (i=1; i<=max_i; i++) { // exit loop if both split files return 0 bytes
	int linesSplit1, linesSplit2;
	FILE *splitinput;

	PRINT_VERBOSE(stderr, "split input1 %d/%d\n", i, max_i);
	splitinput = mypopen ("split -n l/%d/%d '%s' | wc -l", "r", i, max_i, config.filename1);
	retval = fscanf(splitinput, "%d\n", &linesSplit1);
	if (1 > retval) {
	    if (errno) {
		fprintf(stderr, "%s error: reading from 'split -n l/%d/%d '%s' | wc -l': %s\n", mybasename(argv[0]), i, max_i, config.filename1, strerror(errno));
	    }
	    else {
		fprintf(stderr, "%s error: no valid input from 'split -n l/%d/%d '%s' | wc -l': %s\n", mybasename(argv[0]), i, max_i, config.filename1, strerror(errno));
	    }
	    exit(EXIT_FAILURE);
	}
	pclose(splitinput);

	PRINT_VERBOSE(stderr, "split input2 %d/%d\n", i, max_i);
	splitinput = mypopen ("split -n l/%d/%d '%s' | wc -l", "r", i, max_i, config.filename2);
	retval = fscanf(splitinput, "%d\n", &linesSplit2);
	if (1 > retval) {
	    if (errno) {
		fprintf(stderr, "%s error: reading from 'split -n l/%d/%d '%s' | wc -l': %s\n", mybasename(argv[0]), i, max_i, config.filename1, strerror(errno));
	    }
	    else {
		fprintf(stderr, "%s error: no valid input from 'split -n l/%d/%d '%s' | wc -l': %s\n", mybasename(argv[0]), i, max_i, config.filename1, strerror(errno));
	    }
	    exit(EXIT_FAILURE);
	}
	pclose(splitinput);

	if (!linesSplit1 && !linesSplit2)
	    break;	// exit loop if both input files empty

	// use bash for input pipe substitution
	PRINT_VERBOSE(stderr, "diff input %d/%d\n", i, max_i);
	splitinput = mypopen ("bash -c \"diff <(split -n l/%d/%d '%s') <(split -n l/%d/%d '%s')\"", "r", i, max_i, config.filename1, i, max_i, config.filename2);
	while (!feof(splitinput)) {
	    retval = getline(&line, &n, splitinput);
	    if (1 > retval) {
		if (errno) {
		    fprintf(stderr, "%s error: reading from 'bash -c \"diff <(split -n l/%d/%d '%s') <(split -n l/%d/%d '%s')\"': %s\n", mybasename(argv[0]), i, max_i, config.filename1, i, max_i, config.filename2, strerror(errno));
		}
		else if (feof(splitinput)) {
		    // we have reached the end of input.
		    // why didn't we notice earlier at the top of the loop?
		    break;
		}
		else {
		    fprintf(stderr, "%s error: no valid input from 'bash -c \"diff <(split -n l/%d/%d '%s') <(split -n l/%d/%d '%s')\"'\n", mybasename(argv[0]), i, max_i, config.filename1, i, max_i, config.filename2);
		}
		exit(EXIT_FAILURE);
	    }
	    // note: line includes a newline character at the end

	    regmatch_t matchptr[REGEX_MATCHBUFFER_LEN];
	    retval = regexec(&regex, line, REGEX_MATCHBUFFER_LEN, matchptr, 0);
	    if( !retval )
	    {
		// Match

		static const int bufferlen = 32;
		static const int actionlen = 2;
		long linesA, linesB;
		char buffer[bufferlen];
		char action[actionlen];

		// extract data
		myregexbuffercpy(buffer, line, matchptr[1].rm_so, matchptr[1].rm_eo, bufferlen);
		linesA = atol(buffer);
		myregexbuffercpy(buffer, line, matchptr[4].rm_so, matchptr[4].rm_eo, bufferlen);
		linesB = atol(buffer);
		myregexbuffercpy(action, line, matchptr[3].rm_so, matchptr[3].rm_eo, actionlen);

		runtime.currentlineFileA = linesA + runtime.lineOffsetFileA;
		runtime.currentlineFileB = linesB + runtime.lineOffsetFileB;

		// write out decoded and optimized differentials to "outfile"
		// to save some memory
		const long diffAB = diffmanager_get_linediff_A_B(runtime.diffmanager);
		long min_common_lines = MIN(runtime.currentlineFileA,runtime.currentlineFileB-diffAB)-1;
		PRINT_VERBOSE(stderr, "diff output <= line %ld\n", min_common_lines);
		diffmanager_output_diff(runtime.diffmanager, outfile, min_common_lines);
	    }
	    else if( retval == REG_NOMATCH )
	    {
		// No match on diff header

		// string at least needs to have '<' or '>' and space character
		if (2 <= strlen(line)) {
		    switch (*line) {
		    case '<':
			diffmanager_input_diff(runtime.diffmanager, line, runtime.currentlineFileA++);
			break;

		    case '>':
			diffmanager_input_diff(runtime.diffmanager, line, runtime.currentlineFileB++);
			break;

		    case '-':
			// we can test on line[2] without pointer failure
			if ('-'==line[1] && '-'==line[2])
			    break;	// regular split between '<' and '>', do nothing
			// else
			//   error out in default case

			// no break, fall through
		    default:
			fprintf(stderr, "%s error: can not recognise diff line \"%s\"\n", mybasename(argv[0]), line);
			exit(EXIT_FAILURE);
		    }
		}
		else {
		    fprintf(stderr, "%s error: can not recognise diff line \"%s\"\n", mybasename(argv[0]), line);
		    exit(EXIT_FAILURE);
		}
	    }
	    else
	    {
		size_t len = regerror(retval, &regex, NULL, 0);
		char *buffer = malloc(len);
		assert(buffer);
		(void) regerror (retval, &regex, buffer, len);
		fprintf(stderr, "Could not compile regular expression: %s", buffer);
		free(buffer);
		exit(EXIT_FAILURE);
	    }

	}
	pclose(splitinput);

	runtime.lineOffsetFileA += linesSplit1;
	runtime.lineOffsetFileB += linesSplit2;
    }
    free(line);
    regfree(&regex);

    // printout diff
    diffmanager_output_diff(runtime.diffmanager, outfile, 0);

    // clean up
    fclose(outfile);
    diffmanager_delete(runtime.diffmanager);


    return 0;
}

