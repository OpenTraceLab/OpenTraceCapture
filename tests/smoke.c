#include "../include/opentracecapture/libopentracecapture.h"
#include <stdio.h>

int main(void) {
    struct otc_context *ctx;
    
    if (otc_init(&ctx) != OTC_OK) {
        printf("FAIL: otc_init() failed\n");
        return 1;
    }
    
    printf("OK: OpenTraceCapture smoke test passed\n");
    otc_exit(ctx);
    return 0;
}
