/*CS273 Opertating Systems Project

MULTITHREADED HTTP SERVER

by Bidit Sharma (submited to Prof. Richard Brown) 
April 16, 2018

 Server-side use of Berkeley socket calls -- receive one message and print 
   Requires one command line arg:  
     1.  port number to use (on this machine). 
*/
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <pthread.h>
#include "work_queue.h"


/*defining constants*/
#define MAXBUFF 256
#define TOK_BUFSIZE 64
#define LOGFILE "log.txt"
#define NWORKERS 2


/*some global variables*/
time_t now;
char timestamp[30];

int csockd;
int server_status;

/*defining enums for HTTP request types*/
enum requests{
  GET = 1,
  HEAD = 2,
  DELETE = 3
};

/*defining enums for return status*/
enum status_value {
  OK = 200,
  BAD_REQUEST = 400,
  NOT_FOUND = 404,
  NOT_IMPLEMENTED = 501,
  SERVICE_UNAVAILABLE = 503
  };

/*client request
  tok: tokens
  count: no. of tokens
  status: status of response*/
struct request {
  char** tok;
  int count;
  int status;
};


/*getTokens
  Arguments: line: null terminated string
  Status change: breaks the string into tokens
  Return: an array of tokents (null terminated strings)*/
char **getTokens(char * line){
  int bufsize = TOK_BUFSIZE, position = 0;
  char **tokens = (char**) malloc(bufsize * sizeof(char*));   //allocating the space for tokens
  char *token;

  if (!tokens) {
    fprintf(stderr, "lsh: allocation error\n");
    server_status = -1;
    return NULL;
  }

  token = strtok(line, " \t\r\n\a");
  while (token != NULL) {
    tokens[position] = token;
    position++;

  /*implemented if no. of tokents exceed the max no. of tokens*/
    if (position >= bufsize) {  
      bufsize += TOK_BUFSIZE;
      tokens = (char **)realloc(tokens, bufsize * sizeof(char*));
      if (!tokens) {
        fprintf(stderr, "lsh: allocation error\n");
        server_status = -1;
        return NULL;
      }
    }

    token = strtok(NULL, " \t\r\n\a");
  }
  tokens[position] = NULL;
  return tokens;
}

/*read_request
  Arguments: str: pointer to request struct
             buff: null terminated string
             length: integer, length of buff
  status change: reads and interprets the HTTP request type
  return: integer, status value of the request*/
int read_request(struct request * str, char * buff, int length){
  str->tok = getTokens(buff);  //call getTokens

  if (str->tok == NULL){
    str->status = SERVICE_UNAVAILABLE;  //if tokens can't be parsed
    return;
  }

  /*setting return for different request methods, status is stored in the request status struct*/
  if (!strcmp(str->tok[0], "GET") && !strcmp(str->tok[2], "HTTP/1.1") && (str->tok[3] == NULL || !strcmp(str->tok[3], "Host:")))
    str->status = GET;
  else if (!strcmp(str->tok[0], "HEAD") && !strcmp(str->tok[2], "HTTP/1.1") && (str->tok[3] == NULL || !strcmp(str->tok[3], "Host:")))
    str->status = HEAD;
  else if (!strcmp(str->tok[0], "DELETE") && !strcmp(str->tok[2], "HTTP/1.1") && (str->tok[3] == NULL || !strcmp(str->tok[3], "Host:")))
    str->status = DELETE;
  else
    str->status = BAD_REQUEST;  //BAD_REQUEST if request isn't recognized
  return str->status;  
}

/*send_responses
  Arguments: response: integer, denotes response type/return status
             tData: tdata struct, stores thread data
             contentLength, stores length of content following the header
             writelog: int, 1 = update the log file, 0 = don't update
  status change: prerares and send a header file best on the type of return to the HTTP request made by the client
  no return*/
