#include "BaseFeatureAdaptor.h"


$SLICE_FEATURE_CACHE_SIZE = 4;


void BaseFeatureAdaptor_init(BaseFeatureAdaptor *baf) {
  BaseAdaptor_init((BaseAdaptor *)baf);

  my $self = $class->SUPER::new(@args);

  #initialize caching data structures
  $self->{'_slice_feature_cache'} = {};

  tie(%{$self->{'_slice_feature_cache'}}, 
      'Bio::EnsEMBL::Utils::Cache',
      $SLICE_FEATURE_CACHE_SIZE);

  return $self;
}

=head2 generic_fetch

  Arg [1]    : (optional) string $constraint
               An SQL query constraint (i.e. part of the WHERE clause)
  Arg [2]    : (optional) string $logic_name
               the logic_name of the analysis of the features to obtain
  Example    : $fts = $a->generic_fetch('contig_id in (1234, 1235)', 'Swall');
  Description: Performs a database fetch and returns feature objects in
               contig coordinates.
  Returntype : listref of Bio::EnsEMBL::SeqFeature in contig coordinates
  Exceptions : none
  Caller     : BaseFeatureAdaptor, ProxyDnaAlignFeatureAdaptor::generic_fetch

=cut
  
sub BaseFeatureAdaptor_genericFetch(BaseFeatureAdaptor *baf, char *constraint,
                                    char *logicName, AssemblyMapper *mapper, Slice *slice) {
  
  my @tabs = $self->_tables;
  my $columns = join(', ', $self->_columns());
  
  if($logic_name) {
    #determine the analysis id via the logic_name
    my $analysis = 
      $self->db->get_AnalysisAdaptor()->fetch_by_logic_name($logic_name);
    unless(defined $analysis && $analysis->dbID() ) {
      $self->warn("No analysis for logic name $logic_name exists\n");
      return [];
    }
    
    my $analysis_id = $analysis->dbID();

    #get the synonym for the primary table
    my $syn = $tabs[0]->[1];

    if($constraint) {
      $constraint .= " AND ${syn}.analysis_id = $analysis_id";
    } else {
      $constraint = " ${syn}.analysis_id = $analysis_id";
    }
  } 

  #
  # Construct a left join statement if one was defined, and remove the
  # left-joined table from the table list
  #
  my ($tablename, $condition) = $self->_left_join;
  my $left_join = '';
  my @tables;
  if($tablename && $condition) {
    while(my $t = shift @tabs) {
      if($tablename eq $t->[0]) {
	my $syn = $t->[1]; 
	$left_join =  "LEFT JOIN $tablename $syn $condition";
	push @tables, @tabs;
	last;
      } else {
	push @tables, $t;
      }
    }
  } else {
    @tables = @tabs;
  }
      
  #construct a nice table string like 'table1 t1, table2 t2'
  my $tablenames = join(', ', map({ join(' ', @$_) } @tables));

  my $sql = "SELECT $columns FROM $tablenames $left_join";

  my $default_where = $self->_default_where_clause;
  my $final_clause = $self->_final_clause;

  #append a where clause if it was defined
  if($constraint) { 
    $sql .= " where $constraint ";
    if($default_where) {
      $sql .= " and $default_where ";
    }
  } elsif($default_where) {
    $sql .= " where $default_where ";
  }

  #append additional clauses which may have been defined
  $sql .= " $final_clause";

  my $sth = $self->prepare($sql);
  
  $sth->execute;  

  return $self->_objs_from_sth($sth, $mapper, $slice);
}


=head2 fetch_by_dbID

  Arg [1]    : int $id
               the unique database identifier for the feature to be obtained 
  Example    : $feat = $adaptor->fetch_by_dbID(1234);
  Description: Returns the feature created from the database defined by the
               the id $id. 
  Returntype : Bio::EnsEMBL::SeqFeature
  Exceptions : thrown if $id is not defined
  Caller     : general

=cut

sub BaseFeatureAdaptor_fetchByDbID(BaseFeatureAdaptor *baf, int64 dbID) {

  my @tabs = $self->_tables;

  my ($name, $syn) = @{$tabs[0]};

  #construct a constraint like 't1.table1_id = 1'
  my $constraint = "${syn}.${name}_id = $id";

  #return first element of _generic_fetch list
  my ($feat) = @{$self->generic_fetch($constraint)}; 
  return $feat;
}


