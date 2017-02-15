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


void diffmanager_input_diff(struct diffmanager_s *manager, const char *line, long nr) {
    assert(manager);
    assert(line);
    assert(nr>0);

    if (' ' != line[1]) {
	fprintf(stderr, "error: can not recognise diff line \"%s\"\n", line);
	abort();
    }

    switch (*line) {
    case '<':
	diff_add_line(manager->difflistA, nr, strdup(&line[2]));
	if (nr > manager->maxlineA)
	    manager->maxlineA = nr;
	break;

    case '>':
	diff_add_line(manager->difflistB, nr, strdup(&line[2]));
	if (nr > manager->maxlineB)
	    manager->maxlineB = nr;
	break;

    default:
	fprintf(stderr, "error: can not recognise diff line \"%s\"\n", line);
	abort();
    }
}


void diffmanager_output_diff(struct diffmanager_s *manager, FILE *output, long maxLineNr) {
    // remove doublettes before pushing them out
    diffmanager_remove_common_lines(manager, maxLineNr);

    diffmanager_print_diff_to_stream(manager, output, maxLineNr);

    // delete all lines up to this point
//    diffmanager_delete_diff(manager, maxLineNr);
}

void diffmanager_print_diff_to_stream(struct diffmanager_s *manager, FILE *output, long maxLineNr) {
    assert(manager);
    assert(output);
    assert(maxLineNr>=0);

    /* Algorithm:
     * 0) start the current line numbers A and B with 0
     * 1) go to the first entry in container A and B
     * 2) get the line number of first entry in container A and B
     * 3) get the difference to the current line numbers A and B
     * 4) take the minimal difference
     * 5) advance in parallel current line numbers A and B by the min difference
     * 6) evaluate if both container A and B got lines stored by the current line number
     * 7) act accordingly by the result
     * 7a) if container B got no line, then line has been deleted from file A
     * 7b) if container A got no line, then line has been added to file B
     * 7c) if container A and B got line, then the line has been changed from A to B
     *
     *
     */
    struct diff_iterator *itA, *itB;

    // get maximal line A and B
    itA = diff_iterator_get_last(manager->difflistA);
    if (!itA)
	return;	// no data stored for file A? we can leave here
    itB = diff_iterator_get_last(manager->difflistB);
    if (!itB)
	return;	// no data stored for file B? we can leave here

    const long maxLineNrA = itA? diff_get_line_nr(itA): 0;
    const long maxLineNrB = itB? diff_get_line_nr(itB): 0;


    itA = diff_iterator_get_current(manager->difflistA);
    if (!itA)
	itA = diff_iterator_get_first(manager->difflistA);
    diff_iterator_go_equal_after_line(&itA, manager->outputLineNrA);
    itB = diff_iterator_get_current(manager->difflistB);
    if (!itB)
	itB = diff_iterator_get_first(manager->difflistB);
    diff_iterator_go_equal_after_line(&itB, manager->outputLineNrB);
    // itA und itB stehen jetzt auf oder hinter der Zeilennummber von outputLineNr

    // beide Variablen gleichauf ziehen und Zeilen evaluieren.


    while (((manager->outputLineNrA <= maxLineNrA) || (manager->outputLineNrB <= maxLineNrB))
	    && (!maxLineNr || MIN(manager->outputLineNrA,manager->outputLineNrB) < maxLineNr)) {

	// find the next line with content
	{
	    // calculate the offset to the next block of lines
	    long minimalLineNrOffsetToNextBlock = 0;
	    if (itA && itB) {
		const long nextlineA = diff_get_line_nr(itA);
		const long nextlineB = diff_get_line_nr(itB);
		const long difflineA = nextlineA - manager->outputLineNrA;
		const long difflineB = nextlineB - manager->outputLineNrB;
		minimalLineNrOffsetToNextBlock = MIN(difflineA,difflineB);
	    }
	    else if (itA) {
		// iterator B at end
		const long nextlineA = diff_get_line_nr(itA);
		const long difflineA = nextlineA - manager->outputLineNrA;
		minimalLineNrOffsetToNextBlock = difflineA;
	    }
	    else if (itB) {
		// iterator A at end
		const long nextlineB = diff_get_line_nr(itB);
		const long difflineB = nextlineB - manager->outputLineNrB;
		minimalLineNrOffsetToNextBlock = difflineB;
	    }
	    else {
		// both iterators at the end
		// exit from while() loop
		break;
	    }
	    manager->outputLineNrA += minimalLineNrOffsetToNextBlock;
	    manager->outputLineNrB += minimalLineNrOffsetToNextBlock;
	}

	const long iterLineNrA = itA? diff_get_line_nr(itA): 0;
	const long iterLineNrB = itB? diff_get_line_nr(itB): 0;
	const long diffAB = manager->outputLineNrB - manager->outputLineNrA;
	const long virtualLineNrA = iterLineNrA + diffAB;

	const char *lineA = itA? diff_get_line(itA): NULL;
	const char *lineB = itB? diff_get_line(itB): NULL;

	if (itA && itB && virtualLineNrA == iterLineNrB) {
	    // line changed from A to B

	    // additional check if both lines are equal
	    // should be removed if list has been optimized before
	    if (strcmp(lineA, lineB)) {

		long diffstartA = manager->outputLineNrA;
		long diffstartB = manager->outputLineNrB;
		long diffendA = manager->outputLineNrA;
		long diffendB = manager->outputLineNrB;

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

		for (manager->outputLineNrA=diffstartA; manager->outputLineNrA<=diffendA; manager->outputLineNrA++) {
		    itA = diff_iterator_get_line(manager->difflistA, manager->outputLineNrA);
		    lineA = diff_get_line(itA);
		    fprintf(output, "< %s", lineA);
		}
		fprintf(output, "---\n");
		for (manager->outputLineNrB=diffstartB; manager->outputLineNrB<=diffendB; manager->outputLineNrB++) {
		    itB = diff_iterator_get_line(manager->difflistB, manager->outputLineNrB);
		    lineB = diff_get_line(itB);
		    fprintf(output, "> %s", lineB);
		}
	    }

	    // advance both to the next line block
	    diff_iterator_next(&itA);
	    diff_iterator_next(&itB);
	    // correct lineNrA + B calculation
	    manager->outputLineNrA--;
	    manager->outputLineNrB--;
	}
	else if ((itA && !itB) || (itA && itB && virtualLineNrA < iterLineNrB)) {
	    // line deleted from A to B

	    long diffstart = manager->outputLineNrA;
	    long diffend = manager->outputLineNrA;
	    struct diff_iterator *itLine = itA;
	    diff_iterator_next(&itLine);
	    while (itLine && diff_get_line_nr(itLine) == diffend+1) {
		diffend = diff_get_line_nr(itLine);
		diff_iterator_next(&itLine);
	    }

	    // correct lineNrB calculation
	    manager->outputLineNrB--;

	    // now we have start line number and end line number
	    // printout the diff lines
	    if (diffstart != diffend)
		fprintf(output, "%ld,%ldd%ld\n", diffstart, diffend, manager->outputLineNrB);
	    else
		fprintf(output, "%ldd%ld\n", diffstart, manager->outputLineNrB);

	    for (manager->outputLineNrA=diffstart; manager->outputLineNrA<=diffend; manager->outputLineNrA++) {
//		itA = diff_iterator_get_line(manager->difflistA, lineNrA);
		lineA = diff_get_line(itA);
		fprintf(output, "< %s", lineA);
		diff_iterator_next(&itA);
	    }

	    // advance A to the next line block
//	    diff_iterator_next(&itA);
	    // correct lineNrA calculation
	    manager->outputLineNrA--;
	}
	else if ((!itA && itB) || (itA && itB && virtualLineNrA > iterLineNrB)) {
	    // line added from A to B

	    long diffstart = manager->outputLineNrB;
	    long diffend = manager->outputLineNrB;
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
	    manager->outputLineNrA--;

	    // now we have start line number and end line number
	    // printout the diff lines
	    if (diffstart != diffend)
		fprintf(output, "%lda%ld,%ld\n", manager->outputLineNrA, diffstart, diffend);
	    else
		fprintf(output, "%lda%ld\n", manager->outputLineNrA, diffstart);

	    for (manager->outputLineNrB=diffstart; manager->outputLineNrB<=diffend; manager->outputLineNrB++) {
		itB = diff_iterator_get_line(manager->difflistB, manager->outputLineNrB);
		lineB = diff_get_line(itB);
		fprintf(output, "> %s", lineB);
	    }

	    // advance B to the next line block
	    diff_iterator_next(&itB);
	    // correct lineNrB calculation
	    manager->outputLineNrB--;
	}
	else {
	    // both lines same
	}

	manager->outputLineNrA++;
	manager->outputLineNrB++;

    }
}

