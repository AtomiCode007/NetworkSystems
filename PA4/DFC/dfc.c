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
#define BUFSIZE   8192  /* max I/O buffer size */ 
CC_LONG addrSz;
CC_MD5_CTX md5;

int portno[4],numServer = 4; 
int n, sockfd[4], sockH[4];
char UserPass[5][100];
unsigned int serverlen[4];
struct sockaddr_in serveraddr[4];
struct hostent *server [4];
char *Line, *Port, *hostname;
char buf[BUFSIZE]; //The message buffer
char fileBuf[MAXLINE];
char conStr [100][100];
char putPt [16][100];
const char delimit[6] = " \n\0"; //char values for the string tokenizer to split on.
size_t len;
FILE *config; //the file pointer for get and puts.
/* 
 * error - wrapper for perror
 */

void error(char *msg) {
    perror(msg);
    exit(0);
}

void sockHB()
{
    numServer = 4;
    for(int s = 0; s < 4;s++)
    {
        sockfd[s] = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd[s] < 0)
        {
            error("ERROR opening socket");
        } 
        // gethostbyname: get the server's DNS entry
        //printf("HOST %d: %s\n",k,conStr[k]);
        hostname = strtok(conStr[s],"\n");
        server[s] = gethostbyname(hostname);
        if (server[s] == NULL) 
        {
            fprintf(stderr,"ERROR, no such host as %s\n", hostname);
            exit(0);
        }
        bzero((char *) &serveraddr[s], sizeof(serveraddr[s]));
        serveraddr[s].sin_family = AF_INET;
        server[s] = gethostbyname(hostname);
        bcopy((char *)server[s]->h_addr, (char *)&serveraddr[s].sin_addr.s_addr, server[s]->h_length);
        serveraddr[s].sin_port = htons(portno[s]);
        serverlen[s] = sizeof(serveraddr[s]);
        if(connect(sockfd[s],(struct sockaddr*)&serveraddr[s],sizeof(serveraddr[s])) < 0)
        {
            printf("\nInvalid address/ Address not supported \n");
            numServer = numServer - 1;
            sockH[s] = 0;
        }
        else
        {
            sockH[s] = 1;
        }
    }
}

int min(int x, int y)
{
    if(x < y)
    {
        return x;
    }
    else
    {
        return y;
    }
}