=head2 fetch_all_by_RawContig_constraint

  Arg [1]    : Bio::EnsEMBL::RawContig $contig
               The contig object from which features are to be obtained
  Arg [2]    : (optional) string $constraint
               An SQL query constraint (i.e. part of the WHERE clause)
  Arg [3]    : (optional) string $logic_name
               the logic name of the type of features to obtain
  Example    : $fs = $a->fetch_all_by_Contig_constraint($ctg,'perc_ident>5.0');
  Description: Returns a listref of features created from the database which 
               are on the contig defined by $cid and fulfill the SQL constraint
               defined by $constraint. If logic name is defined, only features
               with an analysis of type $logic_name will be returned. 
  Returntype : listref of Bio::EnsEMBL::SeqFeature in contig coordinates
  Exceptions : thrown if $cid is not defined
  Caller     : general

=cut

Set *BaseFeatureAdaptor_fetchAllByRawContigConstraint(BaseFeatureAdaptor *baf, RawContig *contig,
                                                      char *constraint, char *logicName)  {
  if (contig == NULL) {
    fprintf(stderr,"ERROR: fetch_by_Contig_constraint must have an contig\n");
    exit(1);
  }

  my $cid = $contig->dbID();

  #get the synonym of the primary_table
  my @tabs = $self->_tables;
  my $syn = $tabs[0]->[1];

  if($constraint) {
    $constraint .= " AND ${syn}.contig_id = $cid";
  } else {
    $constraint = "${syn}.contig_id = $cid";
  }

  return $self->generic_fetch($constraint, $logic_name);
}


=head2 fetch_all_by_RawContig

  Arg [1]    : Bio::EnsEMBL::RawContig $contig 
               the contig from which features should be obtained
  Arg [2]    : (optional) string $logic_name
               the logic name of the type of features to obtain
  Example    : @fts = $a->fetch_all_by_RawContig($contig, 'wall');
  Description: Returns a list of features created from the database which are 
               are on the contig defined by $cid If logic name is defined, 
               only features with an analysis of type $logic_name will be 
               returned. 
  Returntype : listref of Bio::EnsEMBL::*Feature in contig coordinates
  Exceptions : none
  Caller     : general

=cut
   
Set *BaseFeatureAdaptor_fetchAllByRawContig(BaseFeatureAdaptor *baf, RawContig *contig,
                                            char *logicName) {
  return BaseFeatureAdaptor_fetchAllByRawContigConstraint(baf,contig,"",logicName);
}


=head2 fetch_all_by_RawContig_and_score
  Arg [1]    : Bio::EnsEMBL::RawContig $contig 
               the contig from which features should be obtained
  Arg [2]    : (optional) float $score
               the lower bound of the score of the features to obtain
  Arg [3]    : (optional) string $logic_name
               the logic name of the type of features to obtain
  Example    : @fts = $a->fetch_by_RawContig_and_score(1, 50.0, 'Swall');
  Description: Returns a list of features created from the database which are 
               are on the contig defined by $cid and which have score greater  
               than score.  If logic name is defined, only features with an 
               analysis of type $logic_name will be returned. 
  Returntype : listref of Bio::EnsEMBL::*Feature in contig coordinates
  Exceptions : thrown if $score is not defined
  Caller     : general

=cut

Set *BaseFeatureAdaptor_fetchAllByRawContigAndScore(BaseFeatureAdaptor *baf, RawContig *contig,
                                                    double score, char *logicName) {
  my($self, $contig, $score, $logic_name) = @_;

  my $constraint;

  if(defined $score){
    #get the synonym of the primary_table
    my @tabs = $self->_tables;
    my $syn = $tabs[0]->[1];
    $constraint = "${syn}.score > $score";
  }
    
  return BaseFeatureAdaptor_fetchAllByRawContigConstraint(RawContig *contig, char *constraint, 
					                  char *logicName);
}


