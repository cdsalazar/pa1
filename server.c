//First commit

#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>

#define MAX_CONNECTIONS 1000
#define MAX_SUPPORTED_FILE_TYPES 250
#define BUFFER_SIZE 1024
#define MAX_RETRIES 100
#define MAX_MSG_LEN 99999

int connected_clients[MAX_CONNECTIONS];
char *supported_file_types[MAX_SUPPORTED_FILE_TYPES];
char *PORT;

void respond(int client_number){
  //Create variable for root directory
	char *root = getenv("PWD"); //The Linux Command: PWD- Print Working Directory

    char mesg[MAX_MSG_LEN], *request_str[3], send_data[BUFFER_SIZE], path[MAX_MSG_LEN];
    int request, fd, bytes_read;

    //initialize mesg variable with nul terminators
    memset((void*)mesg, (int)'\0', MAX_MSG_LEN);

    //Receive message/request from socket
    request = recv(connected_clients[client_number], mesg, MAX_MSG_LEN, 0);

    //There was an error with request
    if (request < 0)
        fprintf(stderr,("Error recv(): Failed to Recieve Request from Socket\n"));
    //Socket is closed
    else if (request == 0)
        fprintf(stderr,"Client Disconnected.\n");
    //The message is received
    else{
        //Get tokens from msg
        request_str[0] = strtok(mesg, " \t\n");
        //If GET request
        if (strncmp(request_str[0], "GET\0", 4)==0){
            //Extract the parameters of GET request
            request_str[1] = strtok (NULL, " \t");
            request_str[2] = strtok (NULL, " \t\n");

            //Handle a bad request
            if ( strncmp( request_str[2], "HTTP/1.0", 8)!=0 && strncmp( request_str[2], "HTTP/1.1", 8)!=0 ){
            	char *bad_request_str = "HTTP/1.1 400 Bad Request: Invalid HTTP-Version: %s\n", request_str[2];
            	int bad_request_strlen = strlen(bad_request_str);
                write(connected_clients[client_number], bad_request_str, bad_request_strlen);
            }
            //Handle request
            else{
              //Start at root directory and find index.html
                if ( strncmp(request_str[1], "/\0", 2)==0 )
                    request_str[1] = "/index.html";

                //Create file path
                strcpy(path, root);
                strcpy(&path[strlen(root)], request_str[1]);
                //Get the data user requests
                if ( (fd = open(path, O_RDONLY)) != -1 ){
                  printf("Handling a Request.\n");
                    send(connected_clients[client_number], "HTTP/1.1 200 OK\n\n", 17, 0);
                    while ( (bytes_read = read(fd, send_data, BUFFER_SIZE)) > 0 )
                        write (connected_clients[client_number], send_data, bytes_read);
                }
                //Otherwise, 404: File Not Found
                else{
                	char *not_found_str = "HTTP/1.1 404 Not Found %s\n", path;
                	int not_found_strlen = strlen(not_found_str);
                	write(connected_clients[client_number], not_found_str, not_found_strlen);
                }
            }
        }
    }

    //Shut down and re-open/re-initialize the client_number
    shutdown(connected_clients[client_number], SHUT_RDWR);
    close(connected_clients[client_number]);
    connected_clients[client_number] = -1;
}

void initialize_server(char *port){

  //Initialize variables
	int init_socket, i;
	socklen_t addrlen;
	char *buffer = malloc(BUFFER_SIZE);
	struct sockaddr_in address;

  //Initialize array
	for(i = 0; i < MAX_CONNECTIONS; i++){
		connected_clients[i] = -1;
	}
  //create a socket, returns a file descriptor
	if ((init_socket = socket(PF_INET, SOCK_STREAM, 0)) > 0){
		printf("Socket Created.\n");
	}
  else{
    printf("Failed to Create Socket.\n");
    exit(EXIT_FAILURE);
  }

  //Fill Socket Address Struct
	address.sin_family = AF_INET; //IP addresses from internet
  address.sin_port = htons(3000); //port number
	address.sin_addr.s_addr = INADDR_ANY; //IP address from any computer

  //Set a socket option
	int option_value = 1;
  //allows a process to bind to a port which remains in TIME_WAIT (reuse addresses)
	if ( setsockopt(init_socket, SOL_SOCKET, SO_REUSEADDR, &option_value, sizeof(int)) == -1 ){
	    perror("setsockopt");
	}

	int retry = 0;
	while(retry < MAX_RETRIES){
    //Bind local socket address (&address) to socket (init_socket)
		if (bind(init_socket, (struct sockaddr *) &address, sizeof(address)) == 0){
			printf("Success: Bind Socket\n");
			break;
		}
		else{
      //Retry until MAX_RETRIES has been reached
			retry++;
			if(retry == MAX_RETRIES-1){
				printf("Error: Maximum Retries Reached. Failed to Bind Socket\n");
				exit(EXIT_FAILURE);
			}
		}
	}

  //Listen for Connections
  //Enter sleep to await incoming connections
	if (listen(init_socket, 1000) < 0){
		perror("Server: Failed to Listen");
		exit(EXIT_FAILURE);
	}

	int client_number = 0;
	while (1){
    //Accept the next/first connection from queue of pending connections
    //also creates new socket and new file descriptor for that socket
		if ((connected_clients[client_number] = accept(init_socket, (struct sockaddr *) &address, &addrlen)) > 0){
      //fork a new process for this client
				if (fork() == 0){
          //Call function respond for each client
					respond(client_number);
					client_number += 1;
					exit(EXIT_SUCCESS);
				}
		}
		else{
			perror("Server: Failed to Accept");
			exit(EXIT_FAILURE);
		}
	}
}


int main(int argc, char* argv[]){
  //Pass in config file by using cmdline args
	if(argc > 1){
    //initialize variables for file read
    char *path = argv[1];
    int bytes_read, fd, i;
    char *file_type;
  	char *send_data = malloc(BUFFER_SIZE);

  	for(i = 0; i < MAX_SUPPORTED_FILE_TYPES; i++){
  		supported_file_types[i] = "";
  	}

    int n = 0;
    //Open the file
  	if((fd = open(path, O_RDONLY))!=-1){
      //Read from the file and store in send_data
  		 while((bytes_read = read(fd, send_data, BUFFER_SIZE)) > 0){
         //create tokens
  		 	char *tok = strtok(send_data, " \t\n");
  		 	while(tok != NULL){
           //Save the PORT number
  		 		if(strcmp(tok, "Listen") == 0){
  		 			tok = strtok(NULL, " \t\n");
  		 			PORT = tok;
  		 		}
           //Save the file extentions from the config file
  		 		else if(tok[0] == '.'){
  		 			char tok_copy[strlen(tok)];
            strcpy(tok_copy, tok);

  		 			char file_ext[strlen(tok) - 1];
  		 			int i, j;

  		 			for(i = 1, j = 0;i < strlen(tok);i++, j++){
  		 				file_ext[j] = tok_copy[i];
  		 			}

  		 			file_type = file_ext;
  		 			supported_file_types[n] = file_type;
  		 			n++;
  		 		}

  		 		tok = strtok(NULL, " \t\n");
  		 	}
  		 }
  	}
    //Exit if there is no config file found
  	else{
  		printf("Not Found: Configuration File\n");
  		exit(EXIT_FAILURE);
  	}

    //After Config file is processed,
    //initialize the server
		initialize_server(PORT);

    //exit with success
		return 0;
	}
  //if there are no cmdline args, give the user instructions
	else{
		printf("Run Command: ./server <input user config file>\n");
	}
}
