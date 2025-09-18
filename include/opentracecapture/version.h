/*
 * This file is part of the OpenTraceCapture project.
 * (Forked from libopentracecapture)
 *
 * Copyright (C) 2024 OpenTraceCapture Contributors
 * Copyright (C) 2010-2013 Uwe Hermann <uwe@hermann-uwe.de>
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

#ifndef OPENTRACECAPTURE_VERSION_H
#define OPENTRACECAPTURE_VERSION_H

/*
 * Only OpenTraceCapture library builds, and only parts of the library build,
 * need to reference the git-version.h header file. Which contains the
 * version suffix, which is relevant to local development, but is not
 * applicable to release builds. Application builds need not bother with
 * internal library version details, and always can get this information
 * in text form for display purposes from the library at runtime.
 */
#if defined WANT_OPENTRACECAPTURE_GIT_VERSION_H
#  include <opentracecapture/git-version.h>
#else
#  undef OTC_PACKAGE_VERSION_STRING_SUFFIX
#  define OTC_PACKAGE_VERSION_STRING_SUFFIX ""
#endif

/**
 * @file
 *
 * Version number definitions and macros.
 */

/**
 * @ingroup grp_versions
 *
 * @{
 */

/*
 * Package version information.
 */

/** The OpenTraceCapture package 'major' version number. */
#define OTC_PACKAGE_VERSION_MAJOR 0

/** The OpenTraceCapture package 'minor' version number. */
#define OTC_PACKAGE_VERSION_MINOR 1

/** The OpenTraceCapture package 'micro' version number. */
#define OTC_PACKAGE_VERSION_MICRO 0

/** The OpenTraceCapture package version ("major.minor.micro") string. */
#define OTC_PACKAGE_VERSION_STRING_PREFIX "0.1.0"

/** The OpenTraceCapture package version with git commit suffix. */
#define OTC_PACKAGE_VERSION_STRING (OTC_PACKAGE_VERSION_STRING_PREFIX OTC_PACKAGE_VERSION_STRING_SUFFIX)

/*
 * Library/libtool version information.
 */

/** The OpenTraceCapture libtool 'current' version number. */
#define OTC_LIB_VERSION_CURRENT 4

/** The OpenTraceCapture libtool 'revision' version number. */
#define OTC_LIB_VERSION_REVISION 0

/** The OpenTraceCapture libtool 'age' version number. */
#define OTC_LIB_VERSION_AGE 0

/** The OpenTraceCapture libtool version ("current:revision:age") string. */
#define OTC_LIB_VERSION_STRING "4:0:0"

/** @} */

#endif /* OPENTRACECAPTURE_VERSION_H */
