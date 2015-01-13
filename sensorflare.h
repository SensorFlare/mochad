#ifndef SENSORFLARE_H
#define SENSORFLARE_H

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdint.h>
#include <unistd.h>

#include <ctype.h>

#include <pthread.h>

#include <amqp_tcp_socket.h>
#include <amqp_framing.h>

#include <assert.h>

/**** system log ****/
#include <syslog.h>

#include <confuse.h>

//#include <encode.h>

#define false 0
#define true 1
typedef int bool; // or #define bool int

#define STATUS_INTERVAL 60

void die(const char *fmt, ...);
void die_on_error(int x, char const *context);
void die_on_amqp_error(amqp_rpc_reply_t x, char const *context);
void microsleep(int usec);
void amqp_dump(void const *buffer, size_t len);

amqp_socket_t *rabbit_socket;
amqp_connection_state_t conn;

void * status_reporting(void *);
void * receiver(void *);
void sendMessage(const char * messagebody);
void init_sensorflare(void);

char exchange[20];
char commands_queue[20];
bool sensorflare_connected;
#endif