/*
Server Author : Akhil Punchavallil Sivankuutan & Aiswarya Siby
Usage : ./client server ip  eg : ./client 127.0.0.1
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <libgen.h> // for dirname()


#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_ARGS 10

#define Mport 8088


int arg_Number=0;
char* commandv[MAX_ARGS];

volatile int socket_fd = 0;

// used to handle ctrl + c exit and let server understand client ended

void ctrl_c_handler(int signal) {
  char command[1024];
  strcpy(command, "quit");
  send(socket_fd, command, strlen(command), 0);
  exit(0);
}


// used for tokenization

void split_input_string(char* cmd_str) {
    char* arg;
    char arg_cp[1024];
    int argc = 0;
    char cmd_string[BUFFER_SIZE];
    strcpy(cmd_string,cmd_str);
    arg = strtok(cmd_string," ");
    while (arg != NULL) {
        commandv[argc] = malloc(strlen(arg));
        strcpy(commandv[argc], arg);
        argc++;
        arg = strtok(NULL, " ");   
    }
    arg_Number = argc;
}
//  receive the tar file data from server
int receive_tar_gz_file(const char *filename, int server_socket) {
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd == -1) {
        perror("Error opening file");
        return -1;
    }

    // Receive file size from server
    off_t file_size;
    ssize_t bytes_received = recv(server_socket, &file_size, sizeof(file_size), 0);
    if (bytes_received == -1) {
        perror("Error receiving file size");
        close(fd);
        return -1;
    }
    // Receive file contents from server
    char buffer[BUFFER_SIZE];
    off_t total_bytes_received = 0;
    while (total_bytes_received < file_size) {
        ssize_t bytes_received = recv(server_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received == -1) {
            perror("Error receiving file contents");
            close(fd);
            return -1;
        }
        ssize_t bytes_written = write(fd, buffer, bytes_received);
        if (bytes_written == -1) {
            perror("Error writing to file");
            close(fd);
            return -1;
        }
        total_bytes_received += bytes_received;
    }

    close(fd);
    printf("File received successfully.\n\n");
    return 0;
}

//function used to unzip the tar file

void unzip_tar(char *filename)
{
    char command[100];
    char *dir = dirname(filename);
    //used to clean client directory
    sprintf(command, "rm -rf %s", "./Users");
    system(command);
    sprintf(command,"tar -xzf %s",filename);
 

    system(command);
}
// used for validation of number
int is_digit_(char txt[BUFFER_SIZE])
{
    int flag =1;
    for(int i=0;i<strlen(txt);i++)
    {
        if(!isdigit(txt[i]))
        {
            flag=0;
            break;
        }
    }
return flag;
}
// to validate date values
int is_valid_date(char txt[BUFFER_SIZE])
{
    int flag =1;
    int year, month, date;
    int num_matched = sscanf(txt, "%d-%d-%d", &year, &month, &date);
    if(num_matched==3)
        return 1;
    else
        return 0;
}


int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr;
    struct hostent *server;
    int sock_fd, conn_status;
    char buffer[1024] = {0};

    // Check if hostname is provided as an argument
    if (argc < 2) {
        fprintf(stderr, "Usage: %s hostname\n", argv[0]);
        return 1;
    }

    // Get server information using hostname
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr, "Error: No such host %s\n", argv[1]);
        return 1;
    }

    // Create socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("Error creating socket");
        return 1;
    }

    // Initialize server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    // Connect to server
    conn_status = connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (conn_status < 0) {
        perror("Error connecting to server");
        return 1;
    }

   
    char connect_add[30];
    int s_port;
    size_t ip_address_length;
    fprintf(stderr,"Connecting to server....\n");
    send(sock_fd, "getip", 5, 0);
    recv(sock_fd, &ip_address_length, sizeof(ip_address_length), 0);
    ssize_t bytes_received = recv(sock_fd, connect_add,ip_address_length, 0);
    ssize_t port_bytes_received = recv(sock_fd, &s_port, sizeof(s_port), 0);

    if(bytes_received<1 || port_bytes_received<1)
    {
        fprintf(stderr,"Hostname and port not received!!!");
        return 1;
    }

    // Read and send commands to server
     /**
    mirror - check response of server to decide whether to reconnect to mirror
    */

    if(s_port!=PORT)
    {
        fprintf(stderr,"\nConnection to Mirror!!!\n");
        close(sock_fd);
        struct sockaddr_in mirror_addr;
        struct hostent *mirror;
        int mirror_fd, mirror_conn_status;
        char mirror_buffer[1024] = {0};

        // Get server information using hostname
        mirror = gethostbyname(connect_add);
        if (mirror == NULL) {
            fprintf(stderr, "Error: No such host %s\n", argv[1]);
            return 1;
        }

        // Create socket
        mirror_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (mirror_fd < 0) {
            perror("Error creating socket");
            return 1;
        }

        // Initialize server address structure
        memset(&mirror_addr, 0, sizeof(mirror_addr));
        mirror_addr.sin_family = AF_INET;
        mirror_addr.sin_port = htons(s_port);
        memcpy(&mirror_addr.sin_addr.s_addr, mirror->h_addr, mirror->h_length);

        // Connect to server
        mirror_conn_status = connect(mirror_fd, (struct sockaddr *)&mirror_addr, sizeof(mirror_addr));
        if (mirror_conn_status < 0) {
            perror("Error connecting to server");
            return 1;
        }
        sock_fd=mirror_fd;

    }

    /*
    mirror
    */

    socket_fd = sock_fd;
    signal(SIGINT, ctrl_c_handler);

    while (1) {
        char command[1024];
        fprintf(stderr,"Enter command: ");
        fgets(command, 1024, stdin);
        
        strtok(command, "\n");  // Remove trailing newline from input
        split_input_string(command);

        //validation

        if (strcmp(commandv[0], "findfile") == 0) {
            // parse filename
            if (arg_Number<2) {
                printf("Invalid filename\n");
                continue;
            }
        } // validation for sgetfile
        else if (strcmp(commandv[0], "sgetfiles") == 0){
            // parse size limits
            if (arg_Number<2) {
                printf("Invalid arguments\n");
                continue;
            }
            else if(strcmp(commandv[arg_Number-1],"-u")==0)
            {
                 if(arg_Number!=4)
                {
                    printf("Invalid arguments\n");
                    continue;
                }
                else{
                    // checking if numbers
                    int flag = is_digit_(commandv[1]);
                    int flag2 = is_digit_(commandv[2]);
                    if(flag==0 || flag2 ==0)
                    {
                        printf("Invalid arguments\n");
                        continue;
                    }
                    else if(atoi(commandv[1])>atoi(commandv[2]))
                    {
                        printf("Invalid Size\n");
                        continue;
                    }
                }
            }
            else if(arg_Number!=3)
            {
                printf("Invalid arguments\n");
                continue;
            } 
            else{
                    // checking if numbers
                    int flag = is_digit_(commandv[1]);
                    int flag2 = is_digit_(commandv[2]);
                    if(flag==0 || flag2 ==0)
                    {
                        printf("Invalid Size\n");
                        continue;
                    }
                    else if(atoi(commandv[1])>atoi(commandv[2]))
                    {
                        printf("Invalid Size\n");
                        continue;
                    }
                }     
        } // validation for dgetfiles
        else if (strcmp(commandv[0], "dgetfiles")==0){
            // parse size limits
            if (arg_Number<2) {
                printf("Invalid arguments\n");
                continue;
            }
            else if(strcmp(commandv[arg_Number-1],"-u")==0)
            {
                 if(arg_Number!=4)
                {
                    printf("Invalid arguments\n");
                    continue;
                }
                else{
                    // checking if its  a date 
                    int flag = is_valid_date(commandv[1]);
                    int flag2 = is_valid_date(commandv[2]);
                    if(flag==0 || flag2 ==0)
                    {
                        printf("Invalid Size\n");
                        continue;
                    }
                }
            }
            else if(arg_Number!=3)
            {
                printf("Invalid arguments\n");
                continue;
            } 
            else{
                    // checking if its  a date 
                    int flag = is_valid_date(commandv[1]);
                    int flag2 = is_valid_date(commandv[2]);
                    if(flag==0 || flag2 ==0)
                    {
                        printf("Invalid Size\n");
                        continue;
                    }
                }
        }
        else if(strcmp(commandv[0], "getfiles") == 0 || strcmp(commandv[0], "gettargz") == 0)
        {
             // parse size limits
            if (arg_Number<2) {
                printf("Invalid arguments\n");
                continue;
            }
            else if(strcmp(commandv[arg_Number-1],"-u")==0)
            {
                 if(arg_Number>8 || arg_Number<3)
                {
                    printf("Invalid arguments\n");
                    continue;
                }
            }
            else if(arg_Number>7 || arg_Number<2)
            {
                 printf("Invalid arguments\n");
                continue;
            }
        }
        else {
            // invalid command
            if(strcmp(commandv[0],"quit")!=0){
            printf("Invalid command\n");
            continue;
            }
        }


        // Send command to server
        send(sock_fd, command, strlen(command), 0);

        // Wait for response from server
        memset(buffer, 0, sizeof(buffer));
        
        char tar_filename[100];
        sprintf(tar_filename,"%d",getpid());
        strcat(tar_filename,"_temp.tar.gz");

        // Check for sgetfiles command
        if (strncmp(commandv[0], "sgetfiles", 9) == 0 || strncmp(commandv[0], "dgetfiles", 9) == 0) {
                receive_tar_gz_file(tar_filename,sock_fd);
                if(strcmp(commandv[arg_Number-1],"-u")==0)
                {
                    unzip_tar(tar_filename);
                }
        }
        else if(strcmp(commandv[0], "getfiles") == 0 || strcmp(commandv[0], "gettargz") == 0){
            //int response;
            //ssize_t bytes_received = recv(sock_fd, &response, 4, 0);
            // Print response
            
            //printf("%d\n", response);
            
                recv(sock_fd, buffer,15, 0);
                //printf("%s\t %lu", buffer,strlen(buffer));
                if(strncmp(buffer,"File Found",10) == 0){
                receive_tar_gz_file(tar_filename,sock_fd);
                if(strcmp(commandv[arg_Number-1],"-u")==0)
                {
                    unzip_tar(tar_filename);
                }
            }
            else
            {
                printf("%s", buffer);
            }
        }
        // Check if client has requested to quit
        else if (strcmp(commandv[0], "quit") == 0) {
            break;
        }
        
        else {
            recv(sock_fd, buffer, sizeof(buffer), 0);
            // Print response
            printf("%s\n", buffer);
        }

        
    }

    // Close socket
    close(sock_fd);

    return 0;
}
