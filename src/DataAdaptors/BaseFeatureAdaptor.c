#include "BaseFeatureAdaptor.h"

#include "AnalysisAdaptor.h"
#include "AssemblyMapperAdaptor.h"
#include "StrUtil.h"

int SLICE_FEATURE_CACHE_SIZE = 4;


#define NAME 0
#define SYN  1

void BaseFeatureAdaptor_init(BaseFeatureAdaptor *bfa, DBAdaptor *dba, int adaptorType) {
  BaseAdaptor_init((BaseAdaptor *)bfa,dba,adaptorType);

  bfa->sliceFeatureCache = Cache_new(SLICE_FEATURE_CACHE_SIZE);

  bfa->objectsFromStatementHandle = BaseFeatureAdaptor_objectsFromStatementHandle;
  bfa->getTables                  = BaseFeatureAdaptor_getTables;
  bfa->getColumns                 = BaseFeatureAdaptor_getColumns;
  bfa->finalClause                = BaseFeatureAdaptor_finalClause;
  bfa->leftJoin                   = BaseFeatureAdaptor_leftJoin;
  bfa->defaultWhereClause         = BaseFeatureAdaptor_defaultWhereClause;
  bfa->store                      = BaseFeatureAdaptor_store;

  return;
}

Set *BaseFeatureAdaptor_genericFetch(BaseFeatureAdaptor *bfa, char *constraint,
                                     char *logicName, AssemblyMapper *mapper, Slice *slice) {
  char qStr[2048]; 
  char ***tables = bfa->getTables();
  char *columns = bfa->getColumns();
  StatementHandle *sth;
  Set *features;
  char allConstraints[512];
  char tableNamesStr[512]; 
  char leftJoinStr[512]; 
  char **lj;
  char tmpStr[256];
  int i;
/* HACK HACK HACK */
  int nTable = 1;
  
  allConstraints[0] = '\0';
  if (constraint) strcpy(allConstraints,constraint);
  
  if (logicName) {
    AnalysisAdaptor *aa = DBAdaptor_getAnalysisAdaptor(bfa->dba);
    Analysis *analysis;
    char *syn;
    int64 analysisId;

    //determine the analysis id via the logic_name
    analysis = AnalysisAdaptor_fetchByLogicName(aa, logicName);

    if (!analysis || !Analysis_getDbID(analysis) ) {
      fprintf(stderr,"No analysis for logic name %s exists\n",logicName);
      return emptySet;
    }
    
    analysisId = Analysis_getDbID(analysis);

    // get the synonym for the primary table
    syn = tables[0][SYN];

    if(constraint) {
      sprintf(allConstraints,"%s  AND %s.analysis_id = " INT64FMTSTR, constraint, syn, analysisId);
    } else {
      sprintf(allConstraints," %s.analysis_id = " INT64FMTSTR, syn, analysisId);
    }
  } 

  //
  // Construct a left join statement if one was defined, and remove the
  // left-joined table from the table list
  // 

  leftJoinStr[0]   = '\0';
  tableNamesStr[0] = '\0';

  lj = bfa->leftJoin();

  for (i=0;i<nTable;i++) {
    char **t = tables[i];
    if (lj!=NULL && !strcmp(lj[0],t[0])) {
      sprintf(leftJoinStr,"LEFT JOIN %s %s %s",lj[0], t[SYN], lj[1]);
    } else {
      if (tableNamesStr[0]) {
        sprintf(tmpStr,", %s %s",t[NAME],t[SYN]);
      } else {
        sprintf(tmpStr,"%s %s",t[NAME],t[SYN]);
      }
      strcat(tableNamesStr,tmpStr);
    }
  }
      
  sprintf(qStr,"SELECT %s FROM %s %s", columns, tableNamesStr, leftJoinStr);

  //append a where clause if it was defined
  if (allConstraints[0]) { 
    sprintf(tmpStr," where %s", allConstraints);
    strcat(qStr,tmpStr);
    if (bfa->defaultWhereClause()) {
      sprintf(tmpStr," and %s", bfa->defaultWhereClause());
      strcat(qStr,tmpStr);
    }
  } else if (bfa->defaultWhereClause()) {
    sprintf(tmpStr," where %s", bfa->defaultWhereClause());
    strcat(qStr,tmpStr);
  }

  //append additional clauses which may have been defined
  if (bfa->finalClause()) strcat(qStr, bfa->finalClause());

  sth = bfa->prepare((BaseAdaptor *)bfa,qStr,strlen(qStr));
  sth->execute(sth);  

  features = bfa->objectsFromStatementHandle(bfa, sth, mapper, slice);
  sth->finish(sth);

  return features;
}

