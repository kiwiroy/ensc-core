/*
  Bamcount

  A program for calculating RPKM type values for features in a BAM format 
  against a set of annotation from an Ensembl database

  Copyright (c) 1999-2013 The European Bioinformatics Institute and
  Genome Research Limited.  All rights reserved.

  This software is distributed under an Apache 2 license.
  For license details, please see

    http://www.ensembl.org/info/about/legal/code_licence.html

  Please email comments or questions to the public Ensembl
  developers list at <dev@ensembl.org>.

  Questions may also be sent to the Ensembl help desk at
  <helpdesk@ensembl.org>.
*/

#include <stdio.h>

#include "EnsC.h"

#include "DBAdaptor.h"
#include "BaseAdaptor.h"
#include "Basic/Vector.h"
#include "Slice.h"
#include "StrUtil.h"
#include "IDHash.h"

#include "sam.h"
#include "bam.h"

void       Bamcount_usage();
int        bamPosNameCompFunc(const void *one, const void *two);
int        bamPosCompFunc(const void *one, const void *two);
int        countReads(char *fName, Slice *slice, samfile_t *in, bam_index_t *idx, int flags, Vector *genes);
bam1_t *   findMateInVector(bam1_t *b, Vector *vec);
int        findPosInVec(Vector *vec, int pos, char *bqname);
int        geneStartCompFunc(const void *one, const void *two);
Vector *   getGenes(Slice *slice, int flags);
bam1_t    *mateFoundInVectors(bam1_t *b, Vector **vectors);
void       printBam(FILE *fp, bam1_t *b, bam_header_t *header);

typedef struct GeneScoreStruct {
  int   index;
  long  score;
  Gene *gene;
} GeneScore;

// Flag values
#define M_UCSC_NAMING 1
#define M_VERIFY 4

// My custom bam core struct flag value
#define MY_FUSEDFLAG 32768

int verbosity = 1;

