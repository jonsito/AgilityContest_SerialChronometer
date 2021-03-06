//
// Created by jantonio on 5/05/19.
//

#define AGILITYCONTEST_SERIALCHRONOMETER_WEB_MGR_C

#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "debug.h"
#include "main.h"
#include "web_mgr.h"
#include "sc_config.h"
#include "sc_tools.h"
#include "sc_sockets.h"
#include "web_server.h"

queue_t *input_queue;
queue_t *output_queue;

// traslate tokens into a message to be sent.
// notice that htmlparse() request to escape special chars is needed later
static int web_mgr_sendMsg(configuration *config, int slot,char **tokens, int ntokens) {
    if (strcmp(SC_WEBSRV,tokens[0])==0) return 0;
    char buffer[1024];
    int len=sprintf(buffer,"%s",tokens[1]);
    for (int n=2;n<ntokens;n++) len += sprintf(buffer+len," %s",tokens[n]);
    queue_put(output_queue,buffer);
    return len;
}

static int web_mgr_sendStart(configuration *config, int slot,char **tokens, int ntokens) {
    if (strcmp(SC_WEBSRV,tokens[0])==0) return 0;
    if (ntokens==3) return web_mgr_sendMsg(config,slot,tokens,ntokens);
    queue_put(output_queue,"start 0");
    return 7; // strlen("start 0")
}

static int web_mgr_sendWalk(configuration *config, int slot,char **tokens, int ntokens) {
    if (strcmp(SC_WEBSRV,tokens[0])==0) return 0;
    if (ntokens==3) return web_mgr_sendMsg(config,slot,tokens,ntokens);
    queue_put(output_queue,"walk 420");
    return 8; // strlen("walk 0")
}

static int web_mgr_sendDown(configuration *config, int slot,char **tokens, int ntokens) {
    if (strcmp(SC_WEBSRV,tokens[0])==0) return 0;
    if (ntokens==3) return web_mgr_sendMsg(config,slot,tokens,ntokens);
    queue_put(output_queue,"down 15");
    return 7; // strlen("start 0")
}

static int web_mgr_sendTurn(configuration *config, int slot,char **tokens, int ntokens) {
    if (strcmp(SC_WEBSRV,tokens[0])==0) return 0;
    char buffer[1024];
    snprintf(buffer,1024,"turn %d",config->status.numero);
    queue_put(output_queue,buffer);
    return strlen(buffer); // strlen("start 0")
}

static int web_mgr_sendBright(configuration *config, int slot,char **tokens, int ntokens) {
    if (strcmp(SC_WEBSRV,tokens[0])==0) return 0;
    char buffer[1024];
    snprintf(buffer,1024,"bright %d",config->bright);
    queue_put(output_queue,buffer);
    return strlen(buffer); // strlen("start 0")
}
static int web_mgr_exit(configuration * config, int slot, char **tokens, int ntokens) {
    debug(DBG_INFO,"Internal html interface thread: exit requested");
    return -1;
}

