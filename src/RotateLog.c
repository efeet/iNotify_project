#include "IncludeLibraries.h"

int logexist (char *filename)
{
  struct stat   buffer;   
  return (stat (filename, &buffer) == 0);
}

FILE *rotatelog(char logpath[PATH_MAX], FILE *logfp, int logsizelimit){
  char rotate[PATH_MAX];
  char rotatelog1[PATH_MAX];
  char rotatelog2[PATH_MAX];
  char rotatelog3[PATH_MAX];
  
  logsizelimit = logsizelimit * 1024 * 1024;
  
  struct stat st;
  
  if(lstat(logpath, &st) == -1){
      printf("Error de acceso a log\n");
      exit(1);
  }
  
  strcpy(rotate, logpath);
  
  strcpy(rotatelog1, logpath);
  strcat(rotatelog1, ".1");
  strcpy(rotatelog2, logpath);
  strcat(rotatelog2, ".2");
  strcpy(rotatelog3, logpath);
  strcat(rotatelog3, ".3");
  
  if(st.st_size >= logsizelimit){
    if(logexist(rotatelog1)){
      if(logexist(rotatelog2)){
	if(logexist(rotatelog3)){
	  remove(rotatelog3);
	  rename(rotatelog2, rotatelog3);
	  rename(rotatelog1, rotatelog2);
	  rename(logpath, rotatelog1);
	}
	else{
	  rename(rotatelog2, rotatelog3);
	  rename(rotatelog1, rotatelog2);
	  rename(logpath, rotatelog1);
	}
      }
      else{
	rename(rotatelog1, rotatelog2);
	rename(logpath, rotatelog1);
      }
    }
    else{
      rename(logpath, rotatelog1);
    }
  }
  logfp = freopen(logpath, "a+", stdout);
  return(logfp);
}