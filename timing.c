/*
 * timing.c
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#include <sys/time.h>
#include <stdlib.h>

int timer_start(struct timeval *tv)
{
    gettimeofday(tv,NULL);
    return 0;
}

int timer_elapsed(struct timeval *tv, double *elapsed)
{
    struct timeval end;

    gettimeofday(&end,NULL);
    *elapsed = (end.tv_sec-tv->tv_sec)+((end.tv_usec-tv->tv_usec)/1000000.0);

    return 0;
}

double timer_remaining(struct timeval *tv, double curr, double total) {
    if( total <= 0 )
        return -1.0;    /* invalid total */
    if( curr <= 0 || curr > total ) {
        return -2.0;    /* curr outside valid range */
    }
    double progress = curr / total;
    double elapsed;
    timer_elapsed(tv,&elapsed);
    double est_total = (elapsed / progress);
    double ret = (1-progress) * est_total;
    return ret;
}
