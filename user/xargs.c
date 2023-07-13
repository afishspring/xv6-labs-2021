#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"
#define MAXCHAR 512

enum char_type{
  C_SPACE,
  C_LINE_END,
  C_CHAR
};

enum char_type getCharType(char ch){
  switch(ch){
    case ' ':  return C_SPACE;
    case '\n': return C_LINE_END;
    default:   return C_CHAR;
  }
}

enum state{
  S_WAIT,
  S_ARG,
  S_ARG_END,
  S_ARG_LINE_END,
  S_LINE_END,
  S_END
};

enum state transState(enum state cur, enum char_type ct){
  switch (cur) {
    case S_WAIT:
      if (ct == C_SPACE)    return S_WAIT;
      if (ct == C_LINE_END) return S_LINE_END;
      if (ct == C_CHAR)     return S_ARG;
      break;
    case S_ARG:
      if (ct == C_SPACE)    return S_ARG_END;
      if (ct == C_LINE_END) return S_ARG_LINE_END;
      if (ct == C_CHAR)     return S_ARG;
      break;
    case S_ARG_END:
    case S_ARG_LINE_END:
    case S_LINE_END:
      if (ct == C_SPACE)    return S_WAIT;
      if (ct == C_LINE_END) return S_LINE_END;
      if (ct == C_CHAR)     return S_ARG;
      break;
    default: break;
    }
  return S_END;
}

int main(int argc,char* argv[]){
  if(argc-1>=MAXARG){
    fprintf(2, "xargs: too many arguments.\n");
    exit(1);   
  }

  char *xargv[MAXARG] = {0};

  for (int i = 1; i < argc; ++i) {
    xargv[i - 1] = argv[i];
  }

  enum state state=S_WAIT;

  int arg_cnt=argc-1;
  int word_beg=0;
  int word_end=0;

  char line[MAXCHAR];
  char *p=line;

  while(state!=S_END){
    if(read(0,p,sizeof(char))!=sizeof(char)){
      state = S_END;
    }
    else{
      state = transState(state,getCharType(*p));
    }

    if(++word_end>=MAXCHAR){
      fprintf(2, "xargs: arguments too long.\n");
      exit(1);
    }

    switch(state){
      case S_WAIT:
        word_beg++;
        break;
      case S_ARG_END:
        xargv[arg_cnt++]=&line[word_beg];
        word_beg=word_end;
        *p='\0';
        break;
      case S_ARG_LINE_END:
        xargv[arg_cnt++]=&line[word_beg];
      case S_LINE_END:
        word_beg=word_end;
        *p='\0';
        if(fork()==0){
          exec(argv[1],xargv);
        }
        arg_cnt = argc-1;
        for(int i=arg_cnt;i<MAXARG;i++){
          xargv[i]=0;
        }
        wait(0);
        break;
      default: break;
    }
    ++p;
  }
  exit(0);
}