SeqFeature *BaseFeatureAdaptor_fetchByDbID(BaseFeatureAdaptor *bfa, int64 dbID) {
  Set *features;
  SeqFeature *sf;
  char constraintStr[256];
  char ***tables = bfa->getTables();

  //construct a constraint like 't1.table1_id = 1'
  sprintf(constraintStr,"%s.%s_id = " INT64FMTSTR, tables[0][SYN], tables[0][NAME], dbID);

  //return first element of _generic_fetch list
  features = BaseFeatureAdaptor_genericFetch(bfa, constraintStr, NULL, NULL, NULL);
  sf = Set_getElementAt(features, 0);
// NIY free func
  Set_free(features, NULL);

  return (SeqFeature *)sf;
}

Set *BaseFeatureAdaptor_fetchAllByRawContigConstraint(BaseFeatureAdaptor *bfa, RawContig *contig,
                                                      char *constraint, char *logicName)  {
  int64 cid;
  char allConstraints[256];
  char ***tables = bfa->getTables();

  if (contig == NULL) {
    fprintf(stderr,"ERROR: fetch_by_Contig_constraint must have an contig\n");
    exit(1);
  }

  cid = RawContig_getDbID(contig);

  if (constraint) {
    sprintf(allConstraints,"%s AND %s.contig_id = " INT64FMTSTR, constraint, tables[0][SYN], cid);
  } else {
    sprintf(allConstraints,"%s.contig_id = " INT64FMTSTR, tables[0][SYN], cid);
  }

  return BaseFeatureAdaptor_genericFetch(bfa, allConstraints, logicName, NULL, NULL);
}

Set *BaseFeatureAdaptor_fetchAllByRawContig(BaseFeatureAdaptor *bfa, RawContig *contig,
                                            char *logicName) {
  return BaseFeatureAdaptor_fetchAllByRawContigConstraint(bfa,contig,NULL,logicName);
}

Set *BaseFeatureAdaptor_fetchAllByRawContigAndScore(BaseFeatureAdaptor *bfa, RawContig *contig,
                                                    double score, char *logicName) {
  char constraintStr[256];
  char ***tables = bfa->getTables();

// Perl does a defined check on score
  sprintf(constraintStr,"%s.score > %f",tables[0][SYN], score);
    
  return BaseFeatureAdaptor_fetchAllByRawContigConstraint(bfa, contig, constraintStr, logicName);
}

Set *BaseFeatureAdaptor_fetchAllBySlice(BaseFeatureAdaptor *bfa, Slice *slice,
                                        char *logicName) {
  return BaseFeatureAdaptor_fetchAllBySliceConstraint(bfa, slice, NULL, logicName);
}

Set *BaseFeatureAdaptor_fetchAllBySliceAndScore(BaseFeatureAdaptor *bfa, Slice *slice,
                                                double score, char *logicName) {
  char constraintStr[256];
  char ***tables = bfa->getTables();

// Perl does a defined check on score
  sprintf(constraintStr,"%s.score > %f",tables[0][SYN], score);

  return BaseFeatureAdaptor_fetchAllBySliceConstraint(bfa, slice, constraintStr, logicName);
}  

