/*
 * Copyright 2010 Brian Uechi <buasst@gmail.com>
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

/* CM15A output FIFO
 * X10 I/O is very slow so store up pending output then wait for X10ACK
 * (0x55) before sending the next item from the queue.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include "global.h"
#include "x10_write.h"


typedef struct x10out {
    size_t outlen;
    unsigned char outdata[8];
} x10out_t;

static x10out_t Outrecs[256];
#define OUTPTRSSIZE             (sizeof(Outrecs)/sizeof(Outrecs[0]))
static int Outhead = 0;
static int Outtail = 0;
static int Outbusy = 0;

static int next_index(int idx)
{
    return ((idx + 1) % OUTPTRSSIZE);
}

static int add_x10out(unsigned char *buf, size_t buflen)
{
    int nxt;
    x10out_t *nxtrec;

    dbprintf("len %lu\n", buflen);
    if ((nxt = next_index(Outtail)) == Outhead) {
        dbprintf("Outptrs full Outhead/tail %d/%d\n", Outhead, Outtail);
        return -1;
    }

    nxtrec = &Outrecs[nxt];
    nxtrec->outlen = buflen;
    memcpy(nxtrec->outdata, buf, buflen);
    Outtail = nxt;
    return buflen;
}

int send_next_x10out(void)
{
    x10out_t *outrec;

    if (Outbusy) {
        dbprintf("Outhead Outtail %d/%d\n", Outhead, Outtail);
        if (Outhead == Outtail) {
            /* Empty */
            Outbusy = 0;
            PollTimeOut = -1;
        }
        else {
            Outhead = next_index(Outhead);
            outrec = &Outrecs[Outhead];
            write_usb(outrec->outdata, outrec->outlen);
        }
    }
    return 0;
}

int x10_write(unsigned char *buf, size_t buflen)
{
    dbprintf("Outbusy=%d\n", Outbusy);
    if (Outbusy) {
        add_x10out(buf, buflen);
    }
    else {
        Outbusy = 1;
        PollTimeOut = 2*1000;   /* 2 seconds */
        write_usb(buf, buflen);
    }
    return buflen;
}
