/*
=head1 DESCRIPTION

This is an adaptor for the retrieval and storage of RepeatFeature
objects from the database.  Most of the implementation is in the
superclass BaseFeatureAdaptor.
*/

#include "RepeatFeatureAdaptor.h"

#include "IDHash.h"
#include "RepeatConsensusAdaptor.h"
#include "AnalysisAdaptor.h"
#include "SliceAdaptor.h"
#include "ChainedAssemblyMapper.h"

#include "RepeatFeature.h"
#include "RepeatConsensus.h"

NameTableType RepeatFeatureAdaptor_tableNames = {{"repeat_feature","r"},
                                                 {"repeat_consensus","rc"},
                                                 {NULL,NULL}};

RepeatFeatureAdaptor *RepeatFeatureAdaptor_new(DBAdaptor *dba) {
  RepeatFeatureAdaptor *rfa;

  if ((rfa = (RepeatFeatureAdaptor *)calloc(1,sizeof(RepeatFeatureAdaptor))) == NULL) {
    fprintf(stderr, "ERROR: Failed allocating space for RepeatFeatureAdaptor\n");
    return NULL;
  }
  BaseFeatureAdaptor_init((BaseFeatureAdaptor *)rfa, dba, REPEATFEATURE_ADAPTOR);

  rfa->getTables = RepeatFeatureAdaptor_getTables;
  rfa->getColumns = RepeatFeatureAdaptor_getColumns;
  rfa->store = RepeatFeatureAdaptor_store;
  rfa->objectsFromStatementHandle = RepeatFeatureAdaptor_objectsFromStatementHandle;
  rfa->defaultWhereClause = RepeatFeatureAdaptor_defaultWhereClause;

  return rfa;
}


/*
=head2 fetch_all_by_Slice

  Arg [1]    : Bio::EnsEMBL::Slice $slice
  Arg [2]    : (optional) string $logic_name
               Limits RepeatFeatures obtained to those having an Analysis with
               of the specified logic_name.  If no logic name is specified
               Repeats of all analysis types are retrieved.
  Arg [3]    : (optional) string/array $repeat_type
               Limits RepeatFeatures obtained to those of specified 
               repeat_type
  Example    : @rfeats = @{$rfa->fetch_all_by_Slice($slice, undef, 'Type II Transposons')};
               @rfeats = @{$rfa->fetch_all_by_Slice($slice, undef, ['Type II Transposons', 'RNA repeats'])};
  Description: Retrieves repeat features overlapping the area designated by
               the provided slice argument.  Returned features will be in
               in the same coordinate system as the provided slice and will
               have coordinates relative to the slice start.
  Returntype : reference to a list of Bio::EnsEMBL::RepeatFeatures.
  Exceptions : throw on bad argument
  Caller     : Slice::get_all_RepeatFeatures
  Status     : Stable

=cut
*/
Vector *RepeatFeatureAdaptor_fetchAllBySlice(RepeatFeatureAdaptor *rfa, Slice *slice, char *logicName, Vector *repeatTypes) {
  char constraint[1024];
  constraint[0] = '\0';

  // MySQL was optimising the query the incorrect way when joining to
  // the repeat_consensus table on type

  // Hack - direct access
  rfa->straightJoinFlag = 1;

  if (repeatTypes != NULL && Vector_getNumElement(repeatTypes) > 0) {
    if (Vector_getNumElement(repeatTypes) > 1) {
      strcpy(constraint, "rc.repeat_type IN (");
      int i;
      for (i=0; i<Vector_getNumElement(repeatTypes); i++) {
        char *repeatType = Vector_getElementAt(repeatTypes, i);
        if (i!=0) {
          strcat(constraint, ", ");
        }
        sprintf(constraint, "'%s'", repeatType);
      }
      strcat(constraint,")");
    } else {
      char *repeatType = Vector_getElementAt(repeatTypes, 0);
      sprintf(constraint, "rc.repeat_type = '%s'", repeatType);
    }
  }

  Vector *result = RepeatFeatureAdaptor_fetchAllBySliceConstraint(rfa, slice, constraint, logicName);

  rfa->straightJoinFlag = 0;

  return result;
}


/*
#  _tablename
#
#   Arg [1]    : none
#   Example    : none
#   Description: PROTECTED Implementation of abstract superclass method to 
#                provide the name of the tables to query 
#   Returntype : string
#   Exceptions : none
#   Caller     : internal
*/
NameTableType *RepeatFeatureAdaptor_getTables() {
  return &RepeatFeatureAdaptor_tableNames;
}


