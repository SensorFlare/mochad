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


// bridge: Rx RF -> Tx PL
//  if equivalent command exists such as A1 ON, relay RF commands to PL.
//  RF remote keys such as ARM have no equivalent PL command. See RF
//  key map.
// bridge: Rx PL -> Tx RF
//  if equivalent command exists such as A1 ON, relay PL command to RF.
// relay : Rx RF -> Tx RF
//   for all RF commands
// relay : Rx PL -> Tx PL
//   for all PL commands
// echo  : Tx RF -> Rx RF (done)
//   Sending an RF command generetes a fake RF receive message. Consider 2
//   clients connected to cm15ad. Client 1 send RF A1 ON. If this option is
//   off, client 2 will not receive any indication this happened. The CM15A
//   does not receive its own transmissions. When this option is on, cm15ad
//   generates a fake Rx RF A1 ON for all clients except the client that sent
//   the command.
// echo  : Tx PL -> Rx PL (done)
//   See above.
// RF remote key translation
//   For backwards compatibility with systems that can only work with house/unit
//   commands, translation RF keys into HouseUnit ON/OFF. For example,
//   RF ARM MIN DELAY -> PL P1 ON
//   RF DISARM -> RF P1 OFF
//   RF ARM MAX DELAY -> RF P2 ON
//   RF DISARM -> PL P2 OFF
//
//   In the other direction,
//
//   PL P1 ON -> RF ARM MIN DELAY
//
//------------------------
//  Sent by CM15a for B3 Dim 49%, Dim 0%, Dim 100%
//  07 e7 02 1f  31     pl b3 extdim 49
//     H? ?U DA  TF
//     H = house code 'B"
//     U = unit code - 1
//     T  = Type 0..15
//          0 = shutters and sunshades
//          1 = sensors
//          2 = reserved for security
//          3 = control modules (dimmers and appliances)
//              F  = Function
//                  0 = 
//                  1 = Preset receiver output
//                  DA[5..0] = dim amount = 0..63a
//                      NZ=ON,Z=OFF,
//                  DA[7..6] = T1 T0
//                      T 0=3.7s,1=30s,2=60s,3=300s
//          4 = extended secure addressing
//          5 = extended secure addressing for groups
//
//
//  07 e7 02 01  31     pl b3 extdim 0
//  07 e7 02 3e  31     pl b3 extdim 100


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include "global.h"
#include "encode.h"
#include "decode.h"
#include "x10state.h"
#include "x10_write.h"

static void strupper(char *buf)
{
    while (*buf) {
        if (islower(*buf)) *buf = toupper(*buf);
        buf++;
    }
}

static int gethexdata(unsigned char buf8[])
{
    char *parm;
    unsigned char *ptr = buf8;
    unsigned char *endbuf = buf8 + 8;

    while ((parm = strtok(NULL, " ")) && (ptr < endbuf)) {
        dbprintf("parm %s\n", parm);
        errno = 0;
        *ptr++ = (unsigned char)strtoul(parm, NULL, 16);
        if (errno) {
            dbprintf("Hex digit conversion problem %d\n", errno);
            return -1;
        }
    }
    return (ptr - buf8);
}


#define    FUNC_ALL_UNITS_OFF   (0)
#define    FUNC_ALL_LIGHTS_ON   (1)
#define    FUNC_ON              (2)
#define    FUNC_OFF             (3)
#define    FUNC_DIM             (4)
#define    FUNC_BRIGHT          (5)
#define    FUNC_ALL_LIGHTS_OFF  (6)
#define    FUNC_EXTENDED_CODE_1 (7)
#define    FUNC_HAIL_REQUEST    (8)
#define    FUNC_HAIL_ACK        (9)
#define    FUNC_EXTENDED_CODE_3 (10)
#define    FUNC_UNUSED          (11)
#define    FUNC_EXTENDED_CODE_2 (12)
#define    FUNC_STATUS_ON       (13)
#define    FUNC_STATUS_OFF      (14)
#define    FUNC_STATUS_REQUEST  (15)
#define    FUNC_EXTENDED_DIM    (16)

