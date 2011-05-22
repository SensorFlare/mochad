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
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include "global.h"
#include "x10state.h"
#include "decode.h"

/* 16 house codes and 16 unit codes = 256 devices
 * For normal (non-security) X10 devices.
 */
typedef unsigned char houseunitaddrs_t[16][16];

/* For X10 security sensors such as MS10A motion sensor and DS10A 
 * door/window sensor
 */
typedef struct _x10secsensor {
    unsigned long secaddr;
    time_t        lastupdate;
    unsigned char secaddr8;     /* 0 17 bit addr, 1 8 bit addr */
    unsigned char sensorstatus;
} x10secsensor_t;

static x10secsensor_t X10sensors[512];
static unsigned int  X10sensorcount = 0;

/* 0x00 = unknown, '1' = ON, '0' = OFF */
static houseunitaddrs_t HouseUnitState;
/* 0x00 = not selected, '1' = selected */
static houseunitaddrs_t HouseUnitSelected;
/* 0..255 dim as set by xdim command */
static houseunitaddrs_t HouseUnitDim;
/* house/unit/func state per house code
 * 0 = house/unit code, 1 = house/function */
static int X10protostate[16];

/* HouseUnitSelect and X10protostate are needed because of the following case.
 * pl b3    # Select b3
 * pl b4    # Select b4
 * pl b on  # Turn on b3 and b4
 */

void hua_sec_init(void) 
{
    memset(HouseUnitState, 0, sizeof(HouseUnitState));
    memset(HouseUnitSelected, 0, sizeof(HouseUnitSelected));
    memset(HouseUnitDim, 0, sizeof(HouseUnitDim));
    memset(X10protostate, 0, sizeof(X10protostate));

    memset(X10sensors, 0, sizeof(X10sensors));
    X10sensorcount = 0;
}

#define issecfunc(x) (((x & 0xF0) == 0x80) || ((x & 0xF0) == 0x00))

/* Remember RF security event */
void hua_sec_event(unsigned char *secaddr, unsigned int funcint, 
        unsigned int secaddr8)
{
    int i;
    unsigned long secaddr32;
    x10secsensor_t *sen;

    secaddr32 = (secaddr[0] << 16) | (secaddr[1] << 8) | secaddr[2] ;
    /* dbprintf("secaddr32 %X func %X issecfunc %d\n", 
            secaddr32, funcint, issecfunc(funcint)); */

    for (i = 0; i < X10sensorcount; i++) {
        sen = &X10sensors[i];
        if (secaddr32 == sen->secaddr) {
            sen->sensorstatus = funcint;
            //if ((funcint & (1<<7)) == 0) {
                sen->lastupdate = time(NULL);
            //}
            return;
        }
    }

    /* Add new device */
    if ((X10sensorcount + 1) >= (sizeof(X10sensors)/sizeof(x10secsensor_t)))
        return;
//    if (issecfunc(funcint)) {
        sen = &X10sensors[X10sensorcount];
        sen->secaddr = secaddr32;
        sen->secaddr8 = secaddr8;
        sen->sensorstatus = funcint;
        sen->lastupdate = time(NULL);
        X10sensorcount++;
//    }
}

static void hua_dbprint(void)
{
    int h, u;
    char buf[4096];

    strcpy(buf, "Selected:");
    for (h = 0; h < 16; h++) {
        for (u = 0; u < 16; u++) {
            if (HouseUnitSelected[h][u])
                snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "%c%d,", h+'A', u+1);
        }
    }
    strcat(buf, "\n");
    dbprintf(buf);

    strcpy(buf, "State: ");
    for (h = 0; h < 16; h++) {
        for (u = 0; u < 16; u++) {
            if (HouseUnitState[h][u])
                snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "%c%d %c,", h+'A', u+1, 
                        HouseUnitState[h][u]);
        }
    }
    strcat(buf, "\n");
    dbprintf(buf);
}

static void hua_init(int house)
{
    memset(&HouseUnitSelected[house], 0, sizeof(HouseUnitSelected)/16);
}

unsigned char hua_getstatus(int house, int unit)
{
    return HouseUnitState[house][unit];
}

unsigned char hua_getstatus_xdim(int house, int unit)
{
    return HouseUnitDim[house][unit];
}

void hua_setstatus_xdim(int house, int xdim)
{
    int u;

    /* dbprintf("%s(%d,%d)\n", __func__, house, xdim); */
    X10protostate[house] = 1;
    for (u = 0; u < 16; u++) {
        if (HouseUnitSelected[house][u]) {
            HouseUnitDim[house][u] = xdim;
            if (xdim > 0)
                HouseUnitState[house][u] = '1';
            else
                HouseUnitState[house][u] = '0';
        }
    }
}

void hua_add(int house, int unit)
{
    switch (X10protostate[house]) {
        case 0:
            HouseUnitSelected[house][unit] = '1';
            break;
        case 1:
            hua_init(house);
            HouseUnitSelected[house][unit] = '1';
            X10protostate[house] = 0;
            break;
        default:
            dbprintf("Invalid state\n");
    }
    // hua_dbprint();
}