/*
# _columns
#
#   Arg [1]    : none
#   Example    : none
#   Description: PROTECTED Implementation of abstract superclass method to 
#                provide the name of the columns to query 
#   Returntype : list of strings
#   Exceptions : none
#   Caller     : internal
*/
char *RepeatFeature_cols[] = {
         "r.repeat_feature_id",
         "r.seq_region_id",
         "r.seq_region_start",
         "r.seq_region_end",
         "r.seq_region_strand",
         "r.repeat_consensus_id",
         "r.repeat_start",
         "r.repeat_end",
         "r.analysis_id",
         "r.score",
         "rc.repeat_name",
         "rc.repeat_class",
         "rc.repeat_type",
         "rc.repeat_consensus",
         NULL };

char **RepeatFeatureAdaptor_getColumns() {
  return RepeatFeature_cols;
}


/*
# _default_where_clause
#  Arg [1]    : none
#  Example    : none
#  Description: Overrides superclass method to provide an additional 
#               table joining constraint before the SQL query is performed.
#  Returntype : string
#  Exceptions : none
#  Caller     : generic_fetch
#
*/
char *RepeatFeatureAdaptor_defaultWhereClause() {
  return "r.repeat_consensus_id = rc.repeat_consensus_id";
}


/*
#  Description: PROTECTED implementation of abstract superclass method.
#               responsible for the creation of RepeatFeatures from a
#               hashref generated from an SQL query
*/