Set *BaseFeatureAdaptor_fetchAllBySliceConstraint(BaseFeatureAdaptor *bfa, Slice *slice,
                                                  char *constraint, char *logicName) {

  char cacheKey[256];
  void *val;
  Set *features;
  Set *out;
  char *allConstraints;
  int sliceChrId;
  int sliceEnd;
  int sliceStart;
  int sliceStrand;
  char ***tables = bfa->getTables();
  int nContigId;
  int64 *contigIds;
  char tmpStr[512];
  AssemblyMapper *assMapper;
  AssemblyMapperAdaptor *ama;
  int i;

  // check the cache and return if we have already done this query

  sprintf(cacheKey,"%s%s%s",Slice_getName(slice), constraint, logicName);
  StrUtil_strupr(cacheKey);

  if ((val = Cache_findElem(bfa->sliceFeatureCache, cacheKey)) != NULL) {
    return (Set *)val;
  }
    
  sliceChrId = Slice_getChrId(slice),
  sliceStart = Slice_getChrStart(slice),
  sliceEnd   = Slice_getChrEnd(slice),
  sliceStrand= Slice_getStrand(slice),

  ama = DBAdaptor_getAssemblyMapperAdaptor(bfa->dba);
  assMapper = AssemblyMapperAdaptor_fetchByType(ama,Slice_getAssemblyType(slice));

  nContigId = AssemblyMapper_listContigIds(assMapper,
                                           sliceChrId,
                                           sliceStart,
                                           sliceEnd,
                                           &contigIds);


  if (!nContigId) {
    return emptySet;
  }

  //construct the SQL constraint for the contig ids 
  if (constraint) {
    sprintf(tmpStr,"%s AND %s.contig_id IN (", constraint, tables[0][SYN]);
  } else {
    sprintf(tmpStr,"%s.contig_id IN (", tables[0][SYN]);
  }

  allConstraints = StrUtil_copyString(&allConstraints,tmpStr,0);

  if (!allConstraints) {
    Error_trace("fetch_all_by_Slice",NULL);
    return emptySet;
  }

  for (i=0; i<nContigId; i++) {
    char numStr[256];
    if (i!=(nContigId-1)) {
      sprintf(numStr,INT64FMTSTR ",",contigIds[i]);
    } else {
      sprintf(numStr,INT64FMTSTR,contigIds[i]);
    }
    allConstraints = StrUtil_appendString(allConstraints, numStr);
  }


  allConstraints = StrUtil_appendString(allConstraints,")");

  // for speed the remapping to slice may be done at the time of object creation
  features = 
    BaseFeatureAdaptor_genericFetch(bfa, allConstraints, logicName, assMapper, slice); 
  
  if (Set_getNumElement(features)) {
// Can't easily do this in C     && (!$features->[0]->can('contig') || 
    SeqFeature *sf = (SeqFeature *)Set_getElementAt(features,0);
    if (SeqFeature_getContig(sf) == (BaseContig *)slice) {
      // features have been converted to slice coords already, cache and return
      Cache_addElement(bfa->sliceFeatureCache, out);
      return features;
    }
  } 

  //remapping has not been done, we have to do our own conversion from
  //raw contig coords to slice coords
  out = Set_new();

    
  for (i=0;i<Set_getNumElement(features); i++) {
    //since feats were obtained in contig coords, attached seq is a contig
    SeqFeature *f = Set_getElementAt(features, i);
    int64 contigId = RawContig_getDbID(SeqFeature_getContig(f));
    MapperCoordinate fRange;
  
    int mapSucceeded = AssemblyMapper_fastToAssembly(assMapper, contigId, 
                                               SeqFeature_getStart(f), 
                                               SeqFeature_getEnd(f), 
                                               SeqFeature_getStrand(f), 
  				               &fRange);
  
    // undefined start means gap
    if (!mapSucceeded) continue;
  
    // maps to region outside desired area 
    if (fRange.start > sliceEnd || fRange.end < sliceStart) continue;
      
    // shift the feature start, end and strand in one call
    // In C I can't be arsed to write this call - it should be quick enough
    if(sliceStrand == -1) {
      SeqFeature_setStart (f, sliceEnd - fRange.end + 1);
      SeqFeature_setEnd   (f, sliceEnd - fRange.start + 1);
      SeqFeature_setStrand(f, fRange.strand * -1 );
    } else {
      SeqFeature_setStart (f, fRange.start - sliceStart + 1);
      SeqFeature_setEnd   (f, fRange.end - sliceStart + 1);
      SeqFeature_setStrand(f, fRange.strand);
    }
      
    SeqFeature_setContig(f,slice);
      
    Set_addElement(out,f);
  }
    
  //update the cache
  Cache_addElement(bfa->sliceFeatureCache, out);
  return out;
}

