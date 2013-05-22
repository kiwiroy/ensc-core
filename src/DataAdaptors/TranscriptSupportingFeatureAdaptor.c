/*
=head1 NAME

Bio::EnsEMBL::DBSQL::TranscriptSupportingFeatureAdaptor - Retrieves
supporting features from the database.
*/

#include "TranscriptSupportingFeatureAdaptor.h"

#include "DNAAlignFeatureAdaptor.h"
#include "ProteinAlignFeatureAdaptor.h"
#include "BaseAlignFeature.h"
#include "DBAdaptor.h"
#include "BaseFeatureAdaptor.h"

#include <string.h>

TranscriptSupportingFeatureAdaptor *TranscriptSupportingFeatureAdaptor_new(DBAdaptor *dba) {
  TranscriptSupportingFeatureAdaptor *tsfa;

  if ((tsfa = (TranscriptSupportingFeatureAdaptor *)calloc(1,sizeof(TranscriptSupportingFeatureAdaptor))) == NULL) {
    fprintf(stderr, "ERROR: Failed allocating space for TranscriptSupportingFeatureAdaptor\n");
    return NULL;
  }
  BaseAdaptor_init((BaseAdaptor *)tsfa, dba, TRANSCRIPTSUPPORTINGFEATURE_ADAPTOR);

  return tsfa;
}

/*
=head2 fetch_all_by_Transcript

  Arg [1]    : Bio::EnsEMBL::Transcript $transcript 
               The transcript to fetch supporting features for
  Example    : @sfs = @{$supporting_feat_adaptor->fetch_all_by_Transcript($transcript)};
  Description: Retrieves supporting features (evidence) for a given transcript. 
  Returntype : list of Bio::EnsEMBL::BaseAlignFeatures in the same coordinate
               system as the $transcript argument
  Exceptions : warning if $transcript is not in the database (i.e. dbID not defined)
               throw if a retrieved supporting feature is of unknown type 
  Caller     : Bio::EnsEMBL::Transcript
  Status     : Stable

=cut
*/
Vector *TranscriptSupportingFeatureAdaptor_fetchAllByTranscript(TranscriptSupportingFeatureAdaptor *tsfa, Transcript *transcript) {
  StatementHandle *sth;
  char qStr[512];
  ResultRow *row;
  DNAAlignFeatureAdaptor *dafa;
  ProteinAlignFeatureAdaptor *pafa;


  if (!Transcript_getDbID(transcript)) {
    fprintf(stderr,"WARNING: transcript has no dbID can't fetch evidence from db "
                   "no relationship exists\n");
    return Vector_new();
  }

  Vector *out = Vector_new();
  sprintf(qStr,"SELECT tsf.feature_type, tsf.feature_id "
               "FROM   transcript_supporting_feature tsf "
               "WHERE  transcript_id = " IDFMTSTR, Transcript_getDbID(transcript));

  sth = tsfa->prepare((BaseAdaptor *)tsfa, qStr, strlen(qStr));

  sth->execute(sth);

  pafa = DBAdaptor_getProteinAlignFeatureAdaptor(tsfa->dba);
  dafa = DBAdaptor_getDNAAlignFeatureAdaptor(tsfa->dba);

  while ((row = sth->fetchRow(sth))) {
    SeqFeature *sf = NULL;
    char *type = row->getStringAt(row,0);
    IDType dbId = row->getLongLongAt(row,1);

// sf is HACK HACK HACK
    if (!strcmp(type,"protein_align_feature")) {
      sf = (SeqFeature *)ProteinAlignFeatureAdaptor_fetchByDbID(pafa, dbId);
    } else if (!strcmp(type,"dna_align_feature")) {
      sf = (SeqFeature *)DNAAlignFeatureAdaptor_fetchByDbID(dafa, dbId);
    } else {
      fprintf(stderr,"Error: Unknown feature type [%s]\n",type);
      exit(1);
    }

    if (sf == NULL) {
      fprintf(stderr,"Warning: Transcript supporting feature %s "IDFMTSTR" does not exist in DB\n", type, dbId);
    } else {
      SeqFeature *newSf = SeqFeature_transfer(sf, (Slice *)Transcript_getSlice(transcript));
      // NIY: Free pretranferred one??
      if (newSf) {
        Vector_addElement(out, newSf);
      } else {
        fprintf(stderr,"Warning: Failed to transfer transcript supporting feature %s "IDFMTSTR" onto transcript slice\n", type, dbId);
      }
    }
  }

  sth->finish(sth);
  return out;
}


