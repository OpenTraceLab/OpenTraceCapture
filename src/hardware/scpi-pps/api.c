/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2014 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2017,2019 Frank Stettner <frank-stettner@gmx.net>
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
#include <string.h>
#include <strings.h>
#include "../../scpi.h"
#include "protocol.h"

static struct otc_dev_driver scpi_pps_driver_info;
static struct otc_dev_driver hp_ib_pps_driver_info;

static const uint32_t scanopts[] = {
	OTC_CONF_CONN,
	OTC_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
	OTC_CONF_POWER_SUPPLY,
};

static const struct pps_channel_instance pci[] = {
	{ OTC_MQ_VOLTAGE, SCPI_CMD_GET_MEAS_VOLTAGE, "V" },
	{ OTC_MQ_CURRENT, SCPI_CMD_GET_MEAS_CURRENT, "I" },
	{ OTC_MQ_POWER, SCPI_CMD_GET_MEAS_POWER, "P" },
	{ OTC_MQ_FREQUENCY, SCPI_CMD_GET_MEAS_FREQUENCY, "F" },
};

static struct otc_dev_inst *probe_device(struct otc_scpi_dev_inst *scpi,
		int (*get_hw_id)(struct otc_scpi_dev_inst *scpi,
		struct otc_scpi_hw_info **scpi_response))
{
	struct dev_context *devc;
	struct otc_dev_inst *sdi;
	struct otc_scpi_hw_info *hw_info;
	struct otc_channel_group *cg;
	struct otc_channel *ch;
	const struct scpi_pps *device;
	struct pps_channel *pch;
	struct channel_spec *channels;
	struct channel_group_spec *channel_groups, *cgs;
	struct pps_channel_group *pcg;
	GRegex *model_re;
	GMatchInfo *model_mi;
	GSList *l;
	uint64_t mask;
	unsigned int num_channels, num_channel_groups, ch_num, ch_idx, i, j;
	int ret;
	const char *vendor;
	char ch_name[16];

	if (get_hw_id(scpi, &hw_info) != OTC_OK) {
		otc_info("Couldn't get IDN response.");
		return NULL;
	}

	device = NULL;
	for (i = 0; i < num_pps_profiles; i++) {
		vendor = otc_vendor_alias(hw_info->manufacturer);
		if (g_ascii_strcasecmp(vendor, pps_profiles[i].vendor))
			continue;
		model_re = g_regex_new(pps_profiles[i].model, 0, 0, NULL);
		if (g_regex_match(model_re, hw_info->model, 0, &model_mi))
			device = &pps_profiles[i];
		g_match_info_unref(model_mi);
		g_regex_unref(model_re);
		if (device)
			break;
	}
	if (!device) {
		otc_scpi_hw_info_free(hw_info);
		return NULL;
	}

	sdi = g_malloc0(sizeof(struct otc_dev_inst));
	sdi->vendor = g_strdup(vendor);
	sdi->model = g_strdup(hw_info->model);
	sdi->version = g_strdup(hw_info->firmware_version);
	sdi->conn = scpi;
	sdi->driver = &scpi_pps_driver_info;
	sdi->inst_type = OTC_INST_SCPI;
	sdi->serial_num = g_strdup(hw_info->serial_number);

	devc = g_malloc0(sizeof(struct dev_context));
	devc->device = device;
	otc_sw_limits_init(&devc->limits);
	sdi->priv = devc;

	if (device->num_channels) {
		/* Static channels and groups. */
		channels = (struct channel_spec *)device->channels;
		num_channels = device->num_channels;
		channel_groups = (struct channel_group_spec *)device->channel_groups;
		num_channel_groups = device->num_channel_groups;
	} else {
		/* Channels and groups need to be probed. */
		ret = device->probe_channels(sdi, hw_info, &channels, &num_channels,
				&channel_groups, &num_channel_groups);
		if (ret != OTC_OK) {
			otc_err("Failed to probe for channels.");
			return NULL;
		}
		/*
		 * Since these were dynamically allocated, we'll need to free them
		 * later.
		 */
		devc->channels = channels;
		devc->channel_groups = channel_groups;
	}

	ch_idx = 0;
	for (ch_num = 0; ch_num < num_channels; ch_num++) {
		/* Create one channel per measurable output unit. */
		for (i = 0; i < ARRAY_SIZE(pci); i++) {
			if (!otc_scpi_cmd_get(devc->device->commands, pci[i].command))
				continue;
			g_snprintf(ch_name, 16, "%s%s", pci[i].prefix,
					channels[ch_num].name);
			ch = otc_channel_new(sdi, ch_idx++, OTC_CHANNEL_ANALOG, TRUE,
					ch_name);
			pch = g_malloc0(sizeof(struct pps_channel));
			pch->hw_output_idx = ch_num;
			pch->hwname = channels[ch_num].name;
			pch->mq = pci[i].mq;
			ch->priv = pch;
		}
	}