static const char *funccommands[] = {
    "ALL_UNITS_OFF",
    "ALL_LIGHTS_ON",
    "ON",
    "OFF",
    "DIM",
    "BRIGHT",
    "ALL_LIGHTS_OFF",
    "EXTENDED_CODE_1",
    "HAIL_REQUEST",
    "HAIL_ACK",
    "EXTENDED_CODE_3",
    "UNUSED",
    "EXTENDED_CODE_2",
    "STATUS_ON",
    "STATUS_OFF",
    "STATUS_REQUEST",
    "XDIM"
};

static const unsigned char x10housecode[] = {
    0x06,   /* House code A */
    0x0e,   /* House code B */
    0x02,   /* House code C */
    0x0a,   /* House code D */
    0x01,   /* House code E */
    0x09,   /* House code F */
    0x05,   /* House code G */
    0x0d,   /* House code H */
    0x07,   /* House code I */
    0x0f,   /* House code J */
    0x03,   /* House code K */
    0x0b,   /* House code L */
    0x00,   /* House code M */
    0x08,   /* House code N */
    0x04,   /* House code O */
    0x0c,   /* House code P */
};

static const unsigned char x10housecoderf[] = {
    0x06,   /* House code A */
    0x07,   /* House code B */
    0x04,   /* House code C */
    0x05,   /* House code D */
    0x08,   /* House code E */
    0x09,   /* House code F */
    0x10,   /* House code G */
    0x11,   /* House code H */
    0x14,   /* House code I */
    0x15,   /* House code J */
    0x12,   /* House code K */
    0x13,   /* House code L */
    0x00,   /* House code M */
    0x01,   /* House code N */
    0x02,   /* House code O */
    0x03,   /* House code P */
};

static int getfunc(void)
{
    char *command;
    int i;

    command = strtok(NULL, " ");
    if (!command) return -1;
    for (i = 0; i < (sizeof(funccommands)/sizeof(funccommands[0])); i++)
    {
        if (strcmp(command, funccommands[i]) == 0) {
            return i;
        }
    }
    return -1;
}

static const struct SecEventRec SecEventNameslongaddr[] = {
    {0x0C, "MOTION_ALERT"},
    {0x8C, "MOTION_NORMAL"},
    {0x0D, "MOTION_ALERT_LOW"},     /* MS10 does not emit this */
    {0x8D, "MOTION_NORMAL_LOW"},    /* MS10 does not emit this */
    {0x04, "CONTACT_ALERT_MIN"},
    {0x84, "CONTACT_NORMAL_MIN"},
    {0x00, "CONTACT_ALERT_MAX"},
    {0x80, "CONTACT_NORMAL_MAX"},
    {0x01, "CONTACT_ALERT_MIN_LOW"},    /* _LOW = LOW battery */
    {0x81, "CONTACT_NORMAL_MIN_LOW"},
    {0x05, "CONTACT_ALERT_MAX_LOW"},
    {0x85, "CONTACT_NORMAL_MAX_LOW"},
    {0x06, "ARM"},
    {0x86, "DISARM"},
    {0x46, "LIGHTS_ON"},
    {0xC6, "LIGHTS_OFF"},
    {0x26, "PANIC"},
    {0x00, NULL},
};

static const struct SecEventRec SecRemoteKeyNames8bitaddr[] = {
    {0x06, "ARM"},
    {0x0E, "ARM_HOME_MIN"},
    {0x06, "ARM_AWAY_MIN"},
    {0x0A, "ARM_HOME_MAX"},
    {0x02, "ARM_AWAY_MAX"},
    {0x82, "DISARM"},
    {0x22, "PANIC"},
    {0x42, "LIGHTS_ON"},
    {0xC2, "LIGHTS_OFF"},
    {0x00, NULL},
};

