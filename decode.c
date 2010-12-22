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
#include "global.h"
#include "decode.h"
#include "x10state.h"
#include "x10_write.h"
#include "encode.h"

union x10addr {
    unsigned char houseunit;    // Normalized codes 7..4 house 3..0 unit
    unsigned char security8;    // RF security code, old 8 bit 
    unsigned int  security17;   // RF security code, 17 bits 23..7 inclusive
};

enum eventfunc {
    EV_ON,EV_OFF,EV_DIM,EV_BRIGHT
};
typedef enum eventfunc eventfunc_t;

enum eventinterface {
    EV_RX_PLC=0,
    EV_RX_RF=1,
    EV_TX_PLC=2,
    EV_TX_RF=3
};

typedef enum eventinterface eventinterface_t;

typedef struct x10event {

    eventinterface_t evint;
    eventfunc_t evfunc;
    union {
        struct {
            char housec;            // 'A'..'P'
            unsigned char unitc;    // 1..16
        };
        unsigned char security8;    // 8 bit security code
        unsigned int  security17;   // 17 bit security code bits 23..7 
    };
} x10event_t;

/* For all input values 0..255, return 1 for odd parity, 0 for even. */
static const char Paritytable[256] = {
    0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,1,0,0,1,0,1,
    1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,
    1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,
    0,1,0,1,1,0,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,
    1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,
    0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,1,0,0,1,
    0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0
};

static const unsigned char HouseUnitTable[] = {
//  House code char Unit code
    'M',    //      13
    'E',    //       5
    'C',    //       3
    'K',    //      11
    'O',    //      15
    'G',    //       7
    'A',    //       1
    'I',    //       9
    'N',    //      14
    'F',    //       6
    'D',    //       4
    'L',    //      12
    'P',    //      15
    'H',    //       8
    'B',    //       2
    'J',    //      10
};

static const unsigned char HouseUnitTableRF[] = {
//  House code char House num
    'M',    //      00
    'N',    //      01
    'O',    //      02
    'P',    //      03
    'C',    //      04
    'D',    //      05
    'A',    //      06
    'B',    //      07
    'E',    //      08
    'F',    //      09
    'G',    //      10
    'H',    //      11
    'K',    //      12
    'L',    //      13
    'I',    //      14
    'J',    //      15
};

static const char *Funcname[] = {
    "All units off",    /* 00 */
    "All lights on",    /* 01 */
    "On",
    "Off",
    "Dim",
    "Bright",
    "All lights off",   /* 06 */
    "Ext code 1, data, control",
    "Hail request",
    "Hail ack",
    "Ext code 3, security msg",
    "Unused",
    "Ext code 2, meter read, DSM",
    "Status on",
    "Status off",
    "Status request"
};

static int huc_decode(unsigned char houseunit, char *housechar)
{
    *housechar = HouseUnitTable[(houseunit & 0xf0) >> 4];
    return (HouseUnitTable[houseunit & 0x0f] -'A') + 1;
}

static int hfc_decode(unsigned char houseunit, char *housechar)
{
    *housechar = HouseUnitTable[(houseunit & 0xf0) >> 4];
    return (houseunit & 0x0f);
}

// 5D 20 64 9B 00 FB -- A9 ON
// 64 = A = *housechar
// 00 = unitfunc & 1<<5 = *funcint 0=ON,1=OFF
// 5D 20 E0 1F 20 DF -- P1 OFF
static int hufc_decode(unsigned char houseunit, unsigned char unitfunc, 
        char *housechar, unsigned int *funcint)
{
    int unitint;

    *housechar = HouseUnitTableRF[(houseunit & 0xf0) >> 4];
    switch (unitfunc)
    {
        case 0x88:  // Bright
            *funcint = 3;
            return 0;
        case 0x98:  // Dim
            *funcint = 2;
            return 0;
        default:
            *funcint = (unitfunc & (1<<5)) >> 5; // 0=ON, 1=OFF
            unitint = (houseunit & (1<<2)) << 1;
            unitint |= (unitfunc & (1<<6)) >> 4;
            unitint |= (unitfunc & (1<<3)) >> 2;
            unitint |= (unitfunc & (1<<4)) >> 4;
            return unitint + 1;
    }
}