	for (i = 0; i < num_channel_groups; i++) {
		cgs = &channel_groups[i];
		cg = otc_channel_group_new(sdi, cgs->name, NULL);
		for (j = 0, mask = 1; j < 64; j++, mask <<= 1) {
			if (cgs->channel_index_mask & mask) {
				for (l = sdi->channels; l; l = l->next) {
					ch = l->data;
					pch = ch->priv;
					/* Add mqflags from channel_group_spec only to voltage
					 * and current channels.
					 */
					if (pch->mq == OTC_MQ_VOLTAGE || pch->mq == OTC_MQ_CURRENT)
						pch->mqflags = cgs->mqflags;
					else
						pch->mqflags = 0;
					if (pch->hw_output_idx == j)
						cg->channels = g_slist_append(cg->channels, ch);
				}
			}
		}
		pcg = g_malloc0(sizeof(struct pps_channel_group));
		pcg->features = cgs->features;
		cg->priv = pcg;
	}

	otc_scpi_hw_info_free(hw_info);
	hw_info = NULL;

	if (devc->device->dialect == SCPI_DIALECT_SIGLENT) {
		scpi->quirks |= SCPI_QUIRK_CMD_OMIT_LF;
		scpi->quirks |= SCPI_QUIRK_OPC_UNSUPPORTED;
		scpi->quirks |= SCPI_QUIRK_SLOW_CHANNEL_SELECT;
	}

	/* Don't send SCPI_CMD_LOCAL for HP 66xxB using SCPI over GPIB. */
	if (!(devc->device->dialect == SCPI_DIALECT_HP_66XXB &&
			scpi->transport == SCPI_TRANSPORT_LIBGPIB))
		otc_scpi_cmd(sdi, devc->device->commands, 0, NULL, SCPI_CMD_LOCAL);

	return sdi;
}

static gchar *hpib_get_revision(struct otc_scpi_dev_inst *scpi)
{
	int ret;
	gboolean matches;
	char *response;
	GRegex *version_regex;

	ret = otc_scpi_get_string(scpi, "ROM?", &response);
	if (ret != OTC_OK && !response)
		return NULL;

	/* Example version string: "B01 B01" */
	version_regex = g_regex_new("[A-Z][0-9]{2} [A-Z][0-9]{2}", 0, 0, NULL);
	matches = g_regex_match(version_regex, response, 0, NULL);
	g_regex_unref(version_regex);

	if (!matches) {
		/* Not a valid version string. Ignore it. */
		g_free(response);
		response = NULL;
	} else {
		/* Replace space with dot. */
		response[3] = '.';
	}

	return response;
}

/*
 * This function assumes the response is in the form "HP<model_number>"
 *
 * HP made many GPIB (then called HP-IB) instruments before the SCPI command
 * set was introduced into the standard. We haven't seen any non-HP instruments
 * which respond to the "ID?" query, so assume all are HP for now.
 */
static int hpib_get_hw_id(struct otc_scpi_dev_inst *scpi,
			  struct otc_scpi_hw_info **scpi_response)
{
	int ret;
	char *response;
	struct otc_scpi_hw_info *hw_info;

	ret = otc_scpi_get_string(scpi, "ID?", &response);
	if ((ret != OTC_OK) || !response)
		return OTC_ERR;

	hw_info = g_malloc0(sizeof(struct otc_scpi_hw_info));

	*scpi_response = hw_info;
	hw_info->model = response;
	hw_info->firmware_version = hpib_get_revision(scpi);
	hw_info->manufacturer = g_strdup("HP");

	return OTC_OK;
}

static struct otc_dev_inst *probe_scpi_pps_device(struct otc_scpi_dev_inst *scpi)
{
	return probe_device(scpi, otc_scpi_get_hw_id);
}

static struct otc_dev_inst *probe_hpib_pps_device(struct otc_scpi_dev_inst *scpi)
{
	return probe_device(scpi, hpib_get_hw_id);
}

static GSList *scan_scpi_pps(struct otc_dev_driver *di, GSList *options)
{
	return otc_scpi_scan(di->context, options, probe_scpi_pps_device);
}

static GSList *scan_hpib_pps(struct otc_dev_driver *di, GSList *options)
{
	const char *conn;

	/*
	 * Only scan for HP-IB devices when conn= was specified, to not
	 * break SCPI devices' operation.
	 */
	conn = NULL;
	(void)otc_serial_extract_options(options, &conn, NULL);
	if (!conn)
		return NULL;

	return otc_scpi_scan(di->context, options, probe_hpib_pps_device);
}

