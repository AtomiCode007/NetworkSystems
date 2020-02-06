/* 
 * tcpechosrv.c - A concurrent TCP echo server using threads
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>      /* for fgets */
#include <strings.h>     /* for bzero, bcopy */
#include <unistd.h>      /* for read, write */
#include <sys/socket.h>  /* for socket use */
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAXLINE  8192  /* max text line length */
#define MAXBUF   8192  /* max I/O buffer size */
#define LISTENQ  1024  /* second argument to listen() */

int open_listenfd(int port);
void echo(int connfd);
void *thread(void *vargp);
const char delimit[6] = " \n\0";// Delimiter values for the string tokenizer.

int main(int argc, char **argv) 
{
    int listenfd, *connfdp, port, clientlen=sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid; 

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(0);
    }
    port = atoi(argv[1]);

    listenfd = open_listenfd(port);
    while (1) {
	connfdp = malloc(sizeof(int));
	*connfdp = accept(listenfd, (struct sockaddr*)&clientaddr, (socklen_t *) &clientlen);
	pthread_create(&tid, NULL, thread, connfdp);
    }
}

char *get_filename_ext(const char *filename) {
    char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) 
    {
        return "";
    }
    else if(strncmp(dot,".gif",4) == 0)
    {
        dot = " image/gif";
    }
    else if(strncmp(dot,".png",4) == 0)
    {
        dot = " image/png";
    }    
    else if(strncmp(dot,".jpg",4) == 0)
    {
        dot = " image/jpg";
    }
    else if(strncmp(dot,".txt",4) == 0)
    {
        dot = " text/plain";
    }
    else if(strncmp(dot,".html",5) == 0)
    {
        dot = " text/html";
    }
    else if(strncmp(dot,".css",4) == 0)
    {
        dot = " text/css";
    }
    else if(strncmp(dot,".js",3) == 0)
    {
        dot = " application/javascript";
    }
    return dot + 1;
}


/* thread routine */
void * thread(void * vargp) 
{  
    int connfd = *((int *)vargp);
    pthread_detach(pthread_self()); 
    free(vargp);
    echo(connfd);
    close(connfd);
    return NULL;
}

/*
 * echo - read and echo text lines until client closes connection
 */
void echo(int connfd) 
{
    size_t n;  
    FILE *FilePt; //A file pointer
    char* command; //Command string token
    char* Filename; //Filename
    char* fileType; //HTML file responce
    char buf[MAXLINE]; 
    char fileBuf[MAXLINE];
    char fileSzS [32];
    char fileTy[30];
    char httpmsg[200]="HTTP/1.1 200 Document Follows\r\nContent-Type:";
    int fileSize;
    //char httpmsg[]="HTTP/1.1 200 Document Follows\r\nContent-Type:text/html\r\nContent-Length:%d\r\n\r\n"; 
    int pkSz = 0;

    n = read(connfd, buf, MAXLINE);
    printf("server received the following request:\n%s\n",buf);
    printf("BUF len: %lu\n",strlen(buf));
    if(strlen(buf) != 0)
    {
        command = strtok(buf,delimit);//Tokenize the received buffer
        Filename = strtok(NULL, delimit);
        printf("Type of request: %s\n",command);
        printf("URI: %s\n", Filename);
    
        if(strncmp(command,"GET",3) == 0 && Filename != NULL)
        {
            //printf("Type of request: %s\n",command);
            
            if(strncmp(Filename,"/",2) == 0)
            {
                Filename = "index.html"; //If there is no directory requested, then default to the homepage.
            }
            else
            {
                Filename++; //Else, remove the '/' from the request to actually open the file.
            }
            FilePt = fopen(Filename,"rb");

            fileType = get_filename_ext(Filename); //helper function to get the HTML file extension
            printf("File TYPE: %s \n \n",fileType); 

            if(FilePt != NULL)
            {
                //get the file size, same as usual.
                fseek(FilePt,0,SEEK_END);
                fileSize = ftell(FilePt);
                rewind(FilePt);
                sprintf(fileSzS,"%d",fileSize); //Place the file size into the HTML response message
            }
            
            strcpy(fileTy,fileType);//Gather all the bits and pieces of the respose string together.
            strcat(httpmsg,fileTy);
            strcat(httpmsg,"\r\nContent-Length:");
            strcat(httpmsg,fileSzS);
            strcat(httpmsg,"\r\n\r\n");
            //printf("HTTP MESSAGE: %s \n\n\n\n",httpmsg);
            //printf("URI: %s\n", Filename);
            //printf("File check\n");

            if(FilePt !=  NULL)
            {
                strcpy(buf,httpmsg);
                printf("server returning a http message with the following content.\n%s\n",buf);
                write(connfd, buf,strlen(buf));// Send the respose back to the client, in order to properly stream the file.
                while((pkSz = fread(fileBuf,1,MAXLINE-1,FilePt)) > 0)
                {
                    if(send(connfd, fileBuf, pkSz, 0) < 0)//Begin streaming the file 
                    {
                        printf("ERROR: Failed to send file %s.\n", Filename);//If there is an error in sending the file, attempt to send this 500 error.
                        strcpy(buf,"HTTP/1.1 500 Internal Server Error\n");
                        write(connfd,buf,strlen(buf));
                        //exit(1);
                    }
                    bzero(fileBuf,MAXLINE);
                    //printf("File read %d\n",pkSz);
                }
                //write(connfd, fileBuf,strlen(httpmsg));"""""""""\r\nContent-Length:%d\r\n\r\n";
            }
            else
            {
                strcpy(buf,"HTTP/1.1 500 Internal Server Error\n");//Error respose

                write(connfd,buf,strlen(buf));
            }
        }
    }
    else
    {
        strcpy(buf,"HTTP/1.1 500 Internal Server Error\n");

        write(connfd,buf,strlen(buf));
    }
}



/* 
 * open_listenfd - open and return a listening socket on port
 * Returns -1 in case of failure 
 */
int open_listenfd(int port) 
{
    int listenfd, optval=1;
    struct sockaddr_in serveraddr;
  
    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
                   (const void *)&optval , sizeof(int)) < 0)
        return -1;

    /* listenfd will be an endpoint for all requests to port
       on any IP address for this host */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET; 
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    serveraddr.sin_port = htons((unsigned short)port); 
    if (bind(listenfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0)
        return -1;
    return listenfd;
} /* end open_listenfd */

