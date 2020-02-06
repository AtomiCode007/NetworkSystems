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
#include <errno.h>
#include <CommonCrypto/CommonDigest.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#define MAXLINE  8192  /* max text line length */
#define MAXBUF   8192  /* max I/O buffer size */
#define LISTENQ  1024  /* second argument to listen() */

int open_listenfd(int port);
void echo(int connfd);
void *thread(void *vargp);
const char delimit[6] = " \n\0";// Delimiter values for the string tokenizer.
char Blist[15][60];
char IPcache[80][240];
int IPCind = 0;
int TimeOut = 69;

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    //exit(0);
}

void blackList()
{
    FILE* blackL;
    size_t streamlen;
    char *bList;
    int i = 0;
    
    blackL = fopen("blacklist.txt","r");
    if(blackL != NULL)
    {
        while(getline(&bList,&streamlen,blackL) > 0)
        {   
            bList = strtok(bList,delimit);
            sprintf(Blist[i],"%s",bList); 
            i++;
        };
    }
}

int IPcacheCHK(char* Address)
{
    int flg = -1;
    int u=0;
    for(;u < 80;u++)
    {
        //printf("SEARCHING IP!\n");
        if(strncmp(IPcache[u],Address,strlen(Address)) == 0)
        {
            flg = 1;
            //printf("FOUND A CACHED IP! %d\n",u);
            return u;
        }
    }
    if(flg == -1)
    {
        return flg;
    }
    else
    {
        return u;
    } 
}