Vector *RepeatFeatureAdaptor_objectsFromStatementHandle(BaseFeatureAdaptor *bfa,
                                                        StatementHandle *sth,
                                                        AssemblyMapper *assMapper,
                                                        Slice *destSlice) {

  RepeatConsensusAdaptor *rpca = DBAdaptor_getRepeatConsensusAdaptor(bfa->dba);
  SliceAdaptor *sa             = DBAdaptor_getSliceAdaptor(bfa->dba);
  AnalysisAdaptor *aa          = DBAdaptor_getAnalysisAdaptor(bfa->dba);

  Vector *features = Vector_new();
  IDHash *rcHash = IDHash_new(IDHASH_SMALL);
  IDHash *sliceHash = IDHash_new(IDHASH_SMALL);

  long         destSliceStart;
  long         destSliceEnd;
  int          destSliceStrand;
  long         destSliceLength;
  char *       destSliceSrName;
  IDType       destSliceSrId = 0;

  if (destSlice) {
    destSliceStart  = Slice_getStart(destSlice);
    destSliceEnd    = Slice_getEnd(destSlice);
    destSliceStrand = Slice_getStrand(destSlice);
    destSliceLength = Slice_getLength(destSlice);
    destSliceSrName = Slice_getSeqRegionName(destSlice);
    destSliceSrId   = Slice_getSeqRegionId(destSlice);
  }

  ResultRow *row;
  while (row = sth->fetchRow(sth)) {
    IDType repeatFeatureId   = row->getLongLongAt(row,0);
    IDType seqRegionId       = row->getLongLongAt(row,1);
    long seqRegionStart      = row->getLongAt(row,2);
    long seqRegionEnd        = row->getLongAt(row,3);
    int seqRegionStrand      = row->getIntAt(row,4);
    IDType repeatConsensusId = row->getLongLongAt(row,5);
    int repeatStart          = row->getIntAt(row,6);
    int repeatEnd            = row->getIntAt(row,7);
    IDType analysisId        = row->getLongLongAt(row,8);
    double score             = row->getDoubleAt(row,9);
    char *repeatName         = row->getStringAt(row,10);
    char *repeatClass        = row->getStringAt(row,11);
    char *repeatType         = row->getStringAt(row,12);
    char *repeatConsensus    = row->getStringAt(row,13);

    //create a repeat consensus object
    RepeatConsensus *rc;

    if (!IDHash_contains(rcHash, repeatConsensusId)) {
      rc = RepeatConsensus_new();
      RepeatConsensus_setDbID(rc, repeatConsensusId);
      RepeatConsensus_setName(rc, repeatName);
      RepeatConsensus_setRepeatClass(rc, repeatClass);
      RepeatConsensus_setRepeatType(rc, repeatType);
      RepeatConsensus_setConsensus(rc, repeatConsensus);
      RepeatConsensus_setLength(rc, strlen(repeatConsensus));
      RepeatConsensus_setAdaptor(rc,(BaseAdaptor *)rpca);

      IDHash_add(rcHash,repeatConsensusId,rc);
    } else {
      rc = IDHash_getValue(rcHash,repeatConsensusId);
    }

    // get the analysis object
    Analysis *analysis = AnalysisAdaptor_fetchByDbID(aa, analysisId);

    if (! IDHash_contains(sliceHash, seqRegionId)) {
      IDHash_add(sliceHash, seqRegionId, SliceAdaptor_fetchBySeqRegionId(sa, seqRegionId, POS_UNDEF, POS_UNDEF, STRAND_UNDEF));
    }
    Slice *slice = IDHash_getValue(sliceHash, seqRegionId);

    Slice *rfSlice = slice;

    char *srName      = Slice_getSeqRegionName(slice);
    CoordSystem *srCs = Slice_getCoordSystem(slice);

    //
    // remap the feature coordinates to another coord system
    // if a mapper was provided
    //
    if (assMapper != NULL) {
      MapperRangeSet *mrs;

      // Slightly suspicious about need for this if statement so left in perl statements for now
      if (destSlice != NULL &&
          assMapper->objectType == CLASS_CHAINEDASSEMBLYMAPPER) {
        MapperRangeSet *mrs = ChainedAssemblyMapper_map(assMapper, srName, seqRegionStart, seqRegionEnd, seqRegionStrand, srCs, 1, destSlice);
      } else {
        MapperRangeSet *mrs = AssemblyMapper_fastMap(assMapper, srName, seqRegionStart, seqRegionEnd, seqRegionStrand, srCs, NULL);
      }

      // skip features that map to gaps or coord system boundaries
      if (MapperRangeSet_getNumRange(mrs) == 0) {
        continue;
      }
      MapperRange *range = MapperRangeSet_getRangeAt(mrs, 0);
      if (range->rangeType == MAPPERRANGE_GAP) {
        fprintf(stderr,"Got a mapper gap in gene obj_from_sth - not sure if this is allowed\n");
        exit(1);
      } else {
        MapperCoordinate *mc = (MapperCoordinate *)range;

        seqRegionId     = mc->id;
        seqRegionStart  = mc->start;
        seqRegionEnd    = mc->end;
        seqRegionStrand = mc->strand;
      }

      MapperRangeSet_free(mrs);

      if (! IDHash_contains(sliceHash, seqRegionId)) {
        IDHash_add(sliceHash, seqRegionId, SliceAdaptor_fetchBySeqRegionId(sa, seqRegionId, POS_UNDEF, POS_UNDEF, STRAND_UNDEF));
      }
      rfSlice = IDHash_getValue(sliceHash, seqRegionId);
    }

    if (destSlice != NULL) {
      if (destSliceStart != 1 || destSliceStrand != 1) {
        if (destSliceStrand == 1) {
          seqRegionStart = seqRegionStart - destSliceStart + 1;
          seqRegionEnd   = seqRegionEnd - destSliceStart + 1;
        } else {
          long tmpSeqRegionStart = seqRegionStart;
          seqRegionStart = destSliceEnd - seqRegionEnd + 1;
          seqRegionEnd   = destSliceEnd - tmpSeqRegionStart + 1;

          seqRegionStrand = -seqRegionStrand;
        }
      }
      // throw away features off the end of the requested slice
      if (seqRegionEnd < 1 || seqRegionStart > destSliceLength || (destSliceSrId != seqRegionId)) {
        continue;
      }
      rfSlice = destSlice;
    }


    // Finally, create the new RepeatFeature.
    RepeatFeature *rf = RepeatFeature_new();
    RepeatFeature_setDbID(rf,repeatFeatureId);
    RepeatFeature_setAnalysis(rf,analysis);

    RepeatFeature_setStart(rf,seqRegionStart);
    RepeatFeature_setEnd(rf,seqRegionEnd);
    RepeatFeature_setStrand(rf,seqRegionStrand);

    RepeatFeature_setScore(rf,score);

    RepeatFeature_setHitStart(rf,repeatStart);
    RepeatFeature_setHitEnd(rf,repeatEnd);

    RepeatFeature_setConsensus(rf,rc);
    RepeatFeature_setAdaptor(rf, (BaseAdaptor *) bfa);
    RepeatFeature_setSlice(rf,rfSlice);

    Vector_addElement(features, rf);
  }

  IDHash_free(sliceHash, NULL);
  IDHash_free(rcHash, NULL);

  return features;
}


/*
=head2 store

  Arg [1]    : list of Bio::EnsEMBL::RepeatFeatures $repeat_feature_id
               the list of repeat features to store in the database
  Example    : $repeat_feature_adaptor->store(@repeat_features);
  Description: stores a repeat feature in the database
  Returntype : none
  Exceptions : if the repeat features do not have attached sequences 
               or if repeat_consensus are not present 
  Caller     : general
  Status     : Stable

=cut
*/

