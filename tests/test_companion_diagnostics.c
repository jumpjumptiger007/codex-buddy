#include "companion_diagnostics.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    companion_diagnostics_t d={0}; char output[128]; size_t n;
    d.accepted=UINT32_MAX; companion_diagnostic_increment(&d.accepted);
    assert(d.accepted==UINT32_MAX);
    assert(companion_diagnostics_encode(&d,output,sizeof(output),&n));
    assert(n<128 && !strncmp(output,"D1|FFFFFFFF",11));
    for(size_t j=0;j<n;j++) assert(output[j]=='|' || (output[j]>='0'&&output[j]<='9') || (output[j]>='A'&&output[j]<='F'));
    assert(!strstr(output,"prompt")&&!strstr(output,"session")&&!strstr(output,"command"));
    assert(!companion_diagnostics_encode(&d,output,2,&n) && n==0 && output[0]==0);
    d.queued=9; assert(!companion_diagnostics_encode(&d,output,sizeof(output),&n));
    assert(output[0]==0);
    puts("diagnostics actual representation tests: PASS");
}
