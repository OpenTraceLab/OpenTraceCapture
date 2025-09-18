/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2015 Uwe Hermann <uwe@hermann-uwe.de>
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

/* Check whether at least one transform module is available. */
START_TEST(test_transform_available)
{
	const struct otc_transform_module **transforms;

	transforms = otc_transform_list();
	fail_unless(transforms != NULL, "No transform modules found.");
}
END_TEST

/* Check whether otc_transform_id_get() works. */
START_TEST(test_transform_id)
{
	const struct otc_transform_module **transforms;
	const char *id;

	transforms = otc_transform_list();

	id = otc_transform_id_get(transforms[0]);
	fail_unless(id != NULL, "No ID found in transform module.");
}
END_TEST

/* Check whether otc_transform_name_get() works. */
START_TEST(test_transform_name)
{
	const struct otc_transform_module **transforms;
	const char *name;

	transforms = otc_transform_list();

	name = otc_transform_name_get(transforms[0]);
	fail_unless(name != NULL, "No name found in transform module.");
}
END_TEST

/* Check whether otc_transform_description_get() works. */
START_TEST(test_transform_desc)
{
	const struct otc_transform_module **transforms;
	const char *desc;

	transforms = otc_transform_list();

	desc = otc_transform_description_get(transforms[0]);
	fail_unless(desc != NULL, "No description found in transform module.");
}
END_TEST

/* Check whether otc_transform_find() works. */
START_TEST(test_transform_find)
{
	const struct otc_transform_module *tmod;
	const char *id;

	tmod = otc_transform_find("nop");
	fail_unless(tmod != NULL, "Couldn't find the 'nop' transform module.");
	id = otc_transform_id_get(tmod);
	fail_unless(id != NULL, "No ID found in transform module.");
	fail_unless(!strcmp(id, "nop"), "That is not the 'nop' module!");
}
END_TEST

/* Check whether otc_transform_options_get() works. */
START_TEST(test_transform_options)
{
	const struct otc_option **opt;

	opt = otc_transform_options_get(otc_transform_find("nop"));
	fail_unless(opt == NULL, "Transform module 'nop' doesn't have options.");
}
END_TEST

Suite *suite_transform_all(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("transform-all");

	tc = tcase_create("basic");
	tcase_add_test(tc, test_transform_available);
	tcase_add_test(tc, test_transform_id);
	tcase_add_test(tc, test_transform_name);
	tcase_add_test(tc, test_transform_desc);
	tcase_add_test(tc, test_transform_find);
	tcase_add_test(tc, test_transform_options);
	suite_add_tcase(s, tc);

	return s;
}
