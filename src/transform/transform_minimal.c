/*
 * Minimal transform module registry for OpenTraceCapture
 */

#include <config.h>
#include <opentracecapture/libopentracecapture.h>
#include "../libopentracecapture-internal.h"

/** @cond PRIVATE */
extern OTC_PRIV struct otc_transform_module transform_nop;
extern OTC_PRIV struct otc_transform_module transform_scale;
extern OTC_PRIV struct otc_transform_module transform_invert;
/** @endcond */

static const struct otc_transform_module *transform_module_list[] = {
	&transform_nop,
	&transform_scale,
	&transform_invert,
	NULL,
};

/**
 * Returns a NULL-terminated list of all available transform modules.
 */
OTC_API const struct otc_transform_module **otc_transform_list(void)
{
	return transform_module_list;
}
