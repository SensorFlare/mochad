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

/*
 * TCP gateway to X10 CM15A X10 RF and PL controller and CM19A RF controller.
 * Decode data from CM15A/CM19A ignoring macros and timers. This driver treats
 * the CM15A as a transceiver. The CM15A macros, timers, and real-time clock
 * (RTC) are ignored. In fact, the CM15A memory should be cleared using
 * ActiveHome Pro (AHP) before using the CM15A is with this driver. Batteries
 * are not necessary because the RTC is not used. The CM15A RF to PL converter
 * should be disabled for all house codes using AHP.  The CM19A does not
 * supports macros, timers, or RTC so it can be used as-is.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <signal.h>
#include <poll.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>

/**** system log ****/
#include <syslog.h>

/* Multiple On-line Controllers Home Automation Daemon */
#define DAEMON_NAME "mochad"

/**** socket ****/

#include <sys/socket.h>
#include <netinet/in.h>

#include "global.h"

#define SERVER_PORT     (1099)
#define MAXCLISOCKETS   (32)
#define MAXSOCKETS      (1+MAXCLISOCKETS)
                            /* first socket=listen socket, 20 client sockets */
#define USB_FDS         (10)    /* libusb file descriptors */
static struct pollfd Clients[(3*MAXSOCKETS)+USB_FDS];
/* Client sockets */
static struct pollfd Clientsocks[MAXCLISOCKETS];
static struct pollfd Clientxmlsocks[MAXCLISOCKETS];
static struct pollfd Clientor20socks[MAXCLISOCKETS];
static size_t NClients;     /* # of valid entries in Clientsocks */
static size_t NxmlClients;  /* # of valid entries in Clientxmlsocks */
static size_t Nor20Clients; /* # of valid entries in Clientor20socks */

/**** USB usblib 1.0 ****/

#include <libusb-1.0/libusb.h>
uint8_t InEndpoint, OutEndpoint;

static struct libusb_device_handle *Devh = NULL;
static struct libusb_transfer *IntrOut_transfer = NULL;
static struct libusb_transfer *IntrIn_transfer = NULL;
static unsigned char IntrOutBuf[8];
static unsigned char IntrInBuf[8];

/*
 * Like printf but print to socket without date/time stamp.
 * Used to send back result of getstatus command.
 */
int statusprintf(int fd, const char *fmt, ...)
{
    va_list args;
    char buf[1024];
    int buflen;

    va_start(args,fmt);
    buflen = vsnprintf(buf, sizeof(buf)-2, fmt, args);
    va_end(args);
    return send(fd, buf, buflen, MSG_NOSIGNAL);
}

static int xmlclient(int fd)
{
    int i;
    for (i = 0; i < MAXCLISOCKETS; i++) {
        if (fd == Clientxmlsocks[i].fd) return 1;
    }
    return 0;
}

/* Return 0 if the socket fd is not an OpenRemote 2.0 client.
 * Else return 1. OR clients connect to SERVER_PORT+2 (1101)  so that is
 * used.
 *
 */
int or20client(int fd)
{
    struct sockaddr_in locl;
    socklen_t locllen;

    locllen = sizeof(locl);
    if (getsockname(fd, (struct sockaddr *)&locl, &locllen) < 0) {
        dbprintf("getsockname -1/%d\n", errno);
        return 0;
    }
    dbprintf("locl port %d\n", ntohs(locl.sin_port));
    return (ntohs(locl.sin_port) == (SERVER_PORT + 2));
}

/*
 * Like printf but prefix each line with date/time stamp.
 * If fd == -1, send to all socket clients else send only to fd.
 */
