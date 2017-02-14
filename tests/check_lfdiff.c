/*
 * check_lfdiff.c
 *
 *  Created on: 14.02.2017
 *      Author: jh
 */


#include <stdlib.h>
#include <check.h>

#include "../src/difflist.h"


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

START_TEST (test_difflist_remove_line)
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




/* --- Test framework --- */

Suite *
difflist_suite (void)
{
  Suite *s = suite_create ("Diff List");

  /* Core test case */
  TCase *tc_difflist = tcase_create ("Core");
  tcase_add_checked_fixture (tc_difflist, setup_difflist, teardown_difflist);
  tcase_add_test (tc_difflist, test_difflist_create);
  tcase_add_test (tc_difflist, test_difflist_add_line);
  tcase_add_test (tc_difflist, test_difflist_remove_line);
  suite_add_tcase (s, tc_difflist);

  return s;
}

int main(void)
{
    int number_failed;
    Suite *s = difflist_suite ();
    SRunner *sr = srunner_create (s);
    srunner_run_all (sr, CK_VERBOSE);
    number_failed = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