static int getrffunc(int rf8bitaddr)
{
    char *command;
    int i;

    command = strtok(NULL, " ");
    if (!command) return -1;

    if (rf8bitaddr == 1) {
        for (i = 0; SecRemoteKeyNames8bitaddr[i].name; i++)
        {
            if (strcmp(command, SecRemoteKeyNames8bitaddr[i].name) == 0)
                return SecRemoteKeyNames8bitaddr[i].funct;
        }
        return -1;
    }
    else if (rf8bitaddr == 0) {
        for (i = 0; SecEventNameslongaddr[i].name; i++)
        {
            if (strcmp(command, SecEventNameslongaddr[i].name) == 0)
                return SecEventNameslongaddr[i].funct;
        }
        return -1;
    }
    else
        return -1;
}


static int getparam(void)
{
    char *param;


    param = strtok(NULL, " ");
    if (!param) return -1;
    return strtol(param, NULL, 10);
}

static int ishouse(char h)
{
    return ((h >= 'A') && (h <= 'P'));
}

/* House code: 'A'..'P', house value: 0..15
 * Unit  code: "1".."16", unit value: 0..15
 * Valid strings:
 * A            house=0, *unit=-1
 * A1           house=0  *unit=0
 * P15          house=15 *unit=14
 */
static int getdeviceaddr(int *unit)
{
    char *parm, c;

    int house=-1;

    *unit=-1;

    parm = strtok(NULL, " ");
    if (!parm) return house;

    dbprintf("deviceaddr %s\n", parm);
    c = *parm++;
    if (!ishouse(c)) return house;
    house = c - 'A';
    dbprintf("house %d\n", house);

    c = *parm++;
    if (!isdigit(c)) return house;
    *unit = c - '0';
    dbprintf("*unit %d\n", *unit);

    c = *parm++;
    if (!isdigit(c)) {
        (*unit)--;
        dbprintf("*unit %d\n", *unit);
        return house;
    }
    *unit = (*unit * 10) + (c - '0');
    if (*unit > 16) {
        *unit = -1;
        return house;
    }
    (*unit)--;
    return house;
}

static int getrfaddr(unsigned long *rfaddr)
{
    char *parm;

    parm = strtok(NULL, " ");
    if (!parm) return -1;

    dbprintf("rfaddr %s\n", parm);

    if ((strncmp(parm, "0x", 2) == 0) || (strncmp(parm, "0X", 2) == 0)) {
        /* 8 bit RF address starts with 0x or 0X */
        *rfaddr = strtoul(parm+2, NULL, 16);
        return 1;
    }

    /* Long RF address must be 6 hex digits */
    *rfaddr = strtoul(parm, NULL, 16);
    return 0;
}

static unsigned short gethousecodes(void)
{
    char *parm;
    unsigned char house;
    unsigned short rc = 0;

    parm = strtok(NULL, " ");
    /* No parameter means turn off all house codes */
    if (!parm || *parm == '\0') return 0;

    /* parameter == "*" means turn on all house codes */
    if (strcmp(parm, "*") == 0) return 0xFFFF;

    while ((house = *parm++)) {
        dbprintf("house %c\n", house);
        if (ishouse(house)) {
            house -= 'A';
            dbprintf("house %d\n", house);
            rc |= (1 << house);
            dbprintf("rc %04X\n", rc);
        }
    }
    return rc;
}

/*
 * byte 0
 *   bits 7..3  Dim amount
 *   bit 2      always 1
 *   bit 1      following byte is 1=function,0=address
 *   bit 0      1=extended,0=standard
 * byte 1
 *   if function, byte 0:bit 1=1
 *     bits 7..4    house code
 *     bits 3..0    function code
 *   if address, byte 0:bit 1=0
 *     bits 7..4    house code
 *     bits 3..0    unit code
 */
