/*
 * This file is part of the opentracecapture project.
 *
 * Copyright (C) 2025 Carl-Fredrik Sundstrom <carl.f.sundstrom@gmail.com>
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
#include <sys/time.h>
#include <inttypes.h>
#include "protocol.h"
#include <libusb.h>
#include "opentracecapture/opentracecapture.h"
#include "opentracecapture-internal.h"

static struct otc_dev_driver labjack_u12_driver_info;

static GSList *dev_scan(struct otc_dev_driver *di, GSList *options)
{
    struct otc_dev_inst *sdi;
    struct otc_usb_dev_inst *usb;
    struct dev_context *devc;
    struct otc_channel *ch;
    struct otc_channel_group *cg;
    libusb_context *usb_ctx;
    libusb_device **devlist;
    libusb_device *dev;
    struct libusb_device_descriptor desc;
    ssize_t num_devs;
    int i, r, ch_idx;
    GSList *found_devs = NULL;
    char ch_name[16], cg_name[16];

    (void)options;

    if ((r = libusb_init(&usb_ctx)) != 0) {
        otc_err("libusb_init failed: %s.", libusb_error_name(r));
        return NULL;
    }

    num_devs = libusb_get_device_list(usb_ctx, &devlist);
    for (i = 0; i < num_devs; i++) {
        dev = devlist[i];
        r = libusb_get_device_descriptor(dev, &desc);
        if (r != 0)
            continue;

        if (desc.idVendor == LABJACK_VENDOR_ID && desc.idProduct == LABJACK_PRODUCT_ID) {
            sdi = g_malloc0(sizeof(struct otc_dev_inst));
            sdi->status = OTC_ST_INACTIVE;
            sdi->driver = di;
            sdi->vendor = g_strdup("LabJack");
            sdi->model = g_strdup("U12");

            /* Allocate device context */
            devc = g_malloc0(sizeof(struct dev_context));
            
            /* Initialize device context with defaults */
            devc->ai_mode = AI_MODE_SINGLE_ENDED; /* Default to single-ended */
            for (int j = 0; j < 8; j++) {
                devc->ai_enabled[j] = FALSE;
                devc->ai_range[j] = LABJACK_AI_RANGE_10V; /* Default to ±10V */
            }
            for (int j = 0; j < 4; j++)
                devc->ai_diff_enabled[j] = FALSE;
            for (int j = 0; j < 2; j++)
                devc->ao_voltage[j] = 0.0;
            for (int j = 0; j < 4; j++)
                devc->io_mode[j] = IO_MODE_INPUT;
            for (int j = 0; j < 16; j++)
                devc->d_mode[j] = D_MODE_INPUT;
            devc->counter_value = 0;
            devc->is_open = FALSE;
            
            /* Initialize USB communication */
            usb = g_malloc0(sizeof(struct otc_usb_dev_inst));
            usb->address = libusb_get_device_address(dev);
            usb->bus = libusb_get_bus_number(dev);
            devc->usb = usb;
            g_mutex_init(&devc->usb_mutex);
            
            /* Initialize acquisition state */
            devc->limit_samples = 0;
            devc->num_samples = 0;
            devc->acquisition_running = FALSE;
            devc->continuous = FALSE;
            
            sdi->priv = devc;
            sdi->conn = usb;

            ch_idx = 0;

            /* Create analog input channels (AI0-AI7) */
            /* Note: In differential mode, AI0+AI1, AI2+AI3, AI4+AI5, AI6+AI7 become pairs */
            for (int j = 0; j < 8; j++) {
                snprintf(ch_name, sizeof(ch_name), "AI%d", j);
                ch = otc_channel_new(sdi, ch_idx++, OTC_CHANNEL_ANALOG, FALSE, ch_name);
                sdi->channels = g_slist_append(sdi->channels, ch);
            }

            /* Create channel groups for differential pairs */
            for (int j = 0; j < 4; j++) {
                snprintf(cg_name, sizeof(cg_name), "AI%d+AI%d", j*2, j*2+1);
                cg = g_malloc0(sizeof(struct otc_channel_group));
                cg->name = g_strdup(cg_name);
                cg->channels = NULL;
                /* Add the two channels that form this differential pair */
                struct otc_channel *ch1 = g_slist_nth_data(sdi->channels, j*2);
                struct otc_channel *ch2 = g_slist_nth_data(sdi->channels, j*2+1);
                if (ch1 && ch2) {
                    cg->channels = g_slist_append(cg->channels, ch1);
                    cg->channels = g_slist_append(cg->channels, ch2);
                    sdi->channel_groups = g_slist_append(sdi->channel_groups, cg);
                } else {
                    otc_err("Failed to create differential pair AI%d+AI%d", j*2, j*2+1);
                    g_free(cg->name);
                    g_free(cg);
                }
            }

            /* Create analog output channels (AO0-AO1) */
            for (int j = 0; j < 2; j++) {
                snprintf(ch_name, sizeof(ch_name), "AO%d", j);
                ch = otc_channel_new(sdi, ch_idx++, OTC_CHANNEL_ANALOG, FALSE, ch_name);
                sdi->channels = g_slist_append(sdi->channels, ch);
                
                /* Create channel group for each AO */
                snprintf(cg_name, sizeof(cg_name), "AO%d", j);
                cg = g_malloc0(sizeof(struct otc_channel_group));
                cg->name = g_strdup(cg_name);
                cg->channels = g_slist_append(cg->channels, ch);
                sdi->channel_groups = g_slist_append(sdi->channel_groups, cg);
            }

            /* Create digital I/O channels (IO0-IO3) */
            for (int j = 0; j < 4; j++) {
                snprintf(ch_name, sizeof(ch_name), "IO%d", j);
                ch = otc_channel_new(sdi, ch_idx++, OTC_CHANNEL_LOGIC, FALSE, ch_name);
                sdi->channels = g_slist_append(sdi->channels, ch);
            }

            /* Create digital channels (D0-D15) */
            for (int j = 0; j < 16; j++) {
                snprintf(ch_name, sizeof(ch_name), "D%d", j);
                ch = otc_channel_new(sdi, ch_idx++, OTC_CHANNEL_LOGIC, FALSE, ch_name);
                sdi->channels = g_slist_append(sdi->channels, ch);
            }

            /* Create counter channel */
            ch = otc_channel_new(sdi, ch_idx++, OTC_CHANNEL_LOGIC, FALSE, "CNT");
            sdi->channels = g_slist_append(sdi->channels, ch);

            sdi->inst_type = OTC_INST_USB;

            found_devs = g_slist_append(found_devs, sdi);
        }
    }

    libusb_free_device_list(devlist, 1);
    libusb_exit(usb_ctx);

    return found_devs;
}



