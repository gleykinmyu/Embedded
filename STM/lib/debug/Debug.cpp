#include "Debug.h"

inline void DBG_printFileNameLine(const char * FileName, unsigned FileLine) {
  DBG_printMsg("File: ");
  DBG_printMsg(FileName);
  DBG_printMsg(" Line: ");
  DBG_printNUM(FileLine);
}

void DBG_printExMsg(const char * FileName, unsigned FileLine, const char * MSG) {
  DBG_printFileNameLine(FileName, FileLine);
  DBG_printlnMsg(MSG);
}

void DBG_printExMsg(const char * FileName, unsigned FileLine, const char * MSG, unsigned NUM) {
  DBG_printFileNameLine(FileName, FileLine);
  DBG_printMsg(MSG);
  DBG_printNUM(NUM);
}


void DBG_printError(const char * FileName, unsigned FileLine, const char * MSG) {
  DBG_printFileNameLine(FileName, FileLine);
  DBG_printlnMsg(MSG);
  //  while (1) {};
}

void DBG_printError(const char * FileName, unsigned FileLine, const char * MSG, const char* VarName) {
  DBG_printFileNameLine(FileName, FileLine);
  DBG_printMsg(MSG);
  DBG_printlnMsg(VarName);
  //  while (1) {};
}

void DBG_CheckPtr(const void * Ptr, const char * VarName, const char * FileName, unsigned FileLine) {
  if (Ptr == nullptr) DBG_printError(FileName, FileLine, "NullPtr in var:", VarName);
}

void DBG_printMsg(const char* MSG) {
  printf("%s", MSG);
}

void DBG_printNUM(unsigned NUM) {
  printf("%u", NUM);
}

void DBG_printlnMsg(const char* MSG) {
  printf("%s\n", MSG);
}