#ifndef LIBSIGROK_HARDWARE_LABJACK_U12_PROTOCOL_H
#define LIBSIGROK_HARDWARE_LABJACK_U12_PROTOCOL_H

#include <stdint.h>
#include <glib.h>
#include <stdbool.h>
#include <opentracecapture/opentracecapture.h>
#include "opentracecapture-internal.h"

#define LOG_PREFIX "labjack-u12"

#define LABJACK_VENDOR_ID  0x0cd5
#define LABJACK_PRODUCT_ID 0x0001

/* USB communication constants */
#define LABJACK_USB_INTERFACE     0
#define LABJACK_USB_TIMEOUT_MS    250   /* Reduced timeout for faster retry */
#define LABJACK_USB_ENDPOINT_OUT  0x02  /* EP 2 OUT from descriptor */
#define LABJACK_USB_ENDPOINT_IN   0x81  /* EP 1 IN from descriptor */

/* LabJack U12 USB packet structure */
#define LABJACK_USB_PACKET_SIZE   8   /* U12 uses 8-byte packets */

/* LabJack U12 command constants - Official Protocol */
#define LABJACK_CMD_ANALOG_INPUT     0xF8  /* Analog Input (SE/Diff) */
#define LABJACK_CMD_ANALOG_OUTPUT    0xF9  /* Analog Output Set */
#define LABJACK_CMD_DIGITAL_OUTPUT   0xF5  /* Digital Output Set */
#define LABJACK_CMD_DIGITAL_INPUT    0xF6  /* Digital Input Read / EEPROM Read */
#define LABJACK_CMD_COUNTER_ENABLE   0xF2  /* Counter Enable/Disable */
#define LABJACK_CMD_COUNTER_READ     0xF3  /* Counter Read */
#define LABJACK_CMD_WATCHDOG         0xF4  /* Watchdog Configure */
#define LABJACK_CMD_EEPROM_READ      0xF6  /* EEPROM Read (same as digital input) */
#define LABJACK_CMD_EEPROM_WRITE     0xF7  /* EEPROM Write */
#define LABJACK_CMD_SYSTEM           0xFA  /* System/Special commands */

/* Legacy command names for compatibility */
#define LABJACK_CMD_RESET            0x99  /* Reset (if supported) */
#define LABJACK_CMD_AI_SAMPLE        LABJACK_CMD_ANALOG_INPUT
#define LABJACK_CMD_AO_UPDATE        LABJACK_CMD_ANALOG_OUTPUT
#define LABJACK_CMD_DIGITAL_IO       LABJACK_CMD_DIGITAL_INPUT
#define LABJACK_CMD_COUNTER          LABJACK_CMD_COUNTER_READ
#define LABJACK_CMD_BULK_IO          0x5F  /* Bulk I/O (if supported) */

/* AI sampling modes */
#define LABJACK_AI_SINGLE_ENDED   0x00
#define LABJACK_AI_DIFFERENTIAL   0x01

/* Digital I/O modes */
#define LABJACK_IO_INPUT          0x00
#define LABJACK_IO_OUTPUT_LOW     0x01
#define LABJACK_IO_OUTPUT_HIGH    0x02

/* Counter modes */
#define LABJACK_COUNTER_RESET     0x01
#define LABJACK_COUNTER_READ      0x02

/* Voltage ranges and conversion - U12 uses gain settings */
#define LABJACK_AI_GAIN_1X        0x00  /* ±10V (gain x1) */
#define LABJACK_AI_GAIN_2X        0x01  /* ±5V (gain x2) */
#define LABJACK_AI_GAIN_4X        0x02  /* ±2.5V (gain x4) */
#define LABJACK_AI_GAIN_5X        0x03  /* ±2V (gain x5) */
#define LABJACK_AI_GAIN_8X        0x04  /* ±1.25V (gain x8) */
#define LABJACK_AI_GAIN_10X       0x05  /* ±1V (gain x10) */
#define LABJACK_AI_GAIN_16X       0x06  /* ±0.625V (gain x16) */
#define LABJACK_AI_GAIN_20X       0x07  /* ±0.5V (gain x20) */