int main(int argc, char *argv[]) {
  DBAdaptor *      dba;
  StatementHandle *sth;
  ResultRow *      row;
  Vector *         slices;
  int              nSlices;
  samfile_t *      out;

  int   argNum = 1;

  char *inFName  = NULL;
  char *outFName = NULL;

  char *dbUser = "ensro";
  char *dbPass = NULL;
  int   dbPort = 3306;

  char *dbHost = "ens-staging.internal.sanger.ac.uk";
  char *dbName = "homo_sapiens_core_71_37";

  char *assName = "GRCh37";

  char *chrName = "1";


  int flags = 0;

  initEnsC();

  while (argNum < argc) {
    char *arg = argv[argNum];
    char *val;

// Ones without a val go here
    if (!strcmp(arg, "-U") || !strcmp(arg,"--ucsc_naming")) {
      flags |= M_UCSC_NAMING;
    } else {
// Ones with a val go in this block
      if (argNum == argc-1) {
        Bamcount_usage();
      }

      val = argv[++argNum];
  
      if (!strcmp(arg, "-i") || !strcmp(arg,"--in_file")) {
        StrUtil_copyString(&inFName,val,0);
      } else if (!strcmp(arg, "-o") || !strcmp(arg,"--out_file")) {
        StrUtil_copyString(&outFName,val,0);
      } else if (!strcmp(arg, "-h") || !strcmp(arg,"--host")) {
        StrUtil_copyString(&dbHost,val,0);
      } else if (!strcmp(arg, "-p") || !strcmp(arg,"--password")) {
        StrUtil_copyString(&dbPass,val,0);
      } else if (!strcmp(arg, "-P") || !strcmp(arg,"--port")) {
        dbPort = atoi(val);
      } else if (!strcmp(arg, "-n") || !strcmp(arg,"--name")) {
        StrUtil_copyString(&dbName,val,0);
      } else if (!strcmp(arg, "-u") || !strcmp(arg,"--user")) {
        StrUtil_copyString(&dbUser,val,0);
      } else if (!strcmp(arg, "-a") || !strcmp(arg,"--assembly")) {
        StrUtil_copyString(&assName,val,0);
      } else if (!strcmp(arg, "-v") || !strcmp(arg,"--verbosity")) {
        verbosity = atoi(val);
// Temporary
      } else if (!strcmp(arg, "-c") || !strcmp(arg,"--chromosome")) {
        StrUtil_copyString(&chrName,val,0);
      } else {
        fprintf(stderr,"Error in command line at %s\n\n",arg);
        Bamcount_usage();
      }
    }
    argNum++;
  }

  if (verbosity > 0) {
    printf("Program for read counting of BAM reads against annotation \n"
           "Steve M.J. Searle.  searle@sanger.ac.uk  Last update Mar 2013.\n");
  }

  if (!inFName || !outFName) {
    Bamcount_usage();
  }

  dba = DBAdaptor_new(dbHost,dbUser,dbPass,dbName,dbPort,NULL);

  //nSlices = getSlices(dba, destName);
  nSlices = 1;

  slices = Vector_new();

  SliceAdaptor *sa = DBAdaptor_getSliceAdaptor(dba);

  Slice *slice = SliceAdaptor_fetchByChrStartEnd(sa,chrName,1,300000000);

  Vector_addElement(slices,slice);

  if (Vector_getNumElement(slices) == 0) {
    fprintf(stderr, "Error: No slices.\n");
    exit(1);
  }

#ifdef _PBGZF_USE
  bam_set_num_threads_per(5);
#endif
  samfile_t *in = samopen(inFName, "rb", 0);
  if (in == 0) {
    fprintf(stderr, "Fail to open BAM file %s\n", inFName);
    return 1;
  }

  bam_index_t *idx;
  idx = bam_index_load(inFName); // load BAM index
  if (idx == 0) {
    fprintf(stderr, "BAM index file is not available.\n");
    return 1;
  }

  int i;
  for (i=0; i<Vector_getNumElement(slices); i++) {
    Slice *slice = Vector_getElementAt(slices,i);

    if (verbosity > 0) printf("Working on '%s'\n",Slice_getChrName(slice));

    if (verbosity > 0) printf("Stage 1 - retrieving annotation from database\n");
    Vector *genes = getGenes(slice, flags);

    if (verbosity > 0) printf("Stage 2 - counting reads\n");
    countReads(inFName, slice, in, idx, flags, genes);
  }


  bam_index_destroy(idx);
  samclose(in);

  if (verbosity > 0) printf("Done\n");
  return 0;
}

/*
 Program usage message
*/
void Bamcount_usage() {
  printf("bamcount \n"
         "  -i --in_file     Input BAM file to map from (string)\n"
         "  -o --out_file    Output BAM file to write (string)\n"
         "  -U --ucsc_naming Input BAM file has 'chr' prefix on ALL seq region names (flag)\n"
         "  -h --host        Database host name for db containing mapping (string)\n"
         "  -n --name        Database name for db containing mapping (string)\n"
         "  -u --user        Database user (string)\n"
         "  -p --password    Database password (string)\n"
         "  -P --port        Database port (int)\n"
         "  -a --assembly    Assembly name (string)\n"
         "  -v --verbosity   Verbosity level (int)\n"
         "\n"
         "Notes:\n"
         "  -U will cause 'chr' to be prepended to all source seq_region names, except the special case MT which is changed to chrM.\n"
         "  -v Default verbosity level is 1. You can make it quieter by setting this to 0, or noisier by setting it > 1.\n"
         );
  exit(1);
}

int bamPosNameCompFunc(const void *one, const void *two) {
  bam1_t *b1 = *((bam1_t**)one);
  bam1_t *b2 = *((bam1_t**)two);

  if (b1->core.tid < b2->core.tid) {
    printf("Urrr.... this shouldn't happen b1 tid %d   b2 tid %d\n", b1->core.tid, b2->core.tid);
    return -1;
  } else if (b1->core.tid > b2->core.tid) {
    printf("Urrr.... this shouldn't happen b1 tid %d   b2 tid %d\n", b1->core.tid, b2->core.tid);
    return 1;
  } else {
    if (b1->core.pos > b2->core.pos) {
      return 1;
    } else if (b1->core.pos < b2->core.pos) {
      return -1;
    } else {
      return strcmp(bam1_qname(b1),bam1_qname(b2));
    }
  }
}