int sockprintf(int fd, const char *fmt, ...)
{
    va_list args;
    char buf[1024];
    char *aLine;
    int len, buflen;
    time_t tm;
    int i;
    int bytesOut;

    aLine = buf;
    tm = time(NULL);
    len = strftime(aLine, sizeof(buf), "%m/%d %T ", localtime(&tm));
    
    va_start(args,fmt);
    buflen = vsnprintf(aLine+len, sizeof(buf)-len, fmt, args);
    va_end(args);
    buflen += len;
    if (fd != -1) {
        if (xmlclient(fd) && (aLine[buflen-1] == '\n')) {
            aLine[buflen-1] = '\0';
        }
        return send(fd, aLine, buflen, MSG_NOSIGNAL);
    }

    /* Send to sensorflare client */
    sendMessage(aLine);
    
    /* Send to all socket clients */
    for (i = 0; i < MAXCLISOCKETS; i++) {
        if ((fd = Clientsocks[i].fd) > 0) {
            dbprintf("%s i %d fd %d\n", __func__, i, fd);
            bytesOut = send(fd, aLine, buflen, MSG_NOSIGNAL);
            dbprintf("bytesOut %d\n", bytesOut);
            if (bytesOut != buflen)
                dbprintf("%s: %d/%d\n", __func__, bytesOut, errno);
        }
    }
    /* Replace trialing newline with NUL if present. 
     * This assumes newline only at end of buffer.
     */
    if (aLine[buflen-1] == '\n') {
        aLine[buflen-1] = '\0';
    }
    
    /* Send to all xml socket clients */
    for (i = 0; i < MAXCLISOCKETS; i++) {
        if ((fd = Clientxmlsocks[i].fd) > 0) {
            dbprintf("%s i %d fd %d\n", __func__, i, fd);
            /* NOTE: Send xml including trailing NUL '\0' */
            bytesOut = send(fd, aLine, buflen, MSG_NOSIGNAL);
            dbprintf("bytesOut %d\n", bytesOut);
            if (bytesOut != buflen)
                dbprintf("%s: %d/%d\n", __func__, bytesOut, errno);
        }
    }
    
    return buflen;
}

static void _hexdump(void *p, size_t len, char *outbuf, size_t outlen)
{
    unsigned char *ptr = (unsigned char*) p;
    size_t l;

    if (len == 0) return;
    if (len > (outlen / 3))
        l = outlen / 3;
    else
        l = len;
    while (l--) {
        sprintf(outbuf, "%02X ", *ptr++);
        outbuf += 3;
    }
}

void hexdump(void *p, size_t len)
{
    char buf[(3*100)+1];

    _hexdump(p, len, buf, sizeof(buf));
    puts(buf);
}

void sockhexdump(int fd, void *p, size_t len)
{
    char buf[(3*100)+1];

    _hexdump(p, len, buf, sizeof(buf));
    sockprintf(fd, "%s\n", buf);
}

// Output Raw data with header for parsing by misterhouse
void mh_sockhexdump(int fd, void *p, size_t len)
{
    char buf[(3*100)+1];

    _hexdump(p, len, buf, sizeof(buf));
    sockprintf(fd, "Raw data received: %s\n", buf);
}


static int Do_exit = 0;
static int Reattach = 0;

static void init_client(void)
{
    int i;

    for (i = 0; i < MAXCLISOCKETS; i++) {
        Clientsocks[i].fd = Clientxmlsocks[i].fd = Clientor20socks[i].fd = -1;
    }
    NClients = NxmlClients = Nor20Clients = 0;
}

/* Add new socket client */
static int add_client(int fd)
{
    int i;

    dbprintf("add_client(%d)\n", fd);
    for (i = 0; i < MAXCLISOCKETS; i++) {
        if (Clientsocks[i].fd == -1) {
            Clientsocks[i].fd = fd;
            Clientsocks[i].events = POLLIN;
            Clientsocks[i].revents = 0;
            NClients++;
            dbprintf("add_client: i %d NClients %d\n", i, NClients);
            return 0;
        }
    }
    dbprintf("max clients exceeded %d\n", i);
    return -1;
}

/* Add new flashxml socket client */
static int add_xmlclient(int fd)
{
    int i;

    dbprintf("add_xmlclient(%d)\n", fd);
    for (i = 0; i < MAXCLISOCKETS; i++) {
        if (Clientxmlsocks[i].fd == -1) {
            Clientxmlsocks[i].fd = fd;
            Clientxmlsocks[i].events = POLLIN;
            Clientxmlsocks[i].revents = 0;
            NxmlClients++;
            dbprintf("add_xmlclient: i %d NxmlClients %d\n", i, NxmlClients);
            return 0;
        }
    }
    dbprintf("max XML clients exceeded %d\n", i);
    return -1;
}

