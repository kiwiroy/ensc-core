#ifndef __CHASH_H__
#define __CHASH_H__

#define CHASH_ALLOCSIZE 1000

typedef struct CHashTableStruct CHASHTABLE;
typedef struct CHashArrayStruct CHASHARRAY;


struct CHashArrayStruct {
  int   Index;
};

struct CHashTableStruct {
  int         NElement;
  int         NLetter;        
  char      **Strings;
  CHASHARRAY**Letter;
  int        *LetCounts;
  int         (*HashFunc)(char *String,int *LetInd);
};

int  CHash_addAllocedStr(CHASHTABLE *Table,char *String);
int  CHash_addStr(CHASHTABLE *Table,char *String);
int  CHash_addToArray(CHASHTABLE *Table,CHASHARRAY *LetArray,int NArray,
                      char *String,int StrInd, int Position);
int  CHash_binSearch(CHASHTABLE *Table,CHASHARRAY *Array,char *String,int *Ind, 
                     int low, int high);
void CHash_dump(CHASHTABLE *Table);
int  CHash_find(char *String,CHASHTABLE *Table,int *StrInd);
int  CHash_firstFour(char *String,int *LetInd);
int  CHash_fourLets(char *String,int *LetInd);
void CHash_free(CHASHTABLE *Table);
int  CHash_getLetInd(char *String,int *LetInd);
int  CHash_getSorted(CHASHTABLE *Table,char ***Array,int *NArray);
int  CHash_init(CHASHTABLE **Table,int Func(),int NLetter);
int  CHash_insert(char *String,CHASHTABLE *Table,int StrInd);
int  CHash_removeStr(CHASHTABLE *Table,char *String,int StrInd,int LetInd);
int  CHash_setup(char **Array,int NArray,CHASHTABLE **Table,int Func(),int NLet);


#endif /* __CHASH_H__ */
