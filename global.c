/*
 * Copyright 2010-2011 Brian Uechi <buasst@gmail.com>
 *
 * This file is part of mochad.
 *
 * mochad is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * mochad is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mochad.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "global.h"

int Cm19a = 0;

/* 1 bit per house code, 1=RF to PL, 0=off, default all house codes on */
unsigned short RfToPl16 = 0xFFFF;

unsigned short RfToRf16 = 0;


/* #define dbprintf(fmt,...) fprintf(stderr, "%s:%d:" fmt, __FILE__,__LINE__,__VA_ARGS__) */
#define dbprintf(fmt, ...) _dbprintf(fmt, __FILE__,__LINE__, ## __VA_ARGS__)
int _dbprintf(const char *fmt, ...)
{
    va_list args;
    char buf[1024];
    char fmtbig[1024];
    int buflen;

    va_start(args,fmt);
    strcpy(fmtbig, "%s:%d:");
    strcat(fmtbig, fmt);
    buflen = vsprintf(buf, fmtbig, args);
    va_end(args);
    return fprintf(stderr, "%s", buf);
}
