/*
 * check_lfdiff.c
 *
 *  Created on: 14.02.2017
 *      Author: jh
 */


#include <stdlib.h>
#include <check.h>

#include "../src/difflist.h"

/*
 * NOTE: use "CK_FORK=no gdb check_lfdiff" to debug this
 */


/* service function:
 * realloc the given target pointer so the source pointer fits in.
 */
char *strmcat(char **dst, size_t *len, const char *src)
{
    ck_assert(NULL != dst);
    ck_assert(NULL != len);
    ck_assert_int_ge(*len, 0);
    ck_assert(NULL != src);

    const size_t srclen = strlen(src);
    const size_t dstlen = *dst? strlen(*dst): 0;
    const size_t oldlen = *len;

    if (*len < dstlen+srclen+1) {
	*len = dstlen+srclen+1;
	*dst = realloc(*dst, *len);
    }
    if (!oldlen)
	**dst = 0;	// set new allocated string to len=0
    strcat(*dst, src);

    return *dst;
}


struct diff_list_s *difflist = NULL;

void
setup_difflist (void)
{
    // this is executeed before each unit test
    difflist = diff_new();
}

void
teardown_difflist (void)
{
    // this is executeed after each unit test
    diff_delete(difflist);
}



/* --- Tests --- */

START_TEST (test_suptest_malloc_string)
{
    static const char t_1[] = "";

    size_t len = 0;
    char *stcmp = NULL;
    strmcat(&stcmp, &len, t_1);
    ck_assert(NULL != stcmp);
    ck_assert_str_eq(stcmp, t_1);
    ck_assert_int_eq(len, strlen(t_1)+1);
    free(stcmp);
}
END_TEST

START_TEST (test_suptest_add_string)
{
#define ts1	"add\n"
#define ts2	"add2\n"
    static const char t_1[] = ts1;
    static const char t_2[] = ts2;

    size_t len = 0;
    char *stcmp = NULL;
    strmcat(&stcmp, &len, t_1);
    ck_assert(NULL != stcmp);
    ck_assert_str_eq(stcmp, t_1);
    ck_assert_int_eq(len, strlen(t_1)+1);

    strmcat(&stcmp, &len, t_2);
    ck_assert_str_eq(stcmp, ts1 ts2);
    ck_assert_int_eq(len, strlen(t_1)+strlen(t_2)+1);

#undef ts1
#undef ts2
    free(stcmp);
}
END_TEST



START_TEST (test_difflist_create)
{
    ck_assert_msg(difflist != NULL,
	    "diff list new() failed");
}
END_TEST

START_TEST (test_difflist_add_line)
{
    static const char test[] = "test";
    diff_add_line(difflist, 1, strdup(test));
    ck_assert_msg(difflist->tqh_current != NULL,
	    "missing pointer to current item");
    ck_assert_msg(difflist->tqh_current->line != NULL,
	    "missing pointer to added line");
    ck_assert_str_eq(difflist->tqh_current->line, test);
}
END_TEST

START_TEST (test_difflist_remove_last_line)
{
    static const char test[] = "test";
    diff_add_line(difflist, 1, strdup(test));
    // intentional wrong line number, do not remove line 1
    diff_remove_line(difflist, 2);
    ck_assert_msg(difflist->tqh_current != NULL,
	    "missing pointer to current item");
    ck_assert_msg(difflist->tqh_current->line != NULL,
	    "missing pointer to added line");
    ck_assert_str_eq(difflist->tqh_current->line, test);
    ck_assert_int_eq(difflist->tqh_current->n, 1);
    diff_remove_line(difflist, 1);
    ck_assert_msg(difflist->tqh_current == NULL,
	    "not removed last pointer to current item");
}
END_TEST

START_TEST (test_difflist_get_current)
{
    static const char test1[] = "test1";
    static const char test2[] = "test2";
    diff_add_line(difflist, 1, strdup(test1));
    // last added list should be "current" line
    struct diff_iterator *it = diff_iterator_get_current(difflist);
    ck_assert_msg(difflist->tqh_current == it,
	    "wrong pointer to current item");
    ck_assert_msg(difflist->head.tqh_first == it,
	    "wrong pointer to current item");

    diff_add_line(difflist, 2, strdup(test2));
    // last added list should be "current" line not pointer to last fetched
    ck_assert_msg(difflist->tqh_current != it,
	    "wrong pointer to current item");

    it = diff_iterator_get_current(difflist);
    // last added list should be "current" line
    ck_assert_msg(difflist->tqh_current == it,
	    "wrong pointer to current item");
    ck_assert_msg(difflist->head.tqh_first != it,
	    "wrong pointer to first item");

    diff_remove_line(difflist, 1);
    // after removal the current pointer should point to the next (i.e.last available) item
    ck_assert_msg(difflist->tqh_current == it,
	    "wrong pointer to current item");

    it = diff_iterator_get_current(difflist);
    ck_assert_msg(difflist->tqh_current == it,
	    "wrong pointer to current item");

    diff_remove_line(difflist, 2);
    ck_assert_msg(difflist->tqh_current != it,
	    "wrong pointer to current item");

    it = diff_iterator_get_current(difflist);
    ck_assert_msg(difflist->tqh_current == it,
	    "wrong pointer to current item");
}
END_TEST

