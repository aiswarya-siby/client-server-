/*
Server Author : Akhil Punchavallil Sivankuutan & Aiswarya Siby
Usage : ./server mirrorip mirror port number eg : ./server 127.0.0.1 8088
runs on port : 8080
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <limits.h>
#include <time.h>
#include <signal.h>
#include <netdb.h>

#define PORT 8080
#define MPORT 8088
#define MAX_CONNECTIONS 15
#define BUFFER_SIZE 1024
#define MAX_PATH 4096
#define MAX_CMD 8192
#define MAX_ARGS 10

void processclient(int sockfd);

int arg_Number=0;
char* commandv[MAX_ARGS];
int client_Count =0; // to track numner of live connection in both server and mirror
int mirror_Count =0;  // to track number of live connection in mirror

// used to calculate number of process
void handler(int sig) {
    client_Count = client_Count-1;
    fprintf(stderr,"\nCurrent Number of Live connections : %d\n",client_Count);
    fprintf(stderr,"\nCurrent Number of Live mirror connections : %d\n",mirror_Count);
}

//used to track mirror process number
void handler2(int sig) {
    
    mirror_Count = mirror_Count-1;
    
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
//function to send ip and port number to client based on which server and mirror switch occurs
void sendip_to_client(int client_socket,const char *server_ip,int s_port)
{
    char command[5];
    size_t len =strlen(server_ip);
    recv(client_socket, command, sizeof(command), 0);
    send(client_socket,&len, sizeof(size_t), 0);
    send(client_socket,server_ip,len, 0);
    send(client_socket,&s_port, sizeof(s_port), 0);
}

int main(int argc, char const *argv[]) {
    int server_fd, new_socket, pid;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Check if hostname is provided as an argument
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <hostname> <Port>\n", argv[0]);
        return 1;
    }
    
    signal(SIGUSR1, handler);
    signal(SIGUSR2, handler2);
    // Create server socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET,SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Set socket address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind socket to address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_fd, MAX_CONNECTIONS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    int pid_1;
    // to communicate with the mirror server a new child process is created it runs till server is up or mirror fails
    if ((pid_1 = fork()) < 0) 
    {
            perror("Fork failed");
            exit(EXIT_FAILURE);
    }
    else if (pid_1 == 0) {
            // Child process
        close(server_fd);

        fprintf(stderr,"\nConnecting to mirror...\n"); 
        struct sockaddr_in mirror_addr;
        struct hostent *mirror;
        int mirror_fd, mirror_conn_status;
        char mirror_buffer[1024] = {0};

        // Get server information using hostname
        mirror = gethostbyname(argv[1]);
        if (mirror == NULL) {
            fprintf(stderr, "Error: No such mirror host %s\n", argv[1]);
            exit(EXIT_FAILURE);
        }

        // Create socket
        mirror_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (mirror_fd < 0) {
            perror("Error creating socket in mirror");
            exit(EXIT_FAILURE);
        }

        // Initialize server address structure
        memset(&mirror_addr, 0, sizeof(mirror_addr));
        mirror_addr.sin_family = AF_INET;
        mirror_addr.sin_port = htons(atoi(argv[2]));
        memcpy(&mirror_addr.sin_addr.s_addr, mirror->h_addr, mirror->h_length);

        // Connect to server
        mirror_conn_status = connect(mirror_fd, (struct sockaddr *)&mirror_addr, sizeof(mirror_addr));
        if (mirror_conn_status < 0) {
            perror("Error connecting to mirror server");
            exit(EXIT_FAILURE);
        }
    fprintf(stderr,"\nmirror connection established...\n");       
    send(mirror_fd, "server_bind", 11, 0);
    int valread,response=0;
    while (1)
    {
        valread = read(mirror_fd, &response, sizeof(int));
        if(response){
            response =0;
            kill(getppid(),SIGUSR1);
            kill(getppid(),SIGUSR2);
            fprintf(stderr,"\n Mirror completed 1 task\n");

        }
    }
    
    exit(0);

    }

    // Fork a child process to service each client request
    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }
        if ((pid = fork()) < 0) {
            perror("Fork failed");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Child process
            close(server_fd);
            int server_count = client_Count - mirror_Count;
            /*logic for load balancing
            first 4 to server 
            next 4 to mirror
            and then alternate
            we consider also clients that have already exited and load balance based on that with a allocation to less loader server
            */
            if(server_count<4)
            {
                sendip_to_client(new_socket,argv[1],PORT); 
            }
            else if(mirror_Count<4)
            {
                int s_port = atoi(argv[2]);
                sendip_to_client(new_socket,argv[1],s_port);
            }
            else
            {
                if(server_count<=mirror_Count)
                    sendip_to_client(new_socket,argv[1],PORT); 
                else
                    {
                        int s_port = atoi(argv[2]);
                        sendip_to_client(new_socket,argv[1],s_port);
                    }
            }
 
            processclient(new_socket);
            exit(EXIT_SUCCESS);
        } else {
            // Parent process

            int server_count = client_Count - mirror_Count;
            if(server_count<4)
            {
                
            }
            else if(mirror_Count<4)
            {
                mirror_Count++;
            }
            else
            {
                if(server_count<=mirror_Count){}
                else
                    {
                        mirror_Count++;
                    }
            }

            client_Count++;
            close(new_socket);
            //to collect all zombie process
            while (waitpid(-1, NULL, WNOHANG) > 0);
        }
    }

    return 0;
}
// handles the findfile command

