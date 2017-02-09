/*
 * difflist.c
 *
 *  Created on: 09.02.2017
 *      Author: jh
 */


#include "difflist.h"


struct diff_list_s *diff_new(void) {

}

void diff_delete(struct diff_list_s *list) {

}


void diff_add_line(struct diff_list_s *list, int n, char *line) {

}

void diff_remove_line(struct diff_list_s *list, int n) {

}

struct diff_iterator *diff_get_iterator(struct diff_list_s *list) {

}

void diff_iterator_next(struct diff_iterator **iterator) {

}

void diff_iterator_previous(struct diff_iterator **iterator) {

}

struct diff_iterator *diff_go_before_line(struct diff_list_s *list, int n) {

}

struct diff_iterator *diff_go_after_line(struct diff_list_s *list, int n) {

}

