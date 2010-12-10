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

#define SERVER_PORT (1099)
#define MAXCLISOCKETS   (16)
#define MAXSOCKETS      (1+MAXCLISOCKETS)
                            /* first socket=listen socket, 20 client sockets */
#define USB_FDS         (10)    /* libusb file descriptors */
static struct pollfd Clients[MAXSOCKETS+USB_FDS];
/* Client sockets */
static struct pollfd Clientsocks[MAXCLISOCKETS];
static size_t NClients;     /* # of valid entries in Clientsocks */


/**** USB usblib 1.0 ****/

#include <libusb-1.0/libusb.h>
#define INTR_EP_1_IN    (0x81)
#define INTR_EP_2_OUT   (0x02)

static struct libusb_device_handle *Devh = NULL;
static struct libusb_transfer *IntrOut_transfer = NULL;
static struct libusb_transfer *IntrIn_transfer = NULL;
static unsigned char IntrOutBuf[8];
static unsigned char IntrInBuf[8];

/*
 * Like printf but prefix each line with date/time stamp.
 */
int sockprintf(int fd, const char *fmt, ...)
{
    va_list args;
    char buf[1024];
    int len, buflen;
    time_t tm;
    int i;
    int bytesOut;

    tm = time(NULL);
    len = strftime(buf, sizeof(buf), "%m/%d %T ", localtime(&tm));
    va_start(args,fmt);
    buflen = vsnprintf(buf+len, sizeof(buf)-len, fmt, args);
    va_end(args);
    buflen += len;
    if (fd != -1) return send(fd, buf, buflen, MSG_NOSIGNAL);

    /* Send to all socket clients */
    for (i = 0; i < MAXCLISOCKETS; i++) {
        if ((fd = Clientsocks[i].fd) > 0) {
            dbprintf("%s i %d fd %d\n", __func__, i, fd);
            bytesOut = send(fd, buf, buflen, MSG_NOSIGNAL);
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

static int Do_exit = 0;
static int Reattach = 0;

static void init_client(void)
{
    int i;

    for (i = 0; i < MAXCLISOCKETS; i++) Clientsocks[i].fd = -1;
    NClients = 0;
}

/* Add new socket client */
static int add_client(int fd)
{
    int i;

    for (i = 0; i < MAXCLISOCKETS; i++) {
        if (Clientsocks[i].fd == -1) {
            Clientsocks[i].fd = fd;
            Clientsocks[i].events = POLLIN;
            Clientsocks[i].revents = 0;
            NClients++;
            return 0;
        }
    }
    dbprintf("max clients exceeded %d\n", i);
    return -1;
}

/* Delete socket client */
static int del_client(int fd)
{
    int i;

    for (i = 0; i < MAXCLISOCKETS; i++) {
        if (Clientsocks[i].fd == fd) {
            Clientsocks[i].fd = -1;
            NClients--;
            return 0;
        }
    }
    dbprintf("fd not found %d\n", fd);
    return -1;
}

/* Copy socket client records to array */
static int copy_clients(struct pollfd *Clients)
{
    int i;

    for (i = 0; i < MAXCLISOCKETS; i++) {
        if (Clientsocks[i].fd != -1) {
            *Clients++ = Clientsocks[i];
        }
    }
    return NClients;
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
    {8, {0x9b,0x00,0x5b,0x09,0x50,0x90,0x60,0x02}},
    {8, {0x9b,0x00,0x5b,0x09,0x50,0x90,0x60,0x02}},
    {8, {0xbb,0x00,0x00,0x05,0x00,0x14,0x20,0x28}},
    {1, {0x8b}},
    {3, {0xdb,0x1f,0xf0}},
    {3, {0xdb,0x20,0x00}},
    {3, {0xab,0xde,0xaf}},
    {1, {0x8b}},
    {3, {0xab,0x00,0x00}},
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
        syslog(LOG_NOTICE, "Found CM15A");
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
    syslog(LOG_NOTICE, "Found CM15A");
    return 0;
}

static void IntrOut_cb(struct libusb_transfer *transfer)
{
    dbprintf("IntrOut callback len %d\n", transfer->actual_length);
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

    dbprintf("IntrIn callback len %d ", transfer->actual_length);
    hexdump(transfer->buffer, transfer->actual_length);

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
    libusb_fill_interrupt_transfer(IntrIn_transfer, Devh, INTR_EP_1_IN, 
            IntrInBuf, sizeof(IntrInBuf), IntrIn_cb, NULL, 0);

    IntrOut_transfer = libusb_alloc_transfer(0);
    if (!IntrOut_transfer)
        return -ENOMEM;
    return 0;
}

int write_usb(unsigned char *buf, size_t len)
{
    int r;

    dbprintf("usb len %lu ", (unsigned long)len);
    hexdump(buf, len);
    memcpy(IntrOutBuf, buf, len);
    libusb_fill_interrupt_transfer(IntrOut_transfer, Devh, INTR_EP_2_OUT, 
            IntrOutBuf, len, IntrOut_cb, NULL, 0);
    r = libusb_submit_transfer(IntrOut_transfer);
    if (r < 0) {
        libusb_cancel_transfer(IntrOut_transfer);
        while (IntrOut_transfer)
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
    int clifd, listenfd;
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
        dbprintf("failed to initialise libusb %d\r", r);
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
        syslog(LOG_EMERG, "Could not find/open CM15A %d", r);
        dbprintf("Could not find/open CM15A %d\n", r);
        goto out;
    }

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

    if (r < 0)
        goto out_deinit;
    usbfds = libusb_get_pollfds(NULL);
    dbprintf("usbfds %p %p %p %p %p\n", usbfds, 
            usbfds[0], usbfds[1], usbfds[2], usbfds[3]);
    nusbfds = 1;        /* Skip over listen fd at [0] */
    for (i = 0; usbfds[i] != NULL; i++) {
        dbprintf(" %lu: %p fd %d %04X\n", nusbfds, 
                usbfds[i], usbfds[i]->fd, usbfds[i]->events);
        Clients[nusbfds].fd = usbfds[i]->fd;
        Clients[nusbfds].events = usbfds[i]->events;
        Clients[nusbfds].revents = 0;
        nusbfds++;
    }
    nusbfds--;  /* Adjust for skipping [0] */
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
    rc = listen(listenfd, 5);
    dbprintf("listen() %d/%d\n", rc, errno);
    init_client();

    Clients[0].fd = listenfd;
    Clients[0].events = POLLIN;
    PollTimeOut = -1;

    while (!Do_exit)
    {
        int nsockclients;
        int npollfds;

        /* Start appending records for socket clients to Clients array after 
         * listen and USB records
         */
        nsockclients = copy_clients(&Clients[1+nusbfds]);
        /* 1 for listen socket, nusbfds for libusb, nsockclients for socket clients
         */
        npollfds = 1 + nusbfds + nsockclients;
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

            /**** sockets ****/
            if (Clients[0].revents & POLLIN) {
                /* new client connection */
                clilen = sizeof(cliaddr);
                clifd  = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
                dbprintf("accept() %d/%d\n", clifd, errno);

                r = add_client(clifd);

                if (--nready <= 0) continue;
            }

            for (i = 1+nusbfds; i < npollfds; i++) {
                if ((clifd = Clients[i].fd) >= 0) {
                    /* dbprintf("client %d revents 0x%X\n", i, Clients[i].revents); */
                    if (Clients[i].revents & (POLLIN|POLLERR)) {
                        if ((bytesIn = read(clifd, buf, sizeof(buf))) < 0) {
                            dbprintf("read err %d\n", errno);
                            if (errno == ECONNRESET) {
                                close(clifd);
                                del_client(clifd);
                            }
                            else {
                                dbprintf("serious error %d\n", errno);
                            }
                        }
                        else if (bytesIn == 0) {
                            dbprintf("read EOF %d\n", bytesIn);
                            close(clifd);
                            del_client(clifd);
                        }
                        else {
                            cm15a_encode(clifd, buf, (size_t)bytesIn);
                        }
                        if (--nready <= 0) break;
                    }
                }
            }
        }
    }
    syslog(LOG_NOTICE, "detaching CM15A");

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

    while (IntrOut_transfer || IntrIn_transfer)
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

int main(int argc, char *argv[])
{
    int rc;
    int foreground=0;

    /* Initialize logging */
    openlog(DAEMON_NAME, LOG_PID, LOG_LOCAL5);
    syslog(LOG_NOTICE, "starting");

    /* Process command line args */
    if (argc > 1) {
        foreground = !strcmp(argv[1], "-d");
        dbprintf("foreground %d\n", foreground);
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
