#include "kernel/types.h"
#include "user/user.h"
#include "kernel/stat.h"
#include "kernel/fs.h"

char* strrchr(char* path){
  char* p = path+strlen(path);
  while(p>=path){
    if(*p == '/'){
      break;
    }
    p--;
  }
  p++;
  static char buf[DIRSIZ+1];
  memmove(buf, p, strlen(p));
  buf[strlen(p)] = '\0';
  return buf;
}

void cmp(char* a,char*b){
  if(strcmp(strrchr(a),b)==0){
    printf("%s\n", a);
  }
}

void find(char* path,char* target){
  struct stat st;
  struct dirent dir;
  int fd;
  char path_buf[512];
  char *p_buf;

  if((fd=open(path,0))<0){
    fprintf(2,"find: cannot open %s\n",path);
    return;
  }
  if(fstat(fd,&st)<0){
    fprintf(2,"find: cannot stat %s\n",path);
    close(fd);
    return;
  }

  switch(st.type){
    case T_FILE:
      cmp(path,target);
      break;
    case T_DIR:
      if(strlen(path) + 1 + DIRSIZ + 1 > sizeof(path_buf)){
        printf("find: path too long\n");
        break;
      }
      strcpy(path_buf,path);
      p_buf=path_buf+strlen(path_buf);
      *p_buf++ = '/';
      while(read(fd,&dir,sizeof(dir))==sizeof(dir)){
        if(dir.inum==0 || strcmp(dir.name,".")==0 || strcmp(dir.name,"..")==0){
          continue;
        }
        memmove(p_buf,dir.name,strlen(dir.name));
        p_buf[strlen(dir.name)]='\0';
        find(path_buf,target);
      }
      break;
  }
  close(fd);
}

int main(int argc,char* argv[]){
  if(argc<3){
    fprintf(2,"Please input the right arg!\n");
    exit(-1);
  }
  find(argv[1],argv[2]);
  exit(0);
}