int main(int argc, char **argv) 
{    
    errno = 0;
    int qtr;
    FILE* FilP;

    /* check command line arguments */
    if (argc != 2) {
       fprintf(stderr,"usage: %s <config-file>\n", argv[0]);
       exit(0);
    }
    printf("File name: %s\n",argv[1]);

    config = fopen(argv[1],"r");
    int i = 0;
    if(config != NULL)
    {   
        while(getline(&Line,&len,config) > 0)
        {
            if(i < 4)
            {
                //printf("%s\n",Line);
                strtok(Line,delimit);
                Line = strtok(NULL,delimit);
                //printf("%s\n",Line);
                sprintf(conStr[i],"%s",Line);
            }
            else
            {
                //printf("%s\n",Line);
                sprintf(UserPass[0],"%s",Line);
                strtok(UserPass[0],delimit);
                printf("%s\n",UserPass[0]);
            }
            i++;
        }
        for(int j=0; j<4; j++)
        {   
            char* temp;
            temp = strtok(conStr[j],":");
            portno[j] = atoi(strtok(NULL,delimit));
            //printf("%d\n", portno[j]);
        }
    }
    
    while(1)
    {
        sockHB();

        for(int h =0;h<4;h++)
        {
            if(sockH[h] == 1)
            {   
                n = read(sockfd[h],buf,MAXLINE);
                n = write(sockfd[h], UserPass[0], strlen(UserPass[0]));
                if (n < 0) 
                {
                    //error("ERROR in sendto");
                    printf("%d: Blep\n",h);
                }
                n = read(sockfd[h], buf, MAXLINE);
                if(strncmp(buf,"Invalid Username or password. Please try again.\n",strlen(buf)) == 0)
                {
                    printf("Invalid Username or password. Please try again.\n");
                    //exit(0);
                }
                else
                {
                    printf("User Authenticated\n");
                } 
            }
            else
            {
                //printf("Shes dead Jim....\n");
            }
            
        }

        printf("Please enter msg: ");
        fgets(buf, BUFSIZE, stdin);

        for(int i = 0; i< 4;i++)
        {   
            if(sockH[i] == 1)
            {
                n = write(sockfd[i], buf, strlen(buf));
            }
            else
            {

            }
            
        }
    
        //Tokenize the input from the user into a command and a filename
        char* command = strtok(buf,delimit);
        char* Filename = strtok(NULL, delimit);

        if(strcmp(command,"put") == 0)
        {   
            printf("Put BLOCK\n");
            FilP = fopen(Filename, "r+b");

            unsigned char hashbuf[CC_MD5_DIGEST_LENGTH];
            int rByte;
            int tByte = 0;
            char hVal[1024];
            char trSz [15];
            int m = 0;
            if(FilP != NULL)
            {
                CC_MD5_Init(&md5);
                while ((rByte = fread(hVal,1,1024,FilP)) != 0) 
                {
                    CC_MD5_Update(&md5,hVal,rByte);
                    tByte += rByte;
                }
                rewind(FilP);

                CC_MD5_Final(hashbuf,&md5);
                for(int f = 0; f< CC_MD5_DIGEST_LENGTH;f++)
                {
                    sprintf(&hVal[f*2],"%02x",(unsigned int)hashbuf[f]);
                }
                m = atoi(hVal) % 4;
                qtr = tByte / 4;
                printf("MOD VALUE:%d \n",m);

                if(m == 0)
                {
                    char partNum [15]= "1:";
                    if(sockH[0] == 1)
                    {
                        sprintf(trSz,"%d",qtr);
                        strcat(partNum,trSz);
                        n = strlen(partNum);
                        printf("PartNum: %s\n", partNum);
                        write(sockfd[0],partNum,n);
                        read(sockfd[0], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < qtr)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[0], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[0], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[3] == 1)
                    {
                        rewind(FilP);
                        sprintf(trSz,"%d",qtr);
                        n = strlen(partNum);
                        printf("PartNum: %s\n", partNum);
                        write(sockfd[3],partNum,n);
                        read(sockfd[3], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < qtr)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[3], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[3], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[0] == 1)
                    {                
                        sprintf(partNum,"2:");
                        sprintf(trSz,"%d",qtr);
                        strcat(partNum, trSz);
                        printf("PartNum: %s\n", partNum);
                        n = write(sockfd[0], partNum, strlen(partNum));
                        read(sockfd[0], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < qtr)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[0], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[0], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[1] == 1)
                    {
                        fseek(FilP,-qtr,SEEK_CUR);
                        sprintf(partNum,"2:");
                        sprintf(trSz,"%d",qtr);
                        strcat(partNum, trSz);
                        printf("PartNum: %s\n", partNum);
                        n = write(sockfd[1], partNum, strlen(partNum));
                        read(sockfd[1], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < qtr)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[1], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[1], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[1] == 1)
                    {
                        sprintf(partNum,"3:");
                        sprintf(trSz,"%d",qtr);
                        strcat(partNum, trSz);
                        printf("PartNum: %s\n", partNum);
                        n = write(sockfd[1], partNum, strlen(partNum));
                        read(sockfd[1], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < qtr)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[1], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[1], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[2] == 1)
                    {
                        fseek(FilP,-qtr,SEEK_CUR);
                        sprintf(partNum,"3:");
                        sprintf(trSz,"%d",qtr);
                        strcat(partNum, trSz);
                        printf("PartNum: %s\n", partNum);
                        n = write(sockfd[2], partNum, strlen(partNum));
                        read(sockfd[2], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < qtr)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[2], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[2], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[2] == 1)
                    {
                        int remain = qtr+(tByte - qtr*4);
                        sprintf(partNum,"4:");
                        sprintf(trSz,"%d",remain);
                        strcat(partNum, trSz);
                        printf("PartNum: %s\n", partNum);
                        n = write(sockfd[2], partNum, strlen(partNum));
                        read(sockfd[2], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < remain)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[2], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[2], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[3] == 1)
                    {
                        int remain = qtr+(tByte - qtr*4);
                        fseek(FilP,-remain,SEEK_CUR);
                        sprintf(partNum,"4:");
                        sprintf(trSz,"%d",remain);
                        strcat(partNum, trSz);
                        printf("PartNum: %s\n", partNum);
                        n = write(sockfd[3], partNum, strlen(partNum));
                        read(sockfd[3], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < remain)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[3], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[3], buf, MAXLINE);
                    }
                }
                else if(m == 1)
                {
                    char partNum [15]= "1:";
                    if(sockH[1] == 1)
                    {
                        sprintf(trSz,"%d",qtr);
                        strcat(partNum,trSz);
                        n = strlen(partNum);
                        printf("PartNum: %s\n", partNum);
                        write(sockfd[1],partNum,n);
                        read(sockfd[1], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < qtr)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[1], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[1], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[0] == 1)
                    {
                        rewind(FilP); //Rewind to Part 1
                        sprintf(trSz,"%d",qtr);
                        n = strlen(partNum);
                        printf("PartNum: %s\n", partNum);
                        write(sockfd[0],partNum,n);
                        read(sockfd[0], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < qtr)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[0], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[0], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[1] == 1)
                    {                
                        sprintf(partNum,"2:");//Part Two!
                        sprintf(trSz,"%d",qtr);
                        strcat(partNum, trSz);
                        printf("PartNum: %s\n", partNum);
                        n = write(sockfd[1], partNum, strlen(partNum));
                        read(sockfd[1], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < qtr)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[1], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[1], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[2] == 1)
                    {   
                        fseek(FilP,-qtr,SEEK_CUR);             
                        sprintf(partNum,"2:");
                        sprintf(trSz,"%d",qtr);
                        strcat(partNum, trSz);
                        printf("PartNum: %s\n", partNum);
                        n = write(sockfd[2], partNum, strlen(partNum));
                        read(sockfd[2], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < qtr)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[2], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[2], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[2] == 1)
                    {
                        sprintf(partNum,"3:");
                        sprintf(trSz,"%d",qtr);
                        strcat(partNum, trSz);
                        printf("PartNum: %s\n", partNum);
                        n = write(sockfd[2], partNum, strlen(partNum));
                        read(sockfd[2], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < qtr)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[2], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[2], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[3] == 1)
                    {
                        fseek(FilP,-qtr,SEEK_CUR);
                        sprintf(partNum,"3:");
                        sprintf(trSz,"%d",qtr);
                        strcat(partNum, trSz);
                        printf("PartNum: %s\n", partNum);
                        n = write(sockfd[3], partNum, strlen(partNum));
                        read(sockfd[3], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < qtr)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[3], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[3], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[0] == 1)
                    {
                        int remain = qtr+(tByte - qtr*4);
                        sprintf(partNum,"4:");
                        bzero(trSz,15);
                        sprintf(trSz,"%d",remain);
                        strcat(partNum, trSz);
                        printf("PartNum: %s\n", partNum);
                        n = write(sockfd[0], partNum, strlen(partNum));
                        read(sockfd[0], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < remain)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[0], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[0], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[3] == 1)
                    {
                        int remain = qtr+(tByte - qtr*4);
                        fseek(FilP,-remain,SEEK_CUR);
                        sprintf(partNum,"4:");
                        sprintf(trSz,"%d",remain);
                        strcat(partNum, trSz);
                        printf("PartNum: %s\n", partNum);
                        n = write(sockfd[3], partNum, strlen(partNum));
                        read(sockfd[3], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < remain)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[3], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[3], buf, MAXLINE);
                    }
                }
                else if(m == 2)
                {
                    char partNum [15]= "1:";
                    if(sockH[1] == 1)
                    {
                        sprintf(trSz,"%d",qtr);
                        strcat(partNum,trSz);
                        n = strlen(partNum);
                        printf("PartNum: %s\n", partNum);
                        write(sockfd[1],partNum,n);
                        read(sockfd[1], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < qtr)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[1], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[1], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[2] == 1)
                    {
                        rewind(FilP); //Rewind for the first part of the file.
                        sprintf(trSz,"%d",qtr);
                        n = strlen(partNum);
                        printf("PartNum: %s\n", partNum);
                        write(sockfd[2],partNum,n);
                        read(sockfd[2], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < qtr)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[2], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[2], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[3] == 1)
                    {                
                        sprintf(partNum,"2:");
                        sprintf(trSz,"%d",qtr);
                        strcat(partNum, trSz);
                        printf("PartNum: %s\n", partNum);
                        n = write(sockfd[3], partNum, strlen(partNum));
                        read(sockfd[3], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < qtr)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[3], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[3], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[2] == 1)
                    {
                        fseek(FilP,-qtr,SEEK_CUR);//Go back a qurater for part two again.
                        sprintf(partNum,"2:");
                        sprintf(trSz,"%d",qtr);
                        strcat(partNum, trSz);
                        printf("PartNum: %s\n", partNum);
                        n = write(sockfd[2], partNum, strlen(partNum));
                        read(sockfd[2], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < qtr)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[2], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[2], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[3] == 1)
                    {
                        sprintf(partNum,"3:");
                        sprintf(trSz,"%d",qtr);
                        strcat(partNum, trSz);
                        printf("PartNum: %s\n", partNum);
                        n = write(sockfd[3], partNum, strlen(partNum));
                        read(sockfd[3], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < qtr)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[3], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[3], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[0] == 1)
                    {
                        fseek(FilP,-qtr,SEEK_CUR);
                        sprintf(partNum,"3:");
                        sprintf(trSz,"%d",qtr);
                        strcat(partNum, trSz);
                        printf("PartNum: %s\n", partNum);
                        n = write(sockfd[0], partNum, strlen(partNum));
                        read(sockfd[0], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < qtr)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[0], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[0], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[1] == 1)
                    {
                        int remain = qtr+(tByte - qtr*4);
                        
                        sprintf(partNum,"4:");
                        sprintf(trSz,"%d",remain);
                        strcat(partNum, trSz);
                        printf("PartNum: %s\n", partNum);
                        n = write(sockfd[1], partNum, strlen(partNum));
                        read(sockfd[1], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < remain)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[1], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[1], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[0] == 1)
                    {
                        int remain = qtr+(tByte - qtr*4);
                        fseek(FilP,-remain,SEEK_CUR);
                        sprintf(partNum,"4:");
                        sprintf(trSz,"%d",remain);
                        strcat(partNum, trSz);
                        printf("PartNum: %s\n", partNum);
                        n = write(sockfd[0], partNum, strlen(partNum));
                        read(sockfd[0], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < remain)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[0], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[0], buf, MAXLINE);
                    }
                }
                else if(m == 3)
                {
                    char partNum [15]= "1:";
                    if(sockH[2] == 1)
                    {
                        sprintf(trSz,"%d",qtr);
                        strcat(partNum,trSz);
                        n = strlen(partNum);
                        printf("PartNum: %s\n", partNum);
                        write(sockfd[2],partNum,n);
                        read(sockfd[2], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < qtr)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[2], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[2], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[3] == 1)
                    {
                        rewind(FilP); //Rewind for the first part of the file.
                        sprintf(trSz,"%d",qtr);
                        n = strlen(partNum);
                        printf("PartNum: %s\n", partNum);
                        write(sockfd[3],partNum,n);
                        read(sockfd[3], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < qtr)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[3], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[3], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[0] == 1)
                    {                
                        sprintf(partNum,"2:");
                        sprintf(trSz,"%d",qtr);
                        strcat(partNum, trSz);
                        printf("PartNum: %s\n", partNum);
                        n = write(sockfd[0], partNum, strlen(partNum));
                        read(sockfd[0], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < qtr)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[0], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[0], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[3] == 1)
                    {
                        fseek(FilP,-qtr,SEEK_CUR);//Go back a qurater for part two again.
                        sprintf(partNum,"2:");
                        sprintf(trSz,"%d",qtr);
                        strcat(partNum, trSz);
                        printf("PartNum: %s\n", partNum);
                        n = write(sockfd[3], partNum, strlen(partNum));
                        read(sockfd[3], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < qtr)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[3], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[3], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[0] == 1)
                    {
                        sprintf(partNum,"3:");
                        sprintf(trSz,"%d",qtr);
                        strcat(partNum, trSz);
                        printf("PartNum: %s\n", partNum);
                        n = write(sockfd[0], partNum, strlen(partNum));
                        read(sockfd[0], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < qtr)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[0], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[0], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[1] == 1)
                    {
                        fseek(FilP,-qtr,SEEK_CUR);
                        sprintf(partNum,"3:");
                        sprintf(trSz,"%d",qtr);
                        strcat(partNum, trSz);
                        printf("PartNum: %s\n", partNum);
                        n = write(sockfd[1], partNum, strlen(partNum));
                        read(sockfd[1], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < qtr)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[1], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[1], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[2] == 1)
                    {
                        int remain = qtr+(tByte - qtr*4);
                        
                        sprintf(partNum,"4:");
                        sprintf(trSz,"%d",remain);
                        strcat(partNum, trSz);
                        printf("PartNum: %s\n", partNum);
                        n = write(sockfd[2], partNum, strlen(partNum));
                        read(sockfd[2], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < remain)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[2], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[2], buf, MAXLINE);
                    }
                    printf("REMAINDER: %d/%ld\n\n",tByte,ftell(FilP));
                    if(sockH[1] == 1)
                    {
                        int remain = qtr+(tByte - qtr*4);
                        fseek(FilP,-remain,SEEK_CUR);
                        sprintf(partNum,"4:");
                        sprintf(trSz,"%d",remain);
                        strcat(partNum, trSz);
                        printf("PartNum: %s\n", partNum);
                        n = write(sockfd[1], partNum, strlen(partNum));
                        read(sockfd[1], buf, MAXLINE);

                        int rdSz = 0;
                        int readT = 0;
                        while(readT < remain)
                        {
                            bzero(fileBuf,MAXLINE);
                            rdSz = fread(fileBuf,1,min(qtr-readT,MAXLINE),FilP);
                            printf("Bytes read: %d\n",rdSz);
                            write(sockfd[1], fileBuf, rdSz);
                            readT = readT + rdSz;
                        }
                        printf("DONE!\n");
                        n = read(sockfd[1], buf, MAXLINE);
                    }
                }
                fclose(FilP);
            }
            else
            {
                printf("File does not exist!\n");
            }
        }
        if(strcmp(command,"get") == 0)
        {            
            printf("FileName : %s\n",Filename);
            printf("Get BLOCK\n");
            //char* getTok;
            char fileMatch [1000];
            int trSz = 0;
            bzero(fileMatch,1000);
            FilP = fopen(Filename,"w+b");
            if(FilP != NULL)
            {
                printf("Good to Go!\n\n");
                if(sockH[0] == 1)
                {
                    if(sockH[1] == 0)
                    {
                        //Send Both no matter what
                        printf("Asking for Both!\n");
                        write(sockfd[0],"Send both",10);
                        bzero(buf,MAXLINE);

                        read(sockfd[0],buf,MAXLINE);
                        printf("FILE PART:%s \n",buf);
                        read(sockfd[0],buf,MAXLINE);
                        trSz = atoi(buf);
                        printf("FILE PART SIZE: %d\n",trSz);
                        if(FilP != NULL)
                        {
                            int readT = 0;
                            int rn = 0;
                            write(sockfd[0],"GO NOW",8);
                            while(readT < trSz)
                            {   
                                bzero(fileBuf,MAXLINE);
                                rn = read(sockfd[0], fileBuf,MAXLINE); //Getting the file!
                                printf("Bytes read: %d | Total needed: %d\n",readT,trSz);
                                //printf("%s\n",fileBuf);
                                fwrite(fileBuf,1,rn,FilP);
                                readT = readT + rn;
                            }
                            printf("Done!\n\n");
                            write(sockfd[0],"Done!",7);
                        }
                        //SECOND FILE
                        read(sockfd[0],buf,MAXLINE);
                        printf("FILE PART:%s \n",buf);
                        read(sockfd[0],buf,MAXLINE);
                        trSz = atoi(buf);
                        printf("FILE PART SIZE: %d\n",trSz);
                        if(FilP != NULL)
                        {
                            int readT = 0;
                            int rn = 0;
                            write(sockfd[0],"GO NOW",8);
                            while(readT < trSz)
                            {   
                                bzero(fileBuf,MAXLINE);
                                rn = read(sockfd[0], fileBuf,MAXLINE); //Getting the file!
                                printf("Bytes read: %d | Total needed: %d\n",readT,trSz);
                                //printf("%s\n",fileBuf);
                                fwrite(fileBuf,1,rn,FilP);
                                readT = readT + rn;
                            }
                            printf("Done!\n\n");
                            write(sockfd[0],"Done!",7);
                        }
                    }
                    else
                    {
                        printf("ASK FOR FIRST!\n");
                        write(sockfd[0],"Send first",11);
                        bzero(buf,MAXLINE);
                        read(sockfd[0],buf,MAXLINE);
                        printf("FILE PART:%s \n",buf);
                        read(sockfd[0],buf,MAXLINE);
                        trSz = atoi(buf);
                        printf("FILE PART SIZE: %d\n",trSz);
                        if(FilP != NULL)
                        {
                            int readT = 0;
                            int rn = 0;
                            write(sockfd[0],"GO NOW",8);
                            while(readT < trSz)
                            {   
                                bzero(fileBuf,MAXLINE);
                                rn = read(sockfd[0], fileBuf,MAXLINE); //Getting the file!
                                printf("Bytes read: %d | Total needed: %d\n",rn,trSz);
                                //printf("%s\n",fileBuf);
                                fwrite(fileBuf,1,rn,FilP);
                                readT = readT + rn;
                            }
                            printf("Done!\n\n");
                            write(sockfd[0],"Done!",7);
                        }
                    }
                }
                printf("SECOND SERVER!\n\n");
                if(sockH[1] == 1)
                {
                    if(sockH[2] == 0)
                    {
                        //Send Both no matter what
                        printf("Asking for Both!\n");

                        write(sockfd[1],"Send both",10);
                        bzero(buf,MAXLINE);

                        read(sockfd[1],buf,MAXLINE);
                        printf("FILE PART:%s \n",buf);
                        read(sockfd[1],buf,MAXLINE);
                        trSz = atoi(buf);
                        printf("FILE PART SIZE: %d\n",trSz);
                        if(FilP != NULL)
                        {
                            int readT = 0;
                            int rn = 0;
                            write(sockfd[1],"GO NOW",8);
                            while(readT < trSz)
                            {   
                                bzero(fileBuf,MAXLINE);
                                rn = read(sockfd[1], fileBuf,MAXLINE); //Getting the file!
                                printf("Bytes read: %d | Total needed: %d\n",readT,trSz);
                                //printf("%s\n",fileBuf);
                                fwrite(fileBuf,1,rn,FilP);
                                readT = readT + rn;
                            }
                            printf("Done!\n\n");
                            write(sockfd[1],"Done!",7);
                        }
                        //SECOND FILE
                        read(sockfd[1],buf,MAXLINE);
                        printf("FILE PART:%s \n",buf);
                        read(sockfd[1],buf,MAXLINE);
                        trSz = atoi(buf);
                        printf("FILE PART SIZE: %d\n",trSz);
                        if(FilP != NULL)
                        {
                            int readT = 0;
                            int rn = 0;
                            write(sockfd[1],"GO NOW",8);
                            while(readT < trSz)
                            {   
                                bzero(fileBuf,MAXLINE);
                                rn = read(sockfd[1], fileBuf,MAXLINE); //Getting the file!
                                printf("Bytes read: %d | Total needed: %d\n",readT,trSz);
                                //printf("%s\n",fileBuf);
                                fwrite(fileBuf,1,rn,FilP);
                                readT = readT + rn;
                            }
                            printf("Done!\n\n");
                            write(sockfd[1],"Done!",7);
                        }
                    }
                    else
                    {
                        write(sockfd[1],"Send first",11);
                        bzero(buf,MAXLINE);
                        read(sockfd[1],buf,MAXLINE);
                        printf("FILE PART:%s \n",buf);
                        read(sockfd[1],buf,MAXLINE);
                        trSz = atoi(buf);
                        printf("FILE PART SIZE: %d\n",trSz);
                        if(FilP != NULL)
                        {
                            int readT = 0;
                            int rn = 0;
                            write(sockfd[1],"GO NOW",8);
                            while(readT < trSz)
                            {   
                                bzero(fileBuf,MAXLINE);
                                rn = read(sockfd[1], fileBuf,MAXLINE); //Getting the file!
                                printf("Bytes read: %d | Total needed: %d\n",readT,trSz);
                                //printf("%s\n",fileBuf);
                                fwrite(fileBuf,1,rn,FilP);
                                readT = readT + rn;
                            }
                            printf("Done!\n\n");
                            write(sockfd[1],"Done!",7);
                        }
                    }
                }
                printf("Third SERVER!\n\n");
                if(sockH[2] == 1)
                {
                    if(sockH[3] == 0)
                    {
                        //Send Both no matter what
                        printf("Asking for Both!\n");

                        write(sockfd[2],"Send both",10);
                        bzero(buf,MAXLINE);

                        read(sockfd[2],buf,MAXLINE);
                        printf("FILE PART:%s \n",buf);
                        read(sockfd[2],buf,MAXLINE);
                        trSz = atoi(buf);
                        printf("FILE PART SIZE: %d\n",trSz);
                        if(FilP != NULL)
                        {
                            int readT = 0;
                            int rn = 0;
                            write(sockfd[2],"GO NOW",8);
                            while(readT < trSz)
                            {   
                                bzero(fileBuf,MAXLINE);
                                rn = read(sockfd[2], fileBuf,MAXLINE); //Getting the file!
                                printf("Bytes read: %d | Total needed: %d\n",readT,trSz);
                                //printf("%s\n",fileBuf);
                                fwrite(fileBuf,1,rn,FilP);
                                readT = readT + rn;
                            }
                            printf("Done!\n\n");
                            write(sockfd[2],"Done!",7);
                        }
                        //SECOND FILE
                        read(sockfd[2],buf,MAXLINE);
                        printf("FILE PART:%s \n",buf);
                        read(sockfd[2],buf,MAXLINE);
                        trSz = atoi(buf);
                        printf("FILE PART SIZE: %d\n",trSz);
                        if(FilP != NULL)
                        {
                            int readT = 0;
                            int rn = 0;
                            write(sockfd[2],"GO NOW",8);
                            while(readT < trSz)
                            {   
                                bzero(fileBuf,MAXLINE);
                                rn = read(sockfd[2], fileBuf,MAXLINE); //Getting the file!
                                printf("Bytes read: %d | Total needed: %d\n",readT,trSz);
                                //printf("%s\n",fileBuf);
                                fwrite(fileBuf,1,rn,FilP);
                                readT = readT + rn;
                            }
                            printf("Done!\n\n");
                            write(sockfd[2],"Done!",7);
                        }
                    }
                    else
                    {
                        write(sockfd[2],"Send first",11);
                        bzero(buf,MAXLINE);
                        read(sockfd[2],buf,MAXLINE);
                        printf("FILE PART:%s \n",buf);
                        read(sockfd[2],buf,MAXLINE);
                        trSz = atoi(buf);
                        printf("FILE PART SIZE: %d\n",trSz);
                        if(FilP != NULL)
                        {
                            int readT = 0;
                            int rn = 0;
                            write(sockfd[2],"GO NOW",8);
                            while(readT < trSz)
                            {   
                                bzero(fileBuf,MAXLINE);
                                rn = read(sockfd[2], fileBuf,MAXLINE); //Getting the file!
                                printf("Bytes read: %d | Total needed: %d\n",readT,trSz);
                                //printf("%s\n",fileBuf);
                                fwrite(fileBuf,1,rn,FilP);
                                readT = readT + rn;
                            }
                            printf("Done!\n\n");
                            write(sockfd[2],"Done!",7);
                        }
                    }
                }
                printf("FOURTh SERVER!\n\n");
                if(sockH[3] == 1)
                {
                    if(sockH[1] == 0)
                    {
                        //Send Both no matter what
                        printf("Asking for Both!\n");

                        write(sockfd[3],"Send both",10);
                        bzero(buf,MAXLINE);

                        read(sockfd[3],buf,MAXLINE);
                        printf("FILE PART:%s \n",buf);
                        read(sockfd[3],buf,MAXLINE);
                        trSz = atoi(buf);
                        printf("FILE PART SIZE: %d\n",trSz);
                        if(FilP != NULL)
                        {
                            int readT = 0;
                            int rn = 0;
                            write(sockfd[3],"GO NOW",8);
                            while(readT < trSz)
                            {   
                                bzero(fileBuf,MAXLINE);
                                rn = read(sockfd[3], fileBuf,MAXLINE); //Getting the file!
                                printf("Bytes read: %d | Total needed: %d\n",readT,trSz);
                                //printf("%s\n",fileBuf);
                                fwrite(fileBuf,1,rn,FilP);
                                readT = readT + rn;
                            }
                            printf("Done!\n\n");
                            write(sockfd[3],"Done!",7);
                        }
                        //SECOND FILE
                        read(sockfd[3],buf,MAXLINE);
                        printf("FILE PART:%s \n",buf);
                        read(sockfd[2],buf,MAXLINE);
                        trSz = atoi(buf);
                        printf("FILE PART SIZE: %d\n",trSz);
                        if(FilP != NULL)
                        {
                            int readT = 0;
                            int rn = 0;
                            write(sockfd[3],"GO NOW",8);
                            while(readT < trSz)
                            {   
                                bzero(fileBuf,MAXLINE);
                                rn = read(sockfd[3], fileBuf,MAXLINE); //Getting the file!
                                printf("Bytes read: %d | Total needed: %d\n",readT,trSz);
                                //printf("%s\n",fileBuf);
                                fwrite(fileBuf,1,rn,FilP);
                                readT = readT + rn;
                            }
                            printf("Done!\n\n");
                            write(sockfd[3],"Done!",7);
                        }
                    }
                    else
                    {
                        write(sockfd[3],"Send first",11);
                        bzero(buf,MAXLINE);
                        read(sockfd[3],buf,MAXLINE);
                        printf("FILE PART:%s \n",buf);
                        read(sockfd[3],buf,MAXLINE);
                        trSz = atoi(buf);
                        printf("FILE PART SIZE: %d\n",trSz);
                        if(FilP != NULL)
                        {
                            int readT = 0;
                            int rn = 0;
                            write(sockfd[3],"GO NOW",8);
                            while(readT < trSz)
                            {   
                                bzero(fileBuf,MAXLINE);
                                rn = read(sockfd[3], fileBuf,MAXLINE); //Getting the file!
                                printf("Bytes read: %d | Total needed: %d\n",readT,trSz);
                                //printf("%s\n",fileBuf);
                                fwrite(fileBuf,1,rn,FilP);
                                readT = readT + rn;
                            }
                            printf("Done!\n\n");
                            write(sockfd[3],"Done!",7);
                        }
                    }
                }
                fclose(FilP);
            }
            else
            {
                printf("File failed to open\n");
            }
            
        }
        if(strcmp(command,"list") == 0)
        {
            printf("List BLOCK\n");
        }
        if(strcmp(command,"exit") == 0)
        {
            printf("Exit BLOCK\n");
            exit(0);
        }

    }
    fclose(config);
    return 0;
}
