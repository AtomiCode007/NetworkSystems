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
char folder [100];
void echo(int connfd);
void *thread(void *vargp);
const char delimit[6] = " \n\0";// Delimiter values for the string tokenizer.
char userPass[10][60];
int numUsers;
char* Line;
size_t len;


int main(int argc, char **argv) 
{
    int listenfd, *connfdp, port, clientlen=sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid; 
    FILE* Users;

    if (argc != 3) 
    {
	fprintf(stderr, "usage: %s <folder> <port>\n", argv[0]);
	exit(0);
    }

    port = atoi(argv[2]);
    listenfd = open_listenfd(port);
    strcpy(folder, argv[1]);
    Users = fopen("dfs.conf","r");
    int i = 0;

    if(Users != NULL)
    {
        while(getline(&Line,&len,Users) > 0)
        {
            sprintf(userPass[i],"%s",Line);
            strtok(userPass[i],delimit);
            numUsers++;
            printf("%s\n",userPass[i]);
            i++;
        }
        
    }

    while (1) 
    {
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
    DIR* directo;
    struct dirent *dptr = NULL;//Usage of the dirent.h structure to navigate the file directory
    FILE* Filp;
    char* command = NULL;
    char* Filename = NULL;
    char fileCpy[100];
    char buf[MAXLINE];
    char fileBuf[MAXLINE];
    char uFolder[100];
    char invalid[100] = "Invalid Username or password. Please try again.\n";
    char filePt[45] = "/."; 
    char User[55];
    int trSz = 0;
    char trCh [1000];

    while (1)
    {
        printf("Waiting for next request...\n\n");
        bzero(buf,MAXBUF);
        n = write(connfd,"Ready!",7);
        n = read(connfd, buf, MAXLINE);
       
        if(strlen(buf) > 0)
        {   
            printf("USER PASS:%s\n",buf);
            for(int i = 0; i < numUsers;i++)
            {
                if(strncmp(userPass[i],buf,strlen(userPass[i])) == 0)
                {   
                    printf("User Authenticated\n");
                    write(connfd,"Very nice!",12);
                    sprintf(uFolder,"./%s/",folder);
                    strcpy(User,strtok(buf,":"));
                    strcat(uFolder,User);
                    printf("%s\n",uFolder);
                    mkdir(uFolder,0777);
                    directo = opendir(uFolder);
                    if(directo != NULL)
                    {
                        printf("Yee HAW\n");
                    }
                    bzero(buf,MAXBUF);

                    n = read(connfd, buf, MAXLINE);

                    printf("BUF len: %lu\n",strlen(buf));
                    printf("server received the following request:\n%s",buf);                
                    if(strlen(buf) == 0)
                    {
                        exit(0);
                    }
                    command = strtok(buf,delimit);
                    Filename = strtok(NULL,delimit);
                    if(Filename != NULL)
                    {
                        memcpy(fileCpy,Filename,strlen(Filename));
                    }
                    printf("Command: %s\n",command);
                    printf("Filename: %s\n",Filename);
                    if(strcmp(buf,"exit") == 0)
                    {
                        printf("Exiting\n");
                        exit(0);
                    } 
                    else if(strcmp(command,"put") == 0)
                    {   
                        printf("Put BLOCK\n");     
                        
                        strcat(filePt,Filename);
                        strcat(filePt,".");
                        bzero(buf,MAXLINE);

                        n = read(connfd, buf, 15);//Getting the part number of the file

                            write(connfd,"OK",3);

                            printf("FilePart: %s\n",buf);
                            strcat(filePt,strtok(buf,":"));
                            //printf("FilePart: %s\n",filePt);
                            strcpy(trCh,strtok(NULL,delimit));
                            //printf("Transfer SZ: %s\n",trCh);
                            trSz = atoi(trCh);
                            printf("Transfer size: %d\n",trSz);

                            strcat(uFolder,filePt);
                            printf("FOLDER DIR: %s\n\n",uFolder);
                            Filp = fopen(uFolder,"wb");

                        if(Filp != NULL)
                        {
                            printf("File made!\n");
                            int readT = 0;
                            int rn = 0;
                            while(readT < trSz)
                            {   
                                bzero(fileBuf,MAXLINE);
                                rn = read(connfd, fileBuf,MAXLINE); //Getting the file!
                                printf("Bytes read: %d Total needed: %d\n",readT,trSz);
                                //printf("%s\n",fileBuf);
                                fwrite(fileBuf,1,rn,Filp);
                                readT = readT + rn;
                            }
                            fclose(Filp);
                            printf("Done!\n\n");
                            write(connfd,"Done!",7);
                        }
                        else
                        {
                            printf("File failed to open\n");
                        }

                        bzero(filePt,strlen(filePt));
                        strcat(filePt,"/.");
                        strcat(filePt,fileCpy);
                        strcat(filePt,".");

                        printf("File Pointer 2; %s\n",filePt);
                        bzero(buf,MAXLINE);

                        n = read(connfd, buf, 15);//Getting the part number of the file
                        
                            write(connfd,"OK",3);
                            printf("FilePart: %s\n",buf);
                            strcat(filePt,strtok(buf,":"));
                            //printf("FilePart: %s\n",filePt);
                            strcpy(trCh,strtok(NULL,delimit));
                            //printf("Transfer SZ: %s\n",trCh);
                            trSz = atoi(trCh);
                            printf("Transfer size: %d\n",trSz);
                            bzero(uFolder,100);
                            sprintf(uFolder,"./%s/",folder);
                            strcat(uFolder,User);
                            strcat(uFolder,filePt);
                            printf("File Dirct: %s\n",uFolder);
                            Filp = fopen(uFolder,"wb");

                        if(Filp != NULL)
                        {
                            printf("File made!\n");
                            int readT = 0;
                            int rn = 0;
                            while(readT < trSz)
                            {   
                                bzero(fileBuf,MAXLINE);
                                rn = read(connfd, fileBuf,MAXLINE); //Getting the file!
                                printf("Bytes read: %d Total needed: %d\n",readT,trSz);
                                //printf("%s\n",fileBuf);
                                fwrite(fileBuf,1,rn,Filp);
                                readT = readT + rn;
                            }
                            fclose(Filp);
                            printf("Done 2!\n\n");
                            write(connfd,"Done!",7);
                        }

                        else
                        {
                            printf("File failed to open\n");
                        }

                    }
                    else if(strcmp(command,"get") == 0)
                    {
                        printf("Get BLOCK\n"); 
                        int first = 0; 
                        int fileSize;
                        char filesend [150];
                        char fCp[230] = ".";
                        char f1 [100];
                        char f2 [100];
                        strcat(fCp,Filename);
                        strcat(fCp,".");
                        strcat(filePt,Filename);
                        strcat(filePt,".");
                        printf("Dir: %s | File: %s\n",uFolder,fCp);
                        //Move to the next pointer in the linked list of files.

                        for(int ind = 0;(dptr = readdir(directo)) != NULL;ind++)//Begin the parsing of the folder
                        { 
                            if(strncmp(dptr->d_name,fCp,strlen(fCp)) == 0)
                            {
                                //printf("File name: %s\n",dptr->d_name);
                                if(first == 0)
                                {
                                    sprintf(f1,"%s",dptr->d_name);
                                    first =1;
                                }
                                else
                                {
                                    sprintf(f2,"%s",dptr->d_name);
                                }
                            }
                        }

                        printf("Found file: %s \n",f1);
                        printf("Found file: %s \n",f2);

                        read(connfd,buf,MAXLINE);

                        printf("responce recieved!\n\n");
                        if(strcmp(buf, "Send both") ==0)
                        {
                            printf("Sending both!\n");
                            printf("File order %d\n",strcmp(f1,f2));

                            if(strcmp(f1,f2) == -1)
                            {
                                printf("Actually going to send the first file!\n");
                                write(connfd,f1,strlen(f1));
                                printf("FILE PART: %s\n",f1);
                                strcat(filesend,uFolder);
                                strcat(filesend,"/");
                                strcat(filesend,f1);
                                printf("FILE LOC: %s\n",filesend);
                                Filp = fopen(filesend,"r+b");
                                if(Filp != NULL)
                                {
                                    fseek(Filp,0,SEEK_END);
                                    fileSize = ftell(Filp);
                                    rewind(Filp);
                                    bzero(buf,MAXLINE);
                                    sprintf(buf,"%d",fileSize);
                                    printf("FILE SIZE: %d\n",fileSize);
                                    write(connfd,buf,strlen(buf));
                                    int rdSz = 0;
                                    int readT = 0;
                                    n = read(connfd, buf, MAXLINE);
                                    while(readT < fileSize)
                                    {
                                        bzero(fileBuf,MAXLINE);
                                        rdSz = fread(fileBuf,1,MAXLINE,Filp);
                                        printf("Bytes read: %d\n",rdSz);
                                        write(connfd, fileBuf, rdSz);
                                        readT = readT + rdSz;
                                    }

                                    printf("DONE!\n");
                                    n = read(connfd, buf, MAXLINE);
                                    fclose(Filp);
                                }

                                //PART TWO
                                printf("Actually going to send the second file!\n");
                                write(connfd,f2,strlen(f2));
                                printf("FILE PART: %s\n",f2);
                                bzero(filesend,150);
                                strcat(filesend,uFolder);
                                strcat(filesend,"/");
                                strcat(filesend,f2);
                                printf("FILE LOC: %s\n",filesend);
                                Filp = fopen(filesend,"r+b");
                                if(Filp != NULL)
                                {
                                    fseek(Filp,0,SEEK_END);
                                    fileSize = ftell(Filp);
                                    rewind(Filp);
                                    bzero(buf,MAXLINE);
                                    sprintf(buf,"%d",fileSize);
                                    printf("FILE SIZE: %d\n",fileSize);
                                    write(connfd,buf,strlen(buf));
                                    int rdSz = 0;
                                    int readT = 0;
                                    n = read(connfd, buf, MAXLINE);
                                    while(readT < fileSize)
                                    {
                                        bzero(fileBuf,MAXLINE);
                                        rdSz = fread(fileBuf,1,MAXLINE,Filp);
                                        printf("Bytes read: %d\n",rdSz);
                                        write(connfd, fileBuf, rdSz);
                                        readT = readT + rdSz;
                                    }

                                    printf("DONE!\n");
                                    n = read(connfd, buf, MAXLINE);
                                    fclose(Filp);
                                }
                            }
                            else if(strcmp(f1,f2) == -3 || strcmp(f1,f2) == 1)
                            {
                                printf("Going to send the Second file first!\n");

                                //PART TWO
                                printf("Actually going to send the second file!\n");
                                write(connfd,f2,strlen(f2));
                                printf("FILE PART: %s\n",f2);
                                bzero(filesend,150);
                                printf("FILE LOC: %s\n",filesend);
                                strcat(filesend,uFolder);
                                strcat(filesend,"/");
                                strcat(filesend,f2);
                                printf("FILE LOC: %s\n",filesend);
                                Filp = fopen(filesend,"r+b");
                                if(Filp != NULL)
                                {
                                    fseek(Filp,0,SEEK_END);
                                    fileSize = ftell(Filp);
                                    rewind(Filp);
                                    bzero(buf,MAXLINE);
                                    sprintf(buf,"%d",fileSize);
                                    printf("FILE SIZE: %d\n",fileSize);
                                    write(connfd,buf,strlen(buf));
                                    int rdSz = 0;
                                    int readT = 0;
                                    n = read(connfd, buf, MAXLINE);
                                    while(readT < fileSize)
                                    {
                                        bzero(fileBuf,MAXLINE);
                                        rdSz = fread(fileBuf,1,MAXLINE,Filp);
                                        printf("Bytes read: %d\n",rdSz);
                                        write(connfd, fileBuf, rdSz);
                                        readT = readT + rdSz;
                                    }

                                    printf("DONE!\n");
                                    n = read(connfd, buf, MAXLINE);
                                    fclose(Filp);
                                }
                                //PART ONE BUT SECOND NOW
                                write(connfd,f1,strlen(f1));
                                printf("FILE PART: %s\n",f1);
                                printf("FILE LOC: %s\n",filesend);
                                bzero(filesend,150);
                                strcat(filesend,uFolder);
                                strcat(filesend,"/");
                                strcat(filesend,f1);
                                printf("FILE LOC: %s\n",filesend);
                                Filp = fopen(filesend,"r+b");
                                if(Filp != NULL)
                                {
                                    fseek(Filp,0,SEEK_END);
                                    fileSize = ftell(Filp);
                                    rewind(Filp);
                                    bzero(buf,MAXLINE);
                                    sprintf(buf,"%d",fileSize);
                                    printf("FILE SIZE: %d\n",fileSize);
                                    write(connfd,buf,strlen(buf));
                                    int rdSz = 0;
                                    int readT = 0;
                                    n = read(connfd, buf, MAXLINE);
                                    while(readT < fileSize)
                                    {
                                        bzero(fileBuf,MAXLINE);
                                        rdSz = fread(fileBuf,1,MAXLINE,Filp);
                                        printf("Bytes read: %d\n",rdSz);
                                        write(connfd, fileBuf, rdSz);
                                        readT = readT + rdSz;
                                    }

                                    printf("DONE!\n");
                                    n = read(connfd, buf, MAXLINE);
                                    fclose(Filp);
                                }
                            }
                        }
                        else if(strcmp(buf, "Send first") ==0)
                        {
                            if(strcmp(f1,f2) == -1)
                            {
                                printf("Actually going to ONLY send the first file!\n");

                                write(connfd,f1,strlen(f1));
                                printf("FILE PART: %s\n",f1);
                                strcat(filesend,uFolder);
                                strcat(filesend,"/");
                                strcat(filesend,f1);
                                printf("FILE LOC: %s\n",filesend);
                                Filp = fopen(filesend,"r+b");
                                if(Filp != NULL)
                                {
                                    fseek(Filp,0,SEEK_END);
                                    fileSize = ftell(Filp);
                                    rewind(Filp);
                                    bzero(buf,MAXLINE);
                                    sprintf(buf,"%d",fileSize);
                                    printf("FILE SIZE: %d\n",fileSize);
                                    write(connfd,buf,strlen(buf));
                                    int rdSz = 0;
                                    int readT = 0;
                                    n = read(connfd, buf, MAXLINE);
                                    while(readT < fileSize)
                                    {
                                        bzero(fileBuf,MAXLINE);
                                        rdSz = fread(fileBuf,1,MAXLINE,Filp);
                                        printf("Bytes read: %d\n",rdSz);
                                        write(connfd, fileBuf, rdSz);
                                        readT = readT + rdSz;
                                    }

                                    printf("DONE!\n");
                                    n = read(connfd, buf, MAXLINE);
                                    fclose(Filp);
                                }
                            }
                            else if(strcmp(f1,f2) == -3 || strcmp(f1,f2) == 1)
                            {
                                printf("Going to send the Second file!\n");
                                write(connfd,f2,strlen(f2));
                                printf("FILE PART: %s\n",f2);
                                bzero(filesend,150);
                                strcat(filesend,uFolder);
                                strcat(filesend,"/");
                                strcat(filesend,f2);
                                printf("FILE LOC: %s\n",filesend);
                                Filp = fopen(filesend,"r+b");
                                if(Filp != NULL)
                                {
                                    fseek(Filp,0,SEEK_END);
                                    fileSize = ftell(Filp);
                                    rewind(Filp);
                                    bzero(buf,MAXLINE);
                                    sprintf(buf,"%d",fileSize);
                                    printf("FILE SIZE: %d\n",fileSize);
                                    write(connfd,buf,strlen(buf));
                                    int rdSz = 0;
                                    int readT = 0;
                                    n = read(connfd, buf, MAXLINE);
                                    while(readT < fileSize)
                                    {
                                        bzero(fileBuf,MAXLINE);
                                        rdSz = fread(fileBuf,1,MAXLINE,Filp);
                                        printf("Bytes read: %d\n",rdSz);
                                        write(connfd, fileBuf, rdSz);
                                        readT = readT + rdSz;
                                    }

                                    printf("DONE!\n");
                                    n = read(connfd, buf, MAXLINE);
                                    fclose(Filp);
                                }
                            }
                        }

                    }
                    else if(strcmp(command,"list") == 0)
                    {
                        printf("List BLOCK\n");
                    }
                }
                else
                {
                    write(connfd,invalid/*Proxbuf*/,strlen(invalid));
                    printf("%s\n",invalid);
                    //exit(0);
                }
            }
        }
        else
        {
            printf("Error in read, client disconnect?\n");
            exit(0);
        }
            
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