int bamPosCompFunc(const void *one, const void *two) {
  bam1_t *b1 = *((bam1_t**)one);
  bam1_t *b2 = *((bam1_t**)two);

  return b1->core.pos - b2->core.pos;
}

int geneStartCompFunc(const void *one, const void *two) {
  Gene *g1 = *((Gene**)one);
  Gene *g2 = *((Gene**)two);

  return Gene_getStart(g1) - Gene_getStart(g2);
}

/*
 print out a bam1_t entry, particularly the flags (for debugging)
*/
void printBam(FILE *fp, bam1_t *b, bam_header_t *header) {    
  fprintf(fp, "%s %s %d %d %s (%d) %d %d %d\t\tP %d PP %d U %d MU %d R %d MR %d R1 %d R2 %d S %d QC %d D %d U %d\n",
                                  bam1_qname(b), 
                                  header->target_name[b->core.tid], 
                                  b->core.pos, 
                                  bam_calend(&b->core,bam1_cigar(b)),
                                  header->target_name[b->core.mtid], 
                                  b->core.mtid, 
                                  b->core.mpos, 
                                  b->core.isize, 
                                  bam_cigar2qlen(&(b->core),bam1_cigar(b)),
                                  b->core.flag & BAM_FPAIRED,
                                  b->core.flag & BAM_FPROPER_PAIR ? 1 : 0,
                                  b->core.flag & BAM_FUNMAP ? 1 : 0,
                                  b->core.flag & BAM_FMUNMAP ? 1 : 0,
                                  b->core.flag & BAM_FREVERSE ? 1 : 0,
                                  b->core.flag & BAM_FMREVERSE ? 1 : 0,
                                  b->core.flag & BAM_FREAD1 ? 1 : 0,
                                  b->core.flag & BAM_FREAD2 ? 1 : 0,
                                  b->core.flag & BAM_FSECONDARY ? 1 : 0,
                                  b->core.flag & BAM_FQCFAIL ? 1 : 0,
                                  b->core.flag & BAM_FDUP ? 1 : 0,
                                  b->core.flag & MY_FUSEDFLAG ? 1 : 0
                                  );
  fflush(fp);
}

Vector *getGenes(Slice *slice, int flags) { 
  Vector *genes;
  genes = Slice_getAllGenes(slice, NULL);

  Vector_sort(genes, geneStartCompFunc);

  return genes;
}

