/*
 * difflist.h
 *
 *  Created on: 09.02.2017
 *      Author: jh
 */

#ifndef SRC_ANSIC_DIFFLIST_H_
#define SRC_ANSIC_DIFFLIST_H_

#include <sys/queue.h>

//struct diff_item_s
//{
//    int n;		// line number
//    const char *line;	// diff string
//};

struct diff_list_s
{
    TAILQ_HEAD(listhead, diff_iterator) head;	/* Linked list head */
    struct diff_iterator *tqh_current;		/* pointer to current element */
};

struct diff_iterator
{
    TAILQ_ENTRY(diff_iterator) entries;		/* Linked list prev./next entry */
    int n;		// line number
    char *line;	// diff string
};



struct diff_list_s *diff_new(void);
void diff_delete(struct diff_list_s *list);
void diff_add_line(struct diff_list_s *list, int n, char *line);
void diff_add_line_i(struct diff_iterator **iterator, int n, char *line);
void diff_remove_line(struct diff_list_s *list, int n);
void diff_remove_line_i(struct diff_iterator **iterator, int n);
struct diff_iterator *diff_get_first(struct diff_list_s *list);
struct diff_iterator *diff_get_last(struct diff_list_s *list);
struct diff_iterator *diff_get_current(struct diff_list_s *list);
void diff_next(struct diff_list_s *list);
void diff_previous(struct diff_list_s *list);
void diff_iterator_next(struct diff_iterator **iterator);
void diff_iterator_previous(struct diff_iterator **iterator);
struct diff_iterator *diff_go_before_line(struct diff_list_s *list, int n);
struct diff_iterator *diff_go_after_line(struct diff_list_s *list, int n);
void diff_iterator_go_before_line(struct diff_iterator **iterator, int n);
void diff_iterator_go_after_line(struct diff_iterator **iterator, int n);


#endif /* SRC_ANSIC_DIFFLIST_H_ */
