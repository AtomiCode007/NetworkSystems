/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <errno.h>

#define BUFSIZE 1024

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char **argv) 
{
    int sockfd, portno, n;
    unsigned int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname; //Host name
    char buf[BUFSIZE]; //The message buffer
    char FileBuf[BUFSIZE]; //The file buffer, seperate from the message buffer
    char ACKBuf[BUFSIZE];
    const char delimit[6] = " \n\0"; //char values for the string tokenizer to split on.
    FILE *fileP; //the file pointer for get and puts.
    
    errno = 0;
    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }

    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    while(1)
    {
      /*get a message from the user for the server*/
      bzero(buf, BUFSIZE); //clear the buffer
      bzero(ACKBuf,500);

      printf("Please enter msg: ");
      fgets(buf, BUFSIZE, stdin);
      //printf("buf length: %lu \n",strlen(buf));

      /* send the message to the server straight out of the gate */
      serverlen = sizeof(serveraddr);
      n = sendto(sockfd, buf, strlen(buf), 0,(const struct sockaddr *) &serveraddr, serverlen);      
      if (n < 0) 
      {
        error("ERROR in sendto");
      }
      //Tokenize the input from the user into a command and a filename
      char* command = strtok(buf,delimit);
      char* Filename = strtok(NULL, delimit);
      //printf("String Comp value: %s \n",buf);

      //Find out what the user entered.
      if(strcmp(command,"exit") == 0 && Filename == NULL)
      { 
        //exit the program with grace and dignity.
        printf("Closing\n");
        break;
      }
      else if(strncmp(command,"get",3) == 0 && Filename != NULL)
      { 
        //printf("FILENAME: %s \n",Filename);
        printf("Get mode\n");

        n = recvfrom(sockfd, ACKBuf, BUFSIZE, 0, (struct sockaddr *) &serveraddr, &serverlen);

        printf("%lu\n",strlen(ACKBuf));

        if(strlen(ACKBuf)==12)
        {
          //printf("FILENAME: %s \n",Filename);
          fileP = fopen(Filename,"wb"); //attempt to open the file

          //printf("Errno Code: %d\n",errno); 
          bzero(buf,BUFSIZE);//clear the message buffer for use
          //errno = 0;    error check

          if(fileP != NULL)
          {
            //Init local vars and get to work
            long byteTotal = 0;
            long WriteCount = 0;
            long writeAmt = 0;
            int ACKConf = 0;
            int counter = 0;
            //char FileSize[BUFSIZE];

            byteTotal = recvfrom(sockfd,buf,BUFSIZE,0,(struct sockaddr *) &serveraddr, &serverlen);
            ACKConf = sendto(sockfd, ACKBuf, strlen(ACKBuf), 0, (const struct sockaddr *) &serveraddr, serverlen);
            //printf("Total size string: %s \n",FileSize);

            sscanf(buf,"%ld",&byteTotal);//take the size from the buffer and put it into the byteTotal var.
            printf("Total size of file: %ld \n",byteTotal);
            bzero(buf,BUFSIZE);//clear the message buffer for use
            //printf("Bytes written: %ld \n",WriteCount);

            //Begin the file transfer.
            while(1)
            { 
              if(WriteCount >= byteTotal)
              {
                //printf("WriteCount:%ld byteTotal:%ld\n",WriteCount,byteTotal);
                //printf("Done!\n");
                break;
              }
              else
              {
                bzero(FileBuf,BUFSIZE);//clear the File buffer for use.

                //printf("Contents of FileBuf: %s \n",FileBuf);
                if(counter == 50)
                {
                  printf("Write Progress: %ld/%ld\n",WriteCount,byteTotal); //Progress update.
                }
      
                writeAmt = recvfrom(sockfd,FileBuf, BUFSIZE, 0, (struct sockaddr *) &serveraddr, &serverlen);//receieve the packet from the server
                
                //printf("%s",FileBuf);

                /*if(writeAmt != strlen(FileBuf))
                {
                  printf("bad packet!: %ld\n",writeAmt);
                }*/
                //else
                //{
                  ACKConf = sendto(sockfd, ACKBuf, strlen(ACKBuf), 0, (const struct sockaddr *) &serveraddr, serverlen);
                //}
                
                n = fwrite(FileBuf,writeAmt,1, fileP); //Write to the file.

                WriteCount = WriteCount + writeAmt; //update the file index counter.
                if(counter >= 50)
                {
                  counter = 0;
                }
                counter++;
                //printf("Bytes written: %ld \n",WriteCount); 
              }
            }
            fclose(fileP);//close the file
            printf("Bytes written to file: %ld / %ld \n",WriteCount,byteTotal);
          }
          else
          {
            printf("File failed to open.\n");
          }
        }
        else
        {
          printf("%s",ACKBuf);
        }     
      }
      else if(strncmp(command,"put",3) == 0)
      {
      //printf("%s\n",Filename);

      //Init local vars
      long fileSize = 0;
      int pkSz = 0;
      long readIndex = 0;
      long ACKConf = 0;
      fileP = fopen(Filename,"rb");//Attempt to open the file.
      //printf("%d\n",errno);
      
      if(fileP != NULL)
      {
        //printf("Just after the file success!\n");

        bzero(buf,BUFSIZE);//Clear the message buffer for use
        //bzero(ACKBuf,BUFSIZE);
        fseek(fileP,0,SEEK_END);//Scan through the file in order to determine how large it is.
        fileSize = ftell(fileP);//set the size of the file to a var.
        rewind(fileP);//set the file pointer to the beginning of the file.

        sprintf(buf,"%ld",fileSize);//put the size of the file into the message buffer for the server.

        printf("File size: %ld\n",fileSize);
        //printf("File size string: %s\n",buf);
        //printf("Index begin: %ld\n",readIndex);

        n = sendto(sockfd, buf, BUFSIZE, 0, (const struct sockaddr *) &serveraddr, serverlen);//Send the file size to the host
        bzero(buf,BUFSIZE);//clear the buffer

        while(readIndex < fileSize)//Begin the file transfer.
        {
          //printf("Index Update: %ld/%ld\n",readIndex,fileSize); 
        
          pkSz = fread(FileBuf,1,BUFSIZE-1,fileP); //Read from the file, put the data into the file buffer.
          n = sendto(sockfd, FileBuf, pkSz, 0, (const struct sockaddr *) &serveraddr, serverlen);//Send the data to the server.
          
          //printf("%d\n",errno);
          if(n == -1)
          {
            //Send this message to the client in the chance that there is an error with the file transfer.
            char ErrorRes[42] = "Error in file transfer. Please try again."; 
            n = sendto(sockfd, ErrorRes, strlen(ErrorRes), 0, (const struct sockaddr *) &serveraddr, serverlen);
            printf("error\n");
            break;
          }
          else
          {
            //printf("Waiting on ACK\n");
            ACKConf = recvfrom(sockfd,ACKBuf,BUFSIZE,0, (struct sockaddr *) &serveraddr, &serverlen);
            //printf("%ld\n",ACKConf);
          }
          readIndex += pkSz; //Increment the readIndex
          //printf("N:%d",n);
          printf("File Progress: %ld/%ld\n",readIndex,fileSize); 
          bzero(FileBuf,BUFSIZE);//clear the File buffer.
        }
        printf("File Complete %ld/%ld\n",readIndex,fileSize);
        fclose(fileP);//close the file.
        bzero(FileBuf,BUFSIZE);//clear the File and message buffer once more
        bzero(buf,BUFSIZE);
        }
      }
      else if(strncmp(command,"delete",6) == 0)
      {
        //Make sure there is a file to send.
        if(Filename == NULL)
        {
          printf("Usage: delete filename.*\n");
        }
        else
        {
          printf("Delete mode\n");
          //Recieve confirmation that the file was deleted successfuly or if there was a problem.
          n = recvfrom(sockfd,buf,BUFSIZE,0, (struct sockaddr *) &serveraddr, &serverlen);
          printf("%s",buf);
          bzero(buf,BUFSIZE);
        }
      }
      else if(strcmp(command,"ls") == 0)
      {
        printf("List mode\n");
        bzero(buf,BUFSIZE);//clear the message buffer
        n = recvfrom(sockfd,buf,BUFSIZE,0, (struct sockaddr *) &serveraddr, &serverlen);//Recieve the list of files in the directory, or an error message.
        printf("%s",buf);
        bzero(buf,BUFSIZE);//clear the buffer just for redundancy.
      }
      else
      {
        //The user did not enter a vaild command, tell them as such and try again.
        printf("This is not a legit command dude...\n");
      }
      /* print the server's reply */
      //n = recvfrom(sockfd, buf, BUFSIZE, 0, &serveraddr, &serverlen);

      if (n < 0)
      {
        error("ERROR in recvfrom");
      }
        //printf("Echo from server: %s\n", buf);

    }   
    return 0;
}
