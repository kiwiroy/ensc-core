#ifndef __SEQUENCE_H__
#define __SEQUENCE_H__

#include "DataModelTypes.h"
#include "EnsRoot.h"

#define SEQUENCEFUNC_TYPES(CLASSTYPE) \
  typedef char * (*CLASSTYPE ## _GetNameFunc)(CLASSTYPE *seq); \
  typedef char * (*CLASSTYPE ## _GetSeqFunc)(CLASSTYPE *seq); \
  typedef char * (*CLASSTYPE ## _GetSubSeqFunc)(CLASSTYPE *seq, int start, int end, int strand);


#define SEQUENCEFUNCS_DATA(CLASSTYPE) \
  CLASSTYPE ## _GetNameFunc getName; \
  CLASSTYPE ## _GetSeqFunc getSeq; \
  CLASSTYPE ## _GetSubSeqFunc getSubSeq;

SEQUENCEFUNC_TYPES(Sequence)

typedef struct SequenceFuncsStruct {
  SEQUENCEFUNCS_DATA(Sequence)
} SequenceFuncs;

#define SEQUENCE_DATA \
  ENSROOT_DATA \
  char *seq; \
  char *name; \
  int length;

#define FUNCSTRUCTTYPE SequenceFuncs
struct SequenceStruct {
  SEQUENCE_DATA
};
#undef FUNCSTRUCTTYPE


#ifdef __SEQUENCE_MAIN__
 SequenceFuncs sequenceFuncs;
#else
 extern SequenceFuncs sequenceFuncs;
#endif


#endif