static int dev_open(struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	struct otc_scpi_dev_inst *scpi;
	GVariant *beeper;

	scpi = sdi->conn;
	if (otc_scpi_open(scpi) < 0)
		return OTC_ERR;

	devc = sdi->priv;

	/* Don't send SCPI_CMD_REMOTE for HP 66xxB using SCPI over GPIB. */
	if (!(devc->device->dialect == SCPI_DIALECT_HP_66XXB &&
			scpi->transport == SCPI_TRANSPORT_LIBGPIB))
		otc_scpi_cmd(sdi, devc->device->commands, 0, NULL, SCPI_CMD_REMOTE);

	devc->beeper_was_set = FALSE;
	if (otc_scpi_cmd_resp(sdi, devc->device->commands, 0, NULL,
			&beeper, G_VARIANT_TYPE_BOOLEAN, SCPI_CMD_BEEPER) == OTC_OK) {
		if (g_variant_get_boolean(beeper)) {
			devc->beeper_was_set = TRUE;
			otc_scpi_cmd(sdi, devc->device->commands,
				0, NULL, SCPI_CMD_BEEPER_DISABLE);
		}
		g_variant_unref(beeper);
	}

	return OTC_OK;
}

static int dev_close(struct otc_dev_inst *sdi)
{
	struct otc_scpi_dev_inst *scpi;
	struct dev_context *devc;

	devc = sdi->priv;
	scpi = sdi->conn;

	if (!scpi)
		return OTC_ERR_BUG;

	if (devc->beeper_was_set)
		otc_scpi_cmd(sdi, devc->device->commands,
			0, NULL, SCPI_CMD_BEEPER_ENABLE);

	/* Don't send SCPI_CMD_LOCAL for HP 66xxB using SCPI over GPIB. */
	if (!(devc->device->dialect == SCPI_DIALECT_HP_66XXB &&
			scpi->transport == SCPI_TRANSPORT_LIBGPIB))
		otc_scpi_cmd(sdi, devc->device->commands, 0, NULL, SCPI_CMD_LOCAL);

	return otc_scpi_close(scpi);
}

static void clear_helper(struct dev_context *devc)
{
	g_free(devc->channels);
	g_free(devc->channel_groups);
}

static int dev_clear(const struct otc_dev_driver *di)
{
	return std_dev_clear_with_callback(di, (std_dev_clear_callback)clear_helper);
}