static int dev_open(struct otc_dev_inst *sdi)
{
	struct otc_usb_dev_inst *usb;
	struct dev_context *devc;
	libusb_device **devlist;
	struct libusb_device_descriptor des;
	libusb_device_handle *hdl;
	int ret, i;

	if (sdi->status != OTC_ST_INACTIVE) {
		otc_err("Device already open.");
		return OTC_ERR;
	}

	usb = sdi->conn;
	devc = sdi->priv;

	if (!usb || !devc)
		return OTC_ERR_BUG;

	/* Find and open the device */
	if (libusb_get_device_list(NULL, &devlist) < 0) {
		otc_err("Failed to get USB device list.");
		return OTC_ERR;
	}

	for (i = 0; devlist[i]; i++) {
		ret = libusb_get_device_descriptor(devlist[i], &des);
		if (ret != 0)
			continue;

		if (des.idVendor != LABJACK_VENDOR_ID || des.idProduct != LABJACK_PRODUCT_ID)
			continue;

		if (libusb_get_bus_number(devlist[i]) != usb->bus ||
		    libusb_get_device_address(devlist[i]) != usb->address)
			continue;

		if ((ret = libusb_open(devlist[i], &hdl)) < 0) {
			otc_err("Failed to open device: %s.", libusb_error_name(ret));
			break;
		}

		/* Try to unbind HID driver before claiming interface */
		labjack_u12_unbind_hid_driver(libusb_get_bus_number(devlist[i]), 
		                              libusb_get_device_address(devlist[i]));

		usb->devhdl = hdl;
		break;
	}

	libusb_free_device_list(devlist, 1);

	if (!usb->devhdl) {
		otc_err("Failed to find and open LabJack U12 device.");
		return OTC_ERR;
	}

	/* Try multiple approaches to detach kernel drivers */
	
	/* Method 1: Detach from interface 0 (HID interface) */
	if (libusb_kernel_driver_active(usb->devhdl, 0) == 1) {
		otc_info("Detaching HID driver from interface 0");
		ret = libusb_detach_kernel_driver(usb->devhdl, 0);
		if (ret == LIBUSB_SUCCESS) {
			otc_info("Successfully detached HID driver");
		} else {
			otc_warn("Failed to detach HID driver: %s", libusb_error_name(ret));
		}
	}

	/* Method 2: Detach from our target interface */
	if (libusb_kernel_driver_active(usb->devhdl, LABJACK_USB_INTERFACE) == 1) {
		otc_info("Detaching kernel driver from interface %d", LABJACK_USB_INTERFACE);
		ret = libusb_detach_kernel_driver(usb->devhdl, LABJACK_USB_INTERFACE);
		if (ret == LIBUSB_SUCCESS) {
			otc_info("Successfully detached kernel driver");
		} else {
			otc_warn("Failed to detach kernel driver: %s", libusb_error_name(ret));
		}
	}

	/* Small delay to let the system settle */
	g_usleep(100 * 1000); /* 100ms */

	/* Claim the interface */
	ret = libusb_claim_interface(usb->devhdl, LABJACK_USB_INTERFACE);
	if (ret < 0) {
		otc_err("Failed to claim USB interface %d: %s", 
		       LABJACK_USB_INTERFACE, libusb_error_name(ret));
		
		/* Try to claim interface 0 instead (HID interface) */
		otc_info("Attempting to claim interface 0 instead");
		ret = libusb_claim_interface(usb->devhdl, 0);
		if (ret < 0) {
			otc_err("Failed to claim interface 0: %s", libusb_error_name(ret));
			libusb_close(usb->devhdl);
			usb->devhdl = NULL;
			return OTC_ERR;
		} else {
			otc_info("Successfully claimed interface 0");
		}
	} else {
		otc_info("Successfully claimed interface %d", LABJACK_USB_INTERFACE);
	}

	sdi->status = OTC_ST_ACTIVE;
	devc->is_open = TRUE;

	/* Reset device to known state */
	ret = labjack_u12_reset_device(sdi);
	if (ret != OTC_OK) {
		otc_warn("Failed to reset device, continuing anyway.");
	}

	otc_info("LabJack U12 device opened successfully.");

	return OTC_OK;
}