int countReads(char *fName, Slice *slice, samfile_t *in, bam_index_t *idx, int flags, Vector *genes) {
  int  ref;
  int  begRange;
  int  endRange;
  char region[1024];
  IDHash *geneCountsHash = IDHash_new(IDHASH_LARGE);
  GeneScore *gs; 


  if (Slice_getChrStart(slice) != 1) {
    fprintf(stderr, "Currently only allow a slice start position of 1\n");
    return 1;
  }
  if (flags & M_UCSC_NAMING) {
    sprintf(region,"chr%s:%d-%d", Slice_getChrName(slice), 
                                  Slice_getChrStart(slice), 
                                  Slice_getChrEnd(slice));
  } else {
    sprintf(region,"%s:%d-%d", Slice_getChrName(slice), 
                               Slice_getChrStart(slice), 
                               Slice_getChrEnd(slice));
  }
  bam_parse_region(in->header, region, &ref, &begRange, &endRange);
  if (ref < 0) {
    fprintf(stderr, "Invalid region %s %d %d\n", Slice_getChrName(slice), 
                                                 Slice_getChrStart(slice), 
                                                 Slice_getChrEnd(slice));
    return 1;
  }

// Pregenerate the hash for gene scores, so we don't have to test for existence within the BAM read loop
// Also means all genes will have a score hash entry
  int i;
  for (i=0; i < Vector_getNumElement(genes); i++) {
    Gene *gene = Vector_getElementAt(genes,i);

    if ((gs = (GeneScore *)calloc(1,sizeof(GeneScore))) == NULL) {
      fprintf(stderr,"ERROR: Failed allocating GeneScore\n");
      exit(1);
    }
    gs->index = i;
    gs->gene  = gene;
    IDHash_add(geneCountsHash,Gene_getDbID(gene),gs);
  }

  bam_iter_t iter = bam_iter_query(idx, ref, begRange, endRange);
  bam1_t *b = bam_init1();

  long counter = 0;
  long overlapping = 0;
  long bad = 0;
  int startIndex = 0;
  while (bam_iter_read(in->x.bam, iter, b) >= 0) {
    if (b->core.flag & (BAM_FUNMAP | BAM_FSECONDARY | BAM_FQCFAIL | BAM_FDUP)) {
      bad++;
      continue;
    }

    int end;
    end = bam_calend(&b->core, bam1_cigar(b));

    // There is a special case for reads which have zero length and start at begRange (so end at begRange ie. before the first base we're interested in).
    // That is the reason for the || end == begRange test
    if (end == begRange) {
      continue;
    }
    counter++;

    if (!(counter%1000000)) {
      if (verbosity > 1) { printf("."); }
      fflush(stdout);
    }

    int j;
    int done = 0;
    int hadOverlap = 0;
    
    for (j=startIndex; j < Vector_getNumElement(genes) && !done; j++) {
      Gene *gene = Vector_getElementAt(genes,j); 
      if (!gene) {
        continue;
      }
      if (b->core.pos < Gene_getEnd(gene) && end >= Gene_getStart(gene)) {
//        if (DNAAlignFeature_getEnd(snp) >= Gene_getStart(gene) && 
//           DNAAlignFeature_getStart(snp) <= Gene_getEnd(gene)) {
        int k;

        int doneGene = 0;
        for (k=0; k<Gene_getTranscriptCount(gene) && !doneGene; k++) {
          Transcript *trans = Gene_getTranscriptAt(gene,k);

          if (b->core.pos < Transcript_getEnd(trans) && end >= Transcript_getStart(trans)) {
            // if (DNAAlignFeature_getEnd(snp) >= Transcript_getStart(trans) && 
            //     DNAAlignFeature_getStart(snp) <= Transcript_getEnd(trans)) {
            int m;
     
            for (m=0; m<Transcript_getExonCount(trans) && !doneGene; m++) {
              Exon *exon = Transcript_getExonAt(trans,m);

              if (b->core.pos < Exon_getEnd(exon) && end >= Exon_getStart(exon)) {
                //if (DNAAlignFeature_getEnd(snp) >= Exon_getStart(exon) && 
                //    DNAAlignFeature_getStart(snp) <= Exon_getEnd(exon)) {

                // Only count as overlapping once (could be that a read overlaps more than one gene)
                if (!hadOverlap) {
                  overlapping++;
                  hadOverlap = 1;
                }

                gs = IDHash_getValue(geneCountsHash, Gene_getDbID(gene));
                gs->score++;
                
                doneGene = 1;
              }
            }
          }
        }
      } else if (Gene_getStart(gene) > end) {
        done = 1;
      } else if (Gene_getEnd(gene) < b->core.pos+1) {
        gs = IDHash_getValue(geneCountsHash, Gene_getDbID(gene));
        printf("Gene %s (%s) score %ld\n",Gene_getStableId(gene), 
                                          Gene_getDisplayXref(gene) ? DBEntry_getDisplayId(Gene_getDisplayXref(gene)) : "", 
                                          gs->score);

        if (verbosity > 1) { 
          printf("Removing gene %s (index %d) with extent %d to %d\n", 
                 Gene_getStableId(gene), 
                 gs->index,
                 Gene_getStart(gene),
                 Gene_getEnd(gene));
        }
        Vector_setElementAt(genes,j,NULL);

        // Magic (very important for speed) - move startIndex to first non null gene
        int n;
        startIndex = 0;
        for (n=0;n<Vector_getNumElement(genes);n++) {
          void *v = Vector_getElementAt(genes,n);

          if (v != NULL) {
            break;
          }
          startIndex++;
        }
        if (verbosity > 1) { 
          printf("startIndex now %d\n",startIndex);
        }
      }
    }
  }
  if (verbosity > 1) { printf("\n"); }

// Print out read counts for what ever's left in the genes array
  int n;
  for (n=0;n<Vector_getNumElement(genes);n++) {
    Gene *gene = Vector_getElementAt(genes,n);

    if (gene != NULL) {
      gs = IDHash_getValue(geneCountsHash, Gene_getDbID(gene));
      printf("Gene %s (%s) score %ld\n",Gene_getStableId(gene), 
                                        Gene_getDisplayXref(gene) ? DBEntry_getDisplayId(Gene_getDisplayXref(gene)) : "", 
                                        gs->score);
    }
  }

  printf("Read %ld reads. Num overlapping exons %ld. Number of bad reads (unmapped, qc fail, secondary, dup) %ld\n", counter, overlapping, bad);

  bam_iter_destroy(iter);
  bam_destroy1(b);
}