static int pl_tx_houseunit(int fd, int house, int unit)
{
    char unsigned buf[4];

    /* Make buffer as if received so decoder prints */
    buf[0] = 0x00;
    buf[1] = 0x02;
    buf[2] = 0x00;
    buf[3] = x10housecode[house] << 4 | x10housecode[unit];
    cm15a_decode_plc(-1, buf, sizeof(buf));

    /* Transmit only requires last 2 bytes and first byte must be 0x04 */
    buf[2] = 0x04;
    return x10_write(buf+2, 2);
}

static int pl_tx_housefunc(int fd, int house, int func, int param)
{
    unsigned char buf[7];
    int dims;
    size_t nbuf;
    unsigned char *xmitptr;

    dbprintf("%s(%d,%d,%d,%d)\n", __func__, fd, house, func, param);
    /* Make buffer as if received so decoder prints */
    buf[0] = 0x00;
    switch (func) {
        case FUNC_DIM:
        case FUNC_BRIGHT:
            buf[1] = 0x03;
            nbuf = 5;  /* Decode 5 bytes */
            buf[2] = 0x02;
            dims = ((param & 0x1F) << 3);
            buf[3] = dims | 0x01;
            buf[4] = x10housecode[house] << 4 | func;
            xmitptr = &buf[3];
            cm15a_decode_plc(-1, buf, nbuf);
            dbprintf("%d:", nbuf); hexdump(buf, nbuf);

            /* Transmit only requires last 2 bytes */
            *xmitptr = 0x06 | dims;
            hexdump(xmitptr, 2);
            return x10_write(xmitptr, 2);
        case FUNC_EXTENDED_DIM:
            buf[1] = 0x05;
            buf[2] = 0x07;
            nbuf = 7;  /* Decode 7 bytes */
            buf[3] = x10housecode[house] << 4 | 0x7;
            buf[4] = 0x02;  /* 2 data bytes follow */
            dims = param & 0xFF;
            buf[5] = dims;
            buf[6] = 0x31;  /* Dim/Bright */
            xmitptr = &buf[2];
            cm15a_decode_plc(-1, buf, nbuf);
            dbprintf("%d:", nbuf); hexdump(buf, nbuf);

            /* Transmit only requires last 5 bytes */
            hexdump(xmitptr, 5);
            return x10_write(xmitptr, 5);
        default:
            buf[1] = 0x02;
            nbuf = 4;  /* Decode 4 bytes */
            buf[2] = 0x01;
            buf[3] = x10housecode[house] << 4 | func;
            xmitptr = &buf[2];
            cm15a_decode_plc(-1, buf, nbuf);
            dbprintf("%d:", nbuf); hexdump(buf, nbuf);

            /* Transmit only requires last 2 bytes */
            *xmitptr = 0x06;
            hexdump(xmitptr, 2);
            return x10_write(xmitptr, 2);
    }
}

/*
 * EB 20 E2 ED 0A F5 from SH624 8 bit RF address
 *        |  |  |  | 
 *        |  |  |  XOR with prev byte==0xff
 *        |  |  key code
 *        |  XOR with prev byte==0x0f
 *        8 bit security code change by pressing CODE button
 *
 * EB 29 7F 70 8C 73 CA 00 from MS10,DS10,KR10 17(?) bit RF address 
 *        |  |  |  |  |  |
 *        |  |  |  |  |  addr3
 *        |  |  |  |  addr2
 *        |  |  |  XOR with prev byte=0xff
 *        |  |  function/key
 *        |  XOR with prev byte==0x0f
 *        addr1
 */
           
static int rfsec_tx(int fd, int rf8bitaddr, unsigned long rfaddr, int func)
{
    unsigned char buf[8];
    unsigned char *p = buf;
    unsigned char addr1;

    *p++ = 0xEB;
    if (rf8bitaddr) {
        *p++ = 0x20;
        *p++ = rfaddr;
        *p++ = rfaddr ^ 0x0f;
        *p++ = func;
        *p   = ~func;
        cm15a_decode_rf(-1, buf, 6);
        if (Cm19a)
            return x10_write(buf+1, 5);
        else
            return x10_write(buf, 6);
    }
    else {
        *p++ = 0x29;
        addr1 = rfaddr >> 16;
        *p++ = addr1;
        *p++ = addr1 ^ 0x0f;
        *p++ = func;
        *p++ = ~func;
        *p++ = rfaddr >> 8;
        *p   = rfaddr;
        cm15a_decode_rf(-1, buf, 8);
        if (Cm19a)
            return x10_write(buf+1, 7);
        else
            return x10_write(buf, 8);
    }
}