int BaseFeatureAdaptor_store(BaseFeatureAdaptor *bfa, Set *features) {
  fprintf(stderr,"ERROR: Abstract method store not defined by implementing subclass\n");
  exit(1);
}

int BaseFeatureAdaptor_remove(BaseFeatureAdaptor *bfa, SeqFeature *feature) {
  char qStr[256];
  char *tableName = (bfa->getTables())[0][0];
  StatementHandle *sth;
  
  if (!SeqFeature_getDbID(feature)) {
    fprintf(stderr, "BaseFeatureAdaptor_remove - dbID not defined - "
                    "feature could not be removed\n");
  }


  sprintf(qStr,"DELETE FROM %s WHERE %s_id = " INT64FMTSTR,tableName,tableName,SeqFeature_getDbID(feature));

  sth = bfa->prepare((BaseAdaptor *)bfa, qStr,strlen(qStr));
  sth->execute(sth);
  sth->finish(sth);

  //unset the feature dbID
  SeqFeature_setDbID(feature, 0);
  
  return;
}

int BaseFeatureAdaptor_removeByRawContig(BaseFeatureAdaptor *bfa, RawContig *contig) {
  char qStr[256];
  char *tableName = (bfa->getTables())[0][0];
  StatementHandle *sth;

  if (contig == NULL) {
    fprintf(stderr,"BaseFeatureAdaptor_removeByRawContig - no contig supplied: "
		   "Deletion of features failed.\n");
    return 0;
  }


  sprintf(qStr, "DELETE FROM %s WHERE contig_id = " INT64FMTSTR, tableName, RawContig_getDbID(contig));

  sth = bfa->prepare((BaseAdaptor *)bfa,qStr,strlen(qStr));
  sth->execute(sth);
  sth->finish(sth);

  return 1;
}

char ***BaseFeatureAdaptor_getTables(void) {
  fprintf(stderr,"ERROR: Abstract method getTables not defined by implementing subclass\n");
  exit(1);
}

char *BaseFeatureAdaptor_getColumns(void) {
  fprintf(stderr,"ERROR: Abstract method getColumns not defined by implementing subclass\n");
  exit(1);
}

/* Actually added to default where */
char *BaseFeatureAdaptor_defaultWhereClause(void) {
  return NULL;
}

/* Overridden in Markers and Qtls (not implemented in C) */
char **BaseFeatureAdaptor_leftJoin(void) {
  return NULL;
}

/* Overridden in PredictionTranscriptAdaptor */
char *BaseFeatureAdaptor_finalClause(void) {
  return NULL;
}

Set *BaseFeatureAdaptor_objectsFromStatementHandle(BaseFeatureAdaptor *bfa, StatementHandle *sth, 
                                                   AssemblyMapper *mapper, Slice *slice) {
  fprintf(stderr,"ERROR: Abstract method objectsFromStatementHandle not defined by implementing subclass\n");
  exit(1);
} 