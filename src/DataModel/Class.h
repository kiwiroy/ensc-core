#ifndef __CLASS_H__
#define __CLASS_H__

typedef struct ClassStruct Class;
typedef struct ClassHierarchyNodeStruct ClassHierarchyNode;
typedef enum ClassTypeEnum {
  CLASS_NONE,
  CLASS_OBJECT,
  CLASS_STATEMENTHANDLE,
  CLASS_MYSQLSTATEMENTHANDLE,
  CLASS_RESULTROW,
  CLASS_MYSQLRESULTROW,
  CLASS_SEQFEATURE,
  CLASS_EXON,
  CLASS_STICKYEXON,
  CLASS_TRANSCRIPT,
  CLASS_GENE,
  CLASS_FEATURESET,
  CLASS_SIMPLEFEATURE,
  CLASS_REPEATFEATURE,
  CLASS_BASEALIGNFEATURE,
  CLASS_FEATUREPAIR,
  CLASS_DNADNAALIGNFEATURE,
  CLASS_DNAPEPALIGNFEATURE,
  CLASS_REPEATCONSENSUS,
  CLASS_ENSROOT,
  CLASS_DBENTRY,
  CLASS_ANALYSIS,
  CLASS_SLICE,
  CLASS_RAWCONTIG,
  CLASS_BASECONTIG,
  CLASS_SEQUENCE,
  CLASS_CHROMOSOME,
  CLASS_CLONE,
  CLASS_TRANSLATION,
  CLASS_PREDICTIONTRANSCRIPT,
  CLASS_ANNOTATEDSEQFEATURE,
  CLASS_NUMCLASS
} ClassType;

struct ClassHierarchyNodeStruct {
  int nSubClass;
  Class *class;
  ClassHierarchyNode **subClasses;
};

struct ClassStruct {
  ClassType type;
  char *name;
};


int Class_isDescendent(ClassType parentType, ClassType descType);
int Class_assertType(ClassType wantedType, ClassType actualType);


#endif