static int dev_close(struct otc_dev_inst *sdi)
{
	struct otc_usb_dev_inst *usb;
	struct dev_context *devc;

	if (sdi->status != OTC_ST_ACTIVE)
		return OTC_OK;

	usb = sdi->conn;
	devc = sdi->priv;

	if (!usb || !devc)
		return OTC_ERR_BUG;

	/* Stop any ongoing acquisition */
	if (devc->acquisition_running) {
		devc->acquisition_running = FALSE;
	}

	/* Release USB interface and close device */
	if (usb->devhdl) {
		libusb_release_interface(usb->devhdl, LABJACK_USB_INTERFACE);
		
		/* Reattach kernel driver if it was detached */
		if (libusb_kernel_driver_active(usb->devhdl, LABJACK_USB_INTERFACE) == 0) {
			if (libusb_attach_kernel_driver(usb->devhdl, LABJACK_USB_INTERFACE) < 0) {
				otc_spew("Failed to reattach kernel driver");
			}
		}
		
		libusb_close(usb->devhdl);
		usb->devhdl = NULL;
	}

	sdi->status = OTC_ST_INACTIVE;
	devc->is_open = FALSE;

	otc_info("LabJack U12 device closed.");

	return OTC_OK;
}


static int config_get(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi,
	const struct otc_channel_group *cg)
{
	struct dev_context *devc = sdi->priv;

	(void)cg;

	if (!devc || !data)
		return OTC_ERR_ARG;

	switch (key) {
	case OTC_CONF_DEVICE_OPTIONS:
		/* Not used in get; handled in list */
		return OTC_ERR_NA;
	
	case OTC_CONF_DEVICE_MODE:
		*data = g_variant_new_string(
			(devc->ai_mode == AI_MODE_DIFFERENTIAL) ? "differential" : "single-ended");
		return OTC_OK;
	
	case OTC_CONF_VOLTAGE:
		/* For AO channels - this would need channel group context to determine AO0 vs AO1 */
		*data = g_variant_new_double(devc->ao_voltage[0]); /* Default to AO0 */
		return OTC_OK;
	
	case OTC_CONF_LIMIT_SAMPLES:
		*data = g_variant_new_uint64(devc->limit_samples);
		return OTC_OK;
	
	case OTC_CONF_CONTINUOUS:
		*data = g_variant_new_boolean(devc->continuous);
		return OTC_OK;
	
	case OTC_CONF_PATTERN_MODE:
		/* Used for digital I/O mode configuration */
		if (cg && cg->channels) {
			struct otc_channel *ch = cg->channels->data;
			if (ch && ch->type == OTC_CHANNEL_LOGIC) {
				const char *mode_str = "input";
				if (ch->index >= 10 && ch->index < 14) {
					/* IO channels */
					int io_idx = ch->index - 10;
					switch (devc->io_mode[io_idx]) {
					case IO_MODE_OUTPUT_LOW:
						mode_str = "output-low";
						break;
					case IO_MODE_OUTPUT_HIGH:
						mode_str = "output-high";
						break;
					default:
						mode_str = "input";
						break;
					}
				} else if (ch->index >= 14 && ch->index < 30) {
					/* D channels */
					int d_idx = ch->index - 14;
					switch (devc->d_mode[d_idx]) {
					case D_MODE_OUTPUT_LOW:
						mode_str = "output-low";
						break;
					case D_MODE_OUTPUT_HIGH:
						mode_str = "output-high";
						break;
					default:
						mode_str = "input";
						break;
					}
				}
				*data = g_variant_new_string(mode_str);
				return OTC_OK;
			}
		}
		return OTC_ERR_NA;
	
	default:
		return OTC_ERR_NA;
	}
}

