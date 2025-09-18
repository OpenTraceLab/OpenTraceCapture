/*
 * Minimal input module registry for OpenTraceCapture
 */

#include <config.h>
#include <opentracecapture/libopentracecapture.h>
#include "../libopentracecapture-internal.h"

/** @cond PRIVATE */
extern OTC_PRIV struct otc_input_module input_binary;
extern OTC_PRIV struct otc_input_module input_null;
extern OTC_PRIV struct otc_input_module input_vcd;
extern OTC_PRIV struct otc_input_module input_wav;
/** @endcond */

static const struct otc_input_module *input_module_list[] = {
	&input_binary,
	&input_null,
	&input_vcd,
	&input_wav,
	NULL,
};

/**
 * Returns a NULL-terminated list of all available input modules.
 */
OTC_API const struct otc_input_module **otc_input_list(void)
{
	return input_module_list;
}