/* Legacy range definitions for compatibility */
#define LABJACK_AI_RANGE_10V      LABJACK_AI_GAIN_1X
#define LABJACK_AI_RANGE_5V       LABJACK_AI_GAIN_2X
#define LABJACK_AI_RANGE_2V       LABJACK_AI_GAIN_5X
#define LABJACK_AI_RANGE_1V       LABJACK_AI_GAIN_10X

#define LABJACK_AI_RESOLUTION_12BIT  4096
#define LABJACK_AI_MAX_VOLTAGE       10.0
#define LABJACK_AO_MAX_VOLTAGE       5.0
#define LABJACK_AO_RESOLUTION_12BIT  4096

/* Config keys as strings */
#define CONFIG_KEY_AI_MODE        "ai-mode"
#define CONFIG_KEY_AI_ENABLED     "ai-enabled"
#define CONFIG_KEY_AI_DIFF_ENABLED "ai-diff-enabled"
#define CONFIG_KEY_AO_VOLTAGE     "ao-voltage"
#define CONFIG_KEY_IO_MODE        "io-mode"
#define CONFIG_KEY_D_MODE         "d-mode"
#define CONFIG_KEY_COUNTER_VALUE  "counter"
#define CONFIG_KEY_IS_OPEN        "is-open"

/* Allowed values for analog input modes */
#define AI_MODE_SINGLE_ENDED_STR   "single-ended"
#define AI_MODE_DIFFERENTIAL_STR   "differential"

/* Allowed values for IO/D modes */
#define IO_MODE_INPUT_STR          "input"
#define IO_MODE_OUTPUT_LOW_STR     "output-low"
#define IO_MODE_OUTPUT_HIGH_STR    "output-high"

#define D_MODE_INPUT_STR           "input"
#define D_MODE_OUTPUT_LOW_STR      "output-low"
#define D_MODE_OUTPUT_HIGH_STR     "output-high"

/* USB packet structures - Official U12 8-byte protocol */
struct labjack_u12_packet {
	uint8_t command;
	uint8_t data[6];
	uint8_t checksum;  /* Usually 0 for simple commands */
};

struct labjack_u12_ai_request {
	uint8_t command;        /* LABJACK_CMD_ANALOG_INPUT (0xF8) */
	uint8_t channel_config; /* Channel + gain + mode (see below) */
	uint8_t reserved[5];    /* Must be 0 */
	uint8_t checksum;       /* Usually 0 */
};

/* Channel config byte format (byte 1 of AI request):
 * Bits 0-2: Channel number (0-7)
 * Bit 3:    Not used (0)
 * Bits 4-6: Gain setting (0-7, see LABJACK_AI_GAIN_*)
 * Bit 7:    Mode (0=Single-ended, 1=Differential)
 */
#define LABJACK_AI_CHANNEL_MASK   0x07
#define LABJACK_AI_GAIN_SHIFT     4
#define LABJACK_AI_GAIN_MASK      0x70
#define LABJACK_AI_DIFF_BIT       0x80

struct labjack_u12_ai_response {
	uint8_t command;       /* Echo of command (0xF8) */
	uint8_t channel_config; /* Echo of channel config */
	uint16_t raw_value;    /* 16-bit ADC value (little-endian) */
	uint8_t reserved[3];   /* Padding */
	uint8_t checksum;      /* Usually 0 */
};

struct labjack_u12_ao_request {
	uint8_t command;       /* LABJACK_CMD_ANALOG_OUTPUT (0xF9) */
	uint8_t dac0_value;    /* DAC0 value (0-255) */
	uint8_t dac1_value;    /* DAC1 value (0-255) */
	uint8_t reserved[4];   /* Must be 0 */
	uint8_t checksum;      /* Usually 0 */
};

