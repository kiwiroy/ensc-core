#ifndef __SLICE_H__
#define __SLICE_H__

#include "DataModelTypes.h"
#include "Storable.h"
#include "BaseContig.h"
#include "Gene.h"
#include "Vector.h"
#include "EcoString.h"


#define FUNCSTRUCTTYPE BaseContigFuncs
struct SliceStruct {
  BASECONTIG_DATA
  int strand;
  char emptyFlag;
  char *chrName;
  char *assemblyType;
  IDType chrId;
};
#undef FUNCSTRUCTTYPE

#ifdef __SLICE_C__
 Slice emptySliceData = {CLASS_SLICE,0,NULL,{-1,NULL},-1,-1,0,1,NULL,NULL,-1};
 Slice *emptySlice = &emptySliceData;
#else
 extern Slice *emptySlice;
#endif

Slice *Slice_new(char *chr, int start, int end, int strand, char *assemblyType,
                 SliceAdaptor *sa, IDType dbID, int empty);

#define Slice_setDbID(s,dbID) BaseContig_setDbID((s),(dbID))
#define Slice_getDbID(s) BaseContig_getDbID((s))

#define Slice_setAdaptor(s,ad) BaseContig_setAdaptor((s),(ad))
#define Slice_getAdaptor(s) BaseContig_getAdaptor((s))

#define Slice_setEmptyFlag(s,e) (s)->emptyFlag = (e)
#define Slice_getEmptyFlag(s) (s)->emptyFlag

#define Slice_setChrStart(sl,s) (sl)->start = (s)
#define Slice_getChrStart(sl) (sl)->start

#define Slice_setChrEnd(sl,e) (sl)->end = (e)
#define Slice_getChrEnd(sl) (sl)->end

#define Slice_setChrId(sl,c) (sl)->chrId = (c)
#define Slice_getChrId(sl) (sl)->chrId

#define Slice_setStrand(sl,s) (sl)->strand = (s)
#define Slice_getStrand(sl) (sl)->strand

#define Slice_getLength(sl) ((sl)->end - (sl)->start + 1)

char *Slice_setAssemblyType(Slice *sl,char *type);
#define Slice_getAssemblyType(sl) (sl)->assemblyType

char *Slice_setChrName(Slice *sl,char *chrName);
#define Slice_getChrName(sl) (sl)->chrName

ECOSTRING Slice_getName(Slice *sl);
Vector *Slice_getAllGenes(Slice *slice, char *logicName);
Vector *Slice_getAllSimpleFeatures(Slice *slice, char *logicName, double *score);
Vector *Slice_getAllDNAAlignFeatures(Slice *slice, char *logicName, double *score);
Vector *Slice_getAllDNAPepAlignFeatures(Slice *slice, char *logicName, double *score);
Vector *Slice_getAllPredictionTranscripts(Slice *slice, char *logicName);
Vector *Slice_getAllRepeatFeatures(Slice *slice, char *logicName);



#endif