/* Add new or20 socket client */
static int add_or20client(int fd)
{
    int i;

    dbprintf("add_or20client(%d)\n", fd);
    for (i = 0; i < MAXCLISOCKETS; i++) {
        if (Clientor20socks[i].fd == -1) {
            Clientor20socks[i].fd = fd;
            Clientor20socks[i].events = POLLIN;
            Clientor20socks[i].revents = 0;
            Nor20Clients++;
            dbprintf("add_or20client: i %d Nor20Clients %d\n", i, Nor20Clients);
            return 0;
        }
    }
    dbprintf("max OR20 clients exceeded %d\n", i);
    return -1;
}

/* Delete socket client */
int del_client(int fd)
{
    int i;

    dbprintf("del_client(%d)\n", fd);
    for (i = 0; i < MAXCLISOCKETS; i++) {
        if (Clientsocks[i].fd == fd) {
            Clientsocks[i].fd = -1;
            NClients--;
            dbprintf("del_client: i %d NClients %d\n", i, NClients);
            return 0;
        }
        if (Clientxmlsocks[i].fd == fd) {
            Clientxmlsocks[i].fd = -1;
            NxmlClients--;
            dbprintf("del_client: i %d NxmlClients %d\n", i, NxmlClients);
            return 0;
        }
        if (Clientor20socks[i].fd == fd) {
            shutdown(fd, SHUT_RDWR);
            close(fd);
            Clientor20socks[i].fd = -1;
            Nor20Clients--;
            dbprintf("del_client: i %d Nor20Clients %d\n", i, Nor20Clients);
            return 0;
        }
    }
    dbprintf("del_client:fd not found %d\n", fd);
    return -1;
}

/* Copy socket client records to array */
static int copy_clients(struct pollfd *Clients)
{
    int i;

    dbprintf("copy_clients\n");
    for (i = 0; i < MAXCLISOCKETS; i++) {
        if (Clientsocks[i].fd != -1) {
            *Clients++ = Clientsocks[i];
        }
        if (Clientxmlsocks[i].fd != -1) {
            *Clients++ = Clientxmlsocks[i];
        }
        if (Clientor20socks[i].fd != -1) {
            *Clients++ = Clientor20socks[i];
        }
    }
    dbprintf("copy_clients %d\n", NClients+NxmlClients+Nor20Clients);
    return NClients+NxmlClients+Nor20Clients;
}
/* Client sockets */

#include "x10state.h"
#include "x10_write.h"
#include "encode.h"
#include "decode.h"



struct binarydata {
    size_t binlength;
    unsigned char bindata[8];
};

static const struct binarydata initcm15abinary[] = {
#if 0
    {8, {0x9b,0x00,0x5b,0x09,0x50,0x90,0x60,0x02}},
    {8, {0x9b,0x00,0x5b,0x09,0x50,0x90,0x60,0x02}},
    {8, {0xbb,0x00,0x00,0x05,0x00,0x14,0x20,0x28}},
    {1, {0x8b}},
    {3, {0xdb,0x1f,0xf0}},
    {3, {0xdb,0x20,0x00}},
    {3, {0xab,0xde,0xaf}},
    {1, {0x8b}},
    {3, {0xab,0x00,0x00}},
#endif
    {0}
};

static const struct binarydata initcm19abinary[] = {
    {8, {0x80,0x05,0x1b,0x14,0x28,0x20,0x24,0x29}},
    {2, {0x83,0x03}},
    {8, {0x84,0x37,0x02,0x60,0x00,0x00,0x00,0x00}},
    {8, {0x80,0x01,0x00,0x14,0x20,0x24,0x28,0x29}},
    {3, {0x83,0x02,0x0f}},
    {8, {0x83,0x37,0x02,0x60,0x00,0x00,0x00,0x00}},
    {5, {0x20,0x34,0xcb,0x58,0xa7}},
    {8, {0x80,0x05,0x01,0x14,0x20,0x24,0x28,0x29}},
    {0}
};


static void initcm1Xa(const struct binarydata *p)
{
    dbprintf("initcm1Xa\n");
    while (p->binlength) {
        x10_write((unsigned char *)p->bindata, p->binlength);
        p++;
    }
}

/* Find CM15A or CM19A. The EU versions (CM15Pro and CM19Pro) have the same
 * vendor and product IDs, respectively.
 */
