#pragma once
#include <stdbool.h>
#include <stddef.h>
/* ASCII only. Canonical uppercase exactly matches ambient_auth_fingerprint. */
static inline bool r3_fingerprint_normalize(const char *input, char output[25])
{
    if(!input || !output)return false;
    for(size_t i=0;i<24;i++){
        unsigned char c=(unsigned char)input[i];
        if(c>='a'&&c<='f')c=(unsigned char)(c-'a'+'A');
        if(!((c>='0'&&c<='9')||(c>='A'&&c<='F')))return false;
        output[i]=(char)c;
    }
    if(input[24])return false;
    output[24]=0;return true;
}
