#ifndef __MAPPERGAP_H__
#define __MAPPERGAP_H__

#include "MapperRange.h"

typedef struct MapperGapStruct MapperGap;

struct MapperGapStruct {
  MAPPERRANGE_DATA
  int rank;
};

MapperGap *MapperGap_new(long start, long end, int rank);
#endif
