/*
 * This file is part of the OpenTraceCapture project.
 *
 * Copyright (C) 2024 OpenTraceCapture Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include "../include/opentracecapture/libopentracecapture.h"

int main(void)
{
    printf("OpenTraceCapture smoke test\n");
    
    /* Basic library initialization test */
    struct otc_context *ctx;
    int ret = otc_init(&ctx);
    if (ret != OTC_OK) {
        printf("Failed to initialize OpenTraceCapture\n");
        return 1;
    }
    
    printf("OpenTraceCapture initialized successfully\n");
    otc_exit(ctx);
    
    return 0;
}