/*
=head2 store
  Arg [2]    : Int $transID
               The dbID of an EnsEMBL transcript to associate with supporting
               features
  Arg [1]    : Ref to array of Bio::EnsEMBL::BaseAlignFeature (the support)
  Example    : $dbea->store($transcript_id, \@features);
  Description: Stores a set of alignment features and associates an EnsEMBL transcript
               with them
  Returntype : none
  Exceptions : thrown when invalid dbID is passed to this method
  Caller     : TranscriptAdaptor
  Status     : Stable

=cut

sub store {
  my ( $self, $tran_dbID, $aln_objs ) = @_;

  my $pep_check_sql = 
      "SELECT protein_align_feature_id " . 
      "FROM protein_align_feature " . 
      "WHERE seq_region_id = ? " . 
      "AND   seq_region_start = ? " . 
      "AND   seq_region_end   = ? " .
      "AND   seq_region_strand = ? " . 
      "AND   hit_name = ? " . 
      "AND   hit_start = ? " . 
      "AND   hit_end   = ? " . 
      "AND   analysis_id = ? " . 
      "AND   cigar_line = ? " .
      "AND   hcoverage = ? ";

  my $dna_check_sql = 
      "SELECT dna_align_feature_id " . 
      "FROM  dna_align_feature " . 
      "WHERE seq_region_id = ? " . 
      "AND   seq_region_start = ? " . 
      "AND   seq_region_end   = ? " .
      "AND   seq_region_strand = ? " . 
      "AND   hit_name = ? " . 
      "AND   hit_start = ? " . 
      "AND   hit_end   = ? " . 
      "AND   analysis_id = ? " . 
      "AND   cigar_line = ? " .
      "AND   hcoverage = ? " . 
      "AND   hit_strand = ? ";

  my $assoc_check_sql = 
      "SELECT * " .  
      "FROM  transcript_supporting_feature " . 
      "WHERE transcript_id = $tran_dbID " . 
      "AND   feature_type = ? " . 
      "AND   feature_id   = ? ";

  my $assoc_write_sql = "INSERT into transcript_supporting_feature " . 
      "(transcript_id, feature_id, feature_type) " . 
      "values(?, ?, ?)";

  my $pep_check_sth = $self->prepare($pep_check_sql);
  my $dna_check_sth = $self->prepare($dna_check_sql);
  my $assoc_check_sth = $self->prepare($assoc_check_sql);
  my $sf_sth = $self->prepare($assoc_write_sql);

  my $dna_adaptor = $self->db->get_DnaAlignFeatureAdaptor();
  my $pep_adaptor = $self->db->get_ProteinAlignFeatureAdaptor();

  foreach my $f (@$aln_objs) {
    # check that the feature is in toplevel coords

    if($f->slice->start != 1 || $f->slice->strand != 1) {
    #move feature onto a slice of the entire seq_region
      my $tls = $self->db->get_sliceAdaptor->fetch_by_region($f->slice->coord_system->name(),
                                                             $f->slice->seq_region_name(),
                                                             undef, #start
                                                             undef, #end
                                                             undef, #strand
                                                             $f->slice->coord_system->version());
      $f = $f->transfer($tls);

      if(!$f) {
        throw('Could not transfer Feature to slice of ' .
              'entire seq_region prior to storing');
      }
    }

    if(!$f->isa("Bio::EnsEMBL::BaseAlignFeature")){
      throw("$f must be an align feature otherwise" .
            "it can't be stored");
    }
       
    my ($sf_dbID, $type, $adap, $check_sth);
    
    my @check_args = ($self->db->get_SliceAdaptor->get_seq_region_id($f->slice),
                      $f->start,
                      $f->end,
                      $f->strand,
                      $f->hseqname,
                      $f->hstart,
                      $f->hend,
                      $f->analysis->dbID,
                      $f->cigar_string,
		      $f->hcoverage);
    
    if($f->isa("Bio::EnsEMBL::DnaDnaAlignFeature")){
      $adap = $dna_adaptor;      
      $check_sth = $dna_check_sth;
      $type = 'dna_align_feature';
      push @check_args, $f->hstrand;
    } elsif($f->isa("Bio::EnsEMBL::DnaPepAlignFeature")){
      $adap = $pep_adaptor;
      $check_sth = $pep_check_sth;
      $type = 'protein_align_feature';
    } else {
      warning("Supporting feature of unknown type. Skipping : [$f]\n");
      next;
    }

    $check_sth->execute(@check_args);
    $sf_dbID = $check_sth->fetchrow_array;
    
    if (not $sf_dbID) {
 
      $adap->store($f);
      $sf_dbID = $f->dbID;
    }

    # now check association
    $assoc_check_sth->execute($type,
                              $sf_dbID);
    if (not $assoc_check_sth->fetchrow_array) {    
      $sf_sth->bind_param(1, $tran_dbID, SQL_INTEGER);
      $sf_sth->bind_param(2, $sf_dbID, SQL_INTEGER);
      $sf_sth->bind_param(3, $type, SQL_VARCHAR);
      $sf_sth->execute();
    }
  }

  $dna_check_sth->finish;
  $pep_check_sth->finish;
  $assoc_check_sth->finish;
  $sf_sth->finish;
  
}
*/

