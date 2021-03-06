//
// Created by jantonio on 20/06/19.
//
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "modules.h"
#include "sc_config.h"
#include "debug.h"


static char error_str[1024];

static configuration *config;
/* Declare our Add function using the above definitions. */
int ADDCALL module_init(configuration *cfg){
    config=cfg;
    enum sp_return ret=sp_get_port_by_name(config->comm_port,&config->serial_port);
    if (ret!= SP_OK) {
        snprintf(error_str,strlen(error_str),"Cannot locate serial port '%s'",config->comm_port);
        debug(DBG_ERROR,error_str);
        return -1;
    }
    return 0;
}

int ADDCALL module_end() {
    if (config->serial_port) {
        sp_free_port(config->serial_port);
    }
    config->serial_port=NULL;
    return 0;
}

int ADDCALL module_open(){
    if (config->serial_port) {
        enum sp_return ret = sp_open(config->serial_port,SP_MODE_READ_WRITE);
        if (ret != SP_OK) {
            snprintf(error_str,strlen(error_str),"Cannot open serial port %s",config->comm_port);
            debug(DBG_ERROR,error_str);
            return -1;
        }
    }
    sp_set_baudrate(config->serial_port, config->baud_rate);
    sp_set_bits(config->serial_port, 8);
    sp_set_flowcontrol(config->serial_port, SP_FLOWCONTROL_NONE);
    sp_set_parity(config->serial_port, SP_PARITY_NONE);
    sp_set_stopbits(config->serial_port, 1);
    sp_set_rts(config->serial_port,SP_RTS_ON);
    return 0;
}

int ADDCALL module_close(){
    if (config->serial_port) {
        sp_close(config->serial_port);
    }
    return 0;
}

int ADDCALL module_read(char *buffer,size_t length){
    enum sp_return ret;
    static char *inbuff=NULL;
    if (inbuff==NULL) inbuff=malloc(1024*sizeof(char));
    memset(inbuff,0,1024*sizeof(char));
    char *pt=inbuff;
    do {
        ret = sp_blocking_read(config->serial_port,pt,1,0); // read single char. wait forever
        if (*pt=='\r') continue; // ignore carriage return ( stupid windows )
        if (*pt=='\n')  ret=0;
        if (pt==&inbuff[1022]) { *pt='\n'; ret=0; } // avoid buffer overflow
        pt++;
    } while( ret>0);
    if (ret <0 ) {
        debug(DBG_ERROR,"module_read() error: '%s'",sp_last_error_message());
        snprintf(inbuff,1024,"");
        return ret;
    }
    debug(DBG_TRACE,"module_read() received '%s'",inbuff);
    snprintf(buffer,length,"%s",inbuff);
    return strlen(inbuff);
}

int ADDCALL module_write(char **tokens,size_t ntokens){
    static char *buffer=NULL;
    int len=0;
    if (buffer==NULL) buffer=calloc(1024,sizeof(char));
    // compose message by adding tokens
    // int len=sprintf(buffer,"%s",tokens[1]);
    for (len=0;len<strlen(tokens[1]);len++) buffer[len]=toupper(tokens[1][len]); // uppercase command (as manual says)
    for (int n=2;n<ntokens;n++) len += sprintf(buffer+len," %s",tokens[n]);
    len += sprintf(buffer+len,"\r\n");
    // notice "blocking" mode. needed as most modules do not support full duplex communications
    debug(DBG_TRACE,"module_write(), send %d bytes: '%s'",len,buffer);
    enum sp_return ret=sp_blocking_write(config->serial_port,buffer,strlen(buffer),0);
    return ret;
}

char * ADDCALL module_error() {
    return error_str;
}