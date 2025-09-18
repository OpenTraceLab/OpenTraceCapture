/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2012 Uwe Hermann <uwe@hermann-uwe.de>
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
#include <opentracecapture/libopentracecapture.h>

/**
 * @file
 *
 * Error handling in libopentracecapture.
 */

/**
 * @defgroup grp_error Error handling
 *
 * Error handling in libopentracecapture.
 *
 * libopentracecapture functions usually return @ref OTC_OK upon success, or a negative
 * error code on failure.
 *
 * @{
 */

/**
 * Return a human-readable error string for the given libopentracecapture error code.
 *
 * @param error_code A libopentracecapture error code number, such as OTC_ERR_MALLOC.
 *
 * @return A const string containing a short, human-readable (English)
 *         description of the error, such as "memory allocation error".
 *         The string must NOT be free'd by the caller!
 *
 * @see otc_strerror_name
 *
 * @since 0.2.0
 */
OTC_API const char *otc_strerror(int error_code)
{
	/*
	 * Note: All defined OTC_* error macros from libopentracecapture.h must have
	 * an entry in this function, as well as in otc_strerror_name().
	 */

	switch (error_code) {
	case OTC_OK:
		return "no error";
	case OTC_ERR:
		return "generic/unspecified error";
	case OTC_ERR_MALLOC:
		return "memory allocation error";
	case OTC_ERR_ARG:
		return "invalid argument";
	case OTC_ERR_BUG:
		return "internal error";
	case OTC_ERR_SAMPLERATE:
		return "invalid samplerate";
	case OTC_ERR_NA:
		return "not applicable";
	case OTC_ERR_DEV_CLOSED:
		return "device closed but should be open";
	case OTC_ERR_TIMEOUT:
		return "timeout occurred";
	case OTC_ERR_CHANNEL_GROUP:
		return "no channel group specified";
	case OTC_ERR_DATA:
		return "data is invalid";
	case OTC_ERR_IO:
		return "input/output error";
	default:
		return "unknown error";
	}
}

/**
 * Return the "name" string of the given libopentracecapture error code.
 *
 * For example, the "name" of the OTC_ERR_MALLOC error code is "OTC_ERR_MALLOC",
 * the name of the OTC_OK code is "OTC_OK", and so on.
 *
 * This function can be used for various purposes where the "name" string of
 * a libopentracecapture error code is useful.
 *
 * @param error_code A libopentracecapture error code number, such as OTC_ERR_MALLOC.
 *
 * @return A const string containing the "name" of the error code as string.
 *         The string must NOT be free'd by the caller!
 *
 * @see otc_strerror
 *
 * @since 0.2.0
 */
OTC_API const char *otc_strerror_name(int error_code)
{
	/*
	 * Note: All defined OTC_* error macros from libopentracecapture.h must have
	 * an entry in this function, as well as in otc_strerror().
	 */

	switch (error_code) {
	case OTC_OK:
		return "OTC_OK";
	case OTC_ERR:
		return "OTC_ERR";
	case OTC_ERR_MALLOC:
		return "OTC_ERR_MALLOC";
	case OTC_ERR_ARG:
		return "OTC_ERR_ARG";
	case OTC_ERR_BUG:
		return "OTC_ERR_BUG";
	case OTC_ERR_SAMPLERATE:
		return "OTC_ERR_SAMPLERATE";
	case OTC_ERR_NA:
		return "OTC_ERR_NA";
	case OTC_ERR_DEV_CLOSED:
		return "OTC_ERR_DEV_CLOSED";
	case OTC_ERR_TIMEOUT:
		return "OTC_ERR_TIMEOUT";
	case OTC_ERR_CHANNEL_GROUP:
		return "OTC_ERR_CHANNEL_GROUP";
	case OTC_ERR_DATA:
		return "OTC_ERR_DATA";
	case OTC_ERR_IO:
		return "OTC_ERR_IO";
	default:
		return "unknown error code";
	}
}

/** @} */