static int config_get(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct dev_context *devc;
	const GVariantType *gvtype;
	unsigned int i;
	int channel_group_cmd;
	char *channel_group_name;
	int cmd, ret;
	const char *s;
	int reg;
	gboolean is_hmp_sqii, is_keysight_e36300a;

	if (!sdi)
		return OTC_ERR_ARG;

	devc = sdi->priv;

	if (cg) {
		/*
		 * These options only apply to channel groups with a single
		 * channel -- they're per-channel settings for the device.
		 */

		/*
		 * Config keys are handled below depending on whether a channel
		 * group was provided by the frontend. However some of these
		 * take a CG on one PPS but not on others. Check the device's
		 * profile for that here, and NULL out the channel group as needed.
		 */
		for (i = 0; i < devc->device->num_devopts; i++) {
			if (devc->device->devopts[i] == key) {
				cg = NULL;
				break;
			}
		}
	}

	gvtype = NULL;
	cmd = -1;
	switch (key) {
	case OTC_CONF_ENABLED:
		if (devc->device->dialect == SCPI_DIALECT_SIGLENT)
			gvtype = G_VARIANT_TYPE_STRING;
		else
			gvtype = G_VARIANT_TYPE_BOOLEAN;
		cmd = SCPI_CMD_GET_OUTPUT_ENABLED;
		break;
	case OTC_CONF_VOLTAGE:
		gvtype = G_VARIANT_TYPE_DOUBLE;
		cmd = SCPI_CMD_GET_MEAS_VOLTAGE;
		break;
	case OTC_CONF_VOLTAGE_TARGET:
		gvtype = G_VARIANT_TYPE_DOUBLE;
		cmd = SCPI_CMD_GET_VOLTAGE_TARGET;
		break;
	case OTC_CONF_OUTPUT_FREQUENCY:
		gvtype = G_VARIANT_TYPE_DOUBLE;
		cmd = SCPI_CMD_GET_MEAS_FREQUENCY;
		break;
	case OTC_CONF_OUTPUT_FREQUENCY_TARGET:
		gvtype = G_VARIANT_TYPE_DOUBLE;
		cmd = SCPI_CMD_GET_FREQUENCY_TARGET;
		break;
	case OTC_CONF_CURRENT:
		gvtype = G_VARIANT_TYPE_DOUBLE;
		cmd = SCPI_CMD_GET_MEAS_CURRENT;
		break;
	case OTC_CONF_CURRENT_LIMIT:
		gvtype = G_VARIANT_TYPE_DOUBLE;
		cmd = SCPI_CMD_GET_CURRENT_LIMIT;
		break;
	case OTC_CONF_OVER_VOLTAGE_PROTECTION_ENABLED:
		if (devc->device->dialect == SCPI_DIALECT_HMP) {
			/* OVP is always enabled. */
			*data = g_variant_new_boolean(TRUE);
			return 0;
		}
		gvtype = G_VARIANT_TYPE_BOOLEAN;
		cmd = SCPI_CMD_GET_OVER_VOLTAGE_PROTECTION_ENABLED;
		break;
	case OTC_CONF_OVER_VOLTAGE_PROTECTION_ACTIVE:
		if (devc->device->dialect == SCPI_DIALECT_HP_66XXB ||
			devc->device->dialect == SCPI_DIALECT_HP_COMP)
			gvtype = G_VARIANT_TYPE_STRING;
		else
			gvtype = G_VARIANT_TYPE_BOOLEAN;
		cmd = SCPI_CMD_GET_OVER_VOLTAGE_PROTECTION_ACTIVE;
		break;
	case OTC_CONF_OVER_VOLTAGE_PROTECTION_THRESHOLD:
		gvtype = G_VARIANT_TYPE_DOUBLE;
		cmd = SCPI_CMD_GET_OVER_VOLTAGE_PROTECTION_THRESHOLD;
		break;
	case OTC_CONF_OVER_CURRENT_PROTECTION_ENABLED:
		gvtype = G_VARIANT_TYPE_BOOLEAN;
		cmd = SCPI_CMD_GET_OVER_CURRENT_PROTECTION_ENABLED;
		break;
	case OTC_CONF_OVER_CURRENT_PROTECTION_ACTIVE:
		if (devc->device->dialect == SCPI_DIALECT_HP_66XXB ||
			devc->device->dialect == SCPI_DIALECT_HP_COMP)
			gvtype = G_VARIANT_TYPE_STRING;
		else
			gvtype = G_VARIANT_TYPE_BOOLEAN;
		cmd = SCPI_CMD_GET_OVER_CURRENT_PROTECTION_ACTIVE;
		break;
	case OTC_CONF_OVER_CURRENT_PROTECTION_THRESHOLD:
		gvtype = G_VARIANT_TYPE_DOUBLE;
		cmd = SCPI_CMD_GET_OVER_CURRENT_PROTECTION_THRESHOLD;
		break;
	case OTC_CONF_OVER_CURRENT_PROTECTION_DELAY:
		gvtype = G_VARIANT_TYPE_DOUBLE;
		cmd = SCPI_CMD_GET_OVER_CURRENT_PROTECTION_DELAY;
		break;
	case OTC_CONF_OVER_TEMPERATURE_PROTECTION:
		if (devc->device->dialect == SCPI_DIALECT_HMP) {
			/* OTP is always enabled. */
			*data = g_variant_new_boolean(TRUE);
			return 0;
		}
		gvtype = G_VARIANT_TYPE_BOOLEAN;
		cmd = SCPI_CMD_GET_OVER_TEMPERATURE_PROTECTION;
		break;
	case OTC_CONF_OVER_TEMPERATURE_PROTECTION_ACTIVE:
		if (devc->device->dialect == SCPI_DIALECT_HP_66XXB ||
			devc->device->dialect == SCPI_DIALECT_HP_COMP ||
			devc->device->dialect == SCPI_DIALECT_HMP ||
			devc->device->dialect == SCPI_DIALECT_KEYSIGHT_E36300A)
			gvtype = G_VARIANT_TYPE_STRING;
		else
			gvtype = G_VARIANT_TYPE_BOOLEAN;
		cmd = SCPI_CMD_GET_OVER_TEMPERATURE_PROTECTION_ACTIVE;
		break;
	case OTC_CONF_REGULATION:
		gvtype = G_VARIANT_TYPE_STRING;
		cmd = SCPI_CMD_GET_OUTPUT_REGULATION;
		break;
	case OTC_CONF_CHANNEL_CONFIG:
		gvtype = G_VARIANT_TYPE_STRING;
		cmd = SCPI_CMD_GET_CHANNEL_CONFIG;
		break;
	default:
		return otc_sw_limits_config_get(&devc->limits, key, data);
	}
	if (!gvtype)
		return OTC_ERR_NA;

	channel_group_cmd = 0;
	channel_group_name = NULL;
	if (cg) {
		channel_group_cmd = SCPI_CMD_SELECT_CHANNEL;
		channel_group_name = g_strdup(cg->name);
	}

	is_hmp_sqii = FALSE;
	is_hmp_sqii |= cmd == SCPI_CMD_GET_OUTPUT_REGULATION;
	is_hmp_sqii |= cmd == SCPI_CMD_GET_OVER_TEMPERATURE_PROTECTION_ACTIVE;
	is_hmp_sqii &= devc->device->dialect == SCPI_DIALECT_HMP;

	is_keysight_e36300a = FALSE;
	is_keysight_e36300a |= cmd == SCPI_CMD_GET_OUTPUT_REGULATION;
	is_keysight_e36300a |= cmd == SCPI_CMD_GET_OVER_TEMPERATURE_PROTECTION_ACTIVE;
	is_keysight_e36300a &= devc->device->dialect == SCPI_DIALECT_KEYSIGHT_E36300A;

	if (is_hmp_sqii || is_keysight_e36300a) {
		if (!cg) {
			/* STAT:QUES:INST:ISUMx query requires channel spec. */
			otc_err("Need a channel group for regulation or OTP-active query.");
			return OTC_ERR_NA;
		}
		ret = otc_scpi_cmd_resp(sdi, devc->device->commands,
			0, NULL, data, gvtype, cmd, channel_group_name);
	} else {
		ret = otc_scpi_cmd_resp(sdi, devc->device->commands,
			channel_group_cmd, channel_group_name, data, gvtype, cmd,
			channel_group_name);
	}

	/*
	 * Handle special cases
	 */

	if (cmd == SCPI_CMD_GET_OUTPUT_ENABLED) {
		if (devc->device->dialect == SCPI_DIALECT_SIGLENT) {
			long regl;
			s = g_variant_get_string(*data, NULL);
			otc_atol_base(s, &regl, NULL, 16);
			g_variant_unref(*data);
			if (channel_group_name) {
				otc_atoi(channel_group_name, &reg);
			} else {
				reg=1;
			}
			regl = (regl >> (reg+3));
			*data = g_variant_new_boolean(regl & 0x01);
		}
	}

	g_free(channel_group_name);

	if (cmd == SCPI_CMD_GET_OUTPUT_REGULATION) {
		if (devc->device->dialect == SCPI_DIALECT_PHILIPS) {
			/*
			* The Philips PM2800 series returns VOLT/CURR. We always return
			* a GVariant string in the Rigol notation (CV/CC/UR).
			*/
			s = g_variant_get_string(*data, NULL);
			if (!g_strcmp0(s, "VOLT")) {
				g_variant_unref(*data);
				*data = g_variant_new_string("CV");
			} else if (!g_strcmp0(s, "CURR")) {
				g_variant_unref(*data);
				*data = g_variant_new_string("CC");
			}
		}
		if (devc->device->dialect == SCPI_DIALECT_HP_COMP) {
			/* Evaluate Status Register from a HP 66xx in COMP mode. */
			s = g_variant_get_string(*data, NULL);
			otc_atoi(s, &reg);
			g_variant_unref(*data);
			if (reg & (1 << 0))
				*data = g_variant_new_string("CV");
			else if (reg & (1 << 1))
				*data = g_variant_new_string("CC");
			else if (reg & (1 << 2))
				*data = g_variant_new_string("UR");
			else if (reg & (1 << 9))
				*data = g_variant_new_string("CC-");
			else
				*data = g_variant_new_string("");
		}
		if (devc->device->dialect == SCPI_DIALECT_HP_66XXB) {
			/* Evaluate Operational Status Register from a HP 66xxB. */
			s = g_variant_get_string(*data, NULL);
			otc_atoi(s, &reg);
			g_variant_unref(*data);
			if (reg & (1 << 8))
				*data = g_variant_new_string("CV");
			else if (reg & (1 << 10))
				*data = g_variant_new_string("CC");
			else if (reg & (1 << 11))
				*data = g_variant_new_string("CC-");
			else
				*data = g_variant_new_string("UR");
		}
		if (devc->device->dialect == SCPI_DIALECT_HMP) {
			/* Evaluate Condition Status Register from a HMP series device. */
			s = g_variant_get_string(*data, NULL);
			otc_atoi(s, &reg);
			g_variant_unref(*data);
			if (reg & (1 << 0))
				*data = g_variant_new_string("CC");
			else if (reg & (1 << 1))
				*data = g_variant_new_string("CV");
			else
				*data = g_variant_new_string("UR");
		}
		if (devc->device->dialect == SCPI_DIALECT_KEYSIGHT_E36300A) {
			/* Evaluate Condition Status Register from a Keysight E36300A series device. */
			s = g_variant_get_string(*data, NULL);
			otc_atoi(s, &reg);
			reg &= 0x03u;
			g_variant_unref(*data);
			if (reg == 0x01u)
				*data = g_variant_new_string("CC");
			else if (reg == 0x02u)
				*data = g_variant_new_string("CV");
			else if (reg == 0x03u)
				/* 2 LSBs == 11: HW Failure*/
				*data = g_variant_new_string("");
			else
				/* 2 LSBs == 00: Unregulated */
				*data = g_variant_new_string("UR");
		}
		if (devc->device->dialect == SCPI_DIALECT_SIGLENT) {
			long regl;
			/* evaluate status register */
			s = g_variant_get_string(*data, NULL);
			otc_atol_base(s, &regl, NULL, 16);
			g_variant_unref(*data);
			if (channel_group_name) {
				otc_atoi(channel_group_name, &reg);
			} else {
				reg=1;
			}
			regl = (regl >> (reg-1));
			if (regl & 0x01)
				*data = g_variant_new_string("CC");
			else
				*data = g_variant_new_string("CV");
		}

		s = g_variant_get_string(*data, NULL);
		if (g_strcmp0(s, "CV") && g_strcmp0(s, "CC") && g_strcmp0(s, "CC-") &&
			g_strcmp0(s, "UR") && g_strcmp0(s, "")) {

			otc_err("Unknown response to SCPI_CMD_GET_OUTPUT_REGULATION: %s", s);
			ret = OTC_ERR_DATA;
		}
	}

	if (cmd == SCPI_CMD_GET_CHANNEL_CONFIG) {
		if (devc->device->dialect == SCPI_DIALECT_SIGLENT) {
			long regl;
			/* evaluate status register */
			s = g_variant_get_string(*data, NULL);
			otc_atol_base(s, &regl, NULL, 16);
			g_variant_unref(*data);

			regl = (regl >> 2) & 0x03;
			if (regl == 0x02)
				*data = g_variant_new_string("Parallel");
			else if (regl == 0x03)
				*data = g_variant_new_string("Series");
			else
				*data = g_variant_new_string("Independent");
		}
	}

	if (cmd == SCPI_CMD_GET_OVER_VOLTAGE_PROTECTION_ACTIVE) {
		if (devc->device->dialect == SCPI_DIALECT_HP_COMP) {
			/* Evaluate Status Register from a HP 66xx in COMP mode. */
			s = g_variant_get_string(*data, NULL);
			otc_atoi(s, &reg);
			g_variant_unref(*data);
			*data = g_variant_new_boolean(reg & (1 << 3));
		}
		if (devc->device->dialect == SCPI_DIALECT_HP_66XXB) {
			/* Evaluate Questionable Status Register bit 0 from a HP 66xxB. */
			s = g_variant_get_string(*data, NULL);
			otc_atoi(s, &reg);
			g_variant_unref(*data);
			*data = g_variant_new_boolean(reg & (1 << 0));
		}
	}

	if (cmd == SCPI_CMD_GET_OVER_CURRENT_PROTECTION_ACTIVE) {
		if (devc->device->dialect == SCPI_DIALECT_HP_COMP) {
			/* Evaluate Status Register from a HP 66xx in COMP mode. */
			s = g_variant_get_string(*data, NULL);
			otc_atoi(s, &reg);
			g_variant_unref(*data);
			*data = g_variant_new_boolean(reg & (1 << 6));
		}
		if (devc->device->dialect == SCPI_DIALECT_HP_66XXB) {
			/* Evaluate Questionable Status Register bit 1 from a HP 66xxB. */
			s = g_variant_get_string(*data, NULL);
			otc_atoi(s, &reg);
			g_variant_unref(*data);
			*data = g_variant_new_boolean(reg & (1 << 1));
		}
	}

	if (cmd == SCPI_CMD_GET_OVER_TEMPERATURE_PROTECTION_ACTIVE) {
		if (devc->device->dialect == SCPI_DIALECT_HP_COMP) {
			/* Evaluate Status Register from a HP 66xx in COMP mode. */
			s = g_variant_get_string(*data, NULL);
			otc_atoi(s, &reg);
			g_variant_unref(*data);
			*data = g_variant_new_boolean(reg & (1 << 4));
		}
		if (devc->device->dialect == SCPI_DIALECT_HP_66XXB ||
		    devc->device->dialect == SCPI_DIALECT_HMP ||
		    devc->device->dialect == SCPI_DIALECT_KEYSIGHT_E36300A) {
			/* Evaluate Questionable Status Register bit 4 from a HP 66xxB. */
			/* For Keysight E36300A, the queried register is the Questionable Instrument Summary register, */
			/* but the bit position is the same as an HP 66xxB's Questionable Status Register. */

			s = g_variant_get_string(*data, NULL);
			otc_atoi(s, &reg);
			g_variant_unref(*data);
			*data = g_variant_new_boolean(reg & (1 << 4));
		}
	}

	return ret;
}

