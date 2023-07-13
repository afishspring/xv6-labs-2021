#include "kernel/types.h"
#include "user.h"

void Eratosthenes_Sieve(int fd[]){
  close(fd[1]);
  
  int num[34];
  int head;
  int len=read(fd[0],&head,sizeof(int));
  if(head==0){
    exit(0);
  }
  printf("prime %d\n",head);
  
  int p=0,temp;
  while(len>0){
    len=read(fd[0],&temp,sizeof(int));
    if(len>0){
      if(temp%head!=0){
        num[p++]=temp;
      }
    }
  }

  close(fd[0]);

  int next_fd[2];
  pipe(next_fd);
  if(fork()==0){
    Eratosthenes_Sieve(next_fd);
  }
  else{
    close(next_fd[0]);
    write(next_fd[1],&num,p*sizeof(int));
    close(next_fd[1]);
    wait(0);
    exit(0);
  }
}

int main(int argc,char* argv[]){
  int fd[2];
  pipe(fd);
  int num[34];
  for(int i=2;i<=34;i++){
    num[i-2]=i;
  }

  if(fork() == 0){
    Eratosthenes_Sieve(fd);
  }
  else{
    close(fd[0]);
    write(fd[1],num,34*sizeof(int));
    close(fd[1]);
    wait(0);
    exit(0);
  }
  return 0;
}