/* EB 20 60 9F 00 FF
 *        |  |  |
 *        |  |  unit/function
 *        |  XOR with prev byte=0xff
 *        house code/unit
 */
static int rf_tx_houseunitfunc(int fd, int house, int unit, int func)
{
    unsigned char buf[6];
    unsigned char unit8, unit4, unit2, unit1;

    if (house < 0) return -1;
    unit8 = unit & 0x08;
    unit4 = unit & 0x04;
    unit2 = unit & 0x02;
    unit1 = unit & 0x01;

    buf[0] = 0xeb;
    buf[1] = 0x20;
    buf[2] = (x10housecoderf[house] << 4) | (unit8 >> 1);

    buf[4] = 0;
    switch (func) {
        /* X10 standard RF */
        case FUNC_OFF:
            buf[4] = 1<<5;      // 1=OFF, 0=ON
            /* fall through */
        case FUNC_ON:
            if (unit < 0) return -1;
            buf[4] |= (unit4 << 4) | (unit2 << 2) | (unit1 << 4);
            break;
        case FUNC_DIM:
            buf[4] = 0x98;
            break;
        case FUNC_BRIGHT:
            buf[4] = 0x88;
            break;
            /* X10 SECURITY RF */
        default:
            dbprintf("Invalid function\n");
            return -1;
    }
    buf[3] = ~buf[2];     // add check bytes
    buf[5] = ~buf[4];

    cm15a_decode_rf(-1, buf, sizeof(buf));
    if (Cm19a)
        return x10_write(buf+1, sizeof(buf)-1);
    else
        return x10_write(buf, sizeof(buf));
}

/*
 * pl a1 on
 * rf a1 on
 */