/* PLC A1 ON
 * 5A 02 00 66
 *  5A PLC
 *  02 2 bytes follow
 *  00 house/unit code follows
 *  66 house/unit code
 * 5A 02 01 62 
 *  5A PLC
 *  02 2 bytes follow
 *  01 house/function code follows
 *  62 house/function code
 *
 * PLC Bright
 * 5A 03 02 16 65
 *  5A PLC
 *  03 3 bytes follow
 *  02 bright/dim
 *  16 00010 1 1 0
 *      00010 # of dims (2)
 *      1 always 1
 *      1 function follows
 *      0 standard data
 *  65 0110 0101
 *      0110 house code 6=A
 *      0101 function=Bright
*/
void cm15a_decode_plc(int fd, unsigned char *buf, size_t len)
{
    char housechar;
    int unitint, funcint;
    int codelen;
    int dims;

    dbprintf("%s(%d,%u) ", __func__, fd, len);
    hexdump(buf, len);
    // new event
    // ev->evint = EV_RX_PLC;
    if (len < 4) {
        dbprintf("len too short %d\n", len);
        return;
    }
    if (len > 8) {
        dbprintf("len too long %d\n", len);
        return;
    }

    switch (buf[2]) 
    {
        case 0x00:  // house/unit code follows
            codelen = buf[1];
            if (codelen != 2) {
                dbprintf("codelen must be 2 != %d\n", codelen);
                return;
            }
            unitint = huc_decode(buf[3], &housechar);
            hua_add(housechar-'A', unitint-1);
            sockprintf(fd, "%cx PL HouseUnit: %c%d\n", 
                    (buf[0] == 0x5a) ? 'R' : 'T',
                    housechar, unitint);
            break;
        case 0x01:  // house/function code follows
            codelen = buf[1];
            if (codelen != 2) {
                dbprintf("codelen must be 2 != %d\n", codelen);
                return;
            }
            funcint = hfc_decode(buf[3], &housechar);
            dbprintf("h %c func %d\n", housechar, funcint);
            switch (funcint)
            {
                /* There is no way to determine which modules are lamp vs.
                 * applicance modules. 
                 * All units = lamp and appliance modules
                 * All lights = lamp modules
                 */
                case 0: /* All units off */
                case 6: /* All lights off */
                    hua_func_all_off(housechar-'A');
                    break;
                case 1: /* All lights on */
                    hua_func_all_on(housechar-'A');
                    break;
                case 2:
                case 13: /* Status On     */
                    hua_func_on(housechar-'A');
                    dbprintf("%s case 2 break\n", __func__);
                    break;
                case 3:
                case 14: /* Status Off    */
                    hua_func_off(housechar-'A');
                    break;
                case 4: /* Dim XXX */
                    break;
                case 5: /* Bright XXX */
                    break;
                case 7: /* Extended Code */
                    break;
                case 8: /* Hail request  */
                    break;
                case 9: /* Hail Ack      */
                    break;
                case 10: /* Pre-set Dim   */
                    break;
                case 11: /* Pre-set Dim   */
                    break;
                case 12: /* Extended data */
                    break;
                case 15: /* Status Request */
                    break;
                default:
                    break;
            }
            sockprintf(fd, "%cx PL House: %c Func: %s\n",
                    (buf[0] == 0x5a) ? 'R' : 'T',
                    housechar, Funcname[funcint]);
            dbprintf("exit case 0x01\n");
            break;
        case 0x02:  // dim/bright code follows
            codelen = buf[1];
            if (codelen != 3) {
                dbprintf("codelen must be 3 != %d\n", codelen);
                return;
            }
            dims = (buf[3] & 0xF8) >> 3;
            funcint = hfc_decode(buf[4], &housechar);
            sockprintf(fd, "%cx PL House: %c Func: %s(%d)\n",
                    (buf[0] == 0x5a) ? 'R' : 'T',
                    housechar, Funcname[funcint], dims);
            break;
        case 0x07:  /* Extended transmission follows */
            /* This is never received from the CM51A but is here to decode Tx. */
            /* Sample: 5a 05 07 e7 02 1f 31 */
            /* PL B3 XDIM 31
             * Header 8 bits (7)
             * House code: 4 bits, function code: 4 bits (7)
             * Length: 8 bits = 2
             * Data: 8 bits
             * Command: 8 bits
             */
            if (len < 7) {
                dbprintf("too short %d\n", len);
                return;
            } else if (len > 7) {
                dbprintf("too long %d\n", len);
                return;
            }
            funcint = hfc_decode(buf[3], &housechar);
            if (buf[4] != 2) {
                dbprintf("data length must be 2 != %d\n", buf[4]);
            }
            sockprintf(fd, "%cx PL House: %c Func: %s Data: %02X Command: %02X\n",
                    (buf[0] == 0x5a) ? 'R' : 'T',
                    housechar, Funcname[funcint], buf[5], buf[6]);

            break;
        case 0x08:  /* Extended receive follows */
            /* This is received when an extended pre-set dim is sent.
             * Note the byte order of the last 4 bytes is reversed compared
             * to Tx above.
             */
            /* Sample: 5A 05 08 31 21 02 E7 */
            /* PL B3 XDIM 33
             * Header 8 bits (8)
             * Command: 8 bits  0x31 pre-set dim
             * Data: 8 bits     0x21 = 33 dim level
             * Length: 8 bits = 2
             * House code: 4 bits, function code: 4 bits (7)
             */
            if (len < 7) {
                dbprintf("too short %d\n", len);
                return;
            } else if (len > 7) {
                dbprintf("too long %d\n", len);
                return;
            }
            funcint = hfc_decode(buf[6], &housechar);
            if (buf[5] != 2) {
                dbprintf("data length must be 2 != %d\n", buf[5]);
            }
            sockprintf(fd, "%cx PL House: %c Func: %s Data: %02X Command: %02X\n",
                    (buf[0] == 0x5a) ? 'R' : 'T',
                    housechar, Funcname[funcint], buf[4], buf[3]);

            break;
        default:
            dbprintf("Not supported %d\n", buf[2]);
            break;
    }
    dbprintf("%s exit\n", __func__);
}