static int config_set(uint32_t key, GVariant *data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct dev_context *devc;
	double d;
	int channel_group_cmd;
	char *channel_group_name;
	int ret;

	if (!sdi)
		return OTC_ERR_ARG;

	channel_group_cmd = 0;
	channel_group_name = NULL;
	if (cg) {
		channel_group_cmd = SCPI_CMD_SELECT_CHANNEL;
		channel_group_name = g_strdup(cg->name);
	}

	devc = sdi->priv;

	switch (key) {
	case OTC_CONF_ENABLED:
		if (g_variant_get_boolean(data))
			ret = otc_scpi_cmd(sdi, devc->device->commands,
					channel_group_cmd, channel_group_name,
					SCPI_CMD_SET_OUTPUT_ENABLE,
				        channel_group_name);
		else
			ret = otc_scpi_cmd(sdi, devc->device->commands,
					channel_group_cmd, channel_group_name,
					SCPI_CMD_SET_OUTPUT_DISABLE,
					channel_group_name);
		break;
	case OTC_CONF_VOLTAGE_TARGET:
		d = g_variant_get_double(data);
		if (devc->device->dialect == SCPI_DIALECT_SIGLENT) {
			ret = otc_scpi_cmd(sdi, devc->device->commands,
					  channel_group_cmd, channel_group_name,
					  SCPI_CMD_SET_VOLTAGE_TARGET,
					  channel_group_name, d);
		} else {
			ret = otc_scpi_cmd(sdi, devc->device->commands,
					  channel_group_cmd, channel_group_name,
					  SCPI_CMD_SET_VOLTAGE_TARGET, d);
		}
		break;
	case OTC_CONF_OUTPUT_FREQUENCY_TARGET:
		d = g_variant_get_double(data);
		ret = otc_scpi_cmd(sdi, devc->device->commands,
				channel_group_cmd, channel_group_name,
				SCPI_CMD_SET_FREQUENCY_TARGET, d);
		break;
	case OTC_CONF_CURRENT_LIMIT:
		d = g_variant_get_double(data);
		if (devc->device->dialect == SCPI_DIALECT_SIGLENT) {
			ret = otc_scpi_cmd(sdi, devc->device->commands,
					  channel_group_cmd, channel_group_name,
					  SCPI_CMD_SET_CURRENT_LIMIT,
					  channel_group_name, d);
		} else {
			ret = otc_scpi_cmd(sdi, devc->device->commands,
					  channel_group_cmd, channel_group_name,
					  SCPI_CMD_SET_CURRENT_LIMIT, d);
		}
		break;
	case OTC_CONF_OVER_VOLTAGE_PROTECTION_ENABLED:
		if (g_variant_get_boolean(data))
			ret = otc_scpi_cmd(sdi, devc->device->commands,
					channel_group_cmd, channel_group_name,
					SCPI_CMD_SET_OVER_VOLTAGE_PROTECTION_ENABLE);
		else
			ret = otc_scpi_cmd(sdi, devc->device->commands,
					channel_group_cmd, channel_group_name,
					SCPI_CMD_SET_OVER_VOLTAGE_PROTECTION_DISABLE);
		break;
	case OTC_CONF_OVER_VOLTAGE_PROTECTION_THRESHOLD:
		d = g_variant_get_double(data);
		ret = otc_scpi_cmd(sdi, devc->device->commands,
				channel_group_cmd, channel_group_name,
				SCPI_CMD_SET_OVER_VOLTAGE_PROTECTION_THRESHOLD, d);
		break;
	case OTC_CONF_OVER_CURRENT_PROTECTION_ENABLED:
		if (g_variant_get_boolean(data))
			ret = otc_scpi_cmd(sdi, devc->device->commands,
					channel_group_cmd, channel_group_name,
					SCPI_CMD_SET_OVER_CURRENT_PROTECTION_ENABLE);
		else
			ret = otc_scpi_cmd(sdi, devc->device->commands,
					channel_group_cmd, channel_group_name,
					SCPI_CMD_SET_OVER_CURRENT_PROTECTION_DISABLE);
		break;
	case OTC_CONF_OVER_CURRENT_PROTECTION_THRESHOLD:
		d = g_variant_get_double(data);
		ret = otc_scpi_cmd(sdi, devc->device->commands,
				channel_group_cmd, channel_group_name,
				SCPI_CMD_SET_OVER_CURRENT_PROTECTION_THRESHOLD, d);
		break;
	case OTC_CONF_OVER_CURRENT_PROTECTION_DELAY:
		d = g_variant_get_double(data);
		ret = otc_scpi_cmd(sdi, devc->device->commands,
				channel_group_cmd, channel_group_name,
				SCPI_CMD_SET_OVER_CURRENT_PROTECTION_DELAY, d);
		break;
	case OTC_CONF_OVER_TEMPERATURE_PROTECTION:
		if (g_variant_get_boolean(data))
			ret = otc_scpi_cmd(sdi, devc->device->commands,
					channel_group_cmd, channel_group_name,
					SCPI_CMD_SET_OVER_TEMPERATURE_PROTECTION_ENABLE);
		else
			ret = otc_scpi_cmd(sdi, devc->device->commands,
					channel_group_cmd, channel_group_name,
					SCPI_CMD_SET_OVER_TEMPERATURE_PROTECTION_DISABLE);
		break;
	case OTC_CONF_CHANNEL_CONFIG:
		{
			const char *s = g_variant_get_string(data, NULL);
			if (devc->device->dialect == SCPI_DIALECT_SIGLENT) {
				if (!strncmp(s, "Parallel", 8))
					s = "2";
				else if (!strncmp(s, "Series", 6))
					s = "1";
				else
					s = "0";
			}
			ret = otc_scpi_cmd(sdi, devc->device->commands,
					  channel_group_cmd, channel_group_name,
					  SCPI_CMD_SET_CHANNEL_CONFIG, s);
		}
		break;
	default:
		ret = otc_sw_limits_config_set(&devc->limits, key, data);
	}

	g_free(channel_group_name);

	return ret;
}

