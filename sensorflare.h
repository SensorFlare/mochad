#ifndef SENSORFLARE_H
#define SENSORFLARE_H

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdint.h>

#include <ctype.h>

#include <pthread.h>

#include <amqp_tcp_socket.h>
#include <amqp_framing.h>

#include <assert.h>

/**** system log ****/
#include <syslog.h>

#include <confuse.h>

//#include <encode.h>


void die(const char *fmt, ...);
void die_on_error(int x, char const *context);
void die_on_amqp_error(amqp_rpc_reply_t x, char const *context);
void microsleep(int usec);
void amqp_dump(void const *buffer, size_t len);


int receiver_thread_status;

amqp_socket_t *rabbit_socket;
amqp_connection_state_t conn;
char * username;
char * password;

void * receiver(void *threadid);
void sendMessage(char * messageBody);
void init_sensorflare(long int);

pthread_t rabbit_receiver_thread;
    
char exchange[20];
char commands_queue[20];    
#endif