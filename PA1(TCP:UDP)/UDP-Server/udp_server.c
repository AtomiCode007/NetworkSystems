/* 
 * udpserver.c - A simple UDP echo server 
 * usage: udpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <dirent.h>

#define BUFSIZE 1024

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

int main(int argc, char **argv) {
  int sockfd; /* socket */
  int portno; /* port to listen on */
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buf */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */
  const char delimit[6] = " \n\0";// Delimiter values for the string tokenizer.
  char fileBuf[BUFSIZE]; //Filebuffer, seperate from the normal buffer for consistency
  char ACKBuf[500];
  struct dirent *dptr = NULL;//Usage of the dirent.h structure to navigate the file directory
  char* current_Dir; // pointer to the current directory when open
  DIR *direcPt; //pointer to the current found directory
  FILE *FilePt; //A file pointer
  errno = 0; //Error checking


  /* 
   * check command line arguments 
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  /* 
   * socket: create the parent socket 
   */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* 
   * main loop: wait for a datagram, then echo it
   */
  clientlen = sizeof(clientaddr);
  while (1) {

    /*
     * recvfrom: receive a UDP datagram from a client
     */

    bzero(buf, BUFSIZE);
    n = recvfrom(sockfd, buf, BUFSIZE, 0,(struct sockaddr *) &clientaddr, (socklen_t *) &clientlen);
    if (n < 0)
      error("ERROR in recvfrom");

    /* 
     * gethostbyaddr: determine who sent the datagram
     */

    hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, sizeof(clientaddr.sin_addr.s_addr), AF_INET);

    if (hostp == NULL)
    {
      error("ERROR on gethostbyaddr");
    }
      
    hostaddrp = inet_ntoa(clientaddr.sin_addr);

    if (hostaddrp == NULL)
    {
      error("ERROR on inet_ntoa\n");
    }
    /*
    //Recieve the datagram from the client, tokenize it into the command and if required, the file name.
    */
    printf("server received datagram from %s (%s)\n", hostp->h_name, hostaddrp);
    printf("server received %lu/%d bytes: %s\n", strlen(buf), n, buf);

    char* command = strtok(buf,delimit);
    char* Filename = strtok(NULL, delimit);

    /*printf("%s\n",command);
    if(Filename != NULL)
    {
      printf("%s\n",Filename);
    }*/


    //Begin finding out which if any of the commands are the desired one.
    if(strcmp(command,"exit") == 0 && Filename == NULL)
    {
      n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &clientaddr, clientlen);
      printf("Closing\n");
      bzero(buf, BUFSIZE);
      exit(1);
    }
    else if(strncmp(command,"get",3) == 0 && Filename != NULL)
    { 
      //printf("%s\n",Filename);

      /*
      Init local vars and attempt to open the file.
      */

      long fileSize = 0;
      int pkSz = 0;
      long readIndex = 0;
      long ACKConf = 0;
      char FileAck[13] = "File opened!";
      FilePt = fopen(Filename,"rb");
      //printf("%d\n",errno);
      //errno = 0;
      
      if(FilePt != NULL)
      {
        n = sendto(sockfd, FileAck, strlen(FileAck), 0, (struct sockaddr *) &clientaddr, clientlen);
        //printf("Just after the file success!\n");
        /*
        Find the length of the file in bytes by running through the file once till the end. 
        record that number and save it.
        set the file pointer to the beginning of the file.
        */
        bzero(buf,BUFSIZE);//Clear the buffer so that the only thing in it is the file size.
        
        fseek(FilePt,0,SEEK_END);
        fileSize = ftell(FilePt);
        rewind(FilePt);
        /*
        Place the size of the file into the buffer and send it back to the client so that everyone knows how big the file is going to be.
        */
        sprintf(buf,"%ld",fileSize);

        printf("File size: %ld\n",fileSize);
        //printf("File size string: %s\n",buf);
        //printf("Index begin: %ld\n",readIndex);

        //Get the file size and send it to the host
        n = sendto(sockfd, buf, strlen(buf), 0,(struct sockaddr *) &clientaddr, clientlen);

        ACKConf = recvfrom(sockfd,ACKBuf,BUFSIZE,0,(struct sockaddr *) &clientaddr, (socklen_t *) &clientlen);
        while(!feof(FilePt))
        { 
          bzero(fileBuf,BUFSIZE); //Clear the buffer for the next batch of data.
          //Begin to send the file from the server to the host.
          //Looping over the size of the data that is being sent, that way we know when to stop the transfer,
          //This might make adding TCP protocall a bit easier if everyone knows where they are in the file.
          pkSz = fread(fileBuf,1,BUFSIZE-1,FilePt);

          n = sendto(sockfd, fileBuf, pkSz, 0, (struct sockaddr *) &clientaddr, clientlen);

          if(n == -1)
          {
            //Send this message to the client in the chance that there is an error with the file transfer.
            //Stop the transfer
            printf("Error during file transfer.\n");
            //char ErrorRes[42] = "Error in file transfer. Please try again."; 
            //n = sendto(sockfd, ErrorRes, strlen(ErrorRes), 0, (struct sockaddr *) &clientaddr, clientlen);
            break;
          }
          else
          { 
            //printf("%d\n",pkSz);
            ACKConf = recvfrom(sockfd,ACKBuf,BUFSIZE,0,(struct sockaddr *) &clientaddr, (socklen_t *) &clientlen);
            
          
            readIndex += pkSz; //Increment the readIndex by the amount of data sent to the client. 
            //printf("File Progress: %ld/%ld\n",readIndex,fileSize); 
            
          }
        }
          printf("File Complete %ld/%ld \n",readIndex,fileSize);
          fclose(FilePt);//Close the file
          bzero(fileBuf,BUFSIZE);//Clear the buffers once more.
          bzero(buf,BUFSIZE);
      }
      else
      {
        printf("This file does not exist.\n");
        char ErrorRes[78] = "The file you entered does not exist, or there was an error opening the file.\n"; 
        n = sendto(sockfd, ErrorRes, strlen(ErrorRes), 0, (struct sockaddr *) &clientaddr, clientlen);
      }
    }   
    else if(strncmp(command,"put",3) == 0 && Filename != NULL)
    {
      /*
      Similar in vein to how 'get' works, but with the roles reversed.
      */
      //printf("FILENAME: %s \n",Filename);
      printf("Put mode\n");
        
      FilePt = fopen(Filename,"wb");//Attempt to open the file.

      //printf("Errno Code: %d\n",errno); 
      bzero(buf,BUFSIZE);   
      //errno = 0;    

      if(FilePt != NULL)
      {
        //Init local vars
        long byteTotal = 0;
        long WriteCount = 0;
        long writeAmt = 0;
        long ACKConf = 0;
        //char FileSize[BUFSIZE];

        byteTotal = recvfrom(sockfd,buf,BUFSIZE,0,(struct sockaddr *) &clientaddr, (socklen_t *) &clientlen);//Recieve the size of the file from the client.
        //printf("Total size string: %s \n",FileSize);

        sscanf(buf,"%ld",&byteTotal);
        printf("Total size of file: %ld \n",byteTotal);
        bzero(buf,BUFSIZE);
        
        //printf("Bytes written: %ld \n",WriteCount);

        //Begin to tranfer the file!
        while(WriteCount < byteTotal)
        { 
            bzero(fileBuf,BUFSIZE);//Make sure that the file buffer is totally empty before transfer.
            //printf("Contents of FileBuf: %s \n",FileBuf);
            //printf("Write Progress: %ld/%ld\n",WriteCount,byteTotal);

            writeAmt = recvfrom(sockfd,fileBuf,BUFSIZE,0,(struct sockaddr *) &clientaddr, (socklen_t *) &clientlen);

            //printf("%s",fileBuf);
            //printf("sending ACK\n");
            
            ACKConf = sendto(sockfd, ACKBuf, strlen(ACKBuf), 0,(struct sockaddr *) &clientaddr, clientlen);

            //printf("%ld, %d\n",ACKConf,errno);
            n = fwrite(fileBuf,writeAmt,1, FilePt);
            //printf("N:%d",n);
            WriteCount = WriteCount + writeAmt;//Increment by the amount of bytes written to the file.
            //printf("Bytes written: %ld \n",WriteCount);
        }
        fclose(FilePt);//close the file
        printf("Bytes written to file: %ld/%ld \n",WriteCount,byteTotal);
        bzero(buf,BUFSIZE);//Clear the buffers for redundancy.
        bzero(fileBuf,BUFSIZE);
      }
      else
      {
        printf("Ya, the file wont open...\n");//file failed to open.
      }     
    }
    else if(strncmp(command,"ls",2) == 0 && Filename == NULL)
    {
      printf("List Request\n");  
      bzero(buf,BUFSIZE);
      current_Dir = getenv("PWD");//Use the built in command PWD to find the current working directory.
      if(current_Dir == NULL)
      {
        printf("Couldn't obtain the working directory\n");
      }
      direcPt = opendir((const char*)current_Dir);//Attempt to open the working Directory 
      if(direcPt == NULL)
      {
        printf("Couldn't open the working directory\n");
      }
      //Move to the next pointer in the linked list of files.
      for(int ind = 0;(dptr = readdir(direcPt)) != NULL;ind++)//Begin the parsing of the folder and displaying all of the contents aside from the hidden files.
      {
        if(dptr->d_name[0] == '.')//Indicates a hidden file. So we will not add it to the buffer for respoonse.
        {
          //printf("Invisi file!\n");
        }
        else
        {
          strcat(buf,dptr->d_name);//Add the file name to the buffer 
          strcat(buf,"\n"); //Add an endline to the buffer as well to make the list look pretty.
        }
      }
      //printf("%s",buf);
      n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &clientaddr, clientlen);
      //Once done, send the buffer with all of the files in the directory that the server is in.
    }
    else if(strncmp(command,"delete",6) == 0)
    { 
      //Fairly straight forward, a error check to make sure that the file is deleted or not.
      int delChk = 0;
      //Attempt to delete the selected file.
      delChk = remove(Filename);
      //If successful, send this message back.
      if(delChk == 0)
      {
        char Resp[25] = "File successfuly deleted";
        sprintf(buf,"%s",Resp);
        n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &clientaddr, clientlen);
      }
      else//If there was an error, send this message back.
      {
        char RespE[41] = "File failed to delete or does not exist.";
        sprintf(buf,"%s",RespE);
        n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &clientaddr, clientlen);
      }
      bzero(buf, BUFSIZE);
      //printf("Delete Request\n");
    }
    else//They did not enter a vaild command, tell them to try again.
    {
      char Response[113] = "Improper command, please use the following: \n ls\n get filename.*\n put filename.*\n delete filename.*\n exit\n";
      n = sendto(sockfd, Response, strlen(Response), 0, (struct sockaddr *) &clientaddr, clientlen);
      bzero(buf, BUFSIZE);
      printf("Improper command, please use the following: \n ls\n get\n put\n delete\n exit\n");
    }
    
    /* 
     * sendto: echo the input back to the client 
     */

    //n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &clientaddr, clientlen);
    if (n < 0) 
      error("ERROR in sendto");
  }
}