static int find_cm15a(struct libusb_device_handle **devhptr)
{
    int r;

    Cm19a = 0;
    *devhptr = libusb_open_device_with_vid_pid(NULL,  0x0bc7, 0x0001);
    if (!*devhptr) {
        *devhptr = libusb_open_device_with_vid_pid(NULL,  0x0bc7, 0x0002);
        if (!*devhptr) {
            syslog(LOG_EMERG, "libusb_open_device_with_vid_pid failed");
            return -EIO;
        }
        Cm19a = 1;
    }
    r = libusb_claim_interface(*devhptr, 0);
    if (r == 0) {
        syslog(LOG_NOTICE, (Cm19a) ? "Found CM19A" : "Found CM15A");
        return 0;
    }
    syslog(LOG_EMERG, "usb_claim_interface failed %d", r);
    r = libusb_kernel_driver_active(*devhptr, 0);
    if (r < 0) {
        syslog(LOG_EMERG, "Kernel driver check failed %d", r);
        return -EIO;
    }
    syslog(LOG_NOTICE, "Found kernel driver %d, trying detach", r);
    r = libusb_detach_kernel_driver(*devhptr, 0);
    if (r < 0) {
        syslog(LOG_EMERG, "Kernel driver detach failed %d", r);
        return -EIO;
    }
    Reattach = 1;
    r = libusb_claim_interface(*devhptr, 0);
    if (r < 0) {
        syslog(LOG_EMERG, "claim interface failed again %d", r);
        return -EIO;
    }
    syslog(LOG_NOTICE, (Cm19a) ? "Found CM19A" : "Found CM15A");
    return 0;
}

/* Find the in and out endpoint address in the device descriptors.
 * This is required by newer CM19A that have changed endpoint addresses.
 */
static int get_endpoint_address(libusb_device_handle *devh, uint8_t *inendpt, uint8_t *outendpt)
{
    int r;
    struct libusb_config_descriptor *config;
    const struct libusb_interface *interfaces;
    const struct libusb_interface_descriptor *interface_desc;
    const struct libusb_endpoint_descriptor *endpoint_desc;
    struct libusb_device *uDevice;
    struct libusb_device_descriptor desc;
    int i, j, k;

    uDevice = libusb_get_device(devh);
    if (!uDevice) return -1;

    r = libusb_get_device_descriptor(uDevice, &desc);
    if (r < 0) return r;

    r = libusb_get_active_config_descriptor(uDevice, &config);
    if (r < 0) return r;
    interfaces = config->interface;
    for (i = 0; i < config->bNumInterfaces; i++) {
        interface_desc = interfaces->altsetting;
        for (j = 0; j < interfaces->num_altsetting; j++) {
            endpoint_desc = interface_desc->endpoint;
            for (k = 0; k < interface_desc->bNumEndpoints; k++) {
                if (endpoint_desc->bEndpointAddress & 0x80) {
                    *inendpt = endpoint_desc->bEndpointAddress;
                }
                else {
                    *outendpt = endpoint_desc->bEndpointAddress;
                }
                endpoint_desc++;
            }
            interface_desc++;
        }
        interfaces++;
    }
    libusb_free_config_descriptor(config);
    return 0;
}

static void IntrOut_cb(struct libusb_transfer *transfer)
{
    /* dbprintf("IntrOut callback len %d\n", transfer->actual_length); */
}

static void IntrIn_cb(struct libusb_transfer *transfer)
{
#if 0
    int fd, i;
#endif

    if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        dbprintf("IntrIn transfer status %d?\n", transfer->status);
        Do_exit = 2;
        libusb_free_transfer(transfer);
        IntrIn_transfer = NULL;
        return;
    }

    /* dbprintf("IntrIn callback len %d ", transfer->actual_length); */
    /* hexdump(transfer->buffer, transfer->actual_length); */

/*        if ((transfer->actual_length == 1) && (*transfer->buffer == 0x55)) {  */
    if (transfer->actual_length == 1) {
        send_next_x10out();
    }

#if 0
    /* Incoming USB data is sent to all sockets */
    for (i = 0; i < MAXCLISOCKETS; i++) {
        if ((fd = Clientsocks[i].fd) > 0) {
            cm15a_decode(fd, transfer->buffer, transfer->actual_length);
        }
    }
