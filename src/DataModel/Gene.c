#include "Gene.h"
#include "GeneAdaptor.h"
#include "IDHash.h"

Gene *Gene_new() {
  Gene *gene;

  if ((gene = (Gene *)calloc(1,sizeof(Gene))) == NULL) {
    fprintf(stderr,"ERROR: Failed allocating space for gene\n");
    return NULL;
  }

  return gene;
}

char *Gene_getStableId(Gene *gene) {
  GeneAdaptor *ga = (GeneAdaptor *)Gene_getAdaptor(gene);

  if (StableIdInfo_getStableId(&(gene->si)) == NULL && ga) {
    GeneAdaptor_getStableEntryInfo(ga,gene);
  }
  return StableIdInfo_getStableId(&(gene->si));
}

char *Gene_setType(Gene *g, char *type) {
  if ((g->type = (char *)malloc(strlen(type)+1)) == NULL) {
    fprintf(stderr,"ERROR: Failed allocating space for gene type\n");
    return NULL;
  }

  strcpy(g->type,type);

  return g->type;
}

int Gene_setStart(Gene *gene, int start) {
  SeqFeature_setStart(&(gene->sf),start);
  Gene_setStartIsSet(gene,TRUE);
}

int Gene_getStart(Gene *gene) {
  int multiFlag = 0;

  if (Gene_getStartIsSet(gene) == FALSE) {
    char *lastContig = NULL;
    int i;
    Set *exonSet = Gene_getAllExons(gene);

    for (i=0; i<Set_getNumElement(exonSet); i++) {
      Exon *exon = Set_getElementAt(exonSet,i);

      if (!Gene_getStartIsSet(gene) || 
          Exon_getStart(exon) < SeqFeature_getStart(&(gene->sf))) {
        Gene_setStart(gene, Exon_getStart(exon));
      }
      if (multiFlag || 
          (lastContig && strcmp(lastContig, BaseContig_getName(Exon_getContig(exon))))) {
        multiFlag = 1;
      }
      lastContig =  BaseContig_getName(Exon_getContig(exon));
    }
    Gene_setStartIsSet(gene,TRUE);
    Set_free(exonSet,NULL);
  }

  if (multiFlag) {
    fprintf(stderr, "WARNING: Gene_getStart - Gene spans multiple contigs."
                "The return value from getStart may not be what you want");
  }

  return SeqFeature_getStart(&(gene->sf));
}


Set *Gene_getAllExons(Gene *gene) {
  IDHash *exonHash = IDHash_new(IDHASH_SMALL);
  int i;
  Set *exonSet = Set_new();
  void **values;

  for (i=0;i<Gene_getTranscriptCount(gene);i++) {
    Transcript *trans = Gene_getTranscriptAt(gene,i);
    int j;

    for (j=0;j<Transcript_getExonCount(trans);j++) {
      Exon *exon = Transcript_getExonAt(trans,j);
      if (!IDHash_contains(exonHash,(long)exon)) {
        IDHash_add(exonHash,(long)exon,exon);
      }
    }
  }

  values = IDHash_getValues(exonHash);

  for (i=0;i<IDHash_getNumValues(exonHash);i++) {
    Set_addElement(exonSet, values[i]);
  }
  free(values);
  IDHash_free(exonHash,NULL);
  
  return exonSet;
}

Gene *Gene_transformToSlice(Gene *gene, Slice *slice) {
  IDHash *exonTransforms = IDHash_new(IDHASH_SMALL);
  int i;
  Set *exons = Gene_getAllExons(gene);

  // transform Exons
  for (i=0;i<Set_getNumElement(exons); i++) {
    Exon *exon = (Exon *)Set_getElementAt(exons,i);
     
    Exon *newExon = Exon_transformToSlice(exon,slice);
    IDHash_add(exonTransforms, (long)exon, newExon);
  }

  // now need to re-jiggle the transcripts and their
  // translations to account for the re-mapping process

  for (i=0;i<Gene_getTranscriptCount(gene);i++) {
    Transcript *trans = Gene_getTranscriptAt(gene,i);

    // need to grab the translation before starting to
    // re-jiggle the exons

    Transcript_transform(trans, exonTransforms);
  }

#ifdef DONE
  // unset the start, end, and strand - they need to be recalculated
  $self->{start} = undef;
  $self->{end} = undef;
  $self->{strand} = undef;
  $self->{_chr_name} = undef;
#endif

  IDHash_free(exonTransforms, NULL);

  return gene;
}