void search_for_file(int client_socket, char* filename) {
    char command[PATH_MAX];
    snprintf(command, sizeof(command), "find %s -name \"%s\" -type f | head -n 1", getenv("HOME"), filename);

    FILE* fp = popen(command, "r");
    if (fp == NULL) {
        send(client_socket, "Failed to execute command\n", 26, 0);
        return;
    }

    char filepath[PATH_MAX];
    fgets(filepath, PATH_MAX, fp);
    pclose(fp);

    if (strlen(filepath) == 0) {
        send(client_socket, "File not found\n", 15, 0);
        return;
    }

    filepath[strcspn(filepath, "\n")] = 0;

    struct stat statbuf;
    if (stat(filepath, &statbuf) == -1) {
        send(client_socket, "Failed to get file information\n", 32, 0);
        return;
    }

    char response[1024];
    snprintf(response, sizeof(response), "%s %lld %s\n", filename, statbuf.st_size, ctime(&statbuf.st_ctime));
    send(client_socket, response, strlen(response), 0);
}



//used to create the tar file

void create_tar_archive(char cmd[MAX_CMD]) {
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        fprintf(stderr, "Cannot determine home directory\n");
        //return EXIT_FAILURE;
    }

    int status = system(cmd);
    if (status != 0) {
        fprintf(stderr, "Error creating tar archive\n");
        kill(getppid(),SIGUSR1);
        exit(EXIT_FAILURE);
    }
}

// to calculate size of tar

off_t get_tar_size(const char *archive_name)
{
    // Get the size of the file
    
    struct stat st;
    off_t tar_size ;
    if (stat(archive_name, &st) < 0) {
        tar_size = 0;
    }
    else{
    tar_size = st.st_size;
    }
    return tar_size;
}
//function to send tar file to client

void send_tarfile(int new_socket,const char *archive_name)
{
    char buffer[BUFFER_SIZE] = {0};
    int valread;
    int tar_fd = open(archive_name, O_RDONLY);
    if (tar_fd < 0) {
        perror("open failed");
        kill(getppid(),SIGUSR1);
        exit(EXIT_FAILURE);
    }

    // Get the size of the file
    struct stat st;
    if (stat(archive_name, &st) < 0) {
        perror("stat failed");
        kill(getppid(),SIGUSR1);
        exit(EXIT_FAILURE);
    }
    off_t tar_size = st.st_size;

    // Send the size of the tar file to the client
    if (send(new_socket, &tar_size, sizeof(tar_size), 0) < 0) {
        perror("send failed");
        kill(getppid(),SIGUSR1);
        exit(EXIT_FAILURE);
    }

    // Send the contents of the tar file to the client
    while ((valread = read(tar_fd, buffer, BUFFER_SIZE)) > 0) {
        send(new_socket, buffer, valread, 0);
    }

    // Close the file descriptors
    close(tar_fd);
    int status = remove(archive_name);
    if (status == 0) {
        printf("temp.tar.gz removed successfully.\n");
    } else {
        printf("Unable to remove temp.tar.gz file.\n");
        perror("Error");
    }
}
//function to handle various commands from client

