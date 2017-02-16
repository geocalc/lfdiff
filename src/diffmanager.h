/*
 * diffmanager.h
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

#ifndef SRC_ANSIC_DIFFMANAGER_H_
#define SRC_ANSIC_DIFFMANAGER_H_

#include <stdio.h>


struct diff_list_s;

struct diffmanager_s {
    struct diff_list_s *difflistA;
    struct diff_list_s *difflistB;
    long maxlineA;
    long maxlineB;
    long outputLineNrA;
    long outputLineNrB;
    long removeLineNrA;
    long removeLineNrB;
};


struct diffmanager_s *diffmanager_new(void);
void diffmanager_delete(struct diffmanager_s *manager);

/** put diff line into storage.
 * The storage is memory optimized on the way, i.e. double entries are going
 * to be deleted during this input.
 *
 * @param manager: diffmanager handler
 * @param line: line of file A or B
 * @param nr: line number
 * @param AorB: line belongs to file A or B
 */
void diffmanager_input_diff(struct diffmanager_s *manager, const char *line, long nr);

/** output diff up to line maxLineNr to stream output.
 * note: calls diffmanager_remove_common_lines() and diffmanager_delete_diff() during execution
 *
 * @param manager: diffmanager handler
 * @param output: stream to print to
 * @param maxLineNr: max line number, up to which number printed. maxLineNr=0: print all
 */
void diffmanager_output_diff(struct diffmanager_s *manager, FILE *output, long maxLineNr);

/** free memory up to line nr.
 *
 * @param manager: diffmanager handler
 * @param maxLineNr: max line number, up to which number printed. maxNr=0: delete all
 */
void diffmanager_delete_diff(struct diffmanager_s *manager, long maxLineNr);

long diffmanager_get_max_common_input_line(struct diffmanager_s *manager);

/** get the difference of line numbers which point to the same
 * data in file A and B
 * @return: line difference
 */
long diffmanager_get_linediff_A_B(struct diffmanager_s *manager);

void diffmanager_remove_common_lines(struct diffmanager_s *manager, long maxLineNr);

void diffmanager_print_diff_to_stream(struct diffmanager_s *manager, FILE *output, long maxLineNr);

#endif /* SRC_ANSIC_DIFFMANAGER_H_ */