START_TEST (test_difflist_get_first)
{
    static const char test1[] = "test1";
    static const char test2[] = "test2";
    diff_add_line(difflist, 1, strdup(test1));
    struct diff_iterator *it2 = diff_iterator_get_current(difflist);

    struct diff_iterator *it = diff_iterator_get_first(difflist);
    ck_assert(it == it2);

    // adding one line beond the first line
    // now second line should be current line but not first line
    diff_add_line(difflist, 2, strdup(test2));
    it2 = diff_iterator_get_current(difflist);	// it2 pointing to last line
    it = diff_iterator_get_first(difflist);	// now first line == current line
    ck_assert(it != it2);

    it = diff_iterator_get_last(difflist);	// now last line == current line
    ck_assert(it == it2);

    // remove the first line
    // now second line should be first and current line
    diff_remove_line(difflist, 1);
    it = diff_iterator_get_first(difflist);
    ck_assert(it == it2);

    // remove the first (second) line
    // now current line should return NULL
    diff_remove_line(difflist, 2);
    it2 = diff_iterator_get_current(difflist);
    ck_assert(NULL == it2);
    it = diff_iterator_get_first(difflist);
    ck_assert(NULL == it);
    it2 = diff_iterator_get_current(difflist);
    ck_assert(NULL == it2);
}
END_TEST

START_TEST (test_difflist_get_last)
{
    static const char test1[] = "test1";
    static const char test2[] = "test2";
    diff_add_line(difflist, 1, strdup(test1));
    struct diff_iterator *it2 = diff_iterator_get_current(difflist);	// it2 pointing to first line

    struct diff_iterator *it = diff_iterator_get_first(difflist);
    ck_assert(it == it2);

    // adding one line beond the first line
    // now second line should be current line but not first line
    diff_add_line(difflist, 2, strdup(test2));
    struct diff_iterator *it3 = diff_iterator_get_current(difflist);	// it3 pointing to last line
    it = diff_iterator_get_last(difflist);	// now last line == current line
    ck_assert(it != it2);
    ck_assert(it == it3);

    it = diff_iterator_get_first(difflist);	// now first line == current line
    ck_assert(it == it2);

    // remove the first line
    // now second line should be first and current line
    diff_remove_line(difflist, 1);
    it = diff_iterator_get_first(difflist);
    ck_assert(it == it3);

    // remove the first (second) line
    // now current line should return NULL
    diff_remove_line(difflist, 2);
    it2 = diff_iterator_get_current(difflist);
    ck_assert(NULL == it2);
    it = diff_iterator_get_last(difflist);
    ck_assert(NULL == it);
    it2 = diff_iterator_get_current(difflist);
    ck_assert(NULL == it2);
}
END_TEST

START_TEST (test_difflist_iterator_prev_next)
{
    static const char test1[] = "test1";
    static const char test2[] = "test2";
    diff_add_line(difflist, 1, strdup(test1));
    struct diff_iterator * const it2 = diff_iterator_get_current(difflist);	// it2 pointing to first line
    diff_add_line(difflist, 2, strdup(test2));
    struct diff_iterator * const it3 = diff_iterator_get_current(difflist);	// it3 pointing to last line


    struct diff_iterator *it = diff_iterator_get_first(difflist);

    diff_iterator_next(&it);	// now point to it3
    ck_assert(it != it2);
    ck_assert(it == it3);

    diff_iterator_previous(&it);	// now point to it2
    ck_assert(it == it2);
    ck_assert(it != it3);

    diff_iterator_previous(&it);	// run out of array bounds
    ck_assert(it == NULL);

    it = it3;
    diff_iterator_next(&it);		// run out of array bounds
    ck_assert(it == NULL);
}
END_TEST

START_TEST (test_difflist_get_line)
{
    static const char test1[] = "test1";
    static const char test2[] = "test2";
    diff_add_line(difflist, 1, strdup(test1));
    struct diff_iterator * const it2 = diff_iterator_get_current(difflist);	// it2 pointing to first line
    diff_add_line(difflist, 2, strdup(test2));
    struct diff_iterator * const it3 = diff_iterator_get_current(difflist);	// it3 pointing to last line

    long nr = diff_get_line_nr(it2);
    ck_assert_int_eq(nr, 1);

    nr = diff_get_line_nr(it3);
    ck_assert_int_eq(nr, 2);

    const char *line = diff_get_line(it2);
    ck_assert_str_eq(line, "test1");

    line = diff_get_line(it3);
    ck_assert_str_eq(line, "test2");
}
END_TEST