int processcommandline(int fd, char *aLine)
{
    char *command, *arg1;
    int house, unit, func, param;
    unsigned char x10bytes8[8];
    size_t len;
    unsigned long rfaddr;
    int rf8bitaddr;

    strupper(aLine);
    dbprintf("%lu:%s\n", (unsigned long)strlen(aLine), aLine);
    command = strtok(aLine, " ");
    if (command) {
        if (strcmp(command, "PL") == 0) {
            house = getdeviceaddr(&unit);
            dbprintf("house %d unit %d\n", house, unit);
            if (house < 0) return -1;
            if (unit < 0) {
                /* Unit code is 0 but house code != 0 */
                func = getfunc();
                if (func < 0) return -1;
                if (func == FUNC_DIM || func == FUNC_BRIGHT || 
                        func==FUNC_EXTENDED_DIM) {
                    param = getparam();
                    if (param == -1) param = 1;
                    pl_tx_housefunc(fd, house, func, param);
                }
                else
                    pl_tx_housefunc(fd, house, func, 0);
            }
            else {
                pl_tx_houseunit(fd, house, unit);
                func = getfunc();
                if (func < 0) return -1;
                dbprintf("func %d\n", func);
                if (func == FUNC_DIM || func == FUNC_BRIGHT ||
                        func==FUNC_EXTENDED_DIM) {
                    param = getparam();
                    if (param == -1) param = 1;
                    pl_tx_housefunc(fd, house, func, param);
                }
                else
                    pl_tx_housefunc(fd, house, func, 0);
            }
        }
        else if (strcmp(command, "RF") == 0) {
            house = getdeviceaddr(&unit);
            dbprintf("house %d unit %d\n", house, unit);
            if (house < 0) return -1;
            func = getfunc();
            if (func < 0) return -1;
            rf_tx_houseunitfunc(fd, house, unit, func);
        }
        else if (strcmp(command, "RFSEC") == 0) {
            rfaddr = 0;
            rf8bitaddr = getrfaddr(&rfaddr);
            dbprintf("rfaddr 8bit: %d %X\n", rf8bitaddr, rfaddr);
            if (rf8bitaddr < 0) return -1;
            func = getrffunc(rf8bitaddr);
            dbprintf("rf func %X\n", func);
            if (func < 0) return -1;
            rfsec_tx(fd, rf8bitaddr, rfaddr, func);
        }
        else if (strcmp(command, "RFCAM") == 0) {
            arg1 = strtok(NULL, " ");
            dbprintf("rfcam arg1 %s\n", arg1);
            x10bytes8[0] = 0xeb;
            len = findCamRemoteCommand(arg1, x10bytes8+1, sizeof(x10bytes8)-1);
            if (len > 0) {
                len++;
                hexdump(x10bytes8, len);
                if (Cm19a)
                    x10_write(x10bytes8+1, len-1);
                else
                    x10_write(x10bytes8, len);
            }
            else {
                sockprintf(fd, "Invalid command %s\n", arg1);
            }
        }
        else if (strcmp(command, "PT") == 0) {
            len = gethexdata(x10bytes8);
            hexdump (x10bytes8, len);
            if (len > 0) x10_write(x10bytes8, len);
        }
#if 0
        /* Enable/disable the internal RFTOPL repeater */
        /* This seems to make the device lockup after a while */
        /* bb 40 51 05  00 14 20 28 */
        else if (strcmp(command, "RFTOPL") == 0) {
            unsigned short bitmap16;
            bitmap16 = gethousecodes();
            x10_write("\xdb\x1f\xf0", 3);
            x10_write("\xfb\x20\x00\x02", 4);
            memcpy(x10bytes8, "\xbb\x00\x00\x05\x00\x14\x20\x28", 8);
            memcpy(x10bytes8+1, &bitmap16, sizeof(bitmap16));
            x10_write(x10bytes8, 8);
        }
#endif
        else if (strcmp(command, "RFTOPL") == 0) {
            RfToPl16 = gethousecodes();
            sockprintf(fd, "RfToPl %04X\n", RfToPl16);
        }
        else if (strcmp(command, "RFTORF") == 0) {
            arg1 = strtok(NULL, " ");
            RfToRf16 = (unsigned short)strtoul(arg1, NULL, 10);
            sockprintf(fd, "RfToRf %04X\n", RfToRf16);
        }
        else if (strcmp(command, "ST") == 0) {
            arg1 = strtok(NULL, " ");
            dbprintf("st arg1 %s\n", arg1);
            if (arg1 && (strcmp(arg1, "0") == 0))
                hua_sec_init();
            else
                hua_show(fd);
        }
        else {
            dbprintf("Unknown command: %s\n", command);
            return -1;
        }
    }
    return 0;
}

/*
 * Parse human readable commands and convert to binary X10 protocol.
 * Send to CM15A.
 */
void cm15a_encode(int fd, unsigned char * buf, size_t buflen)
{
    static char remainder[80];      // Any leftover, not seen \n yet.
    static size_t remlen=0;
    char *remptr;

    dbprintf("buflen %lu\n", (unsigned long)buflen);
    hexdump(buf, buflen);
    dbprintf("remlen %lu\n", (unsigned long)remlen);
    hexdump(remainder, remlen);

    /* Break the input stream into \n terminated lines. The stream is not
     * guaranteed to end on a line boundary so there may be left over input
     * which must be processed the next time around.
     */
    remptr = remainder + remlen;
    while (buflen--) {
        *remptr = *buf++;
        if (*remptr == '\n') {
            *remptr = '\0';
            processcommandline(fd, remainder);
            remptr = remainder;
        }
        else
            remptr++;
    }
    remlen = remptr - remainder;
}
