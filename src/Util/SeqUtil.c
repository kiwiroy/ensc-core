#include "SeqUtil.h"
#include "StrUtil.h"

char *SeqUtil_reverseComplement(char *seqStr, int lenSeqStr) {
  int i;

  StrUtil_ReverseString(seqStr,lenSeqStr);

  for (i=0;i<lenSeqStr;i++) {
    switch (seqStr[i]) {
      case 'A':
        seqStr[i]='T';
        break;
      case 'C':
        seqStr[i]='G';
        break;
      case 'G':
        seqStr[i]='C';
        break;
      case 'T':
        seqStr[i]='A';
        break;
      case 'N':
        seqStr[i]='N';
        break;
      case 'R':
        seqStr[i]='Y';
        break;
      case 'Y':
        seqStr[i]='R';
        break;
      case 'M':
        seqStr[i]='K';
        break;
      case 'K':
        seqStr[i]='M';
        break;
      case 'S':
        seqStr[i]='W';
        break;
      case 'W':
        seqStr[i]='S';
        break;
      case 'H':
        seqStr[i]='D';
        break;
      case 'D':
        seqStr[i]='H';
        break;
      case 'B':
        seqStr[i]='V';
        break;
      case 'V':
        seqStr[i]='B';
        break;
      default:
        fprintf(stderr,"ERROR: Failed reverse complementing\n");
        return NULL;
    }
  }
  
  return seqStr;
}
