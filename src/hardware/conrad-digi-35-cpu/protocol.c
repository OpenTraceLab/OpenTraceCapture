/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2014 Matthias Heidbrink <m-opentracelab@heidbrink.biz>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include "protocol.h"

/**
 * Send command with parameter.
 *
 * @param[in] cmd Command
 * @param[in] param Parameter (0..999, depending on command).
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR_ARG Invalid argument.
 * @retval OTC_ERR Error.
 */
OTC_PRIV int send_msg1(const struct otc_dev_inst *sdi, char cmd, int param)
{
	struct otc_serial_dev_inst *serial;
	char buf[5];

	if (!sdi || !(serial = sdi->conn))
		return OTC_ERR_ARG;

	snprintf(buf, sizeof(buf), "%c%03d", cmd, param);
	buf[4] = '\r';

	otc_spew("send_msg1(): %c%c%c%c\\r", buf[0], buf[1], buf[2], buf[3]);

	if (serial_write_blocking(serial, buf, sizeof(buf),
			serial_timeout(serial, sizeof(buf))) < (int)sizeof(buf)) {
		otc_err("Write error for cmd=%c", cmd);
		return OTC_ERR;
	}

	/*
	 * Wait 50ms to ensure that the device does not swallow any of the
	 * following commands.
	 */
	g_usleep(50 * 1000);

	return OTC_OK;
}