void diffmanager_delete_diff(struct diffmanager_s *manager, long maxLineNr) {
    assert(manager);
    assert(maxLineNr>=0);

    struct diff_iterator *it;
    it = diff_iterator_get_first(manager->difflistA);
    if (it) {
	long lineNr = diff_get_line_nr(it);
	while (lineNr <= maxLineNr) {
	    diff_remove_line(manager->difflistA,lineNr);
	    it = diff_iterator_get_first(manager->difflistA);
	    if (!it)
		break;
	    lineNr = diff_get_line_nr(it);
	}
    }

    it = diff_iterator_get_first(manager->difflistB);
    if (it) {
	long lineNr = diff_get_line_nr(it);
	while (lineNr <= maxLineNr) {
	    diff_remove_line(manager->difflistB,lineNr);
	    it = diff_iterator_get_first(manager->difflistB);
	    if (!it)
		break;
	    lineNr = diff_get_line_nr(it);
	}
    }

}

long diffmanager_get_max_common_input_line(struct diffmanager_s *manager) {
    assert(manager);

    return MIN(manager->maxlineA, manager->maxlineB);
}

void diffmanager_remove_common_lines(struct diffmanager_s *manager, long maxLineNr) {
    assert(manager);
    assert(maxLineNr>=0);

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

    long lineNrA = 1, lineNrB = 1;

    while (lineNrA <= maxLineNrA
	    && lineNrB <= maxLineNrB
	    && (!maxLineNr || MAX(lineNrA,lineNrB) <= maxLineNr)
	   ) {
	itA = diff_iterator_get_line(manager->difflistA, lineNrA);
	itB = diff_iterator_get_line(manager->difflistB, lineNrB);

	if (itA && itB) {
	    // both lines defined, maybe the same
	    const char *lineA = diff_get_line(itA);
	    const char *lineB = diff_get_line(itB);
	    if (!strcmp(lineA, lineB)) {
		// lines are same
		// remove them
		diff_remove_line(manager->difflistA, lineNrA);
		diff_remove_line(manager->difflistB, lineNrB);
	    }
	    lineNrA++;
	    lineNrB++;
	}
	else if (itA) {
	    // file A defined, file B not. This line gets removed from file A.
	    // try to find common lines again.
	    lineNrA++;
	}
	else if (itB) {
	    // file B defined, file A not. This line gets removed from file B.
	    // try to find common lines again.
	    lineNrB++;
	}
	else {
	    // both lines undefined (i.e. same)
	    lineNrA++;
	    lineNrB++;
	}

    }
}