struct labjack_u12_digital_output_request {
	uint8_t command;       /* LABJACK_CMD_DIGITAL_OUTPUT (0xF5) */
	uint8_t io_mask;       /* Which IOs to affect (bits 0-3) */
	uint8_t output_mask;   /* Which IOs to set high (bits 0-3) */
	uint8_t reserved[4];   /* Must be 0 */
	uint8_t checksum;      /* Usually 0 */
};

struct labjack_u12_digital_input_request {
	uint8_t command;       /* LABJACK_CMD_DIGITAL_INPUT (0xF6) */
	uint8_t address;       /* 0x00 for digital input, 0-63 for EEPROM */
	uint8_t reserved[5];   /* Must be 0 */
	uint8_t checksum;      /* Usually 0 */
};

struct labjack_u12_digital_input_response {
	uint8_t command;       /* Echo of command (0xF6) */
	uint8_t address;       /* Echo of address */
	uint8_t value;         /* Digital input state or EEPROM value */
	uint8_t reserved[4];   /* Padding */
	uint8_t checksum;      /* Usually 0 */
};

struct labjack_u12_digital_io_response {
	uint8_t command;       /* Echo of command */
	uint8_t io_state;      /* IO0-IO3 input state */
	uint16_t d_state;      /* D0-D15 input state */
	uint8_t reserved[4];   /* Padding */
};

struct labjack_u12_counter_request {
	uint8_t command;       /* LABJACK_CMD_COUNTER_READ (0xF3) or COUNTER_ENABLE (0xF2) */
	uint8_t operation;     /* For enable: 1=enable, 0=disable; For read: ignored */
	uint8_t reserved[5];   /* Must be 0 */
	uint8_t checksum;      /* Usually 0 */
};

struct labjack_u12_counter_response {
	uint8_t command;       /* Echo of command */
	uint8_t operation;     /* Echo of operation */
	uint32_t counter_value; /* 32-bit counter value (little-endian) */
	uint8_t reserved;      /* Padding */
	uint8_t checksum;      /* Usually 0 */
};

struct labjack_u12_bulk_io_request {
	uint8_t command;       /* LABJACK_CMD_BULK_IO */
	uint8_t ai_channels;   /* Bitmask of AI channels to read */
	uint8_t io_direction;  /* IO direction bits */
	uint8_t io_state;      /* IO output state */
	uint16_t d_direction;  /* D direction bits */
	uint8_t reserved;      /* Padding */
};

struct labjack_u12_bulk_io_response {
	uint8_t command;       /* Echo of command */
	uint16_t ai_values[4]; /* AI readings (up to 4 channels) */
	uint8_t io_state;      /* IO input state */
	uint16_t d_state;      /* D input state */
	uint8_t reserved;      /* Padding */
};

struct dev_context {
	/* Analog input configuration */
	enum {
		AI_MODE_SINGLE_ENDED,
		AI_MODE_DIFFERENTIAL
	} ai_mode;

	/* Per-AI-channel enablement (used in single-ended mode) */
	bool ai_enabled[8]; /* AI0–AI7 */

	/* In differential mode, each pair is treated as one channel: AI0+AI1, AI2+AI3, ... */
	bool ai_diff_enabled[4]; /* AI0/1, AI2/3, AI4/5, AI6/7 */

	/* AI voltage ranges per channel */
	uint8_t ai_range[8]; /* Voltage range for each AI channel */

	/* Analog outputs (0–5V), AO0–AO1 */
	float ao_voltage[2]; /* AO0, AO1 */

	/* IO0–IO3 configuration and state */
	enum {
		IO_MODE_INPUT,
		IO_MODE_OUTPUT_LOW,
		IO_MODE_OUTPUT_HIGH
	} io_mode[4]; /* IO0–IO3 */

	/* D0–D15 (raw digital I/O on DB25) configuration */
	enum {
		D_MODE_INPUT,
		D_MODE_OUTPUT_LOW,
		D_MODE_OUTPUT_HIGH
	} d_mode[16]; /* D0–D15 */

