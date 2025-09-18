/*
 * Minimal output module registry for OpenTraceCapture
 */

#include <config.h>
#include <opentracecapture/libopentracecapture.h>
#include "../libopentracecapture-internal.h"

/** @cond PRIVATE */
extern OTC_PRIV struct otc_output_module output_binary;
extern OTC_PRIV struct otc_output_module output_csv;
extern OTC_PRIV struct otc_output_module output_vcd;
/** @endcond */

static const struct otc_output_module *output_module_list[] = {
	&output_binary,
	&output_csv,
	&output_vcd,
	NULL,
};

/**
 * Returns a NULL-terminated list of all available output modules.
 */
OTC_API const struct otc_output_module **otc_output_list(void)
{
	return output_module_list;
}
