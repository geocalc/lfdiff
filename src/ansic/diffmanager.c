/*
 * diffmanager.c
 *
 *  Created on: 10.02.2017
 *      Author: jh
 */

/*
    Management module to hold and manage two diff container

    Copyright (C) 2017  Jörg Habenicht (jh@mwerk.net)

    This file is part of lfdiff

    lfdiff is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    lfdiff is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "diffmanager.h"
#include "difflist.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define MIN(a,b)	((a)<(b)?(a):(b))
#define MAX(a,b)	((a)>(b)?(a):(b))


struct diffmanager_s *diffmanager_new(void) {
    struct diffmanager_s *manager = calloc(1, sizeof(*manager));

    manager->difflistA = diff_new();
    manager->difflistB = diff_new();

    return manager;
}

void diffmanager_delete(struct diffmanager_s *manager) {
    assert(manager);

    diff_delete(manager->difflistA);
    diff_delete(manager->difflistB);

    free(manager);
}


void diffmanager_input_diff(struct diffmanager_s *manager, char *line, long nr, enum file_e AorB) {
    assert(manager);
    assert(line);
    assert(nr>0);
    assert(AorB==A || AorB==B);

    switch (AorB) {
    case A:
	diff_add_line(manager->difflistA, nr, line);
	if (nr > manager->maxlineA)
	    manager->maxlineA = nr;
	break;

    case B:
	diff_add_line(manager->difflistB, nr, line);
	if (nr > manager->maxlineB)
	    manager->maxlineB = nr;
	break;

    default:
	fprintf(stderr, "error: program error, variable AorB=%d", AorB);
	abort();
    }
}


void diffmanager_output_diff(struct diffmanager_s *manager, FILE *output, long maxLineNr) {
    assert(manager);
    assert(output);
    assert(maxLineNr>=0);

    long lineNrA = 1, lineNrB = 1;
    struct diff_iterator *itA, *itB;

    // get maximal line A and B
    itA = diff_iterator_get_last(manager->difflistA);
    if (!itA)
	return;	// no data stored for file A? we can leave here
    itB = diff_iterator_get_last(manager->difflistB);
    if (!itB)
	return;	// no data stored for file B? we can leave here

    const long maxLineNrA = diff_get_line_nr(itA);
    const long maxLineNrB = diff_get_line_nr(itB);

//    // get minimal line A and B
//    // TODO start from last ending not from beginning
    itA = diff_iterator_get_first(manager->difflistA);
    if (!itA)
	return;	// no data stored for file A? we can leave here
    itB = diff_iterator_get_first(manager->difflistB);
    if (!itB)
	return;	// no data stored for file B? we can leave here
//
//    lineA = diff_get_line_nr(itA);
//    lineB = diff_get_line_nr(itB);

//    while (/*!nr ||*/ ((lineA <= nr) || (lineB <= nr))) {
    while (((lineNrA <= maxLineNrA) || (lineNrB <= maxLineNrB))) {

	itA = diff_iterator_get_line(manager->difflistA, lineNrA);
	itB = diff_iterator_get_line(manager->difflistB, lineNrB);
	const char *lineA = itA? diff_get_line(itA): NULL;
	const char *lineB = itB? diff_get_line(itB): NULL;



//	if (lineNrA < lineNrB) {
	if (lineA && lineB) {
	    // line changed from A to B

	    // additional check if both lines are equal
	    // should be removed if list has been optimized before
	    if (strcmp(lineA, lineB)) {

		long diffstartA = lineNrA;
		long diffstartB = lineNrB;
		long diffendA = lineNrA;
		long diffendB = lineNrB;

		diff_iterator_next(&itA);
		while (itA && diff_get_line_nr(itA) == diffendA+1) {
		    diffendA = diff_get_line_nr(itA);
		    diff_iterator_next(&itA);
		}
		diff_iterator_next(&itB);
		while (itB && diff_get_line_nr(itB) == diffendB+1) {
		    diffendB = diff_get_line_nr(itB);
		    diff_iterator_next(&itB);
		}

		if (diffstartA != diffendA)
		    fprintf(output, "%ld,%ld", diffstartA, diffendA);
		else
		    fprintf(output, "%ld", diffstartA);
		if (diffstartB != diffendB)
		    fprintf(output, "c%ld,%ld\n", diffstartB, diffendB);
		else
		    fprintf(output, "c%ld\n", diffstartB);

		for (lineNrA=diffstartA; lineNrA<=diffendA; lineNrA++) {
		    itA = diff_iterator_get_line(manager->difflistA, lineNrA);
		    lineA = diff_get_line(itA);
		    fprintf(output, "< %s", lineA);
		}
		fprintf(output, "---\n");
		for (lineNrB=diffstartB; lineNrB<=diffendB; lineNrB++) {
		    itB = diff_iterator_get_line(manager->difflistB, lineNrB);
		    lineB = diff_get_line(itB);
		    fprintf(output, "> %s", lineB);
		}
	    }
	}
	else if (lineA) {
	    // line deleted from A to B

	    long diffstart = lineNrA;
	    long diffend = lineNrA;
	    diff_iterator_next(&itA);
	    while (itA && diff_get_line_nr(itA) == diffend+1) {
		diffend = diff_get_line_nr(itA);
		diff_iterator_next(&itA);
	    }

	    // correct lineNrB calculation
	    lineNrB--;

	    // now we have start line number and end line number
	    // printout the diff lines
	    if (diffstart != diffend)
		fprintf(output, "%ld,%ldd%ld\n", diffstart, diffend, lineNrB);
	    else
		fprintf(output, "%ldd%ld\n", diffstart, lineNrB);

	    for (lineNrA=diffstart; lineNrA<=diffend; lineNrA++) {
		itA = diff_iterator_get_line(manager->difflistA, lineNrA);
		lineA = diff_get_line(itA);
		fprintf(output, "< %s", lineA);
	    }

	}
	else if (lineB) {
	    // line added from A to B

	    long diffstart = lineNrB;
	    long diffend = lineNrB;
	    diff_iterator_next(&itB);
	    while (itB && diff_get_line_nr(itB) == diffend+1) {
		diffend = diff_get_line_nr(itB);
		diff_iterator_next(&itB);
	    }
//	    for (itB = diff_iterator_get_line(manager->difflistB, ++diffend);
//		    itB;
//		    itB = diff_iterator_get_line(manager->difflistB, ++diffend)) {
//		// empty
//	    }
//	    diffend--;

	    // correct lineNrA calculation
	    lineNrA--;

	    // now we have start line number and end line number
	    // printout the diff lines
	    if (diffstart != diffend)
		fprintf(output, "%lda%ld,%ld\n", lineNrA, diffstart, diffend);
	    else
		fprintf(output, "%lda%ld\n", lineNrA, diffstart);

	    for (lineNrB=diffstart; lineNrB<=diffend; lineNrB++) {
		itB = diff_iterator_get_line(manager->difflistB, lineNrB);
		lineB = diff_get_line(itB);
		fprintf(output, "> %s", lineB);
	    }

	}
	else {
	    // both lines same
	}

	lineNrA++;
	lineNrB++;
    }
}

void diffmanager_delete_diff(struct diffmanager_s *manager, long maxLineNr) {
    assert(manager);
    assert(maxLineNr>=0);

}

long diffmanager_get_max_common_input_line(struct diffmanager_s *manager) {
    assert(manager);

    return MIN(manager->maxlineA, manager->maxlineB);
}


