#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

using namespace std;

int main(int argc, char * argv[]){
	//read in command line inputs
	int port = stol(argv[2]);

	//set up buffer
	char key_buffer[9];
	memset((char *) &key_buffer, '\0', 9);
	int temp;

	//initialize the socket
	int sender_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(sender_socket < 0) perror("ERROR on sender_socket");

	//connect socket
	struct sockaddr_in server_addr;
	struct addrinfo * result = NULL;
	struct addrinfo hint = {0, AF_INET, SOCK_STREAM,0,0,NULL,NULL,NULL};
	memset((char *)&server_addr, 0, sizeof(server_addr));
	int err = getaddrinfo(argv[1], NULL, &hint, &result);
	if(err < 0){
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
	}
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr = ((struct sockaddr_in *)(result->ai_addr))->sin_addr;//question
	temp = connect(sender_socket, (struct sockaddr *) &server_addr, sizeof(server_addr));
	//terminate
	if(argc == 4){
		if(strlen(argv[3]) > 1) perror("invalid length argument 3.\n");
		strcpy(key_buffer, argv[3]);
		send(sender_socket, key_buffer, sizeof(key_buffer), 0);
		close(sender_socket);
		return 0;
	}
	//download
	else if(argc == 6){
		//read in command line inputs
		if(strlen(argv[3]) > 9) perror("invalid length argument 3.\n");
		strncpy(key_buffer, argv[3],strlen(argv[3]));
		long int buffer_length = stol(argv[5]);
		char recv_buffer[buffer_length];
		ofstream out_file(argv[4]);
	
		send(sender_socket, key_buffer, sizeof(key_buffer), 0);

		//download file
		while(1){
			temp = recv(sender_socket, recv_buffer, sizeof(recv_buffer), 0);
			//cout<<temp<<endl;
			if(temp < 0) perror("error on recv\n");
			if(temp == 0) break;
			out_file.write(recv_buffer, temp);
		}

		//cleanup
		out_file.close();
		close(sender_socket);
		return 0;
	}
	//upload
	else if(argc == 7){
		//read in command line inputs
		if(strlen(argv[3]) > 9) perror("invalid length argument 3.\n");
		strncpy(key_buffer, argv[3],strlen(argv[3]));
		int wait_time = stol(argv[6]);
		long int buffer_length = stol(argv[5]);
		char send_buffer[buffer_length];
		ifstream in_file(argv[4]);
		
		long int file_length = strtol(argv[3], NULL, 10);
		//not a virtual file
		if(file_length <= 0){
                	//get length of file
               		in_file.seekg(0, in_file.end);
                	file_length = in_file.tellg();
                	in_file.seekg(0, in_file.beg);
        	}

		send(sender_socket, key_buffer, sizeof(key_buffer), 0);
		//upload file
		while(file_length > 0){
			in_file.read(send_buffer, buffer_length);
			if(in_file){
				temp = send(sender_socket, send_buffer, buffer_length, 0);
				if(temp < 0) perror("upload send call failure.\n");
				file_length -= temp;
			}
			else{
				temp = send(sender_socket, send_buffer, in_file.gcount(), 0);
				if(temp < 0) perror("upload send call failure, last msg.\n");
				file_length -= temp;
				if(file_length > 0) perror("error:file length not 0 after last msg.\n");
			}
			//wait time
			usleep(wait_time);
		}

		//cleanup
		in_file.close();
		close(sender_socket);
		return 0;
	}
	else{
		perror("invalid input argument.");
		return 1;
	}
}
