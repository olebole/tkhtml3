/*
** This file contains code to fetch a single URL into a local file.
*/
#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/in.h>
#include "httpget.h"

#define strnicmp strncasecmp

/*
** Get a URL using HTTP.  Return the number of errors.  Error messages
** are written to stderr.
*/
int HttpFetch(char *zUrl, char *zLocalFile, int quiet){
  int i, j;
  int nErr = 0;             /* Number of errors */
  char *zPath;              /* Pathname to send as second argument to GET */
  int iPort;                /* TCP port for the server */
  struct hostent *pHost;    /* Name information */
  int s;                    /* The main communications socket */
  int c;                    /* A character read from the remote side */
  FILE *sock;               /* FILE corresponding to file descriptor s */
  FILE *out;                /* Write output here */
  int last_was_nl;          /* TRUE if last character received was '\n' */
  struct sockaddr_in addr;  /* The address structure */
  int nByte = 0;
  char zIpAddr[400];        /* The IP address of the server to pint */
  char zMsg[1000];          /* Space to hold error messages */

  out = fopen(zLocalFile, "w");
  if( out==0 ){
    sprintf(zMsg, "can't open output fule \"%.100s\"", zLocalFile);
    perror(zMsg);   
    return 1;
  }

  i = 0;
  if( strnicmp(zUrl,"http:",5)==0 ){ i = 5; }
  while( zUrl[i]=='/' ){ i++; }
  j = 0;
  while( zUrl[i] && zUrl[i]!=':' && zUrl[i]!='/' ){
    if( j<sizeof(zIpAddr)-1 ){ zIpAddr[j++] = zUrl[i]; }
    i++;
  }
  zIpAddr[j] = 0;
  if( zUrl[i]==':' ){
    iPort = 0;
    i++;
    while( isdigit(zUrl[i]) ){
      iPort = iPort*10 + zUrl[i] - '0';
      i++;
    }
  }else{
    iPort = 80;
  }
  zPath = &zUrl[i];

  addr.sin_family = AF_INET;
  addr.sin_port = htons(iPort);
  *(int*)&addr.sin_addr = inet_addr(zIpAddr);
  if( -1 == *(int*)&addr.sin_addr ){
    pHost = gethostbyname(zIpAddr);
    if( pHost==0 ){
      fprintf(stderr,"can't resolve host name: %s\n",zIpAddr);
      return 1;
    }
    memcpy(&addr.sin_addr,pHost->h_addr_list[0],pHost->h_length);
    if( !quiet ){
      fprintf(stderr,"Address resolution: %s -> %d.%d.%d.%d\n",zIpAddr,
              pHost->h_addr_list[0][0]&0xff,
              pHost->h_addr_list[0][1]&0xff,
              pHost->h_addr_list[0][2]&0xff,
              pHost->h_addr_list[0][3]&0xff);
    }
  }
  s = socket(AF_INET,SOCK_STREAM,0);
  if( s<0 ){
    sprintf(zMsg,"can't open socket to %.100s", zIpAddr);
    perror(zMsg);
    return 1;
  }
  if( connect(s,(struct sockaddr*)&addr,sizeof(addr))<0 ){
    sprintf(zMsg,"can't connect to host %.100s", zIpAddr);
    perror(zMsg);
    return 1;
  }
  sock = fdopen(s,"r+");
  if( *zPath==0 ) zPath = "/";
  fprintf(sock,"GET %s HTTP/1.0\n\n",zPath);
  fflush(sock);
  while( (c=getc(sock))!=EOF && (c!='\n' || !last_was_nl) ){
    if( c=='\r' ) continue;
    last_was_nl = (c=='\n');
  }
  if( !quiet ){
    fprintf(stderr, "Reading %s...", zUrl);
  }
  while( (c=getc(sock))!=EOF ){
    nByte++;
    putc(c,out);
  }
  if( !quiet ){
    fprintf(stderr, " %d bytes\n", nByte);
  }
  fclose(sock);
  fclose(out);
  return 0;
}