void send_responses(int response, tdata * tData, int contentLength, int writelog){
  char buff[1024];
  /*creating first line of header*/
  switch (response) {
    case OK:
    sprintf (buff, "HTTP/1.1 200 OK\n");
    break; 

    case BAD_REQUEST:
    sprintf (buff, "HTTP/1.1 400 BAD_REQUEST\n");
    break; 

    case NOT_FOUND:
    sprintf (buff, "HTTP/1.1 400 NOT_FOUND\n");
    break;

    case NOT_IMPLEMENTED:
    sprintf (buff, "HTTP/1.1 501 NOT_IMPLEMENTED\n");
    break; 

    case SERVICE_UNAVAILABLE:
    sprintf (buff, "HTTP/1.1 503 SERVICE_UNAVAILABLE\n");
    break; 
  }

  /*writing log if writelog=1*/
  if (writelog == 1){
    if (tData->logFile != NULL)
        fprintf(tData->logFile, "%d \t %s \n", tData->request_id, buff);
  }
  /*prerating timestamp*/
  if ((strftime(timestamp, 30, "%a, %d %b %Y %T %Z", gmtime(&now))) ==0)
    strcpy(timestamp, "no-time-record");
  
  /*preparing the header*/
  sprintf(buff, "%s\
Date: %s\n\
Connection: close\n\
Content-Type:  text/html; charset=utf-8\n\
Content Length: %d\n", buff, timestamp, contentLength);
  int ret;
  if ((ret = send(tData->clientd, buff, sizeof(buff), 0)) < 0)  //send response to the client
        perror("send()");
  return;
}


/*shutdown and close
argument: comm_id:int, file descriptor of the connection
status change: shuts down and closes the socket connection
no return */
void shutdown_and_close(int comm_id){
  int ret;
  if ((ret = shutdown(comm_id, SHUT_RDWR)) < 0) {
    perror("shutdown(comm_id)");
    return;
  }

  if ((ret = close(comm_id)) < 0) {
    perror("close(comm_id)");
    return;
  }
}

/*close connection:
argument: th_data: tdata struct
wraps shutdown_and_close function to call it when tdata struct is passed*/
void close_connections(tdata * th_data){
    int id = th_data->clientd;
    shutdown_and_close(id);
}


/*filesize
argument: fd, file descriptor of open file
status change: finds the size of the file
return: int, filesize*/
int filesize(FILE * fd){
  fseek(fd, 0, SEEK_END);
  int size = ftell(fd);
  fseek(fd, 0, SEEK_SET);
  return size;
}

/*respond_GET
argument: cl_request: struct request, client request information
          t_data: tdata, thread data
status change: responds to valid GET request by client
no return*/ 
void respond_GET(struct request * cl_request, tdata * t_data){
  char * buffer = NULL;
  size_t bufflen = 0;
  int status;
  FILE * sourceFile;
  int nchars;
  int ret;

  sourceFile = fopen(cl_request->tok[1], "r");  //open the file
      
  if (sourceFile == NULL){
    status = NOT_FOUND;
    send_responses(status, t_data, 0, 1);
    perror ("Error opening file");
  }

  else {
  status = OK;
  // int size = filesize(sourceFile);
  send_responses(status,t_data, filesize(sourceFile), 1);  //send the header
   while ((nchars = getline(&buffer, &bufflen, sourceFile)) >= 0){  //send the file
    printf ("%s", buffer);
    if ((ret = send(t_data->clientd, buffer, nchars, 0)) < 0){
      perror("send()");
      continue;
    }
   }
   bufflen = 0;
   fclose(sourceFile);
  }
}

/*respond_HEAD
argument: cl_request: struct request, client request information
          t_data: tdata, thread data
status change: responds to valid HEAD request by client
no return*/ 
void respond_HEAD(struct request * cl_request, tdata * t_data){
  FILE * sourceFile = fopen(cl_request->tok[1], "r");
  int status = OK;
  send_responses(status,t_data, filesize(sourceFile), 1);  //send the header
  fclose(sourceFile);
}