// F/T/E events are not sent: refresh loop take care on it
static func entries[32]= {
        web_mgr_sendStart, // { 0, "start",   "Start of course run",             "[miliseconds] {0}"},
        web_mgr_sendMsg, // { 1, "int",     "Intermediate time mark",          "<miliseconds>"},
        web_mgr_sendMsg, // { 2, "stop",    "End of course run",               "<miliseconds>"},
        web_mgr_sendMsg, // { 3, "fail",    "Sensor faillure detected",        ""},
        web_mgr_sendMsg, // { 4, "ok",      "Sensor recovery. Chrono ready",   ""},
        web_mgr_sendMsg, // { 5, "msg",     "Show message on chrono display",  "<message> [seconds] {2}"},
        web_mgr_sendWalk, // { 6, "walk",    "Course walk (0:stop)",            "<seconds> {420}"},
        web_mgr_sendDown, // { 7, "down",    "Start 15 seconds countdown",      ""},
        NULL,            // { 8, "fault",   "Mark fault (+/-/#)",              "< + | - | num >"},
        NULL,            // { 9, "refusal", "Mark refusal (+/-/#)",            "< + | - | num >"},
        NULL,            // { 10, "elim",    "Mark elimination [+-]",           "[ + | - ] {+}"},
        NULL,            // { 11, "data",   "Set faults/refusal/disq info",    "<flt>:<reh>:<disq>"},
        web_mgr_sendMsg, // { 12, "reset",  "Reset chronometer and countdown", "" },
        NULL,            // { 13, "help",   "show command list",               "[cmd]"},
        NULL,            // { 14, "version","Show software version",           "" },
        web_mgr_exit,    // { 15, "exit",   "End program (from console)",      "" },
        NULL,            // { 16, "server", "Set server IP address",           "<x.y.z.t> {0.0.0.0}" },
        NULL,            // { 17, "ports",  "Show available serial ports",     "" },
        NULL,            // { 18, "config", "List configuration parameters",   "" },
        NULL,            // { 19, "status", "Show faults/refusal/elim info",   "" },
        web_mgr_sendTurn, // { 20, "turn",   "Set current dog order number [+-#]", "[ + | - | num ] {+}"},
        web_mgr_sendBright,// { 21, "bright", "Set display bright level (0..9) [+-#]","[ + | - | num ] {+}"},
        NULL,            // { 22, "clock",  "Enter clock mode",                "[ hh:mm:ss ] {current time}"},
        NULL,            // { 23, "debug",  "Get/Set debug level",             "[ new_level ]"},
        NULL,            // { 24, "dorsal",  "get dorsal from qrcode reader",  "[ dorsal # ]"},
        NULL             // { -1, NULL,     "",                                "" }
};

void *web_manager_thread(void *arg){
    int res=0;
    int slotIndex= * ((int *)arg);
    sc_thread_slot *slot=&sc_threads[slotIndex];
#ifdef __APPLE__
    pthread_setname_np(slot->tname);
#endif
    configuration *config=slot->config;
    slot->entries=entries;

    // create sock
    char portstr[16];
    snprintf(portstr,16,"%d",config->local_port+config->ring);
    slot->sock=connectUDP("localhost",portstr);
    if (slot->sock <0) {
        debug(DBG_ERROR,"%s: Cannot create local socket",SC_WEBSRV);
        return NULL;
    }

    // initialize queue structures for data I/O
    input_queue= queue_create("input_queue");
    output_queue= queue_create("output_queue");
    if (!input_queue || !output_queue) {
        debug(DBG_ERROR,"%s: Cannot create web server data queues",SC_WEBSRV);
        return NULL;
    }

    // initialize web server in a separate thread. slot index 4 wont be used for managers
    sc_thread_create(4,"Web Server",config,init_webServer);

    // mark web_manager thread alive before entering loop
    slot->index=slotIndex;

    // prepare data before enter loop
    char *p;
    char request[1024];
    sprintf(request,"%s ",SC_WEBSRV);
    char response[1024];
    res=0;
    while(res>=0) {
        char *msg=queue_get(input_queue);
        if (!msg || *msg=='\0') {
            // debug(DBG_TRACE,"queue_get(): queue is empty");
            usleep(10000); // 10 miliseconds
            continue;
        }
        debug(DBG_TRACE,"queue_get(): received '%s'",msg);
        snprintf(request,1024,"%s %s",SC_WEBSRV,msg);
        free(msg); // msg comes strdup()'d from queue
        res=send(slot->sock,request,strlen(request),0);
        if (res<0){
            debug(DBG_ERROR,"%s send(): error sending request: %s",SC_WEBSRV,strerror(errno));
            continue;
        }
        res=recv(slot->sock,response,1024,0);
        if (res<0) {
            debug(DBG_NOTICE,"%s recv(): error waiting response: %s",SC_WEBSRV,strerror(errno));
            res=0; // not really fatal error. reset and continue
        } else {
            response[res]='\0';
            debug(DBG_DEBUG,"%s command response: %s\n",SC_WEBSRV,response);
        }
        // remove messages sent 15 seconds ago
        queue_expire(output_queue);
        if (slot->index<0) {
            debug(DBG_TRACE,"%s thread: 'exit' command invoked",SC_WEBSRV);
            res=-1;
        }
    }
    end_webServer();
    queue_destroy(input_queue);
    queue_destroy(output_queue);
    debug(DBG_TRACE,"Exiting html server thread");
    slot->index=-1;
    return &slot->index;
}