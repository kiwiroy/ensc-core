#ifndef __DBADAPTOR_H__
#define __DBADAPTOR_H__

#include "DBConnection.h"
#include "AdaptorTypes.h"
#include "EnsC.h"

struct DBAdaptorStruct {
  DBConnection  *dbc;

  DBAdaptor     *dnadb;
  char          *assemblyType;
  MetaContainer *metaContainer;
};

DBAdaptor *DBAdaptor_new(char *host, char *user, char *pass, char *dbname,
                         unsigned int port, DBAdaptor *dnadb);

char *DBAdaptor_setAssemblyType(DBAdaptor *dba, char *type);
char *DBAdaptor_getAssemblyType(DBAdaptor *dba);

AnalysisAdaptor   *DBAdaptor_getAnalysisAdaptor(DBAdaptor *dba);
AssemblyMapperAdaptor *DBAdaptor_getAssemblyMapperAdaptor(DBAdaptor *dba);
ChromosomeAdaptor *DBAdaptor_getChromosomeAdaptor(DBAdaptor *dba);
ExonAdaptor       *DBAdaptor_getExonAdaptor(DBAdaptor *dba);
GeneAdaptor       *DBAdaptor_getGeneAdaptor(DBAdaptor *dba);
MetaContainer     *DBAdaptor_getMetaContainer(DBAdaptor *dba);
RawContigAdaptor  *DBAdaptor_getRawContigAdaptor(DBAdaptor *dba);
SliceAdaptor      *DBAdaptor_getSliceAdaptor(DBAdaptor *dba); 
SequenceAdaptor   *DBAdaptor_getSequenceAdaptor(DBAdaptor *dba); 
TranscriptAdaptor *DBAdaptor_getTranscriptAdaptor(DBAdaptor *dba);


#define DBAdaptor_getDNADBAdaptor(dba) (dba)->dnadb
#define DBAdaptor_setDNADBAdaptor(dba, ddb) (dba)->dnadb = ddb

#define DBAdaptor_prepare(dba,qStr,qLen) (dba)->dbc->prepare((dba)->dbc,(qStr),(qLen))


#endif