static int config_set(uint32_t key, GVariant *data,
	const struct otc_dev_inst *sdi,
	const struct otc_channel_group *cg)
{
	struct dev_context *devc;
	struct otc_channel *ch;
	GSList *l;
	int i, pair_index;

	(void)cg;

	if (!sdi) {
		otc_err("Invalid device instance");
		return OTC_ERR_ARG;
	}

	devc = sdi->priv;
	if (!devc) {
		otc_err("Device context not initialized");
		return OTC_ERR_ARG;
	}

	if (!data) {
		otc_err("Configuration data is NULL");
		return OTC_ERR_ARG;
	}

	switch (key) {
	case OTC_CONF_DEVICE_MODE:
		{
			const char *mode_str = g_variant_get_string(data, NULL);
			enum { NEW_MODE_SINGLE_ENDED, NEW_MODE_DIFFERENTIAL } new_mode;
			
			if (g_strcmp0(mode_str, "differential") == 0) {
				new_mode = NEW_MODE_DIFFERENTIAL;
			} else if (g_strcmp0(mode_str, "single-ended") == 0) {
				new_mode = NEW_MODE_SINGLE_ENDED;
			} else {
				return OTC_ERR_ARG;
			}

			/* If mode is changing, update channel availability */
			if ((new_mode == NEW_MODE_DIFFERENTIAL && devc->ai_mode != AI_MODE_DIFFERENTIAL) ||
			    (new_mode == NEW_MODE_SINGLE_ENDED && devc->ai_mode != AI_MODE_SINGLE_ENDED)) {
				
				/* Disable all AI channels first */
				for (i = 0; i < 8; i++)
					devc->ai_enabled[i] = FALSE;
				for (i = 0; i < 4; i++)
					devc->ai_diff_enabled[i] = FALSE;

				/* Update channel enabled state in opentracecapture */
				for (l = sdi->channels; l; l = l->next) {
					ch = l->data;
					if (ch->type == OTC_CHANNEL_ANALOG && ch->index < 8) {
						/* AI0-AI7 channels */
						ch->enabled = FALSE; /* Start disabled, user can enable as needed */
					}
				}

				/* Update the mode */
				devc->ai_mode = (new_mode == NEW_MODE_DIFFERENTIAL) ? 
					AI_MODE_DIFFERENTIAL : AI_MODE_SINGLE_ENDED;
				
				otc_info("LabJack U12 AI mode changed to %s", mode_str);
			}
			return OTC_OK;
		}

	case OTC_CONF_ENABLED:
		{
			gboolean enabled = g_variant_get_boolean(data);
			
			/* This should be called with a channel group context to know which channel */
			if (!cg || !cg->channels) {
				otc_err("Channel enable/disable requires channel group context");
				return OTC_ERR_ARG;
			}
			
			/* Get the first channel in the group */
			ch = cg->channels->data;
			if (!ch) {
				return OTC_ERR_ARG;
			}
			
			/* Handle AI channel enable/disable */
			if (ch->type == OTC_CHANNEL_ANALOG && ch->index < 8) {
				int ai_index = ch->index;
				
				if (enabled) {
					/* Check if this channel can be enabled in current mode */
					if (!labjack_u12_is_ai_channel_available(devc, ai_index)) {
						otc_err("AI%d is not available in %s mode", ai_index,
							(devc->ai_mode == AI_MODE_DIFFERENTIAL) ? "differential" : "single-ended");
						return OTC_ERR_ARG;
					}
					
					if (labjack_u12_ai_channels_conflict(devc, ai_index)) {
						otc_err("AI%d conflicts with current channel configuration", ai_index);
						return OTC_ERR_ARG;
					}
					
					if (devc->ai_mode == AI_MODE_SINGLE_ENDED) {
						devc->ai_enabled[ai_index] = TRUE;
					} else {
						/* Differential mode - enable the pair */
						pair_index = labjack_u12_get_differential_pair(ai_index);
						if (pair_index >= 0 && pair_index < 4) {
							devc->ai_diff_enabled[pair_index] = TRUE;
							/* Also disable the individual channels that form this pair */
							devc->ai_enabled[ai_index] = FALSE;
							devc->ai_enabled[ai_index + 1] = FALSE;
						}
					}
				} else {
					/* Disable channel */
					if (devc->ai_mode == AI_MODE_SINGLE_ENDED) {
						devc->ai_enabled[ai_index] = FALSE;
					} else {
						pair_index = labjack_u12_get_differential_pair(ai_index);
						if (pair_index >= 0 && pair_index < 4) {
							devc->ai_diff_enabled[pair_index] = FALSE;
						}
					}
				}
				
				ch->enabled = enabled;
				otc_info("AI%d %s", ai_index, enabled ? "enabled" : "disabled");
			}
			
			return OTC_OK;
		}

	case OTC_CONF_VOLTAGE:
		{
			double voltage = g_variant_get_double(data);
			if (voltage < 0.0 || voltage > 5.0) {
				otc_err("AO voltage must be between 0.0 and 5.0V");
				return OTC_ERR_ARG;
			}
			
			/* Use channel group to determine which AO channel */
			if (cg && cg->channels) {
				ch = cg->channels->data;
				if (ch && ch->type == OTC_CHANNEL_ANALOG && ch->index >= 8 && ch->index < 10) {
					int ao_index = ch->index - 8; /* AO0 = index 8, AO1 = index 9 */
					devc->ao_voltage[ao_index] = voltage;
					otc_info("AO%d voltage set to %.3fV", ao_index, voltage);
					return OTC_OK;
				}
			}
			
			/* Fallback to AO0 if no channel group specified */
			devc->ao_voltage[0] = voltage;
			otc_info("AO0 voltage set to %.3fV (default)", voltage);
			return OTC_OK;
		}

	case OTC_CONF_LIMIT_SAMPLES:
		{
			uint64_t limit = g_variant_get_uint64(data);
			devc->limit_samples = limit;
			otc_info("Sample limit set to %" PRIu64, limit);
			return OTC_OK;
		}

	case OTC_CONF_CONTINUOUS:
		{
			gboolean continuous = g_variant_get_boolean(data);
			devc->continuous = continuous;
			otc_info("Continuous mode %s", continuous ? "enabled" : "disabled");
			return OTC_OK;
		}

	case OTC_CONF_PATTERN_MODE:
		{
			const char *mode_str = g_variant_get_string(data, NULL);
			int ret;
			
			if (!cg || !cg->channels) {
				otc_err("Digital I/O mode setting requires channel group context");
				return OTC_ERR_ARG;
			}
			
			ch = cg->channels->data;
			if (!ch || ch->type != OTC_CHANNEL_LOGIC) {
				otc_err("Pattern mode only applies to logic channels");
				return OTC_ERR_ARG;
			}
			
			/* Parse mode string */
			enum { NEW_MODE_INPUT, NEW_MODE_OUTPUT_LOW, NEW_MODE_OUTPUT_HIGH } new_mode;
			if (g_strcmp0(mode_str, "input") == 0) {
				new_mode = NEW_MODE_INPUT;
			} else if (g_strcmp0(mode_str, "output-low") == 0) {
				new_mode = NEW_MODE_OUTPUT_LOW;
			} else if (g_strcmp0(mode_str, "output-high") == 0) {
				new_mode = NEW_MODE_OUTPUT_HIGH;
			} else {
				otc_err("Invalid digital I/O mode: %s", mode_str);
				return OTC_ERR_ARG;
			}
			
			/* Apply to appropriate channel */
			if (ch->index >= 10 && ch->index < 14) {
				/* IO channels */
				int io_idx = ch->index - 10;
				switch (new_mode) {
				case NEW_MODE_INPUT:
					devc->io_mode[io_idx] = IO_MODE_INPUT;
					break;
				case NEW_MODE_OUTPUT_LOW:
					devc->io_mode[io_idx] = IO_MODE_OUTPUT_LOW;
					break;
				case NEW_MODE_OUTPUT_HIGH:
					devc->io_mode[io_idx] = IO_MODE_OUTPUT_HIGH;
					break;
				}
				otc_info("IO%d mode set to %s", io_idx, mode_str);
			} else if (ch->index >= 14 && ch->index < 30) {
				/* D channels */
				int d_idx = ch->index - 14;
				switch (new_mode) {
				case NEW_MODE_INPUT:
					devc->d_mode[d_idx] = D_MODE_INPUT;
					break;
				case NEW_MODE_OUTPUT_LOW:
					devc->d_mode[d_idx] = D_MODE_OUTPUT_LOW;
					break;
				case NEW_MODE_OUTPUT_HIGH:
					devc->d_mode[d_idx] = D_MODE_OUTPUT_HIGH;
					break;
				}
				otc_info("D%d mode set to %s", d_idx, mode_str);
			} else {
				otc_err("Invalid logic channel index: %d", ch->index);
				return OTC_ERR_ARG;
			}
			
			/* Apply the configuration to hardware if device is open */
			if (devc->is_open) {
				uint32_t io_direction = 0, io_state = 0;
				uint32_t d_direction = 0, d_state = 0;
				
				/* Build IO direction and state */
				for (int i = 0; i < 4; i++) {
					if (devc->io_mode[i] != IO_MODE_INPUT) {
						io_direction |= (1 << i);
						if (devc->io_mode[i] == IO_MODE_OUTPUT_HIGH) {
							io_state |= (1 << i);
						}
					}
				}
				
				/* Build D direction and state */
				for (int i = 0; i < 16; i++) {
					if (devc->d_mode[i] != D_MODE_INPUT) {
						d_direction |= (1 << i);
						if (devc->d_mode[i] == D_MODE_OUTPUT_HIGH) {
							d_state |= (1 << i);
						}
					}
				}
				
				/* Apply to hardware */
				ret = labjack_u12_write_digital_io(sdi, io_direction, io_state, 
				                                   d_direction, d_state);
				if (ret != OTC_OK) {
					otc_warn("Failed to apply digital I/O configuration to hardware");
				}
			}
			
			return OTC_OK;
		}

	default:
		return OTC_ERR_NA;
	}
}


