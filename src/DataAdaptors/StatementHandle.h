#ifndef __STATEMENTHANDLE_H__
#define __STATEMENTHANDLE_H__

#include "Object.h"
#include "ResultRow.h"
#include "DBConnection.h"

typedef void (*StatementHandle_ExecuteFunc)(StatementHandle *sth, ...);
typedef ResultRow *(*StatementHandle_FetchRowFunc)(StatementHandle *sth);
typedef void (*StatementHandle_FinishFunc)(StatementHandle *sth);
typedef IDType (*StatementHandle_GetInsertIdFunc)(StatementHandle *sth);


OBJECTFUNC_TYPES(StatementHandle)

typedef struct StatementHandleFuncsStruct {
  OBJECTFUNCS_DATA(StatementHandle)
} StatementHandleFuncs;


#define STATEMENTHANDLE_DATA \
  OBJECT_DATA \
  char *statementFormat; \
  char *currentStatement; \
  DBConnection *dbc; \
  StatementHandle_ExecuteFunc execute; \
  StatementHandle_FetchRowFunc fetchRow; \
  StatementHandle_FinishFunc finish; \
  StatementHandle_GetInsertIdFunc getInsertId;
  
#define FUNCSTRUCTTYPE StatementHandleFuncs
struct StatementHandleStruct {
  STATEMENTHANDLE_DATA
};
#undef FUNCSTRUCTTYPE

#endif
