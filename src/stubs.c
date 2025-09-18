/*
 * Minimal stub functions for missing dependencies
 */

#include <config.h>
#include <zip.h>
#include <opentracecapture/libopentracecapture.h>
#include "libopentracecapture-internal.h"

/* LZO stubs */
const char *lzo_version_string(void)
{
    return "none";
}

int __lzo_init_v2(unsigned v, int s1, int s2, int s3, int s4, int s5, int s6, int s7, int s8, int s9)
{
    (void)v; (void)s1; (void)s2; (void)s3; (void)s4; (void)s5; (void)s6; (void)s7; (void)s8; (void)s9;
    return 0;
}

/* ZIP stub */
OTC_PRIV void otc_zip_discard(struct zip *archive)
{
    if (archive)
        zip_discard(archive);
}
