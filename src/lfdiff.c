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
#include <pthread.h>


#define MIN(a,b)	((a)<(b)?(a):(b))
#define MAX(a,b)	((a)>(b)?(a):(b))

#define REGEX_MATCHBUFFER_LEN	6

#define PRINT_VERBOSE(stream, text, ...) \
    do { \
	if (config.be_verbose) \
	    fprintf((stream), (text), ##__VA_ARGS__); \
    } while (0)


static const long long int default_splitsize = 2l*1024*1024*1024; // 2GB

enum {
    FILE_A = 0,
    FILE_B = 1,
    MAX_FILE
};

enum {
    PIPE_READ_CHANNEL = 0,
    PIPE_WRITE_CHANNEL = 1,
    MAX_PIPE_CHANNEL
};

const char *mybasename(const char *path) {
    const char *retval = rindex(path, '/');

    return retval? ++retval: path;
}

struct config {
    long long int splitsize;
    int be_verbose;
    const char *outfilename;
    const char *filename[MAX_FILE];
} config = {0};


struct thread_copy_buffer_args {
    FILE *infile;
    FILE *outfile;
    long long int max_copy_bytes;
    long long int lines_copied;
};


struct runtime {
    FILE *infile[MAX_FILE];
    const char *argv0;
    unsigned long currentline[MAX_FILE];
    unsigned long lineOffset[MAX_FILE];
    struct diffmanager_s *diffmanager;
    pid_t pid;
    struct thread_copy_buffer_args threadbuffer[MAX_FILE];
    pthread_t threads[MAX_FILE];
} runtime = {0};



void usage(const char *argv0) {

    fprintf(stderr, "usage: %s [-h] [-V] [-v] [-o OUTPUT] [-s SPLITSIZE] [--] INPUT1 INPUT2\n"
	    "\t-h: print this help\n"
	    "\t-V: print version\n"
	    "\t-o: write output to OUTFILE instead of stdout\n"
	    "\t-s: split INPUT* into SPLITSIZE chunks. SPLITSIZE can be appended k,kB,M,MB,G,GB to multiply 1024, 1024², 1024³. (default: %lld byte)\n"
	    "\t-v: be verbose\n"
	    "\t--: end option parsing. Next argument is expected to describe INPUT1\n"
	    "INPUT1 and INPUT2 can be any file, socket or pipe which one can read from.\n"
	    "Use '-' to read from standard input.\n"
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


void *thread_copy_infile_to_outpipe(void *args) {
    assert(args);

    struct thread_copy_buffer_args *myargs = (struct thread_copy_buffer_args *) args;
    assert(myargs->infile);
    assert(myargs->outfile);
    assert(myargs->max_copy_bytes >= 0);

    long long int copy_bytes = 0;
    char *line = NULL;
    size_t n;

    while (!feof(myargs->infile) && copy_bytes < myargs->max_copy_bytes) {

	int retval = getline(&line, &n, myargs->infile);
	if (1 > retval) {
	    if (errno) {
		fprintf(stderr, "error: reading from input file: %s\n", strerror(errno));
	    }
	    else if (feof(myargs->infile)) {
		// we have reached the end of input.
		// why didn't we notice earlier at the top of the loop?
		break;
	    }
	    else {
		fprintf(stderr, "error: no valid input from input file\n");
	    }
	    abort();
	}

	retval = fputs(line, myargs->outfile);
	if (EOF==retval) {
	    fprintf(stderr, "error: writing to output buffer: %s\n", strerror(ferror(myargs->outfile)));
	    abort();
	}
	copy_bytes += strlen(line);
	myargs->lines_copied++;

    }

    free(line);

    // close this over here, so the external program gets EOF and is able to
    // close its output stream itself. Which is recognized by this program in
    // its receiving data loop where we evaluate EOF
    int retval = fclose(myargs->outfile);
    if (retval) {
	fprintf(stderr, "error: can not close stream: %s\n", strerror(errno));
	abort();
    }
    myargs->outfile = NULL;

    return args;
}


FILE *diff_open() {

    static const int fdbuffsize = sizeof("/dev/fd/")+9+1;	// = strlen("/dev/fd/") + '\0' + strlen(MAX_INT) + '\0' = sizeof("/dev/fd/")+9+1
    int inputpipe[MAX_FILE][MAX_PIPE_CHANNEL];
    int outputpipe[MAX_PIPE_CHANNEL];
    char fdbuff[MAX_FILE][fdbuffsize];
    int retval;
    int i;

    for (i=0; i<MAX_FILE; i++) {
	retval = pipe(inputpipe[i]);
	if (-1 == retval) {
	    fprintf(stderr, "error: can not create pipe: %s\n", strerror(errno));
	    abort();
	}
    }

    retval = pipe(outputpipe);
    if (-1 == retval) {
	fprintf(stderr, "error: can not create pipe: %s\n", strerror(errno));
	abort();
    }

    runtime.pid = fork();
    if (-1 == runtime.pid) {
	fprintf(stderr, "error: can not fork: %s\n", strerror(errno));
	abort();
    }
    if (0 == runtime.pid) {
	// this is child task

	// close reading channel of output pipe
	close(outputpipe[PIPE_READ_CHANNEL]);

	// set "diff" output channel to pipe, omit "diff" input and error channel
	dup2(outputpipe[PIPE_WRITE_CHANNEL], STDOUT_FILENO);
	close(outputpipe[PIPE_WRITE_CHANNEL]);	// close pipe file handle, we already got stdout

	for (i=0; i<MAX_FILE; i++) {
	    // close writing channel of input pipe
	    close(inputpipe[i][PIPE_WRITE_CHANNEL]);

	    // formulate input file descriptor for extern "diff" program
	    retval = snprintf(fdbuff[i], sizeof(fdbuff[i]), "/dev/fd/%d", inputpipe[i][PIPE_READ_CHANNEL]);
	    if (-1 >= retval) {
		fprintf(stderr, "error: can not print to string: %s\n", strerror(errno));
		abort();
	    }
	    if (sizeof(fdbuff[i]) <= retval) {
		fprintf(stderr, "error: can not print to buffer, number too large: %d", inputpipe[i][PIPE_READ_CHANNEL]);
		abort();
	    }
	}

	// call "diff" program with file descriptors in /dev/fd/
	execlp("diff", "diff", fdbuff[FILE_A], fdbuff[FILE_B], (char *) NULL);
	fprintf(stderr, "error: can not exec: %s\n", strerror(errno));
	abort();
    }

    // this is parent task

    // close writing channel of output pipe
    close(outputpipe[PIPE_WRITE_CHANNEL]);

    for (i=0; i<MAX_FILE; i++) {
	// close reading channel of input pipe
	close(inputpipe[i][PIPE_READ_CHANNEL]);

	// create FILE* descriptor out of file descriptor
	runtime.threadbuffer[i].outfile = fdopen(inputpipe[i][PIPE_WRITE_CHANNEL], "w");
	if (NULL == runtime.threadbuffer[i].outfile) {
	    fprintf(stderr, "error: can not open file descriptor: %s\n", strerror(errno));
	    abort();
	}

	// start the feeding thread
	retval = pthread_create(&runtime.threads[i], NULL, thread_copy_infile_to_outpipe, &runtime.threadbuffer[i]);
	if (retval)
	{
	    fprintf(stderr, "error: can not create thread: %s\n", strerror(errno));
	    abort();
	}
    }

    FILE *ret = fdopen(outputpipe[PIPE_READ_CHANNEL], "r");
    if (NULL == ret) {
	fprintf(stderr, "error: can not open file descriptor: %s\n", strerror(errno));
	abort();
    }

    return ret;
}


int diff_close(FILE *file) {

    int retval;
    int i;

    /* wait for those threads */
    for (i=0; i<MAX_FILE; i++)
    {
	retval = pthread_join(runtime.threads[i], NULL);
	if (retval)
	{
	    fprintf(stderr, "error: can not join thread %d: %s\n", i, strerror(errno));
	    abort();
	}
	runtime.threads[i] = 0;

	// stream is already closed in thread thread_copy_infile_to_outpipe()
    }

    retval = fclose(file);
    if (retval) {
	fprintf(stderr, "error: can not close stream: %s\n", strerror(errno));
	abort();
    }

    int wstatus;
    retval = waitpid(runtime.pid, &wstatus, 0);
    if (-1 == retval) {
	fprintf(stderr, "error: can not wait for child process: %s\n", strerror(errno));
	abort();
    }
    if (WIFEXITED(wstatus)) {
	if (0 == WEXITSTATUS(wstatus)) {
	    // all ok
	    // input files are the same
	}
	else if (1 == WEXITSTATUS(wstatus)) {
	    // all ok
	    // input files differ
	}
	else {
	    // program did not exit normally
	    fprintf(stderr, "error: abnormal exit of diff, return value %d\n", WEXITSTATUS(wstatus));
	    abort();
	}
    }
    else {
	// program was killed by signal
	fprintf(stderr, "error: abnormal exit of diff\n");
	abort();
    }

    runtime.pid = 0;

    return 0;
}



int main(int argc, char **argv) {
    int retval;
    regex_t regex;

    runtime.argv0 = argv[0];
    config.splitsize = default_splitsize;

    int opt;

    while ((opt = getopt(argc, argv, "hVvo:s:")) != -1)
    {
	if ('-' == opt)
	    /* '--' option, end processing */
	    break;

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
		abort();
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
		config.splitsize = atoll(buffer);
		if (matchptr[2].rm_so != matchptr[2].rm_eo) {
		    myregexbuffercpy(buffer, optarg, matchptr[2].rm_so, matchptr[2].rm_eo, bufferlen);
		    switch (*buffer) {
		    case 'G':
			if (config.splitsize >= LLONG_MAX/1024) {
			    fprintf(stderr, "Integer overflow error parsing option -s '%s'\n", optarg);
			    exit(EXIT_FAILURE);
			}
			config.splitsize *= 1024;
			// no break, fall through
		    case 'M':
			if (config.splitsize >= LLONG_MAX/1024) {
			    fprintf(stderr, "Integer overflow error parsing option -s '%s'\n", optarg);
			    exit(EXIT_FAILURE);
			}
			config.splitsize *= 1024;
			// no break, fall through
		    case 'k':
			if (config.splitsize >= LLONG_MAX/1024) {
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
		abort();
	    }
	    regfree(&regex);
	}
	    break;
	default: /* '?' */
	    usage(argv[0]);
	    exit(EXIT_FAILURE);
	}
    }


    int i;
    for (i=0; i<MAX_FILE; i++) {
	if (optind >= argc) {
	    fprintf(stderr, "INPUT%d missing\n", i);
	    usage(argv[0]);
	    exit(EXIT_FAILURE);
	}
	config.filename[i] = argv[optind++];
    }


    if (!strcmp(config.filename[FILE_A], config.filename[FILE_B])) {
	// input is twice the same file name or twice stdin
	fprintf(stderr, "no need to compare same files\n");
	exit(EXIT_FAILURE);
    }


    for (i=0; i<MAX_FILE; i++) {
	if (strcmp(config.filename[i], "-")) {
	    // file is regular
	    runtime.infile[i] = fopen(config.filename[i], "r");
	    if (NULL == runtime.infile[i]) {
		fprintf(stderr, "error: could not open input file '%s': %s\n", config.filename[i], strerror(errno));
		exit(EXIT_FAILURE);
	    }
	}
	else {
	    // file is stdin
	    runtime.infile[i] = stdin;
	}
    }


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
	abort();
    }

    FILE *outfile = config.outfilename?fopen(config.outfilename, "w"):stdout;
    if (NULL == outfile) {
	fprintf(stderr, "error: could not open output file '%s': %s\n", config.outfilename, strerror(errno));
	exit(EXIT_FAILURE);
    }


    int iteration;
    for (iteration=1; iteration; iteration++) { // exit loop if both split files return 0 bytes
	FILE *splitinput;

	if (feof(runtime.infile[FILE_A]) && feof(runtime.infile[FILE_B]))
	    break;

	memset(&runtime.threadbuffer, 0, sizeof(runtime.threadbuffer));
	for (i=0; i<MAX_FILE; i++) {
	    runtime.threadbuffer[i].infile = runtime.infile[i];
	    runtime.threadbuffer[i].max_copy_bytes = config.splitsize;
	}

	PRINT_VERBOSE(stderr, "diff input %d\n", iteration);
	splitinput = diff_open();
	while (!feof(splitinput)) {
	    retval = getline(&line, &n, splitinput);
	    if (1 > retval) {
		if (errno) {
		    fprintf(stderr, "error: reading from \"diff\" programm: %s\n", strerror(errno));
		}
		else if (feof(splitinput)) {
		    // we have reached the end of input.
		    // why didn't we notice earlier at the top of the loop?
		    break;
		}
		else {
		    fprintf(stderr, "error: no valid input from \"diff\" program\n");
		}
		abort();
	    }
	    // note: line includes a newline character at the end

	    regmatch_t matchptr[REGEX_MATCHBUFFER_LEN];
	    retval = regexec(&regex, line, REGEX_MATCHBUFFER_LEN, matchptr, 0);
	    if( !retval )
	    {
		// Match

		static const int bufferlen = 32;
		static const int actionlen = 2;
		long lines[MAX_FILE];
		char buffer[bufferlen];
		char action[actionlen];

		// extract data
		myregexbuffercpy(buffer, line, matchptr[1].rm_so, matchptr[1].rm_eo, bufferlen);
		lines[FILE_A] = atol(buffer);
		myregexbuffercpy(buffer, line, matchptr[4].rm_so, matchptr[4].rm_eo, bufferlen);
		lines[FILE_B] = atol(buffer);
		myregexbuffercpy(action, line, matchptr[3].rm_so, matchptr[3].rm_eo, actionlen);

		for (i=0; i<MAX_FILE; i++)
		    runtime.currentline[i] = lines[i] + runtime.lineOffset[i];

		// write out and free() decoded and optimized differentials to
		// "outfile" to reduce the amount of memory used
		// NOTE: currently disabled due to a bug in the diff stream generation
//		const long diffAB = diffmanager_get_linediff_A_B(runtime.diffmanager);
//		long min_common_lines = (
//			diffAB>0?
//			MIN(runtime.currentline[FILE_A],runtime.currentline[FILE_B]-diffAB):
//			MIN(runtime.currentline[FILE_A]+diffAB,runtime.currentline[FILE_B])
//			)-1;
//		PRINT_VERBOSE(stderr, "diff output <= line %ld\n", min_common_lines);
//		diffmanager_output_diff(runtime.diffmanager, outfile, min_common_lines);
	    }
	    else if( retval == REG_NOMATCH )
	    {
		// No match on diff header

		// string at least needs to have '<' or '>' and space character
		if (2 <= strlen(line)) {
		    switch (*line) {
		    case '<':
			diffmanager_input_diff(runtime.diffmanager, line, runtime.currentline[FILE_A]++);
			break;

		    case '>':
			diffmanager_input_diff(runtime.diffmanager, line, runtime.currentline[FILE_B]++);
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
			abort();
		    }
		}
		else {
		    fprintf(stderr, "%s error: can not recognise diff line \"%s\"\n", mybasename(argv[0]), line);
		    abort();
		}
	    }
	    else
	    {
		size_t len = regerror(retval, &regex, NULL, 0);
		char *buffer = malloc(len);
		assert(buffer);
		(void) regerror (retval, &regex, buffer, len);
		fprintf(stderr, "Could not compile regular expression: %s", buffer);
		abort();
	    }

	}
	diff_close(splitinput);

	for (i=0; i<MAX_FILE; i++)
	    runtime.lineOffset[i] += runtime.threadbuffer[i].lines_copied;
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