bam1_t *getMateFromRemoteMates(bam1_t *b, Vector **remoteMates) {
// For now just delegate, could optimise more
  return mateFoundInVectors(b, remoteMates);
}

bam1_t *mateFoundInVectors(bam1_t *b, Vector **vectors) {
  Vector *vec = vectors[b->core.mtid];
  
  if (!vec) {
    return NULL;
  }

  return findMateInVector(b, vec);
}

bam1_t *findMateInVector(bam1_t *b, Vector *vec) {
  char *bqname = bam1_qname(b);
  int firstInd = findPosInVec(vec, b->core.mpos, bqname);
  int i;

  if (firstInd >=0 ) {
    for (i=firstInd;i<Vector_getNumElement(vec);i++) {
      bam1_t *vb = (bam1_t *)Vector_getElementAt(vec,i);

      if (b->core.mpos == vb->core.pos) {
        //if (b->core.isize == -vb->core.isize &&
        if (abs(b->core.isize) == abs(vb->core.isize) &&
            b->core.pos == vb->core.mpos &&
            !(vb->core.flag & MY_FUSEDFLAG)) {
            
          int qnameCmp =  strcmp(bam1_qname(vb),bqname); // Note order of comparison
          if (!qnameCmp) { // Name match
            return vb;
          } else if (qnameCmp > 0) { // Optimisation: As names in vector are sorted, once string comparison is positive can not be any matches anymore
            break;
          }
        }
      } else if (b->core.mpos < vb->core.pos) {
        break;
      }
    }
  }

  return NULL;
}

int findPosInVec(Vector *vec, int pos, char *bqname) {
  int imin = 0;
  int imax = Vector_getNumElement(vec)-1;

  while (imax >= imin) {
    int imid = (imax+imin) / 2;
    bam1_t *b = Vector_getElementAt(vec,imid);

//    printf("imid = %d imin = %d imax = %d pos = %d b->core.pos = %d\n",imid,imin,imax,pos,b->core.pos);

    if (pos > b->core.pos) {
      imin = imid + 1;
    } else if (pos < b->core.pos) {
      imax = imid - 1;
    } else {
      //int qnameCmp =  strcmp(bqname,bam1_qname(b)); // Note order of comparison
      //if (!qnameCmp) {
        // key found at index imid
        // Back up through array to find first index which matches (can be several)
        for (;imid>=0;imid--) {
          bam1_t *vb = Vector_getElementAt(vec,imid);
       //   if (vb->core.pos != pos || strcmp(bqname,bam1_qname(vb))) {
          if (vb->core.pos != pos) {// || strcmp(bqname,bam1_qname(vb))) {
            return imid+1;
          }
        }
      //} else if (qnameCmp < 0) {
      //  imax = imid - 1;
      //} else {
      //  imin = imid + 1;
      //}

      return 0; // Is the first element
    }
  }
  return -1;
}