=head2 fetch_all_by_Slice

  Arg [1]    : Bio::EnsEMBL::Slice $slice
               the slice from which to obtain features
  Arg [2]    : (optional) string $logic_name
               the logic name of the type of features to obtain
  Example    : $fts = $a->fetch_all_by_Slice($slice, 'Swall');
  Description: Returns a listref of features created from the database 
               which are on the Slice defined by $slice. If $logic_name is 
               defined only features with an analysis of type $logic_name 
               will be returned. 
  Returntype : listref of Bio::EnsEMBL::SeqFeatures in Slice coordinates
  Exceptions : none
  Caller     : Bio::EnsEMBL::Slice

=cut

Set *BaseFeatureAdaptor_fetchAllBySlice(BaseFeatureAdaptor *baf, Slice *slice,
                                        char *logicName) {
  return BaseFeatureAdaptor_fetchAllBySliceConstraint(slice, "", logicName);
}


=head2 fetch_all_by_Slice_and_score

  Arg [1]    : Bio::EnsEMBL::Slice $slice
               the slice from which to obtain features
  Arg [2]    : (optional) float $score
               lower bound of the the score of the features retrieved
  Arg [3]    : (optional) string $logic_name
               the logic name of the type of features to obtain
  Example    : $fts = $a->fetch_all_by_Slice($slice, 'Swall');
  Description: Returns a list of features created from the database which are 
               are on the Slice defined by $slice and which have a score 
               greated than $score. If $logic_name is defined, 
               only features with an analysis of type $logic_name will be 
               returned. 
  Returntype : listref of Bio::EnsEMBL::SeqFeatures in Slice coordinates
  Exceptions : none
  Caller     : Bio::EnsEMBL::Slice

=cut

Set *BaseFeatureAdaptor_fetchAllBySliceAndScore(BaseFeatureAdaptor *baf, Slice *slice,
                                                double score, char *logicName) {
  my ($self, $slice, $score, $logic_name) = @_;
  my $constraint;

  if(defined $score) {
    #get the synonym of the primary_table
    my @tabs = $self->_tables;
    my $syn = $tabs[0]->[1];
    $constraint = "${syn}.score > $score";
  }

  return BaseFeatureAdaptor_fetchAllBySliceConstraint(slice, constraint, logicName);
}  


=head2 fetch_all_by_Slice_constraint

  Arg [1]    : Bio::EnsEMBL::Slice $slice
               the slice from which to obtain features
  Arg [2]    : (optional) string $constraint
               An SQL query constraint (i.e. part of the WHERE clause)
  Arg [3]    : (optional) string $logic_name
               the logic name of the type of features to obtain
  Example    : $fs = $a->fetch_all_by_Slice_constraint($slc, 'perc_ident > 5');
  Description: Returns a listref of features created from the database which 
               are on the Slice defined by $slice and fulfill the SQL 
               constraint defined by $constraint. If logic name is defined, 
               only features with an analysis of type $logic_name will be 
               returned. 
  Returntype : listref of Bio::EnsEMBL::SeqFeatures in Slice coordinates
  Exceptions : thrown if $slice is not defined
  Caller     : Bio::EnsEMBL::Slice

=cut

Set *BaseFeatureAdaptor_fetchAllBySliceConstraint(BaseFeatureAdaptor *baf, Slice *slice,
                                                  char *constraint, char *logicName) {

  #check the cache and return if we have already done this query
  my $key = uc(join($slice->name, $constraint, $logic_name));
  return $self->{'_slice_feature_cache'}{$key} 
    if $self->{'_slice_feature_cache'}{$key};
    
  my $slice_start  = $slice->chr_start();
  my $slice_end    = $slice->chr_end();
  my $slice_strand = $slice->strand();
		 
  my $mapper = 
    $self->db->get_AssemblyMapperAdaptor->fetch_by_type($slice->assembly_type);

  #get the list of contigs this slice is on
  my @cids = 
    $mapper->list_contig_ids( $slice->chr_name, $slice_start ,$slice_end );
  
  return [] unless scalar(@cids);

  my $cid_list = join(',',@cids);

  #get the synonym of the primary_table
  my @tabs = $self->_tables;
  my $syn = $tabs[0]->[1];

  #construct the SQL constraint for the contig ids 
  if($constraint) {
    $constraint .= " AND ${syn}.contig_id IN ($cid_list)";
  } else {
    $constraint = "${syn}.contig_id IN ($cid_list)";
  }

  #for speed the remapping to slice may be done at the time of object creation
  my $features = 
    $self->generic_fetch($constraint, $logic_name, $mapper, $slice); 
  
  if(@$features && (!$features->[0]->can('contig') || 
		    $features->[0]->contig == $slice)) {
    #features have been converted to slice coords already, cache and return
    return $self->{'_slice_feature_cache'}{$key} = $features;
  }

  #remapping has not been done, we have to do our own conversion from
  # raw contig coords to slice coords

  my @out = ();
  
  my ($feat_start, $feat_end, $feat_strand); 

  foreach my $f (@$features) {
    #since feats were obtained in contig coords, attached seq is a contig
    my $contig_id = $f->contig->dbID();

    my ($chr_name, $start, $end, $strand) = 
      $mapper->fast_to_assembly($contig_id, $f->start(), 
				$f->end(),$f->strand(),"rawcontig");

    # undefined start means gap
    next unless defined $start;     

    # maps to region outside desired area 
    next if ($start > $slice_end) || ($end < $slice_start);  
    
    #shift the feature start, end and strand in one call
    if($slice_strand == -1) {
      $f->move( $slice_end - $end + 1, $slice_end - $start + 1, $strand * -1 );
    } else {
      $f->move( $start - $slice_start + 1, $end - $slice_start + 1, $strand );
    }
    
    $f->contig($slice);
    
    push @out,$f;
  }
  
  #update the cache
  return $self->{'_slice_feature_cache'}{$key} = \@out;
}


