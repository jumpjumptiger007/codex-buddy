#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#include "companion_pin_store.h"
#include "companion_auth_crypto.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
static int sync_file(int fd){if(fsync(fd))return -1;
#ifdef __APPLE__
 return fcntl(fd,F_FULLFSYNC);
#else
 return 0;
#endif
}
static bool save(companion_pin_store_t *s,bool pinned,const uint8_t *key)
{
 char temp[1040],parent[1024];snprintf(temp,sizeof(temp),"%s.XXXXXX",s->path);strcpy(parent,s->path);
 char*slash=strrchr(parent,'/');if(slash==parent)slash[1]=0;else if(slash)*slash=0;else strcpy(parent,".");
 uint8_t record[70]={ 'P','I','N','1',0 };record[4]=pinned?1:0;if(pinned)memcpy(record+5,key,65);
 int fd=mkstemp(temp);if(fd<0){s->poisoned=true;return false;}
 bool ok=s->io.write_bytes(fd,record,sizeof(record))==(ssize_t)sizeof(record) && s->io.sync_file(fd)==0;
 if(close(fd))ok=false;
 bool replaced=ok && s->io.replace_file(temp,s->path)==0;
 if(!replaced){unlink(temp);ok=false;}else{
  fd=open(parent,O_RDONLY|O_DIRECTORY);ok=fd>=0 && s->io.sync_directory(fd)==0;if(fd>=0 && close(fd))ok=false;
 }
 if(!ok)s->poisoned=true;
 return ok;
}
void companion_pin_store_close(companion_pin_store_t*s)
{if(s && s->open){close(s->lock_fd);memset(s,0,sizeof(*s));s->lock_fd=-1;}}
bool companion_pin_store_open(companion_pin_store_t*s,const char*path,const companion_generation_io_t*io)
{
 if(!s || s->open || !path || !*path || strlen(path)>=sizeof(s->path))return false;
 memset(s,0,sizeof(*s));strcpy(s->path,path);s->lock_fd=-1;
 s->io=(companion_generation_io_t){.write_bytes=write,.sync_file=sync_file,.sync_directory=fsync,.replace_file=rename};
 if(io){if(io->write_bytes)s->io.write_bytes=io->write_bytes;if(io->sync_file)s->io.sync_file=io->sync_file;if(io->sync_directory)s->io.sync_directory=io->sync_directory;if(io->replace_file)s->io.replace_file=io->replace_file;}
 char lock[1032];snprintf(lock,sizeof(lock),"%s.lock",path);bool fresh=true;
 int fd=open(lock,O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
 if(fd<0 && errno==EEXIST){fresh=false;fd=open(lock,O_RDWR|O_NOFOLLOW);}
 if(fd<0)return false;
 struct stat st;
 if(fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_uid!=geteuid() || (st.st_mode&0077) || flock(fd,LOCK_EX|LOCK_NB)){close(fd);return false;}
 s->lock_fd=fd;s->open=true;
 fd=open(path,O_RDONLY|O_NOFOLLOW);
 if(fd<0){if(fresh && errno==ENOENT && save(s,false,NULL))return true;companion_pin_store_close(s);return false;}
 uint8_t record[71];ssize_t n=read(fd,record,sizeof(record));
 bool valid=fstat(fd,&st)==0 && S_ISREG(st.st_mode) && st.st_uid==geteuid() && !(st.st_mode&0077) && n==70 && !memcmp(record,"PIN1",4) && record[4]<=1;
 if(close(fd))valid=false;
 if(valid && record[4])valid=companion_auth_public_key_valid(record+5);
 if(valid && !record[4]){for(size_t i=5;i<70;i++)if(record[i])valid=false;}
 if(!valid){companion_pin_store_close(s);return false;}
 s->pinned=record[4]!=0;if(s->pinned)memcpy(s->key,record+5,65);return true;
}
bool companion_pin_store_key(const companion_pin_store_t*s,uint8_t key[65])
{if(!s || !s->open || s->poisoned || !s->pinned || !key)return false;memcpy(key,s->key,65);return true;}
bool companion_pin_store_enroll(companion_pin_store_t*s,const uint8_t key[65],const ambient_auth_crypto_t*crypto,companion_pin_approval_fn approve,void*context)
{
 if(!s || !s->open || s->poisoned || !key || !companion_auth_public_key_valid(key))return false;
 if(s->pinned)return memcmp(s->key,key,65)==0; /* Never overwrite an existing pin. */
 char fingerprint[25];if(!approve || !ambient_auth_fingerprint(crypto,key,fingerprint) || !approve(context,key,fingerprint))return false;
 if(!save(s,true,key))return false;
 memcpy(s->key,key,65);s->pinned=true;return true;
}
static bool approve_pin(void *context,const uint8_t key[65],const char *fingerprint)
{
 (void)fingerprint;companion_pin_auth_owner_t*owner=context;
 ambient_auth_crypto_t crypto=companion_auth_crypto();
 return owner && companion_pin_store_enroll(owner->store,key,&crypto,owner->approve,owner->context);
}
ambient_auth_crypto_t companion_pin_auth_crypto(companion_pin_auth_owner_t *owner)
{ambient_auth_crypto_t crypto=companion_auth_crypto();crypto.context=owner;crypto.approve_and_pin=approve_pin;return crypto;}