void processclient(int client_socket) {
    char command[1024];
    while (1) {
        // receive command from client
        ssize_t n = recv(client_socket, command, sizeof(command), 0);
        if (n == 0) {
            continue;
        }
        command[n] = '\0';
        char tar_filename[100];
        sprintf(tar_filename,"%d",getpid());
        strcat(tar_filename,"_temp.tar.gz");
        // parse command
        split_input_string(command);

        if (commandv[0] == NULL) {
            send(client_socket, "Invalid command\n", 16, 0);
            continue;
        }


        if (strcmp(commandv[0], "findfile") == 0) {


            // search for file
            search_for_file(client_socket, commandv[1]);

        } else if (strcmp(commandv[0], "sgetfiles") == 0) {

            int size1 = atoi(commandv[1]);
            int size2 = atoi(commandv[2]);
            char cmd[BUFFER_SIZE];
            snprintf(cmd, sizeof(cmd), "find \"$HOME\" -type f  -name \"*.*\" ! -name \"*.plist\" -size +%dc -size -%dc -print0 | tar -czf %s --null -T -",size1, size2,tar_filename);
            // send tar file to client
            create_tar_archive(cmd);
            send_tarfile(client_socket,tar_filename);

        }
        else if (strcmp(commandv[0], "dgetfiles") == 0){
            

            char cmd[BUFFER_SIZE];
            snprintf(cmd, sizeof(cmd),"find %s -type f  -name \"*.*\" ! -name \"*.plist\" -newermt '%s' ! -newermt '%s' -print0 | tar -czf %s --null -T -",getenv("HOME"),commandv[1], commandv[2], tar_filename);
            // send tar file to client
            create_tar_archive(cmd);
            send_tarfile(client_socket,tar_filename);
            
        }

        else if(strcmp(commandv[0],"getfiles")==0){

            char cmd[1000];
            char file[100];
            sprintf(cmd, "find ~/ -type f \\( -name '%s'", commandv[1]);
            for (int i=2;i<arg_Number;i++)
            {
                sprintf(file, " -o -name '%s'", commandv[i]);
                strcat(cmd, file);
            }
            strcat(cmd, " \\) -print0 | xargs -0 tar -czvf ");
            strcat(cmd,tar_filename);
            create_tar_archive(cmd);
            off_t tar_size = get_tar_size(tar_filename);
            if(tar_size==0){
                send(client_socket, "File Not Found\n", 15, 0);
                //send(client_socket, &tar_size, sizeof(tar_size), 0);
            }
            else{
                printf("%lld",tar_size);
                send(client_socket, "File Found\n\n\n\n\n", 15, 0);
                send_tarfile(client_socket,tar_filename);
            }   
        }
        else if(strcmp(commandv[0],"gettargz")==0){
            char cmd[1000];
            char file[100];
            sprintf(cmd, "find ~/ -type f \\( -name '*.%s'", commandv[1]);
            for (int i=2;i<arg_Number;i++)
            {
                sprintf(file, " -o -name '*.%s'", commandv[i]);
                strcat(cmd, file);
            }
            strcat(cmd, " \\) -print0 | xargs -0 tar -czvf ");
            strcat(cmd,tar_filename);
            create_tar_archive(cmd);
            off_t tar_size = get_tar_size(tar_filename);
            if(tar_size==0){
                send(client_socket, "File Not Found\n", 15, 0);
            }
            else{
                printf("%lld",tar_size);
                send(client_socket, "File Found\n\n\n\n\n", 15, 0);
                send_tarfile(client_socket,tar_filename);
            }   

        }
        
        else if (strcmp(commandv[0], "quit") == 0) {
            // end connection with client
            break;
        } 
         else {
            // invalid command
            send(client_socket, "Invalid command\n", 16, 0);
            continue;
        }
    }

    // close socket
    close(client_socket);
    kill(getppid(),SIGUSR1);
}