	/* Counter (CNT) state */
	uint32_t counter_value;

	/* Timestamp of last CNT sample, if needed */
	uint64_t counter_timestamp;

	/* Additional device state */
	bool is_open;

	/* USB communication state */
	struct otc_usb_dev_inst *usb;
	GMutex usb_mutex;  /* Protect USB operations */
	
	/* Acquisition state */
	uint64_t limit_samples;
	uint64_t num_samples;
	gboolean acquisition_running;
	gboolean continuous;
	
	/* Unified polling thread state */
	gboolean polling_thread_running; /* Polling thread control flag */
	GThread *polling_thread;         /* Unified polling thread */
	GMutex polling_mutex;            /* Protect polling state */
	GCond polling_cond;              /* Polling thread synchronization */
	uint32_t poll_interval_ms;       /* Polling interval in milliseconds */
	uint64_t samples_collected;      /* Total samples collected */
};

/* Helper functions for channel management */
OTC_PRIV bool labjack_u12_is_ai_channel_available(const struct dev_context *devc, int ai_index);
OTC_PRIV int labjack_u12_get_differential_pair(int ai_index);
OTC_PRIV bool labjack_u12_ai_channels_conflict(const struct dev_context *devc, int ai_index);

/* USB communication functions */
OTC_PRIV int labjack_u12_usb_write(const struct otc_dev_inst *sdi, 
                                  const void *data, size_t length);
OTC_PRIV int labjack_u12_usb_read(const struct otc_dev_inst *sdi, 
                                 void *data, size_t length);
OTC_PRIV int labjack_u12_send_command(const struct otc_dev_inst *sdi,
                                     const struct labjack_u12_packet *request,
                                     struct labjack_u12_packet *response);

/* Hardware abstraction functions */
OTC_PRIV int labjack_u12_read_ai_channel(const struct otc_dev_inst *sdi, 
                                        int channel, float *voltage);
OTC_PRIV int labjack_u12_write_ao_channel(const struct otc_dev_inst *sdi,
                                         int channel, float voltage);
OTC_PRIV int labjack_u12_read_digital_io(const struct otc_dev_inst *sdi,
                                        uint32_t *io_state, uint32_t *d_state);
OTC_PRIV int labjack_u12_write_digital_io(const struct otc_dev_inst *sdi,
                                         uint32_t io_direction, uint32_t io_state,
                                         uint32_t d_direction, uint32_t d_state);
OTC_PRIV int labjack_u12_enable_counter(const struct otc_dev_inst *sdi, gboolean enable);
OTC_PRIV int labjack_u12_read_counter(const struct otc_dev_inst *sdi, uint32_t *count);
OTC_PRIV int labjack_u12_reset_counter(const struct otc_dev_inst *sdi);
OTC_PRIV int labjack_u12_reset_device(const struct otc_dev_inst *sdi);
OTC_PRIV int labjack_u12_bulk_io(const struct otc_dev_inst *sdi,
                               uint8_t ai_channels, float *ai_voltages,
                               uint32_t io_direction, uint32_t io_state, uint32_t *io_input,
                               uint32_t d_direction, uint32_t d_state, uint32_t *d_input);

/* Utility functions */
OTC_PRIV float labjack_u12_raw_to_voltage(uint16_t raw_value, uint8_t range);
OTC_PRIV uint16_t labjack_u12_voltage_to_raw(float voltage);
OTC_PRIV int labjack_u12_unbind_hid_driver(int bus, int address);

/* Unified polling thread functions */
OTC_PRIV gpointer labjack_u12_polling_thread(gpointer data);
OTC_PRIV int labjack_u12_start_polling_acquisition(const struct otc_dev_inst *sdi);
OTC_PRIV int labjack_u12_stop_polling_acquisition(const struct otc_dev_inst *sdi);

OTC_PRIV int labjack_u12_receive_data(int fd, int revents, void *cb_data);

#endif