int RepeatFeatureAdaptor_store(BaseFeatureAdaptor *bfa, Vector *repeats) {
  fprintf(stderr,"RepeatFeatureAdaptor_store not implemented\n");
  exit(1);
/*

  my $db = $self->db();
  my $rca = $db->get_RepeatConsensusAdaptor();
  my $sa  = $db->get_SliceAdaptor();
  my ($cons, $db_id);

  my $sth = $self->prepare(qq{
    INSERT into repeat_feature( repeat_feature_id
				, seq_region_id
				, seq_region_start
				, seq_region_end
				, seq_region_strand
				, repeat_consensus_id
				, repeat_start
				, repeat_end
				, score
				, analysis_id )
      VALUES(NULL, ?,?,?,?,?,?,?,?,?)
    });

 FEATURE: foreach my $rf (@repeats) {
    if(!ref($rf) || !$rf->isa('Bio::EnsEMBL::RepeatFeature')) {
      throw('Expected RepeatFeature argument not [' . ref($rf) .'].');
    }

    if($rf->is_stored($db)) {
      warning("RepeatFeature [".$rf->dbID."] is already stored in this DB.");
      next FEATURE;
    }

    my $cons = $rf->repeat_consensus();
    throw("Must have a RepeatConsensus attached") if(!defined($cons));

    # for tandem repeats - simply store consensus and repeat
    # one pair per hit. don't need to check consensi stored
    # already. consensus has name and class set to 'trf'

    if ($cons->repeat_class eq 'trf') {

      # Look for matches already stored
      my @match = @{$rca->fetch_all_by_class_seq('trf', $cons->repeat_consensus)};
      if (@match) {
        $cons->dbID($match[0]->dbID());
      }
      else {
        $rca->store($cons);
      }

    } elsif ($cons->repeat_class eq 'Simple_repeat') {

      my $rcon = $cons->name;
      $rcon =~ s/\((\S+)\)n/$1/;   # get repeat element
      $cons->repeat_consensus($rcon);

      # Look for matches already stored
      my $match = $rca->fetch_by_name_class($cons->name, 'Simple_repeat');
      if ($match) {
        $cons->dbID($match->dbID());
      }
      else {
        $rca->store($cons);
      }

    } else {

      # for other repeats - need to see if a consensus is stored already
      if(!$cons->dbID) {
        my $match = ($rca->fetch_by_name($cons->name));
	
        if($match) {
          #set the consensus dbID to be the same as the database one
          $cons->dbID($match->dbID());
        } else {
          # if we don't match a consensus already stored create a fake one 
          # and set consensus to 'N' as null seq not allowed
          # FIXME: not happy with this, but ho hum ...
          warning("Can't find " . $cons->name . "\n");
          $cons->repeat_consensus("N");
          $rca->store($cons);
        }
      }
	
      #if (@match > 1) {
      #multiple consensi were matched
      #  $self->warn(@match . " consensi for " . $cons->name . "\n");
      #}
    }

    my $slice = $rf->slice();
    if(!ref($slice) || !($slice->isa("Bio::EnsEMBL::Slice") or $slice->isa('Bio::EnsEMBL::LRGSlice'))) {
      throw("RepeatFeature cannot be stored without an associated slice.");
    }

    my $original = $rf;
    my $seq_region_id;
    ($rf, $seq_region_id) = $self->_pre_store($rf);

    $sth->bind_param(1,$seq_region_id,SQL_INTEGER);
    $sth->bind_param(2,$rf->start,SQL_INTEGER);
    $sth->bind_param(3,$rf->end,SQL_INTEGER);
    $sth->bind_param(4,$rf->strand,SQL_TINYINT);
    $sth->bind_param(5,$rf->repeat_consensus->dbID,SQL_INTEGER);
    $sth->bind_param(6,$rf->hstart,SQL_INTEGER);
    $sth->bind_param(7,$rf->hend,SQL_INTEGER);
    $sth->bind_param(8,$rf->score,SQL_DOUBLE);
    $sth->bind_param(9,$rf->analysis->dbID,SQL_INTEGER);

    $sth->execute();

    my $db_id = $sth->{'mysql_insertid'}
      or throw("Didn't get an insertid from the INSERT statement");

    $original->dbID($db_id);
    $original->adaptor($self);
  }
*/
}


/*
=head2 list_dbIDs

  Arg [1]    : none
  Example    : @feature_ids = @{$repeat_feature_adaptor->list_dbIDs()};
  Description: Gets an array of internal ids for all repeat features in the current db
  Arg[1]     : <optional> int. not 0 for the ids to be sorted by the seq_region.
  Returntype : list of ints
  Exceptions : none
  Caller     : ?
  Status     : Stable

=cut
*/
Vector *RepeatFeatureAdaptor_listDbIDs(RepeatFeatureAdaptor *rfa, int ordered) {

// NIY: Shouldn't really do this direct BaseAdaptor call but ...
  return BaseAdaptor_listDbIDs((BaseAdaptor *)rfa, "repeat_feature", NULL, ordered);
}