static int config_list(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct dev_context *devc;
	struct otc_channel *ch;
	struct pps_channel *pch;
	const struct channel_spec *ch_spec;
	int i;
	const char *s[16];

	devc = (sdi) ? sdi->priv : NULL;

	if (!cg) {
		switch (key) {
		case OTC_CONF_SCAN_OPTIONS:
		case OTC_CONF_DEVICE_OPTIONS:
			return std_opts_config_list(key, data, sdi, cg,
				ARRAY_AND_SIZE(scanopts),
				ARRAY_AND_SIZE(drvopts),
				(devc && devc->device) ? devc->device->devopts : NULL,
				(devc && devc->device) ? devc->device->num_devopts : 0);
			break;
		case OTC_CONF_CHANNEL_CONFIG:
			if (!devc || !devc->device)
				return OTC_ERR_ARG;
			/* Not used. */
			i = 0;
			if (devc->device->features & PPS_INDEPENDENT)
				s[i++] = "Independent";
			if (devc->device->features & PPS_SERIES)
				s[i++] = "Series";
			if (devc->device->features & PPS_PARALLEL)
				s[i++] = "Parallel";
			if (i == 0) {
				/*
				 * Shouldn't happen: independent-only devices
				 * shouldn't advertise this option at all.
				 */
				return OTC_ERR_NA;
			}
			*data = g_variant_new_strv(s, i);
			break;
		default:
			return OTC_ERR_NA;
		}
	} else {
		/*
		 * Per-channel-group options depending on a channel are actually
		 * done with the first channel. Channel groups in PPS can have
		 * more than one channel, but they will typically be of equal
		 * specification for use in series or parallel mode.
		 */
		ch = cg->channels->data;
		pch = ch->priv;
		if (!devc || !devc->device)
			return OTC_ERR_ARG;
		ch_spec = &(devc->device->channels[pch->hw_output_idx]);

		switch (key) {
		case OTC_CONF_DEVICE_OPTIONS:
			*data = std_gvar_array_u32(devc->device->devopts_cg, devc->device->num_devopts_cg);
			break;
		case OTC_CONF_VOLTAGE_TARGET:
			*data = std_gvar_min_max_step_array(ch_spec->voltage);
			break;
		case OTC_CONF_OUTPUT_FREQUENCY_TARGET:
			*data = std_gvar_min_max_step_array(ch_spec->frequency);
			break;
		case OTC_CONF_CURRENT_LIMIT:
			*data = std_gvar_min_max_step_array(ch_spec->current);
			break;
		case OTC_CONF_OVER_VOLTAGE_PROTECTION_THRESHOLD:
			*data = std_gvar_min_max_step_array(ch_spec->ovp);
			break;
		case OTC_CONF_OVER_CURRENT_PROTECTION_THRESHOLD:
			*data = std_gvar_min_max_step_array(ch_spec->ocp);
			break;
		case OTC_CONF_OVER_CURRENT_PROTECTION_DELAY:
			*data = std_gvar_min_max_step_array(ch_spec->ocp_delay);
			break;
		default:
			return OTC_ERR_NA;
		}
	}

	return OTC_OK;
}