START_TEST (test_difflist_go_to_line)
{
    static const char test1[] = "test1";
    static const char test2[] = "test2";
    static const char test4[] = "test4";
    diff_add_line(difflist, 1, strdup(test1));
    struct diff_iterator * const it2 = diff_iterator_get_current(difflist);	// it2 pointing to first line
    diff_add_line(difflist, 2, strdup(test2));
    struct diff_iterator * const it3 = diff_iterator_get_current(difflist);	// it3 pointing to second line
    diff_add_line(difflist, 4, strdup(test4));
    struct diff_iterator * const it4 = diff_iterator_get_current(difflist);	// it3 pointing to last line

    const char *line;
    long nr;
    struct diff_iterator *it = it2;	// current point to first entry

    diff_iterator_go_equal_before_line(&it, 2);	// now point to second entry
    ck_assert(it != NULL);
    nr = diff_get_line_nr(it);
    ck_assert_int_eq(nr, 2);

    diff_iterator_go_equal_before_line(&it, 1);	// now point to first entry
    ck_assert(it != NULL);
    nr = diff_get_line_nr(it);
    ck_assert_int_eq(nr, 1);

    diff_iterator_go_equal_before_line(&it, 0);	// now point to first entry
    ck_assert(it != NULL);
    nr = diff_get_line_nr(it);
    ck_assert_int_eq(nr, 1);

    diff_iterator_go_equal_before_line(&it, 3);	// now point to second entry
    ck_assert(it != NULL);
    nr = diff_get_line_nr(it);
    ck_assert_int_eq(nr, 2);

    diff_iterator_go_equal_before_line(&it, 4);	// now point to last entry
    ck_assert(it != NULL);
    nr = diff_get_line_nr(it);
    ck_assert_int_eq(nr, 4);

    diff_iterator_go_equal_before_line(&it, 5);	// now point to last entry
    ck_assert(it != NULL);
    nr = diff_get_line_nr(it);
    ck_assert_int_eq(nr, 4);

    diff_iterator_go_equal_after_line(&it, 1);	// now point to first entry
    ck_assert(it != NULL);
    nr = diff_get_line_nr(it);
    ck_assert_int_eq(nr, 1);

    diff_iterator_go_equal_after_line(&it, 2);	// now point to second entry
    ck_assert(it != NULL);
    nr = diff_get_line_nr(it);
    ck_assert_int_eq(nr, 2);

    diff_iterator_go_equal_after_line(&it, 3);	// now point to last entry
    ck_assert(it != NULL);
    nr = diff_get_line_nr(it);
    ck_assert_int_eq(nr, 4);

    diff_iterator_go_equal_after_line(&it, 4);	// now point to last entry
    ck_assert(it != NULL);
    nr = diff_get_line_nr(it);
    ck_assert_int_eq(nr, 4);

    diff_iterator_go_equal_after_line(&it, 5);	// now point to NULL
    ck_assert(it == NULL);
}
END_TEST




/* --- Test framework --- */

Suite *
support_test_suite (void)
{
    Suite *s = suite_create ("Support Tests");

    /* Core test case */
    TCase *tc_difflist = tcase_create ("Core");
    tcase_add_test (tc_difflist, test_suptest_malloc_string);
    tcase_add_test (tc_difflist, test_suptest_add_string);
    suite_add_tcase (s, tc_difflist);

    return s;
}

Suite *
difflist_suite (void)
{
  Suite *s = suite_create ("Diff List");

  /* Core test case */
  TCase *tc_difflist = tcase_create ("Core");
  tcase_add_checked_fixture (tc_difflist, setup_difflist, teardown_difflist);
  tcase_add_test (tc_difflist, test_difflist_create);
  tcase_add_test (tc_difflist, test_difflist_add_line);
  tcase_add_test (tc_difflist, test_difflist_remove_last_line);
  tcase_add_test (tc_difflist, test_difflist_get_current);
  tcase_add_test (tc_difflist, test_difflist_get_first);
  tcase_add_test (tc_difflist, test_difflist_get_last);
  tcase_add_test (tc_difflist, test_difflist_iterator_prev_next);
  tcase_add_test (tc_difflist, test_difflist_get_line);
  tcase_add_test (tc_difflist, test_difflist_go_to_line);
  suite_add_tcase (s, tc_difflist);

  return s;
}

/*
 * NOTE: use "CK_FORK=no gdb check_lfdiff" to debug this
 */

int main(void)
{
    int number_failed;
    Suite *st = support_test_suite();
    Suite *sl = difflist_suite ();
    SRunner *sr = srunner_create (st);
    srunner_add_suite(sr, sl);
    srunner_run_all (sr, CK_VERBOSE);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
