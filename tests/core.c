/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Uwe Hermann <uwe@hermann-uwe.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <stdlib.h>
#include <check.h>
#include <opentracecapture/libsigrok.h>
#include "lib.h"

/*
 * Check various basic init related things.
 *
 *  - Check whether an otc_init() call with a proper otc_ctx works.
 *    If it returns != OTC_OK (or segfaults) this test will fail.
 *    The otc_init() call (among other things) also runs sanity checks on
 *    all libsigrok hardware drivers and errors out upon issues.
 *
 *  - Check whether a subsequent otc_exit() with that otc_ctx works.
 *    If it returns != OTC_OK (or segfaults) this test will fail.
 */
START_TEST(test_init_exit)
{
	int ret;
	struct otc_context *otc_ctx;

	ret = otc_init(&otc_ctx);
	fail_unless(ret == OTC_OK, "otc_init() failed: %d.", ret);
	ret = otc_exit(otc_ctx);
	fail_unless(ret == OTC_OK, "otc_exit() failed: %d.", ret);
}
END_TEST

/*
 * Check whether two nested otc_init() and otc_exit() calls work.
 * The two functions have two different contexts.
 * If any function returns != OTC_OK (or segfaults) this test will fail.
 */
START_TEST(test_init_exit_2)
{
	int ret;
	struct otc_context *otc_ctx1, *otc_ctx2;

	ret = otc_init(&otc_ctx1);
	fail_unless(ret == OTC_OK, "otc_init() 1 failed: %d.", ret);
	ret = otc_init(&otc_ctx2);
	fail_unless(ret == OTC_OK, "otc_init() 2 failed: %d.", ret);
	ret = otc_exit(otc_ctx2);
	fail_unless(ret == OTC_OK, "otc_exit() 2 failed: %d.", ret);
	ret = otc_exit(otc_ctx1);
	fail_unless(ret == OTC_OK, "otc_exit() 1 failed: %d.", ret);
}
END_TEST

/*
 * Same as above, but otc_exit() in the "wrong" order.
 * This should work fine, it's not a bug to do this.
 */
START_TEST(test_init_exit_2_reverse)
{
	int ret;
	struct otc_context *otc_ctx1, *otc_ctx2;

	ret = otc_init(&otc_ctx1);
	fail_unless(ret == OTC_OK, "otc_init() 1 failed: %d.", ret);
	ret = otc_init(&otc_ctx2);
	fail_unless(ret == OTC_OK, "otc_init() 2 failed: %d.", ret);
	ret = otc_exit(otc_ctx1);
	fail_unless(ret == OTC_OK, "otc_exit() 1 failed: %d.", ret);
	ret = otc_exit(otc_ctx2);
	fail_unless(ret == OTC_OK, "otc_exit() 2 failed: %d.", ret);
}
END_TEST

/*
 * Check whether three nested otc_init() and otc_exit() calls work.
 * The three functions have three different contexts.
 * If any function returns != OTC_OK (or segfaults) this test will fail.
 */
START_TEST(test_init_exit_3)
{
	int ret;
	struct otc_context *otc_ctx1, *otc_ctx2, *otc_ctx3;

	ret = otc_init(&otc_ctx1);
	fail_unless(ret == OTC_OK, "otc_init() 1 failed: %d.", ret);
	ret = otc_init(&otc_ctx2);
	fail_unless(ret == OTC_OK, "otc_init() 2 failed: %d.", ret);
	ret = otc_init(&otc_ctx3);
	fail_unless(ret == OTC_OK, "otc_init() 3 failed: %d.", ret);
	ret = otc_exit(otc_ctx3);
	fail_unless(ret == OTC_OK, "otc_exit() 3 failed: %d.", ret);
	ret = otc_exit(otc_ctx2);
	fail_unless(ret == OTC_OK, "otc_exit() 2 failed: %d.", ret);
	ret = otc_exit(otc_ctx1);
	fail_unless(ret == OTC_OK, "otc_exit() 1 failed: %d.", ret);
}
END_TEST

/*
 * Same as above, but otc_exit() in the "wrong" order.
 * This should work fine, it's not a bug to do this.
 */
START_TEST(test_init_exit_3_reverse)
{
	int ret;
	struct otc_context *otc_ctx1, *otc_ctx2, *otc_ctx3;

	ret = otc_init(&otc_ctx1);
	fail_unless(ret == OTC_OK, "otc_init() 1 failed: %d.", ret);
	ret = otc_init(&otc_ctx2);
	fail_unless(ret == OTC_OK, "otc_init() 2 failed: %d.", ret);
	ret = otc_init(&otc_ctx3);
	fail_unless(ret == OTC_OK, "otc_init() 3 failed: %d.", ret);
	ret = otc_exit(otc_ctx1);
	fail_unless(ret == OTC_OK, "otc_exit() 1 failed: %d.", ret);
	ret = otc_exit(otc_ctx2);
	fail_unless(ret == OTC_OK, "otc_exit() 2 failed: %d.", ret);
	ret = otc_exit(otc_ctx3);
	fail_unless(ret == OTC_OK, "otc_exit() 3 failed: %d.", ret);
}
END_TEST

/* Check whether otc_init(NULL) fails as it should. */
START_TEST(test_init_null)
{
	int ret;

        ret = otc_log_loglevel_set(OTC_LOG_NONE);
        fail_unless(ret == OTC_OK, "otc_log_loglevel_set() failed: %d.", ret);

	ret = otc_init(NULL);
	fail_unless(ret != OTC_OK, "otc_init(NULL) should have failed.");
}
END_TEST

/* Check whether otc_exit(NULL) fails as it should. */
START_TEST(test_exit_null)
{
	int ret;

        ret = otc_log_loglevel_set(OTC_LOG_NONE);
        fail_unless(ret == OTC_OK, "otc_log_loglevel_set() failed: %d.", ret);

	ret = otc_exit(NULL);
	fail_unless(ret != OTC_OK, "otc_exit(NULL) should have failed.");
}
END_TEST

Suite *suite_core(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("core");

	tc = tcase_create("init_exit");
	tcase_add_test(tc, test_init_exit);
	tcase_add_test(tc, test_init_exit_2);
	tcase_add_test(tc, test_init_exit_2_reverse);
	tcase_add_test(tc, test_init_exit_3);
	tcase_add_test(tc, test_init_exit_3_reverse);
	tcase_add_test(tc, test_init_null);
	tcase_add_test(tc, test_exit_null);
	suite_add_tcase(s, tc);

	return s;
}
