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

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include "global.h"
#include "x10state.h"
#include "decode.h"

/* 16 house code and 16 units code = 256 devices
 * For normal X10 devices.
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
/* house/unit/func state per house code
 * 0 = house/unit code, 1 = house/function */
static int X10protostate[16];

void hua_sec_init(void) 
{
    memset(HouseUnitState, 0, sizeof(HouseUnitState));
    memset(HouseUnitSelected, 0, sizeof(HouseUnitSelected));
    memset(X10protostate, 0, sizeof(X10protostate));

    memset(X10sensors, 0, sizeof(X10sensors));
    X10sensorcount = 0;
}

#define issecfunc(x) (((x & 0xF0) == 0x80) || ((x & 0xF0) == 0x00))

void hua_sec_event(unsigned char *secaddr, unsigned int funcint, 
        unsigned int secaddr8)
{
    int i;
    unsigned long secaddr32;
    x10secsensor_t *sen;

    secaddr32 = (secaddr[0] << 16) | (secaddr[1] << 8) | secaddr[2] ;
    dbprintf("secaddr32 %X func %X issecfunc %d\n", 
            secaddr32, funcint, issecfunc(funcint));

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
    char buf[2048];

    strcpy(buf, "Selected:");
    for (h = 0; h < 16; h++) {
        for (u = 0; u < 16; u++) {
            if (HouseUnitSelected[h][u])
                sprintf(buf+strlen(buf), "%c%d,", h+'A', u+1);
        }
    }
    strcat(buf, "\n");
    dbprintf(buf);

    strcpy(buf, "State: ");
    for (h = 0; h < 16; h++) {
        for (u = 0; u < 16; u++) {
            if (HouseUnitState[h][u])
                sprintf(buf+strlen(buf), "%c%d %c,", h+'A', u+1, 
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
    hua_dbprint();
}

static void hua_func_all(int house, unsigned char func)
{
    int u;

    X10protostate[house] = 1;
    for (u = 0; u < 16; u++) {
        HouseUnitState[house][u] = func;
    }
    hua_dbprint();
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

    dbprintf("%s(%d,%d)\n", __func__, house, func);
    X10protostate[house] = 1;
    for (u = 0; u < 16; u++) {
        if (HouseUnitSelected[house][u])
            HouseUnitState[house][u] = func;
    }
    hua_dbprint();
    dbprintf("%s exit\n", __func__);
}

void hua_func_on(int house)
{
    hua_func(house, '1');
}

void hua_func_off(int house)
{
    hua_func(house, '0');
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
        sockprintf(fd, "Sensor addr: %X Last: %02d:%02d %s \n", sen->secaddr,
                mins, deltat, message);
    }

    sockprintf(fd, "End status\n");
}
