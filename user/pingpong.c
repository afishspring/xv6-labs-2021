#include "kernel/types.h"
#include "user/user.h"

int main(int argc,char* argv[]){
  int parent_fd[2];
  int child_fd[2];
  pipe(parent_fd);
  pipe(child_fd);
  char buf;

  if(fork()==0){
    close(parent_fd[1]);
    close(child_fd[0]);
    
    read(parent_fd[0],&buf,sizeof(char));
    printf("%d: received ping\n",getpid());

    buf='7';
    write(child_fd[1],&buf,1);

    exit(0);
  }
  else{
    close(parent_fd[0]);
    close(child_fd[1]);

    buf='8';
    write(parent_fd[1],&buf,1);
    
    read(child_fd[0],&buf,sizeof(char));
    printf("%d: received pong\n",getpid());
    
    exit(0);
  }

}