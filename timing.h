/*
 * timing.h
 * ndt: n-dimensional tracer
 *
 * Copyright (c) 2019 Bryan Franklin. All rights reserved.
 */
#include <sys/time.h>

int timer_start(struct timeval *tv);
int timer_elapsed(struct timeval *tv, double *elapsed);
double timer_remaining(struct timeval *tv, double curr, double total);