static int config_list(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi,
	const struct otc_channel_group *cg)
{
	(void)sdi;

	switch (key) {
	case OTC_CONF_DEVICE_OPTIONS:
		if (!cg) {
			/* Device-wide options */
			const uint32_t opts[] = {
				OTC_CONF_DEVICE_MODE | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
				OTC_CONF_LIMIT_SAMPLES | OTC_CONF_GET | OTC_CONF_SET,
				OTC_CONF_CONTINUOUS | OTC_CONF_GET | OTC_CONF_SET,
			};
			*data = g_variant_new_fixed_array(G_VARIANT_TYPE_UINT32,
					opts, G_N_ELEMENTS(opts), sizeof(uint32_t));
		} else {
			/* Channel group specific options */
			const uint32_t opts[] = {
				OTC_CONF_ENABLED | OTC_CONF_GET | OTC_CONF_SET,
				OTC_CONF_VOLTAGE | OTC_CONF_GET | OTC_CONF_SET, /* For AO channels */
				OTC_CONF_PATTERN_MODE | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST, /* For digital I/O channels */
			};
			*data = g_variant_new_fixed_array(G_VARIANT_TYPE_UINT32,
					opts, G_N_ELEMENTS(opts), sizeof(uint32_t));
		}
		return OTC_OK;

	case OTC_CONF_DEVICE_MODE:
		{
			const char *modes[] = {"single-ended", "differential"};
			*data = g_variant_new_strv(modes, G_N_ELEMENTS(modes));
			return OTC_OK;
		}

	case OTC_CONF_VOLTAGE:
		{
			/* AO0/AO1 output is between 0–5V */
			double range[] = {0.0, 5.0};
			*data = g_variant_new_fixed_array(G_VARIANT_TYPE_DOUBLE,
								range, 2, sizeof(double));
			return OTC_OK;
		}

	case OTC_CONF_PATTERN_MODE:
		{
			const char *modes[] = {"input", "output-low", "output-high"};
			*data = g_variant_new_strv(modes, G_N_ELEMENTS(modes));
			return OTC_OK;
		}

	default:
		return OTC_ERR_NA;
	}
}