/*respond_DELETE
argument: cl_request: struct request, client request information
          t_data: tdata, thread data
status change: responds to valid DELETErequest by client
no return*/ 
void respond_DELETE(struct request * cl_request, tdata * t_data){
  if (remove(cl_request->tok[1]) == 0)
    send_responses(OK,t_data, 0, 1);  //send the header if file deleted
  else
    send_responses(NOT_IMPLEMENTED,t_data, 0, 1);  //send the header else
}


//============================================================================

/*process request
Argument: pArg: contains thread_id number
status change: gets client information from the queue, and implements the HTTP request,
              multiple threads run process_request function at an instant
no return*/
void *process_requests(void * pArg){
  int thread_id = *((int *)pArg);
  int ret;  /* return value from a call */
  char buff[MAXBUFF];  /* message buffer */
  size_t bufflen;
  int nchars;
  int status;

  while (csockd != -1 || server_status != -1){  //run until user asks to terminate
    tdata *th_data = removeq();   //it will wait to have at least one element in the queue because of the mutex locks in removeq()

    if (th_data == 0){ 
      status = BAD_REQUEST;  //can't forward this because there is no client information 
      continue;
    }
  
    if ((ret = recv(th_data->clientd, buff, MAXBUFF-1, 0)) < 0) {  //recieve the string/request from client
      printf("%s ", th_data->prog);
      perror("recv()");
      status = NOT_IMPLEMENTED;
      continue;
    }

    buff[ret] = '\0';  // add terminating nullbyte to received array of char

      /*writing log*/
    if (th_data->logFile != NULL){
      now = time(NULL);
      if ((strftime(timestamp, 30, "%a, %d %b %Y %T %Z", gmtime(&now))) ==0)
        strcpy(timestamp, "no-time-record");
      fprintf(th_data->logFile, "%d \t %d \t %s \n", th_data->request_id, thread_id, timestamp);
      fprintf(th_data->logFile, "%d \t %s \n", th_data->request_id, buff);
    }

    /*safely close connections and stop the thread if user calls terminate*/
    if (csockd == -1 || server_status == -1){  
      send_responses(SERVICE_UNAVAILABLE, th_data, 0 , 1);  //SERVICE_UNAVIALABLE response sent to clients that were connected after server termination was called
      close_connections(th_data);

      free(th_data);
      return NULL;
    }

    printf("Thread:%d \nReceived message (%d chars):\n%s\n", thread_id, ret, buff);

    /*protecting against single charater request while can cause problems while parsing tokens*/
    if (ret < 2){
      send_responses(BAD_REQUEST, th_data, 0 , 1);  //sebd header for bad_request
      close_connections(th_data);
      free(th_data);
      continue;  //skip to next iteration
    }

    /* My implementation starts here */
    struct request client_request;
    int request_ret = read_request(&client_request, buff, ret+1);

    /*responding to different requests by calling the respective functions*/
    if (request_ret == GET)
      respond_GET(&client_request, th_data);
    else if (request_ret == HEAD)
      respond_HEAD(&client_request, th_data);
    else if (request_ret == DELETE)
      respond_DELETE(&client_request, th_data);
    /*if request not recognized, send header with BAD_REQUEST*/
    else {
      status = request_ret;
      send_responses(status,th_data, 0, 1);
    }
    /*close the client connection after sending reply/before the end of iteration in loop*/
    close_connections(th_data);

    /*clearing the allocated space*/
    free(th_data);
  }
  return NULL;
}

/*termination_thread
argument: sd:int, file descriptor to server's socket connection info, not used
status change: runs in the background as a thread while the programming is running, waits for terminate command (X keystroke) by user
updates variable that signal threads to stop
no return*/
void *termination_thread (void* pArg){
  int sd = *((int*)pArg);
  char keystroke;
  int i = 0;
  int ret;
  while (keystroke != 'X'){
    scanf("%s",&keystroke);
  }

  printf("Terminating Server ...\n");
  csockd = -1;
  server_status = -1;
  return;

}

