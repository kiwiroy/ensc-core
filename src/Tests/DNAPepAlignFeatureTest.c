#include <stdio.h>

#include "SliceAdaptor.h"
#include "DBAdaptor.h"
#include "EnsC.h"
#include "DNAPepAlignFeature.h"

#include "BaseRODBTest.h"

int main(int argc, char *argv[]) {
  DBAdaptor *dba;
  ProteinAlignFeatureAdaptor *pafa;
  Slice *slice;
  Vector *features;
  int i;
  int failed;
  
  initEnsC(argc, argv);

  dba = Test_initROEnsDB();

  slice = Test_getStandardSlice(dba);

  ok(1, slice!=NULL);

  pafa = DBAdaptor_getProteinAlignFeatureAdaptor(dba);

  ok(2, pafa!=NULL);

  features =  Slice_getAllProteinAlignFeatures(slice,NULL,NULL,NULL,NULL);

  ok(3, features!=NULL);
  ok(4, Vector_getNumElement(features)!=0);

  failed = 0;
  for (i=0;i<Vector_getNumElement(features) && !failed;i++) {
    DNAPepAlignFeature *daf = Vector_getElementAt(features,i);
    Vector *ungapped;
    char *oldCigar = DNAPepAlignFeature_getCigarString(daf);

    ungapped = DNAPepAlignFeature_getUngappedFeatures((BaseAlignFeature *)daf);
    if (!ungapped) failed = 1;
    // printf(" cigar = %s num ungapped %d\n",DNAPepAlignFeature_getCigarString(daf), Vector_getNumElement(ungapped));

    BaseAlignFeature_parseFeatures(daf,ungapped); 
    // printf(" cigar now = %s\n",DNAPepAlignFeature_getCigarString(daf));
// NIY Make sure free func has been set for vector
    Vector_free(ungapped);
    if (strcmp(oldCigar,DNAPepAlignFeature_getCigarString(daf))) {
      printf(" cigars different %s %s\n",oldCigar, DNAPepAlignFeature_getCigarString(daf));
      failed = 1;
    }
  }
  ok(5, !failed);

  failed = 0;
  for (i=0;i<Vector_getNumElement(features) && !failed;i++) {
    DNAPepAlignFeature *daf = Vector_getElementAt(features,i);
    int start = DNAPepAlignFeature_getStart(daf);
    int end   = DNAPepAlignFeature_getEnd(daf);
    Vector *rdafVector;
    DNAPepAlignFeature *rdaf;

    // printf("slice start = %d end = %d\n",start,end);
    // Temporary:
    rdaf = SeqFeature_transform(daf, "contig", NULL, NULL);
/*
    if (Vector_getNumElement(rdafVector) > 1) {
      printf("Feature mapped to more than one rawcontig\n");
      failed=1;
    }
    rdaf = Vector_getElementAt(rdafVector,0);
*/
    if (rdaf == NULL) {
      printf("Feature didn't map\n");
    } else {

    // printf("rc start = %d end = %d\n",DNAPepAlignFeature_getStart(rdaf),DNAPepAlignFeature_getEnd(rdaf));
      // Temporary:
      daf = SeqFeature_transfer(rdaf, slice);
      if (DNAPepAlignFeature_getStart(daf) != start ||
          DNAPepAlignFeature_getEnd(daf) != end) {
        printf("Remapping to slice produced different coords\n");
        failed =1;
      }
    }
  }
  ok(6, !failed);

  Vector_free(features);

  ProcUtil_mallInfo();


  return 0;
}
