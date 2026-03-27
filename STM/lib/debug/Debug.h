#ifndef DEBUG_H
#define DEBUG_H
#include <stdio.h>
#include "errno.h"


void DBG_printError(const char * FileName, unsigned FileLine, const char * MSG, const char* VarName);
void DBG_printError(const char * FileName, unsigned FileLine, const char * MSG);
void DBG_printExMsg(const char * FileName, unsigned FileLine, const char * MSG);
void DBG_printExMsg(const char * FileName, unsigned FileLine, const char * MSG, unsigned NUM);

void DBG_CheckPtr(const void * Ptr, const char * VarName, const char * FileName, unsigned FileLine);

extern void DBG_printMsg(const char* MSG);
extern void DBG_printNUM(unsigned NUM);
extern void DBG_printlnMsg(const char* MSG);

#define DEBUG_MACROS

#ifdef DEBUG_MACROS

#define debugMSG(MSG) DBG_printExMsg(__FILE__, __LINE__, MSG)
#define debugNUM(MSG, NUM) DBG_printExMsg(__FILE__, __LINE__, MSG, NUM)
#define errorMSG(MSG) DBG_printError(__FILE__, __LINE__, MSG)
#define checkPTR(PTR) DBG_CheckPtr(PTR, #PTR, __FILE__, __LINE__)

#else

#define debugMSG(MSG)
#define debugNUM(MSG, NUM)
#define errorMSG(MSG)
#define checkPTR(PTR)

#endif

#endif