#ifndef __RAWCONTIGADAPTOR_H__
#define __RAWCONTIGADAPTOR_H__

#include "BaseAdaptor.h"
#include "AdaptorTypes.h"
#include "RawContig.h"
#include "IDHash.h"

struct RawContigAdaptorStruct {
  BASEADAPTOR_DATA
  IDHash *rawContigCache;
};

RawContigAdaptor *RawContigAdaptor_new(DBAdaptor *dba);
RawContig *RawContigAdaptor_fetchByDbID(RawContigAdaptor *rca, long dbID);
RawContig *RawContigAdaptor_rawContigFromRow(RawContigAdaptor *rca, MYSQL_ROW row);
void RawContigAdaptor_fillRawContigWithRow(RawContigAdaptor *rca, RawContig *rc, MYSQL_ROW row);
void RawContig_fetchAttributes(RawContigAdaptor *rca, RawContig *rc);




#endif