/* TODO sort and binary search? Or make 256 entry table and index. Not sure how
 * sparse the table would be.
 * 1<<7    1=close/normal, 0=open/alert
 * 1<<2    delay 0=max, 1=min
 * 1<<0    1=battery low?, 0=battery OK
 */
static const struct SecEventRec SecEventNames[] = {
    {0x0C, "Motion_alert_MS10A"},
    {0x8C, "Motion_normal_MS10A"},
    {0x0D, "Motion_alert_low_MS10A"},     /* MS10 does not emit this */
    {0x8D, "Motion_normal_low_MS10A"},    /* MS10 does not emit this */
    {0x04, "Contact_alert_min_DS10A"},
    {0x84, "Contact_normal_min_DS10A"},
    {0x00, "Contact_alert_max_DS10A"},
    {0x80, "Contact_normal_max_DS10A"},
    {0x01, "Contact_alert_min_low_DS10A"},    /* _low = low battery */
    {0x81, "Contact_normal_min_low_DS10A"},
    {0x05, "Contact_alert_max_low_DS10A"},
    {0x85, "Contact_normal_max_low_DS10A"},
    {0x06, "Arm_KR10A"},
    {0x86, "Disarm_KR10A"},
    {0x46, "Lights_On_KR10A"},
    {0xC6, "Lights_Off_KR10A"},
    {0x26, "Panic_KR10A"},
    {0x00, NULL},
};

static const struct SecEventRec SecRemoteKeyNames[] = {
    {0x0E, "Arm_Home_min_SH624"},
    {0x06, "Arm_Away_min_SH624"},
    {0x0A, "Arm_Home_max_SH624"},
    {0x02, "Arm_Away_max_SH624"},
    {0x82, "Disarm_SH624"},
    {0x22, "Panic_SH624"},
    {0x42, "Lights_On_SH624"},
    {0xC2, "Lights_Off_SH624"},
    {0x04, "Motion_alert_SP554A"},   //DG
    {0x84, "Motion_normal_SP554A"},  //DG
    {0x00, NULL},
};

struct CamRemoteRec {
    const size_t commandlen;
    const unsigned char command[8];
    const char *name;
};