int BaseFeatureAdaptor_store(BaseFeatureAdaptor *baf, Set *features) {
  fprintf(stderr,"ERROR: Abstract method store not defined by implementing subclass\n");
  exit(1);
}

int BaseFeatureAdaptor_remove(BaseFeatureAdaptor *baf, SeqFeature *feature) {
  my ($self, $feature) = @_;

  unless($feature->can('dbID')) {
    $self->throw("Feature [$feature] does not implement method dbID");
  }

  unless($feature->dbID) {
    $self->warn("BaseFeatureAdaptor::remove - dbID not defined - " .
                "feature could not be removed");
  }

  my @tabs = $self->_tables;
  my ($table) = @{$tabs[0]};

  my $sth = $self->prepare("DELETE FROM $table WHERE ${table}_id = ?");
  $sth->execute($feature->dbID());

  #unset the feature dbID
  $feature->dbID('');
  
  return;
}



=head2 remove_by_RawContig

  Arg [1]    : Bio::Ensembl::RawContig $contig 
  Example    : $feature_adaptor->remove_by_RawContig($contig);
  Description: This removes features from the database which lie on a removed
               contig.  The table the features are removed from is defined by 
               the abstract method_tablename, and the primary key of the table
               is assumed to be contig_id.
  Returntype : none
  Exceptions : thrown if no contig is supplied
  Caller     : general

=cut

int BaseFeatureAdaptor_removeByRawContig(BaseFeatureAdaptor *baf, RawContig *contig) {
  my ($self, $contig) = @_;

  unless($contig) {
    $self->throw("BaseFeatureAdaptor::remove - no contig supplied: ".
		 "Deletion of features failed.");
  }

  my @tabs = $self->_tables;

  my ($table_name) = @{$tabs[0]};

  my $sth = $self->prepare("DELETE FROM $table_name
                            WHERE contig_id = ?");

  $sth->execute($contig->dbID);

  return;
}

char ***BaseFeatureAdaptor_getTables() {
  fprintf(stderr,"ERROR: Abstract method getTables not defined by implementing subclass\n");
  exit(1);
}

char *BaseFeatureAdaptor_getColumns() {
  fprintf(stderr,"ERROR: Abstract method getColumns not defined by implementing subclass\n");
  exit(1);
}

/* Actually added to default where */
char *BaseFeatureAdaptor_defaultWhereClause() {
  return "";
}

/* Overridden in Markers and Qtls (not implemented in C) */
char **BaseFeatureAdaptor_leftJoin {
  return NULL;
}

/* Overridden in PredictionTranscriptAdaptor */
char *BaseFeatureAdaptor_finalClause() {
  return "";
}

Set *BaseFeatureAdaptor_objectsFromStatementHandle(BaseFeatureAdaptor *baf, StatementHandle *sth) {
  fprintf(stderr,"ERROR: Abstract method objectsFromStatementHandle not defined by implementing subclass\n");
  exit(1);
} 
