#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "companion_generation_store.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static int fault;
static ssize_t injected_write(int fd,const void*b,size_t n){return fault==1?-1:write(fd,b,n);}
static int injected_sync(int fd){return fault==2?-1:fsync(fd);}
static int injected_dir(int fd){return fault==3?-1:fsync(fd);}
static int injected_replace(const char*a,const char*b){return fault==4?-1:rename(a,b);}
static int injected_rng(void*p,size_t n){if(fault==5)return -1;memset(p,7,n);return 0;}
static void remove_test(const char *path){char lock[1100];snprintf(lock,sizeof(lock),"%s.lock",path);assert(unlink(path)==0);assert(unlink(lock)==0);}
int main(void){
 char directory[]="/tmp/codex-generation-test.XXXXXX";assert(mkdtemp(directory));char path[1024];snprintf(path,sizeof(path),"%s/epoch",directory);
 companion_generation_store_t store={0}, other={0};uint64_t first,next;
 assert(companion_generation_store_open(&store,path,NULL));assert(!companion_generation_store_open(&other,path,NULL));
 pid_t child=fork();assert(child>=0);if(child==0){other=(companion_generation_store_t){0};_exit(companion_generation_store_open(&other,path,NULL)?1:0);}int status;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
 assert(ambient_generation_next(&store.backend,&first));companion_generation_store_close(&store);
 assert(companion_generation_store_open(&store,path,NULL));assert(ambient_generation_next(&store.backend,&next)&&next>first);companion_generation_store_close(&store);
 /* Missing state with an existing owner marker must never reset the epoch. */
 assert(unlink(path)==0);assert(!companion_generation_store_open(&store,path,NULL));char lock[1100];snprintf(lock,sizeof(lock),"%s.lock",path);assert(unlink(lock)==0);
 /* Corrupt state is not a new store, even with a fresh owner marker. */
 FILE*f=fopen(path,"wb");assert(f);assert(fwrite("bad",1,3,f)==3);assert(fclose(f)==0);assert(!companion_generation_store_open(&store,path,NULL));remove_test(path);
 companion_generation_io_t ops={injected_write,injected_sync,injected_dir,injected_replace,injected_rng};
 for(int i=1;i<=5;i++){
  fault=0;assert(companion_generation_store_open(&store,path,&ops));assert(ambient_generation_next(&store.backend,&first));fault=i;
  next=123;assert(!ambient_generation_next(&store.backend,&next)&&next==0);if(i<5)assert(store.poisoned);
  fault=0;companion_generation_store_close(&store);assert(companion_generation_store_open(&store,path,NULL));assert(ambient_generation_next(&store.backend,&next)&&next>first);companion_generation_store_close(&store);remove_test(path);
 }
 assert(companion_generation_store_open(&store,path,NULL));companion_generation_store_close(&store);
 int fd=open(path,O_WRONLY|O_TRUNC);assert(fd>=0);const unsigned char exhausted[]={ 'G','E','N','1',255,255,255,255 };assert(write(fd,exhausted,8)==8);assert(close(fd)==0);
 assert(companion_generation_store_open(&store,path,NULL));assert(!ambient_generation_next(&store.backend,&next)&&next==0);companion_generation_store_close(&store);companion_generation_store_close(&store);remove_test(path);assert(rmdir(directory)==0);
 puts("R3 production Companion generation store: PASS (real files/lock/restart/CSPRNG + injected I/O faults)");
}
