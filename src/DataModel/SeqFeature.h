#ifndef __SEQFEATURE_H__
#define __SEQFEATURE_H__


#include "DataModelTypes.h"
#include "EnsC.h"
#include "Storable.h"
#include "Analysis.h"
#include "BaseContig.h"

#include "EnsRoot.h"

typedef int (*SeqFeature_GetStartFunc)(SeqFeature *);
typedef int (*SeqFeature_SetStartFunc)(SeqFeature *, int start);
typedef int (*SeqFeature_GetEndFunc)(SeqFeature *);
typedef int (*SeqFeature_SetEndFunc)(SeqFeature *, int end);

#define SEQFEATUREFUNCS_DATA \
  SeqFeature_GetStartFunc getStart; \
  SeqFeature_SetStartFunc setStart; \
  SeqFeature_GetEndFunc getEnd; \
  SeqFeature_SetEndFunc setEnd;

typedef struct SeqFeatureFuncsStruct {
  SEQFEATUREFUNCS_DATA
} SeqFeatureFuncs;

#define SEQFEATURE_DATA \
  ENSROOT_DATA \
  int         start; \
  int         end; \
  signed char phase; \
  signed char endPhase; \
  signed char frame; \
  signed char strand; \
  char *      seqName; \
  Storable    st; \
  Analysis *  analysis; \
  double      score; \
  double      eValue; \
  double      percentId; \
  BaseContig *contig;

#define FUNCSTRUCTTYPE SeqFeatureFuncs
struct SeqFeatureStruct {
  SEQFEATURE_DATA
};
#undef FUNCSTRUCTTYPE

#define SeqFeature_setStart(sf,s) ((sf)->funcs->setStart == NULL ? ((sf)->start = (s)) : \
                                                                   ((sf)->funcs->setStart((SeqFeature *)(sf),(s))))
#define SeqFeature_getStart(sf) ((sf)->funcs->getStart == NULL ? ((sf)->start) : \
                                                                 ((sf)->funcs->getStart((SeqFeature *)(sf))))

#define SeqFeature_setEnd(sf,e) (sf)->end = (e)
#define SeqFeature_getEnd(sf) (sf)->end

#define SeqFeature_setScore(sf,s) (sf)->score = (s)
#define SeqFeature_getScore(sf) (sf)->score

#define SeqFeature_setEValue(sf,s) (sf)->eValue = (s)
#define SeqFeature_getEValue(sf) (sf)->eValue

#define SeqFeature_setPercId(sf,s) (sf)->percentId = (s)
#define SeqFeature_getPercId(sf) (sf)->percentId

#define SeqFeature_setPhase(sf,p) (sf)->phase = (p)
#define SeqFeature_getPhase(sf) (sf)->phase

#define SeqFeature_setEndPhase(sf,ep) (sf)->endPhase = (ep)
#define SeqFeature_getEndPhase(sf) (sf)->endPhase

#define SeqFeature_setStrand(sf,strnd) (sf)->strand = strnd
#define SeqFeature_getStrand(sf) (sf)->strand

#define SeqFeature_setAnalysis(sf,ana) (sf)->analysis = ana
#define SeqFeature_getAnalysis(sf) (sf)->analysis

char *SeqFeature_setStableId(SeqFeature *sf, char *stableId);
#define SeqFeature_getStableId(sf) (sf)->stableId

#define SeqFeature_setDbID(sf,dbID) Storable_setDbID(&((sf)->st),dbID)
#define SeqFeature_getDbID(sf) Storable_getDbID(&((sf)->st))

#define SeqFeature_setAdaptor(sf,ad) Storable_setAdaptor(&((sf)->st),ad)
#define SeqFeature_getAdaptor(sf) Storable_getAdaptor(&((sf)->st))

#define SeqFeature_setContig(sf,c) (sf)->contig = (BaseContig *)(c)
#define SeqFeature_getContig(sf) (sf)->contig

#define SeqFeature_getLength(sf) ((sf)->end - (sf)->start + 1)

#ifdef __SEQFEATURE_MAIN__
 SeqFeatureFuncs seqFeatureFuncs = {NULL, NULL};
#else
 extern SeqFeatureFuncs seqFeatureFuncs;
#endif

#endif