static const struct CamRemoteRec CameraRemoteNames[] = {
    {4, {0x14,0x47,0x62,0x10}, "CAMUP"},
    {4, {0x14,0x48,0x63,0x10}, "CAMDOWN"},
    {4, {0x14,0x45,0x60,0x10}, "CAMLEFT"},
    {4, {0x14,0x46,0x61,0x10}, "CAMRIGHT"},
    {4, {0x14,0x51,0x6C,0x10}, "CAMCENTER"},
    {4, {0x14,0x53,0x6E,0x10}, "CAMSWEEP"},
    {4, {0x14,0x49,0x64,0x10}, "CAMPRESET1"},
    {4, {0x14,0x4B,0x66,0x10}, "CAMPRESET2"},
    {4, {0x14,0x4D,0x68,0x10}, "CAMPRESET3"},
    {4, {0x14,0x4F,0x6A,0x10}, "CAMPRESET4"},
    {4, {0x14,0x39,0x54,0x10}, "CAMPRESET5"},
    {4, {0x14,0x3B,0x56,0x10}, "CAMPRESET6"},
    {4, {0x14,0x3D,0x58,0x10}, "CAMPRESET7"},
    {4, {0x14,0x3F,0x5A,0x10}, "CAMPRESET8"},
    {4, {0x14,0x41,0x5C,0x10}, "CAMPRESET9"},
    {4, {0x14,0x4A,0x65,0x10}, "CAMEDITPRESET1"},
    {4, {0x14,0x4C,0x67,0x10}, "CAMEDITPRESET2"},
    {4, {0x14,0x4E,0x69,0x10}, "CAMEDITPRESET3"},
    {4, {0x14,0x50,0x6B,0x10}, "CAMEDITPRESET4"},
    {4, {0x14,0x3A,0x55,0x10}, "CAMEDITPRESET5"},
    {4, {0x14,0x3C,0x57,0x10}, "CAMEDITPRESET6"},
    {4, {0x14,0x3E,0x59,0x10}, "CAMEDITPRESET7"},
    {4, {0x14,0x40,0x5B,0x10}, "CAMEDITPRESET8"},
    {4, {0x14,0x42,0x5C,0x10}, "CAMEDITPRESET9"},
    {0, {0x00}, NULL}
};

// Given binary data packet from X10 controller (CM15A or CM19A), 
// find matching human-readable name for remote button

static const char *findCamRemoteName(const unsigned char *camcommand, size_t commandlen)
{
    const struct CamRemoteRec *p;

    p = CameraRemoteNames;
    while (p->commandlen) {
        if (commandlen == p->commandlen) {
            if (memcmp(p->command, camcommand, commandlen) == 0)
            {
                return p->name;
            }
        }
        p++;
    }
    return NULL;
}


// Given human-readable name for remote button, find 
// binary data packet to be sent to X10 controller (CM15A or CM19A)

int findCamRemoteCommand(const char *keyname, unsigned char *command, size_t commandlen)
{
    const struct CamRemoteRec *p;

    p = CameraRemoteNames;
    dbprintf("findCamRemoteCommand(%s,%p,%u)\n", keyname, command, commandlen);
    while (p->commandlen) {
        dbprintf("%s %u\n", p->name, p->commandlen);
        if (strcmp(keyname, p->name) == 0) {
            if (commandlen >= p->commandlen) {
                memcpy(command, p->command, p->commandlen);
                return p->commandlen;
            }
            return 0;
        }
        p++;
    }
    return 0;
}


/*
 * 5D 29 7F 70 8C 73 CA 00 from MS10,DS10,KR10 17(?) bit RF address 
 *        |  |  |  |  |  |
 *        |  |  |  |  |  addr3
 *        |  |  |  |  addr2
 *        |  |  |  XOR with prev byte=0xff
 *        |  |  function/key
 *        |  XOR with prev byte==0x0f
 *        addr1
 */
static int secaf_decode(unsigned char *buf, unsigned int len, 
                                    unsigned char *secaddr, unsigned int *funcint)
{
    if (len < 8) return -1;

    if (((buf[2] ^ buf[3]) != 0x0f) || ((buf[4] ^ buf[5]) != 0xff)) {
        dbprintf("Invalid checksum\n");
        return -1;
    }

    if (Paritytable[buf[6] ^ buf[7]]) {
        dbprintf("Invalid parity\n");
        return -2;
    }
    
    *secaddr++ = buf[2];
    *secaddr++ = buf[6];
    *secaddr++ = buf[7];
    *funcint = buf[4];
    return 0;
}