int main(int argc, char **argv) 
{
    int listenfd, *connfdp, port, clientlen=sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid; 

    if (argc != 3) {
	fprintf(stderr, "usage: %s <port> <Timeout>\n", argv[0]);
	exit(0);
    }
    port = atoi(argv[1]);
    TimeOut = atoi(argv[2]);
    blackList();
    listenfd = open_listenfd(port);
    while (1) {
	connfdp = malloc(sizeof(int));
	*connfdp = accept(listenfd, (struct sockaddr*)&clientaddr, (socklen_t *) &clientlen);
	pthread_create(&tid, NULL, thread, connfdp);
    }
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

void echo(int connfd) 
{   
    size_t n;
    CC_LONG addrSz;
    unsigned char hashbuf[CC_MD5_DIGEST_LENGTH];
    int Flag = 0; int servSock; int IPcLoc; int fileFound = 0;
    long int total = 0;
    char* command; char* addr; char* path; char *portNo;
    char Siteaddr[INET6_ADDRSTRLEN]; char Proxbuf [MAXBUF]; char getMess[MAXBUF]; char buf[MAXLINE]; char hString[CC_MD5_DIGEST_LENGTH*2+1]; char hCpy[CC_MD5_DIGEST_LENGTH*2+1]; char addrCp [200];
    char fileDir[240] = "./CACHE/";
    struct hostent* Site = NULL;
    struct sockaddr_in ServAddr;
    CC_MD5_CTX md5;
    CC_MD5_Init(&md5);
   
    FILE* fileToCACHE = NULL;
    DIR* directo;

    struct dirent* lsTool;

    bzero((char *) &ServAddr, sizeof(ServAddr));

    n = read(connfd, buf, MAXLINE);
    //printf("server received the following request:\n%s\n",buf);
    //printf("BUF len: %lu\n",strlen(buf));

    if(strlen(buf) != 0)
    {
        strcpy(getMess,buf);
        command = strtok(buf,delimit);
        addr = strtok(NULL,delimit);
        
        if(addr != NULL)
        {    
            if(strncmp(addr,"http://",7) == 0)
            {
                addr+=7;
            }
            else if(strncmp(addr,"https://",8) == 0)
            {
                addr+=8;
            }
            //printf("PRE_ADDRESS: %s\n",addr);
            strcpy(addrCp,addr);
            addrSz = strlen(addrCp);

            CC_MD5_Update(&md5,addr,addrSz);
            CC_MD5_Final(hashbuf,&md5);
            for(int f = 0; f< CC_MD5_DIGEST_LENGTH;f++)
            {
                sprintf(&hString[f*2],"%02x",(unsigned int)hashbuf[f]);
            }

            //printf("HASH: %s \n",hString);    

            strcat(fileDir,hString);
            
            strcpy(hCpy,hString);            
            strcat(fileDir,hCpy);
            directo = opendir("./CACHE");
            if(directo != NULL)
            {   
                for(int ind = 0;(lsTool = readdir(directo)) != NULL;ind++)
                {
                    if(strcmp(lsTool->d_name, hString) == 0)
                    {
                        fileFound = 1;
                        //printf("Found the file in the cache! ADDR:%s HASH: %s\n",addr,hString);
                        time_t sTime = time(0);
                        
                        struct stat fileSt;
                        stat(fileDir,&fileSt);
                        double diff = difftime(sTime,fileSt.st_ctime);
                        if(diff > TimeOut)
                        {
                            remove(fileDir);
                            fileFound = 0;
                        }
                        else
                        {
                            break;
                        }
                    }
                }
                if(fileFound == 0)
                {
                    fileToCACHE = fopen(fileDir, "wb");
                    flockfile(fileToCACHE);
                }
                else
                {
                    //printf("USING FILE FROM CACHE\n");
                    fileToCACHE = fopen(fileDir,"rb");
                    flockfile(fileToCACHE);

                    if(fileToCACHE != NULL)
                    {
                        int fileSize = 0;
                        while((fileSize = fread(Proxbuf,1,MAXLINE-1,fileToCACHE)) > 0)
                        {
                            if(send(connfd, Proxbuf, fileSize, 0) < 0)//Begin streaming the file 
                            {
                                printf("ERROR: Failed to send file.\n");//If there is an error in sending the file, attempt to send this 500 error.
                                strcpy(buf,"HTTP/1.0 500 Internal Server Error\n\n");
                                write(connfd,"HTTP/1.0 500 Internal Server Error\n\n"/*Proxbuf*/,strlen(Proxbuf));
                                //exit(1);
                            }
                            bzero(Proxbuf,MAXLINE);
                            //printf("File read %d\n",fileSize);
                        }
                        funlockfile(fileToCACHE);
                        fclose(fileToCACHE);
                    }
                } 
            }
            else
            {
                printf("DIRECTORY IS BUSTED YO\n");
                exit(0);
            }
            if(fileFound == 0)
            {
                path = strrchr(addr,'/');
                addr = strtok(addr, "/");
                addr = strtok(addr, ":");
                portNo = strtok(NULL,":");

                if(portNo != NULL)
                {   
                    int Port = atoi(portNo);
                    ServAddr.sin_port = htons(Port);
                }                
                else
                {   
                    ServAddr.sin_port = htons(80);
                    portNo = "80";
                }
                IPcLoc = IPcacheCHK(addr);
                if(IPcLoc == -1)
                {
                    char IPHOST [80];
                    Site = gethostbyname(addr);
                    if(Site == NULL)
                    {
                        printf("SITE= NULL\n");
                    }
                    else
                    {
                        inet_ntop(AF_INET,Site->h_addr_list[0],Siteaddr,INET_ADDRSTRLEN);
                        strcpy(IPHOST, addr);
                        strcat(IPHOST,":");
                        strcat(IPHOST,Siteaddr);
                        sprintf(IPcache[IPCind],"%s",IPHOST);
                        IPCind++;
                    }
                }
                else
                {   
                    char indexEnt[80];
                    strcpy(indexEnt,IPcache[IPcLoc]);
                    strtok(indexEnt,":");
                    sprintf(Siteaddr,"%s",strtok(NULL,":"));
                }       
                if(strlen(Siteaddr) != 0)
                {
                    ServAddr.sin_family = AF_INET;
                    // Convert IPv4 and IPv6 addresses from text to binary form 
                    for(int j=0; j<15; j++)
                    {
                        if(strcmp(Blist[j],Siteaddr) == 0)
                        {
                            printf("BLACKLISTED\n %s",Blist[j]);
                            strcpy(getMess,"HTTP/1.1 403 Forbidden\n\n");
                            write(connfd,getMess,strlen(getMess));
                            Flag = 1;
                            break;
                        }
                        else if(strcmp(Blist[j],addr) == 0)
                        {
                            printf("BLACKLISTED\n %s",Blist[j]);
                            strcpy(getMess,"HTTP/1.1 403 Forbidden\n\n");
                            write(connfd,getMess,strlen(getMess));
                            Flag = 1;
                            break;
                        }
                    }

                    if(inet_pton(AF_INET,Siteaddr, &ServAddr.sin_addr)<=0)  
                    { 
                        printf("\nInvalid address/ Address not supported \n"); 
                        strcpy(Proxbuf,"HTTP/1.1 404 File Not Found\n\n");
                        write(connfd,"HTTP/1.1 404 File Not Found\r\n\r\n"/*Proxbuf*/,32);
                        //exit(0); 
                    } 

                    if(strncmp(command,"GET",3) == 0 && Flag != 1 && (fileToCACHE != NULL))
                    {
                        //write(connfd,"BUFFALO\n",strlen("BUFFALO\n"));

                        /* socket: create the socket */
                        servSock = socket(AF_INET, SOCK_STREAM, 0);
                        if (servSock < 0) 
                        {
                            error("ERROR opening socket\n");
                        } 
                        if(connect(servSock,(struct sockaddr*)&ServAddr,sizeof(ServAddr)) < 0)
                        {
                            printf("\nInvalid address/ Address not supported \n"); 
                            strcpy(Proxbuf,"HTTP/1.1 404 File Not Found\n\n");
                            write(connfd,"HTTP/1.1 404 File Not Found\r\n\r\n"/*Proxbuf*/,32);
                        }
                
                        send(servSock, getMess, strlen(getMess),0);

                        while((total = read(servSock,Proxbuf,sizeof(Proxbuf))) > 0)
                        {
                            printf("Total bytes read in: %ld\n", total);
                            //printf("CONTENTS:\n\n  %s",Proxbuf);
                            fwrite(Proxbuf,1,total,fileToCACHE);
                            send(connfd,Proxbuf,total,0);
                            bzero(Proxbuf,MAXLINE);
                        }
                        funlockfile(fileToCACHE);
                        printf("DONE WRITING\n\n");
                    }
                    else
                    {
                        strcpy(getMess,"HTTP/1.0 400 Bad Request\n\n");
                        write(connfd,"HTTP/1.1 400 Bad Request\n\n"/*getMess*/,27);
                        printf("USED %s INSTEAD OF GET!\n",command);
                    }
                }
                else
                {
                    strcpy(getMess,"HTTP/1.1 400 Bad Request\n\n");
                    write(connfd,"HTTP/1.1 400 Bad Request\n\n"/*getMess*/,27);
                }
            }
        }  
    }
    fclose(fileToCACHE);
    //printf("---------------END OF ECHO---------------\n\n\n\n\n");
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

