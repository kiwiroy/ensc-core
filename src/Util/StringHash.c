#include <stdio.h>
#include "StringHash.h"
#include "StrUtil.h"

unsigned int StringHash_getBucketNum(StringHash *stringHash, char *key) {
  unsigned int hash, i;
  int len = strlen(key);
  unsigned int hashCode;

  printf("key = %s ",key);
  for (hash=len, i=0; i<len; ++i) {
    hash = (hash<<5)^(hash>>27)^key[i];
  }
  hashCode = (hash%stringHash->size);
  printf("%d\n",hashCode);

  return hashCode;
}

StringHash *StringHash_new(StringHashSizes size) {
  StringHash *stringHash;

  if ((stringHash = (StringHash *)calloc(1,sizeof(StringHash))) == NULL) {
    fprintf(stderr,"ERROR: Failed allocating space for stringHash\n");
    return NULL;
  }

  switch (size) {
    case STRINGHASH_SMALL:
      stringHash->size = 257; 
      break;
    case STRINGHASH_LARGE:
      stringHash->size = 104711; 
      break;
    case STRINGHASH_MEDIUM:
    default:
      stringHash->size = 32353; 
  }

  if ((stringHash->buckets = (KeyValuePair **)calloc(stringHash->size,sizeof(KeyValuePair *))) == NULL) {
    fprintf(stderr,"ERROR: Failed allocating space for stringHash->buckets\n");
    return NULL;
  }

  if ((stringHash->bucketCounts = (int *)calloc(stringHash->size,sizeof(int))) == NULL) {
    fprintf(stderr,"ERROR: Failed allocating space for stringHash->buckets\n");
    return NULL;
  }

  return stringHash;
}

int StringHash_getNumValues(StringHash *stringHash) {
  return stringHash->nValue;
}

void *StringHash_getValues(StringHash *stringHash) {
  int i;
  int j;
  void **values;
  int valCnt = 0;
  
  if (!stringHash->nValue) {
    return NULL;
  }

  if ((values = (void **)calloc(stringHash->nValue,sizeof(void *))) == NULL) {
    fprintf(stderr,"ERROR: Failed allocating space for values\n");
    return NULL;
  }

  for (i=0; i<stringHash->size; i++) {
    if (stringHash->bucketCounts[i]) {
      for (j=0; j<stringHash->bucketCounts[i]; j++) {
        values[valCnt++] = stringHash->buckets[i][j].value;
      }
    }
  }
  if (valCnt != stringHash->nValue) {
    fprintf(stderr,"ERROR: Internal StringHash error - valCnt != stringHash->nValue\n");
  }
  return values;
}

void *StringHash_getValue(StringHash *stringHash, char *key) {
  int bucketNum = StringHash_getBucketNum(stringHash,key);
  int i;

  for (i=0; i<stringHash->bucketCounts[bucketNum]; i++) {
    if (!strcmp(key, stringHash->buckets[bucketNum][i].key)) {
      return stringHash->buckets[bucketNum][i].value;
    }
  }

  fprintf(stderr,"ERROR: Didn't find key %d in StringHash\n",key);
  return NULL;
}

int StringHash_contains(StringHash *stringHash, char *key) {
  int bucketNum = StringHash_getBucketNum(stringHash,key);
  int i;

  for (i=0; i<stringHash->bucketCounts[bucketNum]; i++) {
    if (!strcmp(key,stringHash->buckets[bucketNum][i].key)) {
      return 1;
    }
  }

  return 0;
}

int StringHash_add(StringHash *stringHash, char *key, void *val) {
  int bucketNum = StringHash_getBucketNum(stringHash,key);
  int count = stringHash->bucketCounts[bucketNum];

  if (StringHash_contains(stringHash,key)) {
    int i;

    fprintf(stderr,"WARNING: Duplicate key %s - value will be overwritten\n",key);
    for (i=0; i<stringHash->bucketCounts[bucketNum]; i++) {
      if (!strcmp(key,stringHash->buckets[bucketNum][i].key)) {
        stringHash->buckets[bucketNum][i].value = val;
        return 1;
      }
    }
    
  } else {
    if (!count || !(count%10)) {
      if ((stringHash->buckets[bucketNum] = 
           (KeyValuePair *)realloc(stringHash->buckets[bucketNum],
                                   (count+10) * sizeof(KeyValuePair))) == NULL) {
        fprintf(stderr,"ERROR: Failed allocating space for stringHash bucket\n");
        return 0;
      }
    }
    
    stringHash->buckets[bucketNum][count].key = StrUtil_CopyString(key);
    stringHash->buckets[bucketNum][count].value = val;

    stringHash->nValue++;
    stringHash->bucketCounts[bucketNum]++;
  }

  return 1; 
}

void StringHash_free(StringHash *stringHash, void *freeFunc()) {
  int i;
  int j;
  
  for (i=0; i<stringHash->size; i++) {
    if (stringHash->bucketCounts[i]) {
      for (j=0; j<stringHash->bucketCounts[i]; j++) {
        free(stringHash->buckets[i][j].key);
        if (freeFunc) {
          freeFunc(stringHash->buckets[i][j].value);
        }
      }
      free(stringHash->buckets[i]);
    }
  }
  
  free(stringHash->bucketCounts);
  free(stringHash);
}