#else
    cm15a_decode(-1, transfer->buffer, transfer->actual_length);
#endif
    if (libusb_submit_transfer(IntrIn_transfer) < 0)
        Do_exit = 2;
}

static int start_transfers(void)
{
    int r;

    r = libusb_submit_transfer(IntrIn_transfer);
    if (r < 0)
        return r;
    return 0;
}

static int do_init(void)
{
    // set clock?

    return 0;
}

static int alloc_transfers(void)
{
    IntrIn_transfer = libusb_alloc_transfer(0);
    if (!IntrIn_transfer)
        return -ENOMEM;
    libusb_fill_interrupt_transfer(IntrIn_transfer, Devh, InEndpoint, 
            IntrInBuf, sizeof(IntrInBuf), IntrIn_cb, NULL, 0);

    IntrOut_transfer = libusb_alloc_transfer(0);
    if (!IntrOut_transfer)
        return -ENOMEM;
    return 0;
}

int write_usb(unsigned char *buf, size_t len)
{
    int r, i;

    dbprintf("usb len %lu ", (unsigned long)len);
    hexdump(buf, len);
    memcpy(IntrOutBuf, buf, len);
    libusb_fill_interrupt_transfer(IntrOut_transfer, Devh, OutEndpoint, 
            IntrOutBuf, len, IntrOut_cb, NULL, 0);
    r = libusb_submit_transfer(IntrOut_transfer);
    if (r < 0) {
        libusb_cancel_transfer(IntrOut_transfer);
        i = 100;
        while (IntrOut_transfer && i--)
            if (libusb_handle_events(NULL) < 0)
                break;
        return r;
    }
    return 0;
}

static void sighandler(int signum)
{
    Do_exit = 1;	
}

