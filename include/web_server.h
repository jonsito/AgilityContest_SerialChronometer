//
// Created by jantonio on 5/05/19.
//

#ifndef AGILITYCONTEST_SERIALCHRONOMETER_WEB_SERVER_H
#define AGILITYCONTEST_SERIALCHRONOMETER_WEB_SERVER_H

#include "../include/sc_config.h"

#ifdef  AGILITYCONTEST_SERIALCHRONOMETER_WEB_SERVER_C
#define EXTERN extern
#else
#define EXTERN
#endif

EXTERN void * init_webServer(void *config); // started as a thread.
EXTERN int end_webServer();
#undef EXTERN

#endif //AGILITYCONTEST_SERIALCHRONOMETER_WEB_SERVER_H