static int dev_acquisition_start(const struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	struct otc_scpi_dev_inst *scpi;
	int ret;

	devc = sdi->priv;
	scpi = sdi->conn;

	/* Prime the pipe with the first channel. */
	devc->cur_acquisition_channel = otc_next_enabled_channel(sdi, NULL);

	/* Device specific initialization before acquisition starts. */
	if (devc->device->init_acquisition)
		devc->device->init_acquisition(sdi);

	if ((ret = otc_scpi_source_add(sdi->session, scpi, G_IO_IN, 10,
			scpi_pps_receive_data, (void *)sdi)) != OTC_OK)
		return ret;
	std_session_send_df_header(sdi);
	otc_sw_limits_acquisition_start(&devc->limits);

	return OTC_OK;
}

static int dev_acquisition_stop(struct otc_dev_inst *sdi)
{
	struct otc_scpi_dev_inst *scpi;

	scpi = sdi->conn;

	otc_scpi_source_remove(sdi->session, scpi);

	std_session_send_df_end(sdi);

	return OTC_OK;
}

static struct otc_dev_driver scpi_pps_driver_info = {
	.name = "scpi-pps",
	.longname = "SCPI PPS",
	.api_version = 1,
	.init = std_init,
	.cleanup = std_cleanup,
	.scan = scan_scpi_pps,
	.dev_list = std_dev_list,
	.dev_clear = dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.context = NULL,
};

static struct otc_dev_driver hp_ib_pps_driver_info = {
	.name = "hpib-pps",
	.longname = "HP-IB PPS",
	.api_version = 1,
	.init = std_init,
	.cleanup = std_cleanup,
	.scan = scan_hpib_pps,
	.dev_list = std_dev_list,
	.dev_clear = dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.context = NULL,
};
OTC_REGISTER_DEV_DRIVER(scpi_pps_driver_info);
OTC_REGISTER_DEV_DRIVER(hp_ib_pps_driver_info);