static int mydaemon(void)
{
    int nready, i;

    /**** sockets ****/
    socklen_t clilen; 
    int clifd, listenfd, flashxmlfd, or20fd;
    unsigned char buf[1024];
    int bytesIn;
    struct sockaddr_in cliaddr, servaddr;
    int rc;
    static const int optval=1;

    /**** USB ****/
    struct sigaction sigact;
    int r = 1;
    const struct libusb_pollfd **usbfds;
    nfds_t nusbfds;
    struct timeval timeout;

    hua_sec_init();

    r = libusb_init(NULL);
    if (r < 0) {
        syslog(LOG_EMERG, "failed to initialise libusb %d", r);
        dbprintf("failed to initialise libusb %d\n", r);
        exit(1);
    }
    libusb_set_debug(NULL, 3);

#if 0
    /* This function is not available in older versions of libusb-1.0 */
    r = libusb_pollfds_handle_timeouts(NULL);
    if (!r) {
        dbprintf("poll timeout required %d\n", r);
        goto out;
    }
#endif
    r = find_cm15a(&Devh);
    if (r < 0) {
        syslog(LOG_EMERG, "Could not find/open CM15A/CM19A %d", r);
        dbprintf("Could not find/open CM15A/CM19A %d\n", r);
        goto out;
    }

    r = get_endpoint_address(Devh, &InEndpoint, &OutEndpoint);
    if (r < 0) {
        syslog(LOG_EMERG, "Could not find endpoints %d", r);
        dbprintf("Could not find endpoints %d\n", r);
        goto out_deinit;
    }
    syslog(LOG_NOTICE, "In endpoint 0x%02X, Out endpoint 0x%02X",
            InEndpoint, OutEndpoint);

    r = do_init();
    if (r < 0)
        goto out_deinit;

    r = alloc_transfers();
    if (r < 0)
        goto out_deinit;

    r = start_transfers();
    if (r < 0)
        goto out_deinit;

    sigact.sa_handler = sighandler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);
    sigaction(SIGQUIT, &sigact, NULL);

    usbfds = libusb_get_pollfds(NULL);
    dbprintf("usbfds %p %p %p %p %p\n", usbfds, 
            usbfds[0], usbfds[1], usbfds[2], usbfds[3]);
    nusbfds = 3;        /* Skip over listen fd at [0,1,2] */
    for (i = 0; usbfds[i] != NULL; i++) {
        dbprintf(" %lu: %p fd %d %04X\n", nusbfds, 
                usbfds[i], usbfds[i]->fd, usbfds[i]->events);
        Clients[nusbfds].fd = usbfds[i]->fd;
        Clients[nusbfds].events = usbfds[i]->events;
        Clients[nusbfds].revents = 0;
        nusbfds++;
    }
    nusbfds -= 3;  /* Adjust for skipping 0,1,2 */
    dbprintf("nusbfds %lu\n", nusbfds);
    memset(&timeout, 0, sizeof(timeout));
    if (Cm19a)
        initcm1Xa(initcm19abinary);
    else
        initcm1Xa(initcm15abinary);

    /**** sockets ****/
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    dbprintf("listenfd %d\n", listenfd);
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERVER_PORT);

    rc = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    dbprintf("setsockopt() %d/%d\n", rc, errno);
    rc = bind(listenfd, (struct sockaddr*) &servaddr, sizeof(servaddr));
    dbprintf("bind() %d/%d\n", rc, errno);
    rc = listen(listenfd, 128);
    dbprintf("listen() %d/%d\n", rc, errno);

    /* Listen socket for Flash XML clients */
    flashxmlfd = socket(AF_INET, SOCK_STREAM, 0);
    dbprintf("flashxmlfd %d\n", flashxmlfd);
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERVER_PORT+1);

    rc = setsockopt(flashxmlfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    dbprintf("setsockopt() %d/%d\n", rc, errno);
    rc = bind(flashxmlfd, (struct sockaddr*) &servaddr, sizeof(servaddr));
    dbprintf("bind() %d/%d\n", rc, errno);
    rc = listen(flashxmlfd, 128);
    dbprintf("listen() %d/%d\n", rc, errno);

    /* Listen socket for OR 2.0 clients */
    or20fd = socket(AF_INET, SOCK_STREAM, 0);
    dbprintf("or20fd %d\n", or20fd);
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERVER_PORT+2);

    rc = setsockopt(or20fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    dbprintf("setsockopt() %d/%d\n", rc, errno);
    rc = bind(or20fd, (struct sockaddr*) &servaddr, sizeof(servaddr));
    dbprintf("bind() %d/%d\n", rc, errno);
    rc = listen(or20fd, 128);
    dbprintf("listen() %d/%d\n", rc, errno);

    init_sensorflare(Cm19a);

    char connectedMessage[30];
    sprintf(connectedMessage,"Connected %s\n",Cm19a?"CM19A":"CM15A");
    sendMessage(connectedMessage);
    
    init_client();
    Clients[0].fd = listenfd;
    Clients[0].events = POLLIN;
    Clients[1].fd = flashxmlfd;
    Clients[1].events = POLLIN;
    Clients[2].fd = or20fd;
    Clients[2].events = POLLIN;
    PollTimeOut = -1;

    while (!Do_exit) {
        int nsockclients;
        int npollfds;

        /* Start appending records for socket clients to Clients array after 
         * listen, flashxml listen, or20 listen, and USB records
         */
        nsockclients = copy_clients(&Clients[3+nusbfds]);
        /* 1 for listen socket, 1 for flashxml listen socket, 1 for or20 listen
         * socket, nusbfds for libusb, nsockclients for socket clients
         */
        npollfds = 3 + nusbfds + nsockclients;
        nready = poll(Clients, npollfds, PollTimeOut);
#if 0
        dbprintf("poll() %d\n", nready);
        for (i = 0; i < npollfds; i++) {
            dbprintf("Clients[%d] fd %d events %X revents %X\n",
                    i, Clients[i].fd, Clients[i].events, Clients[i].revents);
        }
#endif
        /**** Time out ****/
        if (nready == 0) {
            send_next_x10out();
        }
        else {
            /**** USB ****/
            libusb_handle_events_timeout(NULL, &timeout);

            /**** listen sockets ****/
            if (Clients[0].revents & POLLIN) {
                /* new client connection */
                clilen = sizeof(cliaddr);
                clifd  = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
                dbprintf("accept() %d/%d\n", clifd, errno);
                syslog(LOG_INFO,"accept() %d/%d\n", clifd, errno);
                r = add_client(clifd);
                if (--nready <= 0) continue;
            }
            if (Clients[1].revents & POLLIN) {
                /* new flashxml client connection */
                clilen = sizeof(cliaddr);
                clifd  = accept(flashxmlfd, (struct sockaddr *)&cliaddr, &clilen);
                dbprintf("flashxml accept() %d/%d\n", clifd, errno);
                r = add_xmlclient(clifd);
                if (--nready <= 0) continue;
            }

            if (Clients[2].revents & POLLIN) {
                /* new OR2.0 client connection */
                clilen = sizeof(cliaddr);
                clifd  = accept(or20fd, (struct sockaddr *)&cliaddr, &clilen);
                dbprintf("or20 accept() %d/%d\n", clifd, errno);
                r = add_or20client(clifd);
                if (--nready <= 0) continue;
            }

            for (i = 3+nusbfds; i < npollfds; i++) {
                if ((clifd = Clients[i].fd) >= 0) {
                    /* dbprintf("client %d revents 0x%X\n", i, Clients[i].revents); */
                    if (Clients[i].revents & (POLLIN|POLLERR)) {
                        if ((bytesIn = read(clifd, buf, sizeof(buf))) < 0) {
                            dbprintf("read err %d\n", errno);
                            if (errno != ECONNRESET) {
                                dbprintf("serious error %d\n", errno);
                            }
                            close(clifd);
                            del_client(clifd);
                        }
                        else if (bytesIn == 0) {
                            dbprintf("read EOF %d\n", bytesIn);
                            close(clifd);
                            del_client(clifd);
                        }
                        else {
                            syslog(LOG_INFO,"encoding command");
                            cm15a_encode(clifd, buf, (size_t)bytesIn);
                        }
                        if (--nready <= 0) break;
                    }
                }
            }
        }
    }
    syslog(LOG_NOTICE, (Cm19a) ? "detaching CM19A" : "detaching CM15A");

    if (IntrOut_transfer) {
        r = libusb_cancel_transfer(IntrOut_transfer);
        if (r < 0)
            goto out_deinit;
    }

    if (IntrIn_transfer) {
        r = libusb_cancel_transfer(IntrIn_transfer);
        if (r < 0)
            goto out_deinit;
    }

    i = 100;
    while ((IntrOut_transfer || IntrIn_transfer) && i--)
        if (libusb_handle_events(NULL) < 0)
            break;

    if (Do_exit == 1)
        r = 0;
    else
        r = 1;

out_deinit:
    libusb_free_transfer(IntrIn_transfer);
    libusb_free_transfer(IntrOut_transfer);
/* out_release: */
    libusb_release_interface(Devh, 0);
out:
    libusb_close(Devh);
    if (Reattach) libusb_attach_kernel_driver(Devh, 0);
    libusb_exit(NULL);
    return r >= 0 ? r : -r;
}

