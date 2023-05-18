/*
Mirror Author : Akhil Punchavallil Sivankuutan & Aiswarya Siby
Usage : ./mirror  
runs on port : 8088
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
#include <sys/mman.h>


#define PORT 8088
#define MAX_CONNECTIONS 15
#define BUFFER_SIZE 1024
#define MAX_PATH 4096
#define MAX_CMD 8192
#define MAX_ARGS 10

void processclient(int sockfd);

int arg_Number=0;
char* commandv[MAX_ARGS];
int client_Count =-1;
pid_t *child_pids;
int track_fd = 0;

// used to calculate number of process in mirror

void handler(int sig) {
    client_Count = client_Count-1;
    fprintf(stderr,"\nCurrent Number of Live connections in mirror : %d",client_Count);
}
// used to send response to server
void handler2(int sig) {
    int response =1;
    send(track_fd, &response, sizeof(int), 0);
    
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


int main(int argc, char const *argv[]) {
    int server_fd, new_socket, pid;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Create a shared memory region that can hold an integer
    int shm_fd = shm_open("my_shared_memory", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(int));
    child_pids = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    signal(SIGUSR1, handler);
    

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
    
     *child_pids=0;


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
            track_fd = new_socket;
            processclient(new_socket);
            exit(EXIT_SUCCESS);
        } else {
            // Parent process
            // only for first child process
            client_Count++;
            if (*child_pids == 0) {
                *child_pids = pid;
            }
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
    // Create temporary tar archive
    // char cmd[MAX_CMD];
    // snprintf(cmd, sizeof(cmd), "find \"$HOME\" -name \"*.*\" -type f -size +%dc -size -%dc -print0 | tar -czf %s --null -T -",*size_1, *size_2, archive_name);
    int status = system(cmd);
    if (status != 0) {
        fprintf(stderr, "Error creating tar archive\n");
        kill(getppid(),SIGUSR1);
        kill(*child_pids,SIGUSR2);
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
        kill(*child_pids,SIGUSR2);
        exit(EXIT_FAILURE);
    }

    // Get the size of the file
    struct stat st;
    if (stat(archive_name, &st) < 0) {
        perror("stat failed");
        kill(getppid(),SIGUSR1);
        kill(*child_pids,SIGUSR2);
        exit(EXIT_FAILURE);
    }
    off_t tar_size = st.st_size;

    // Send the size of the tar file to the client
    if (send(new_socket, &tar_size, sizeof(tar_size), 0) < 0) {
        perror("send failed");
        kill(getppid(),SIGUSR1);
        kill(*child_pids,SIGUSR2);
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
            snprintf(cmd, sizeof(cmd), "find \"$HOME\" -type f  -name \"*.*\" ! -name \"*.plist\" -size +%dc -size -%dc -print0 | tar -czf %s --null -T -",size1, size2, "temp.tar.gz");
            // send tar file to client
            create_tar_archive(cmd);
            send_tarfile(client_socket,"temp.tar.gz");

        }
        else if (strcmp(commandv[0], "dgetfiles") == 0){
            

            char cmd[BUFFER_SIZE];
            snprintf(cmd, sizeof(cmd),"find %s -type f  -name \"*.*\" ! -name \"*.plist\" -newermt '%s' ! -newermt '%s' -print0 | tar -czf %s --null -T -",getenv("HOME"),commandv[1], commandv[2], "temp.tar.gz");
            // send tar file to client
            create_tar_archive(cmd);
            send_tarfile(client_socket,"temp.tar.gz");
            
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
            strcat(cmd,"temp.tar.gz");
            create_tar_archive(cmd);
            off_t tar_size = get_tar_size("temp.tar.gz");
            if(tar_size==0){
                send(client_socket, "File Not Found\n", 15, 0);
                //send(client_socket, &tar_size, sizeof(tar_size), 0);
            }
            else{
                printf("%lld",tar_size);
                send(client_socket, "File Found\n\n\n\n\n", 15, 0);
                //send(client_socket, &tar_size, sizeof(tar_size), 0);
                //send(client_socket, &tar_size, 4, 0);
                send_tarfile(client_socket,"temp.tar.gz");
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
            strcat(cmd,"temp.tar.gz");
            create_tar_archive(cmd);
            off_t tar_size = get_tar_size("temp.tar.gz");
            if(tar_size==0){
                send(client_socket, "File Not Found\n", 15, 0);
            }
            else{
                printf("%lld",tar_size);
                send(client_socket, "File Found\n\n\n\n\n", 15, 0);
                send_tarfile(client_socket,"temp.tar.gz");
            }   

        }
        
        else if (strcmp(commandv[0], "quit") == 0) {
            // end connection with client
            break;
        }
        else if(strcmp(commandv[0], "server_bind")==0){

            fprintf(stderr,"\nserver connected sucesfully");
            signal(SIGUSR2, handler2);
            // this process run to communicate with server
            while(1){
                ;

            }

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
    kill(*child_pids,SIGUSR2);
}