const char *findSecEventName(unsigned char secev)
{
    int i;

    for (i = 0; SecEventNames[i].name; i++)
    {
        if (secev == SecEventNames[i].funct) return SecEventNames[i].name;
    }
    return NULL;
}

const char *findSecRemoteKeyName(unsigned char secev)
{
    int i;

    for (i = 0; SecRemoteKeyNames[i].name; i++)
    {
        if (secev == SecRemoteKeyNames[i].funct) return SecRemoteKeyNames[i].name;
    }
    return NULL;
}

// Repeat received RF command. Just change first byte to 0xEB and send it back
// to the controller.

static int repeatRF(int fd, unsigned char *buf, size_t len)
{
    unsigned char saved = *buf;
    int retval = 0;

    if (RfToRf16) {
        sockprintf(fd, "RfToRf repeat\n");
        // Change Rx code to Tx code
        *buf = 0xEB;
        if (Cm19a)
            retval = x10_write(buf+1, len-1);
        else
            retval = x10_write(buf, len);
        *buf = saved;
    }
    return retval;
}

/* RF A1 ON
 * 5D 20 60 9F 00 FF 
 *  5D RF
 *  20 standard RF
 *  60 0110 house code
 *  9F 10011111 complement
 *  00 0000 funct ON, 0000
 *  FF 11111111 complement
 *
 * RF A1 OFF
 * 5D 20 60 9F 20 DF 
 *  5D RF 
 *  20 standard RF
 *  60 0110 house code
 *  9F 10011111 complement
 *  20 0010 funct OFF   0000
 *  DF 11011111 complement
 *
 * RF A9 ON
 * 5D 20 64 9B 00 FB
 *  5D RF
 *  20 Standard RF 
 *  64 0110 house=0110=A, 0100 unit 9-16
 *  9B 1001 1011 complement of previous byte 
 *  00 0000 0000 function ON
 *  FB 1111 1011 lsnyb complement of buf[2]
 *
*/
void cm15a_decode_rf(int fd, unsigned char *buf, unsigned int len)
{
    char housechar;
    int unitint, rc;
    unsigned int funcint;
    unsigned char secaddr[3];
    unsigned char chksum;
    char cmdbuf[80];
    const char *commandp;

    if (len < 5) {
        sockprintf(fd, "too short %d\n", len);
        sockhexdump(fd, buf, len);
        return;
    }
    if (len > 8) {
        sockprintf(fd, "too long %d\n", len);
        sockhexdump(fd, buf, len);
        return;
    }
    switch (buf[1])
    {
        case 0x14:  // X10 RF camera
            commandp = findCamRemoteName(&buf[1], len-1);
            if (commandp) {
                sockprintf(fd, "%cx RFCAM %s\n", (buf[0] == 0x5d) ? 'R' : 'T',
                       commandp);
                repeatRF(fd, buf, len);
            }
            else {
                sockprintf(fd, "Unknown RF camera command\n");
                sockhexdump(fd, buf, len);
            }
            break;
        case 0x20:  // standard X10 RF
            chksum = buf[2] ^ buf[3];
            if (chksum == 0x0f) {
                /* 5D 20 E2 ED 0A F5 from SH624
                 *        |  |  |  | XOR with prev byte=0xff
                 *        |  |  key code
                 *        |  XOR with prev byte==0x0f
                 *        8 bit security code change by pressing CODE button
                 */
                if ((buf[4] ^ buf[5]) != 0xff) {
                    sockprintf(fd, "Invalid checksum\n");
                    sockhexdump(fd, buf, len);
                    return;
                }
                secaddr[0] = 0;
                secaddr[1] = 0;
                secaddr[2] = buf[2];
                hua_sec_event(secaddr, buf[4], 1);
                sockprintf(fd, "%cx RFSEC Addr: 0x%02X Func: %s\n",
                       (buf[0] == 0x5d) ? 'R' : 'T',
                        buf[2], findSecRemoteKeyName(buf[4]));
                repeatRF(fd, buf, len);
            }
            else if (chksum == 0xff) {
                /* 5D 20 60 9F 00 FF
                 *        |  |  |
                 *        |  |  unit/function
                 *        |  XOR with prev byte=0xff
                 *        house code/unit
                 */
                unitint = hufc_decode(buf[2], buf[4], &housechar, &funcint);
                dbprintf("h %c func %d\n", housechar, funcint);
                if (funcint > 1) {  // Dim or Bright
                    sockprintf(fd, "%cx RF House: %c Func: %s\n", 
                            (buf[0] == 0x5d) ? 'R' : 'T',
                            housechar, Funcname[funcint+2]);
                    repeatRF(fd, buf, len);
                    if (!Cm19a && (RfToPl16 & (1 << (housechar - 'A')))) {
                        rc = snprintf(cmdbuf, sizeof(cmdbuf), "PL %c %s",
                                housechar, Funcname[funcint+2]);
                        if (rc > 0)
                            processcommandline(fd, cmdbuf);
                        else {
                            sockprintf(fd, "cmdbuf too short\n");
                            sockhexdump(fd, buf, len);
                        }
                    }
                }
                else {  // On or Off
                    hua_add(housechar-'A', unitint-1);
                    if (funcint)
                        hua_func_off(housechar-'A');
                    else
                        hua_func_on(housechar-'A');
                    sockprintf(fd, "%cx RF HouseUnit: %c%d Func: %s\n", 
                            (buf[0] == 0x5d) ? 'R' : 'T',
                            housechar, unitint, Funcname[funcint+2]);
                    repeatRF(fd, buf, len);
                    if (!Cm19a && (RfToPl16 & (1 << (housechar - 'A')))) {
                        rc = snprintf(cmdbuf, sizeof(cmdbuf), "PL %c%d %s",
                                housechar, unitint, Funcname[funcint+2]);
                        if (rc > 0)
                            processcommandline(fd, cmdbuf);
                        else {
                            sockprintf(fd, "cmdbuf too short\n");
                            sockhexdump(fd, buf, len);
                        }
                    }
                }
            }
            else {
                sockprintf(fd, "Invalid checksum 0x%02X\n", chksum);
                sockhexdump(fd, buf, len);
            }
            break;
        case 0x24:  // ????
        case 0x28:  // ????
            // 5D 24 EE 11 8E 79 40
            // 5d 28 33 2d 18 e6 a5
            // 5d 28 33 2c 19 e6 a5
            sockprintf(fd, "Not supported %02X\n", buf[1]);
            sockhexdump(fd, buf, len);
            break;
        case 0x29:  // security X10 RF
            rc = secaf_decode(buf, len, secaddr, &funcint);
            switch (rc) {
                case 0:
                    hua_sec_event(secaddr, funcint, 0);
                    sockprintf(fd, "%cx RFSEC Addr: %02X:%02X:%02X Func: %s\n", 
                            (buf[0] == 0x5d) ? 'R' : 'T',
                            secaddr[0], secaddr[1], secaddr[2],
                            findSecEventName(funcint));
                    repeatRF(fd, buf, len);
                    break;
                case -1:
                    sockprintf(fd, "Invalid checksum\n");
                    break;
                case -2:
                    sockprintf(fd, "Invalid parity\n");
                    break;
                default:
                    sockprintf(fd, "???\n");
                    sockhexdump(fd, buf, len);
                    break;
            }
            break;
        default:
            sockprintf(fd, "Not supported %02X\n", buf[1]);
            sockhexdump(fd, buf, len);
            break;
    }
}

void cm15a_decode(int fd, unsigned char *buf, unsigned int len)
{
    unsigned char bufcm19a[9];
    unsigned char *p = buf;

    if (len < 4) return;
    if (Cm19a) {
        /* Add 0x5d to front so USB packet from the CM19A looks just like
         * a USB packet frmo the CM15A. Call the same decode function.
         * The CM19A does RF but not PL.
         */
        p = bufcm19a;
        *p = 0x5d;
        memcpy(p+1, buf, len);
        len++;
        cm15a_decode_rf(fd, p, len);
        return;
    }

    switch (*p) 
    {
        case 0xa5:  // Want clock set
            break;
        case 0x55:  // ACK last send
            break;
        case 0x5a:  // PLC event
            cm15a_decode_plc(fd, p, len);
            break;
        case 0x5b:  // ????
            break;
        case 0x5d:  // RF event
            cm15a_decode_rf(fd, p, len);
            break;
        default:
            break;
    }
}
