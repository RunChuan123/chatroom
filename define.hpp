#pragma once


#include <sys/epoll.h>
#include <unistd.h>
#include <iostream>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define NUM_WORKERS 8

#define SQL_IP "1.94.121.19"

#define SQL_USER "reuser"
#define SQL_PASSWD "12345678"
#define SQL_DB "chatroom"
#define EPOLL_MAX_EVENTS 1024


