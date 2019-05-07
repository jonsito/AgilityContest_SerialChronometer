//
// Created by jantonio on 7/05/19.
//
#define SERIALCHRONOMETER_PARSER_C

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "sc_config.h"
#include "parser.h"

static int help( configuration *config, int argc, char *argv[]);
static int sc_exit( configuration *config, int argc, char *argv[]);

static command_t command_list[]= {
        { "start",  "Start of course run",      NULL,NULL,NULL,NULL },
        { "int",    "Intermediate time mark",   NULL,NULL,NULL,NULL },
        { "stop",   "End of course run",        NULL,NULL,NULL,NULL },
        { "fail",   "Sensor faillure detected", NULL,NULL,NULL,NULL },
        { "ok",     "Sensor recovery. Chrono ready",    NULL,NULL,NULL,NULL },
        { "msg",    "Show message on chrono display",   NULL,NULL,NULL,NULL },
        { "down",   "Start 15 seconds countdown",       NULL,NULL,NULL,NULL },
        { "fault",  "Mark fault (+/-/#)",               NULL,NULL,NULL,NULL },
        { "refusal","Mark refusal (+/-/#)",             NULL,NULL,NULL,NULL },
        { "elim",   "Mark elimination [+-]",            NULL,NULL,NULL,NULL },
        { "reset",  "Reset chronometer and countdown",  NULL,NULL,NULL,NULL },
        { "help",   "show command list",                NULL,NULL,NULL,help },
        { "exit",   "End program (from console)",       NULL,NULL,NULL,sc_exit },
        { "server", "Set server IP address",            NULL,NULL,NULL,NULL },
        { NULL,     "",                                 NULL,NULL,NULL,NULL }
};

static char **tokenize(char *line, int *argc) {
    char *buff = calloc(1024, sizeof(char));
    *argc = 0;
    char **argv = malloc(sizeof(char *));
    *argv = NULL;
    char *to = buff;
    for (char *pt = line; *pt; pt++) {
        if (!isspace(*pt)) {
            // si no es espacio en blanco lo metemos en el token actual
            *to++ = tolower(*pt);
            continue;
        }
        // si ya hay algun caracter en el buffer, significa que tenemos token
        if (to != &buff[0]) {
            char **tmp = realloc(argv, (1 + *argc) * sizeof(char *));
            // si realloc funciona rellenamos entrada con datos nuevos
            if (tmp) {
                argv[*argc] = strdup(buff);
                *argc = 1+ *argc;
                argv[*argc]=NULL;
                memset(buff, 0, 1024);
                to = buff;
            }
        }
    }
    free(buff);
    return argv; // *argc contiene el numero de tokens
}

// libera el array de tokens generado por el tokenizer
static int freetokens(int argc,char *argv[]) {
    for ( int n=0;n<argc; n++) {
        if (argv[n]) free(argv[n]);
    }
    free(argv);
    return 0;
}

static int help( configuration *config, int argc, char *argv[]) {
    if (argc!=1) {
        char *cmd=argv[1]; // get command to obtaing help fromquit
        for ( int n=0; command_list[n].cmd;n++) {
            if (strcmp(command_list[n].cmd,cmd)==0) {
                fprintf(stderr,"\t%s: %s\n",command_list[n].cmd, command_list[n].desc);
                return 0;
            }
        }
        fprintf(stderr,"Command %s not found",cmd);
        return 1;
    } else {
        fprintf(stderr,"List of available commands:");
        for ( int n=0; command_list[n].cmd;n++) {
            fprintf(stderr,"\t%s: %s\n",command_list[n].cmd, command_list[n].desc);
        }
    }
    return 0;
}

static int sc_exit(configuration *config, int argc, char *argv[]) {
    return -1;
}

int parse_cmd(configuration *config, char *tname, char *line) {
    int argc=0;
    // tokenize received data
    char **argv=tokenize(line,&argc);
    // if no tokens, just return
    if ( argc==0) return 0;
    // else look at token table to find match
    for ( int n=0; command_list[n].cmd;n++) {
        int res = 0;
        if (strncmp(command_list[n].cmd, argv[0], strlen(command_list[n].cmd)) != 0) continue;
        if (strcmp(tname, "console") == 0) {
            if (command_list[n].console_func) res = (*command_list[n].console_func)(config, argc, argv);
        }
        else if (strcmp(tname, "ajax") == 0) {
            if (command_list[n].ajax_func) res = (*command_list[n].ajax_func)(config, argc, argv);
        }
        else if (strcmp(tname, "web") == 0) {
            if (command_list[n].web_func) res = (*command_list[n].web_func)(config, argc, argv);
        }
        else if (strcmp(tname, "serial") == 0) {
            if (command_list[n].serial_func) res = (*command_list[n].serial_func)(config, argc, argv);
        }
        freetokens(argc,argv);
        return res;
    }
    debug(DBG_ERROR,"Command %s not found. Type 'help' to see available commands",argv[0]);
    freetokens(argc,argv);
    return -1;
}