static void hua_func_all(int house, unsigned char func)
{
    int u;

    X10protostate[house] = 1;
    for (u = 0; u < 16; u++) {
        HouseUnitState[house][u] = func;
    }
    // hua_dbprint();
}

void hua_func_all_on(int house)
{
    hua_func_all(house, '1');
}

void hua_func_all_off(int house)
{
    hua_func_all(house, '0');
}

static void hua_func(int house, unsigned char func)
{
    int u;

    /* dbprintf("%s(%d,%d)\n", __func__, house, func); */
    X10protostate[house] = 1;
    for (u = 0; u < 16; u++) {
        if (HouseUnitSelected[house][u]) {
            HouseUnitState[house][u] = func;
            if (func == '1')
                HouseUnitDim[house][u] = 63;
            else
                HouseUnitDim[house][u] = 0;
        }
    }
    // hua_dbprint();
    /* dbprintf("%s exit\n", __func__); */
}

void hua_func_on(int house)
{
    hua_func(house, '1');
}

void hua_func_off(int house)
{
    hua_func(house, '0');
}

int hua_getstatus_sec(int rf8bitaddr, unsigned long rfaddr)
{
    x10secsensor_t *sen;
    int sensor;

    dbprintf("hua_getstatus_sec(%d,%X)\n", rf8bitaddr, rfaddr);
    for (sensor = 0; sensor < X10sensorcount; sensor++) {
        sen = &X10sensors[sensor];

        dbprintf("hua_getstatus_sec addr8 %d addr %X status %X\n",
                sen->secaddr8, sen->secaddr, sen->sensorstatus);
        if (rf8bitaddr && sen->secaddr8 && (rfaddr == sen->secaddr)) {
            switch (sen->sensorstatus) {
                case 0x04: return 1;    /* alert */
                case 0x84: return 0;    /* normal */
            }
        }
        else if (!rf8bitaddr && !sen->secaddr8 && (rfaddr == sen->secaddr)) {
            switch (sen->sensorstatus) {
                case 0x00:
                case 0x01:
                case 0x04:
                case 0x05:
                case 0x0C:
                case 0x0D:
                    return 1;    /* alert */
                case 0x80:
                case 0x81:
                case 0x84:
                case 0x85:
                case 0x8C:
                case 0x8D:
                    return 0;    /* normal */
            }
        }
    }
    return -1;
}

static int cmpX10sensor(const void *e1, const void *e2)
{
    const x10secsensor_t *sen1 = e1, *sen2 = e2;

    return sen1->secaddr - sen2->secaddr;
}

void hua_show(int fd)
{
    int h, u;
    char buf[2048];
    int labeldone;
    int sensor;
    x10secsensor_t *sen;

    sockprintf(fd, "Device selected\n");
    for (h = 0; h < 16; h++) {
        labeldone = 0;
        buf[0] = '\0';
        for (u = 0; u < 16; u++) {
            if (HouseUnitSelected[h][u]) {
                if (!labeldone) {
                    labeldone = 1;
                    snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), 
                            "House %c: %d", h+'A', u+1);
                }
                else 
                    snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), 
                            ",%d", u+1);
            }
        }
        if (labeldone) {
            strcat(buf, "\n");
            sockprintf(fd, buf, strlen(buf));
        }
    }
    
    sockprintf(fd, "Device status\n");
    for (h = 0; h < 16; h++) {
        labeldone = 0;
        buf[0] = '\0';
        for (u = 0; u < 16; u++) {
            if (HouseUnitState[h][u]) {
                if (!labeldone) {
                    labeldone = 1;
                    snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), 
                            "House %c: %d=%c", h+'A', u+1, HouseUnitState[h][u]);
                }
                else
                    snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), 
                            ",%d=%c", u+1, HouseUnitState[h][u]);
            }
        }
        if (labeldone) {
            strcat(buf, "\n");
            sockprintf(fd, buf, strlen(buf));
        }
    }
    sockprintf(fd, "Security sensor status\n");
    qsort(X10sensors, X10sensorcount, sizeof(X10sensors[0]), cmpX10sensor);
    for (sensor = 0; sensor < X10sensorcount; sensor++) {
        time_t deltat, mins;
        const char *message;

        sen = &X10sensors[sensor];
        deltat = time(NULL) - sen->lastupdate;
        mins = deltat / 60;
        deltat = deltat - (mins * 60);
        if (sen->secaddr8)
            message = findSecRemoteKeyName(sen->sensorstatus);
        else
            message = findSecEventName(sen->sensorstatus);
        sockprintf(fd, "Sensor addr: %06X Last: %02d:%02d %s \n", sen->secaddr,
                mins, deltat, message);
    }

    sockprintf(fd, "End status\n");
}