static int dev_acquisition_start(const struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	struct otc_datafeed_packet packet;
	struct otc_datafeed_header header;
	struct otc_datafeed_meta meta;
	struct otc_config *src;
	GSList *l;
	struct otc_channel *ch;
	int enabled_channels = 0;

	if (!sdi || !sdi->priv)
		return OTC_ERR_ARG;

	devc = sdi->priv;

	if (devc->acquisition_running) {
		otc_err("Acquisition already running.");
		return OTC_ERR;
	}

	if (!devc->is_open) {
		otc_err("Device not open.");
		return OTC_ERR_DEV_CLOSED;
	}

	/* Count enabled channels */
	for (l = sdi->channels; l; l = l->next) {
		ch = l->data;
		if (ch->enabled) {
			if ((ch->type == OTC_CHANNEL_ANALOG && ch->index < 8) ||  /* AI0-AI7 */
			    (ch->type == OTC_CHANNEL_LOGIC && ch->index >= 8)) {   /* IO0-IO3, D0-D15, CNT */
				enabled_channels++;
			}
		}
	}

	if (enabled_channels == 0) {
		otc_err("No channels enabled for acquisition.");
		return OTC_ERR;
	}

	/* Reset sample counter */
	devc->num_samples = 0;
	devc->acquisition_running = TRUE;

	/* Enable counter if CNT channel is enabled */
	for (l = sdi->channels; l; l = l->next) {
		ch = l->data;
		if (ch->enabled && strcmp(ch->name, "CNT") == 0) {
			int ret = labjack_u12_enable_counter(sdi, TRUE);
			if (ret != OTC_OK) {
				otc_err("Failed to enable counter");
			} else {
				otc_info("Counter enabled for acquisition");
			}
			break;
		}
	}

	/* Send header packet */
	packet.type = OTC_DF_HEADER;
	packet.payload = &header;
	header.feed_version = 1;
	gettimeofday(&header.starttime, NULL);
	otc_session_send(sdi, &packet);

	/* Send metadata */
	packet.type = OTC_DF_META;
	packet.payload = &meta;
	meta.config = NULL;

	src = otc_config_new(OTC_CONF_SAMPLERATE, g_variant_new_uint64(1)); /* 1 Hz for poll mode */
	meta.config = g_slist_append(meta.config, src);

	if (devc->limit_samples > 0) {
		src = otc_config_new(OTC_CONF_LIMIT_SAMPLES, g_variant_new_uint64(devc->limit_samples));
		meta.config = g_slist_append(meta.config, src);
	}

	otc_session_send(sdi, &packet);
	g_slist_free_full(meta.config, (GDestroyNotify)otc_config_free);

	/* Start polling timer - poll every 100ms */
	otc_session_source_add(sdi->session, -1, 0, 100, labjack_u12_receive_data, (void *)sdi);

	otc_info("LabJack U12 acquisition started (poll mode, %d channels enabled).", enabled_channels);

	return OTC_OK;
}

static int dev_acquisition_stop(struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	struct otc_datafeed_packet packet;

	if (!sdi || !sdi->priv)
		return OTC_ERR_ARG;

	devc = sdi->priv;

	if (!devc->acquisition_running) {
		otc_warn("Acquisition not running.");
		return OTC_OK;
	}

	devc->acquisition_running = FALSE;

	/* Remove polling source */
	otc_session_source_remove(sdi->session, -1);

	/* Send end packet */
	packet.type = OTC_DF_END;
	packet.payload = NULL;
	otc_session_send(sdi, &packet);

	otc_info("LabJack U12 acquisition stopped. Collected %" PRIu64 " samples.", devc->num_samples);

	return OTC_OK;
}

static struct otc_dev_driver labjack_u12_driver_info = {
	.name = "labjack-u12",
	.longname = "LabJack U12",
	.api_version = 1,
	.init = std_init,
	.cleanup = std_cleanup,
	.scan = dev_scan,
	.dev_list = std_dev_list,
	.dev_clear = std_dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.context = NULL,
};
OTC_REGISTER_DEV_DRIVER(labjack_u12_driver_info);