static void printcopy(void)
{
    printf("Copyright (C) 2010-2012 Brian Uechi.\n");
    printf("\n");
    printf("This program comes with NO WARRANTY.\n");
    printf("You may redistribute copies of this program\n");
    printf("under the terms of the GNU General Public License.\n");
    printf("For more information about these matters, see the file named COPYING.\n");
    fflush(NULL);
}

// This affects whether decode.c will show raw frame data for debugging RF connectivity
// as well as providing raw data for parsing by users like misterhouse's X10_CMxx module.
int raw_data = 0;
int main(int argc, char *argv[])
{
    int rc, i;
    int foreground=0;

    /* Initialize logging */
    openlog(DAEMON_NAME, LOG_PID, LOG_LOCAL5);
    syslog(LOG_NOTICE, "starting");

    /* Process command line args */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0)
            foreground = 1;
        else if (strcmp(argv[i], "--raw-data") == 0)
            raw_data = 1;
        else if (strcmp(argv[i], "--version") == 0) {
            printf("%s\n", PACKAGE_STRING);
            printcopy();
            exit(0);
        }
        else {
            printf("unknown option %s\n", argv[i]);
            exit(-1);
        }
    }

    /* Daemonize */
    if (!foreground) {
        rc = daemon(0, 0);
        dbprintf("daemon() => %d\n", rc);
    }

    /* Do real work */
    rc = mydaemon();

    /* Finish up */
    syslog(LOG_NOTICE, "terminated");
    closelog();
    return rc;
}
