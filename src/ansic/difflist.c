/*
 * difflist.c
 *
 *  Created on: 09.02.2017
 *      Author: jh
 */


#include "difflist.h"

#include <stdlib.h>
#include <assert.h>

struct diff_list_s *diff_new(void) {
    struct diff_list_s *list = calloc(1, sizeof(*list));
    assert(list);
    TAILQ_INIT(&list->head);
    list->tqh_current = NULL;

    return list;
}

void diff_delete(struct diff_list_s *list) {
    assert(list);

    struct diff_iterator *entry;
    while ( NULL != (entry = TAILQ_FIRST(&list->head)) ) {
	free(entry->line);
	TAILQ_REMOVE(&list->head, entry, entries);
    }
}


void diff_add_line(struct diff_list_s *list, int n, char *line) {
    assert(list);

    struct diff_iterator *knot = calloc(1, sizeof(*knot));
    assert(knot);
    knot->line = line;
    knot->n = n;

    struct diff_iterator *iterator;
    if (NULL != (iterator = list->tqh_current)) {
	diff_iterator_go_after_line(&iterator, n);

	if (iterator) {
	    // somewhere in the list.
	    assert(iterator->n != n);	// line is saved already
	    // (iterator->n is not < n) AND (iterator-> != n) ==> iterator->n > n
	    // insert line before current iterator
	    TAILQ_INSERT_BEFORE(iterator, knot, entries);
	}
	else {
	    // beginning or end of list reached
	    // test on first entry
	    if (n > TAILQ_FIRST(&list->head)->n) {
		TAILQ_INSERT_TAIL(&list->head, knot, entries);
	    }
	    else {
		TAILQ_INSERT_HEAD(&list->head, knot, entries);
	    }
	}
    }
    else {
	// empty list, first entry
	TAILQ_INSERT_HEAD(&list->head, knot, entries);
    }
    list->tqh_current = knot;
}

//void diff_add_line_i(struct diff_iterator **iterator, int n, char *line) {
//    assert(iterator);
//    assert(*iterator);
//
//    while ((*iterator)->n < n) {
//	diff_iterator_next(iterator);
//    }
//}

void diff_remove_line(struct diff_list_s *list, int n) {
    assert(list);

    // remember to correct current_iterator
    struct diff_iterator *iterator;
//    if (NULL != (iterator = TAILQ_FIRST(&list->head))) {
    if (NULL != (iterator = list->tqh_current)) {
	diff_iterator_go_after_line(&iterator, n);
	if (iterator->n == n) {
	    // correct current pointer to previous or next (whatever is valid)
	    if ((list->tqh_current = TAILQ_NEXT(iterator, entries))) {
	    }
	    else {
		list->tqh_current = TAILQ_PREV(iterator, listhead, entries);
	    }

	    TAILQ_REMOVE(&list->head, iterator, entries);
	}
    }
}

//void diff_remove_line_i(struct diff_iterator **iterator, int n) {
//    assert(iterator);
//    assert(*iterator);
//
//}

struct diff_iterator *diff_get_first(struct diff_list_s *list) {
    assert(list);

    return TAILQ_FIRST(&list->head);
}

struct diff_iterator *diff_get_last(struct diff_list_s *list) {
    assert(list);

    return TAILQ_LAST(&list->head, listhead);
}

struct diff_iterator *diff_get_current(struct diff_list_s *list) {
    assert(list);

    return list->tqh_current;
}

void diff_next(struct diff_list_s *list) {
    assert(list);

    if (list->tqh_current)
	diff_iterator_next(&list->tqh_current);
}

void diff_previous(struct diff_list_s *list) {
    assert(list);

    if (list->tqh_current)
	diff_iterator_previous(&list->tqh_current);
}

void diff_iterator_next(struct diff_iterator **iterator) {
    assert(iterator);
    assert(*iterator);

    *iterator = TAILQ_NEXT(*iterator, entries);
}

void diff_iterator_previous(struct diff_iterator **iterator) {
    assert(iterator);
    assert(*iterator);

    *iterator = TAILQ_PREV(*iterator, listhead, entries);
}

//struct diff_iterator *diff_go_before_line(struct diff_list_s *list, int n) {
//    assert(list);
//    assert(n>0);
//
//	while (iterator && iterator->n < n) {
//	    diff_iterator_next(&iterator);
//	}
//}
//
//struct diff_iterator *diff_go_after_line(struct diff_list_s *list, int n) {
//    assert(list);
//    assert(n>0);
//
//
//    while (iterator && iterator->n < n) {
//	diff_iterator_next(&iterator);
//    }
//}

void diff_iterator_go_before_line(struct diff_iterator **iterator, int n) {
    assert(iterator);
    assert(*iterator);

    while (*iterator && (*iterator)->n < n) {
	diff_iterator_next(iterator);
    }
    while (*iterator && (*iterator)->n > n) {
	diff_iterator_previous(iterator);
    }
}

void diff_iterator_go_after_line(struct diff_iterator **iterator, int n) {
    assert(iterator);
    assert(*iterator);

    while (*iterator && (*iterator)->n > n) {
	diff_iterator_previous(iterator);
    }
    while (*iterator && (*iterator)->n < n) {
	diff_iterator_next(iterator);
    }
}