/*main*/
int main(int argc, char **argv) {
  char *prog = argv[0];
  int port;
  int serverd;  /* socket descriptor for receiving new connections */

  /*log file*/
  FILE * logFile;

  logFile = fopen("log.txt","a");
  if (logFile == NULL){    //if logFile != NULL, it is open and will accept file operations
    perror ("Error opening log file");
  }

  if (argc < 2) {
    printf("Usage:  %s port\n", prog);
    return 1;
  }
  port = atoi(argv[1]);

  if ((serverd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("%s ", prog);
    perror("socket()");
    return 1;
  }
  
  struct sockaddr_in sa;
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  sa.sin_addr.s_addr = INADDR_ANY;

  if (bind(serverd, (struct sockaddr*) &sa, sizeof(sa)) < 0) {
    printf("%s ", prog);
    perror("bind()");
    return 1;
  }
  if (listen(serverd, 5) < 0) {
    printf("%s ", prog);
    perror("listen()");
    return 1;
  }
 
  int clientd;  /* socket descriptor for communicating with client */
  struct sockaddr_in ca;
  int size = sizeof(struct sockaddr);
  int request_id = 0;
  int ret = 0; 
  int connections = 0;


  pthread_t handles[NWORKERS];  //thread's descriptors
  pthread_t keythread;  //descriptor for thread that waits for termination command
  
  /*initializing id*/
  int id[NWORKERS];

  int i;
  server_status = 1;
  for (i=0; i<NWORKERS; i++){
    id[i] = i;
  }

  char cmd;
  printf ("Enter X to terminate the server: \n");
 
    /*initializing work_queue and mutex variables*/
  work_queue.start = 0;
  work_queue.end = &work_queue.start;
  pthread_mutex_init(&qMutex, NULL);
  pthread_cond_init(&qEmpty, NULL);  


  /*creating process_request threads NWORKER times*/
  for (i = 0; i<NWORKERS; i++){
    if (pthread_create(&handles[i], NULL, process_requests, &id[i])){
      printf("Error creating pthread ..\n");
      perror("pthread_create(&handles[i]");
      return 1;
    }
  }

  /*creating thread that waits for termination command*/
  if (pthread_create(&keythread, NULL, termination_thread,&serverd)){         //(void*)handles)){
      printf("Error creating pthread ..\n");
      perror("pthread_create(&keythread)");
      return 1;
  }

  /*checks and wait until all the thereads are properly terminated before terminating the service*/
  int cycles_before_exit = 0;  //each thread need to get past the recieve wait, so giving them thread_data for NWORKER times so that they get to unblock themself to return
  
  /*loop runs until termination has been called by user and all open threads can return properly*/
  while( cycles_before_exit != NWORKERS){
    printf("Waiting for a incoming connection...\n");
    if ((clientd = accept(serverd, (struct sockaddr*) &ca, &size)) < 0) {
      printf("%s ", prog);
      perror("accept()");
      break;
    }

    /*thread_data to store client and other information, to be passed to functions*/
    tdata * thread_data;
    thread_data = (tdata*)malloc(sizeof(tdata));

    connections = connections + 1;

    request_id = request_id + 1;

    thread_data->clientd = clientd;
    thread_data->prog = prog;
    thread_data->port = port;
    thread_data->request_id = request_id;
    thread_data->logFile = logFile;

    /*initialize termination process if asked by user, gives chances to worker thread to return properly*/
    if (server_status != 1){
      cycles_before_exit = cycles_before_exit + 1;  //allowing the threads to close properly
    }

    /*add thread_data to the queue that is picked by the threads*/
    addq(thread_data);

    printf("Connections : %d\n", connections);
  }

  if (logFile != NULL)
    fclose(logFile);

  /*after termination is called, termination_thread function thread is joined*/
  if(pthread_join(keythread, NULL))
    printf("Error joining pthread ..\n");

printf("Terminate threads joined! \n");

  /*joining process_request threads after termination is called*/
  for (i=0; i<NWORKERS; i++){
    if(pthread_join(handles[i], NULL))
      printf("Error joining pthread ..\n");
  }

printf("Threads joined! \n");

/*close the open connection from server*/
shutdown_and_close(serverd);

pthread_mutex_destroy(&qMutex);
pthread_cond_destroy(&qEmpty);

  return 0;
}


