#ifndef __STRUTIL_H__
#define __STRUTIL_H__

#include <stdio.h>

char *StrUtil_CopyString(char *str);
char *StrUtil_AppendString(char *to, char *from);
void StrUtil_ReverseString(char *string, int len);


#endif
