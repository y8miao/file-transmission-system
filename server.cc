#include <iostream>
#include <fstream>
#include <string.h>
#include <stdio.h>
#include <string>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <poll.h>
#include <vector>
#include <map>

using namespace std;
static volatile int NON_ACCEPT = 0;

int main(void){
	//initialize variables
	int temp = 0, i;
	int rc = 1, nfds = 1, current_size = 0, new_socket = -1;
	vector<struct pollfd> fds;
	multimap<string, pair<int, int> > look_up;
	char buffer[1024];

	//set up socket
	struct sockaddr_in recv_addr, send_addr; //how do i send msg to different clients with 1 socket
	unsigned int sa_len = sizeof(send_addr);
	int recv_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(recv_socket < 0) perror("ERROR opening socket\n");

	//bind socket	
	memset((char *)&recv_addr, 0, sizeof(recv_addr));
	recv_addr.sin_port = htons(0);
	recv_addr.sin_family = AF_INET;
	recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if(bind(recv_socket, (struct sockaddr *) &recv_addr, sizeof(recv_addr)) < 0) perror("ERROR on binding\n");
	if(listen(recv_socket, 10) < 0) perror("listen() failed\n");
	//set up the listening socket
	struct pollfd listen;
	listen.fd = recv_socket;
	listen.events = POLLIN;
	fds.push_back(listen);
	int timeout = (60 * 1000);
	//cout<<fds.size()<<endl;

	//store and print out address for client
	if(getsockname(recv_socket, (struct sockaddr *) &send_addr, &sa_len) < 0) perror("ERROR on getsockname\n");
	int port_num = ntohs(send_addr.sin_port);
	ofstream port_file("port");
	cout<<port_num<<endl;
	port_file<<port_num<<endl;
	port_file.close();

	//receive calls from clients
	while(1){
		rc = poll(&(fds[0]), nfds, timeout);
		if(rc < 0){
			perror("poll failed\n");
			return 1;
		}
		if(rc == 0){
			perror("poll time out.\n");
			return 1;
		}

		//go through the descriptors
		current_size = nfds; //why current size
		//cout<<current_size<<endl;
		for(i = 0; i < current_size; i++){
			//cout<<"checking fd list: "<<i<<","<<fds[i].fd<<endl;
			//no event
			if(fds[i].revents == 0){
				continue;
			}
			if(fds[i].revents != POLLIN){
				//cout<<i<<" , "<<fds[i].fd<<endl;
				printf("error:revents = %d, fd= %d, errno: %d\n", fds[i].revents, fds[i].fd, errno);
				return 1;
			}

			//listening socket readable
			if(fds[i].fd == recv_socket && NON_ACCEPT == 0){
				//accept new clients in queue
				new_socket = accept(recv_socket, NULL, NULL);
				//cout<<"accept new client:"<<new_socket<<endl;
				if (new_socket < 0){
						perror("accept failed\n");
						return 1;
				}
				struct pollfd new_fd;
				new_fd.fd = new_socket;
				new_fd.events = POLLIN;
				fds.push_back(new_fd);
				nfds++;
			}
			else if(fds[i].fd == recv_socket && NON_ACCEPT == 1){
				//don't accept
				//cout<<"not accepting after NON_ACCEPT"<<endl;
				new_socket = accept(recv_socket, NULL, NULL);
				if(new_socket < 0){
					perror("accept failed\n");
					return 1;
				}
				close(new_socket);
			}
			//socket readable but not listening socket
			else{
				multimap<string, pair<int, int> >::iterator it;
				//check if socket already exist, ask about order and infinite block
				//cout<<"check socket exist:"<<fds[i].fd<<endl;
				for(it=look_up.begin(); it!=look_up.end(); it++){
					//cout<<"map entry:"<<it->first<<","<<it->second.first<<","<<it->second.second<<endl;
					if((it->second).first == fds[i].fd) break;
				}
				//new socket
				if(it == look_up.end()){
					char key_buffer[9];
					temp = recv(fds[i].fd, key_buffer, sizeof(key_buffer), 0);
					if(temp < 0){
						perror("recv command failed.\n");
						return 1;
					}
					char command = key_buffer[0];
					if(command == 'F'){
						//clearn up
						//cout<<"set NON_ACCEPT to 1"<<endl;
						NON_ACCEPT = 1;
						fds.erase(fds.begin()+i);
						nfds--;
						current_size--;
						i--;
						//cout<<"fds size: "<<fds.size()<<endl;
						//no transmition running, close listening socket and return
						if(fds.size() == 1){
							//cout<<"terminating"<<endl;
							close(fds[0].fd);
							return 0;
						}
					}
					else if(command == 'G'){
						//downloader
						char a_key[9];
						strcpy(a_key, &(key_buffer[1]));
						a_key[8] = '\0'; //ask this
						string key(a_key);
						multimap<string, pair<int, int> >::iterator it2;
						for(it2=look_up.begin();it2!=look_up.end();it2++){
							//not match key or not available uploader
							if(it2->first.compare(key) != 0 || (it2->second).second != -1) continue;
							//key match and uploader, match them and put uploader into listening socket list
							else{
								//update uploader info
								(it2->second).second = fds[i].fd;
								//update downloader info
								pair<int, int> down_info(fds[i].fd, (it2->second).first);
								look_up.insert(pair<string, pair<int, int> >(key, down_info));
								//cout<<"add to map: "<<key<<","<<down_info.first<<","<<down_info.second<<endl;
								//add uploader into listening list, remove downloader
								fds[i].fd = (it2->second).first;
								fds[i].events = POLLIN;
								break; //ask about break
							}
						}
						//no matching found
						if(it2==look_up.end()){
							//add to look up table, remove from listening list
							pair<int, int> down_info(fds[i].fd, 0);
							look_up.insert(pair<string, pair<int, int> >(key, down_info));
							//cout<<"add to map: "<<key<<","<<down_info.first<<","<<down_info.second<<endl;
							fds.erase(fds.begin()+i); //ask i or i+1
							nfds--;
							current_size--;
							i--;
						}
					}
					else if(command == 'P'){
						//uploader
						char a_key[9];
						strcpy(a_key, &(key_buffer[1]));
						a_key[8] = '\0'; //ask this
						string key(a_key);
						multimap<string, pair<int, int> >::iterator it2;
						for(it2=look_up.begin();it2!=look_up.end();it2++){
							//key doesnt match or not a downloader
							if(it2->first.compare(key) != 0||(it2->second).second != 0) continue;
							//key match and downloader
							else{
								//update download info
								(it2->second).second = fds[i].fd;
								//add entry for uploader
								//cout<<"upload info: "<<fds[i].fd<<" "<<it2->second.first<<endl;
								pair<int,int> up_info(fds[i].fd, (it2->second).first);
								look_up.insert(pair<string, pair<int, int> >(key, up_info));
								//cout<<"add to map: uploader "<<key<<","<<up_info.first<<","<<up_info.second<<endl;
								break;
							}
						}
						//no matching found
						if(it2==look_up.end()){
							//add to look up table, remove from listening list
							pair<int, int> up_info(fds[i].fd, -1);
							look_up.insert(pair<string, pair<int,int> >(key, up_info));
							//cout<<"add to map: uploader "<<key<<","<<up_info.first<<","<<up_info.second<<endl;
							fds.erase(fds.begin()+i);
							nfds--;
							current_size--;
							i--;
						}
					}
					else{
						printf("invalid command input.\n");
						return 1;
					}
				}
				//existing socket
				else{
					//transmit
					int get_socket = (it->second).first;
					int put_socket = (it->second).second;
					//cout<<"transmit from "<<get_socket<<" to "<<put_socket<<endl;
					temp = recv(get_socket, buffer, sizeof(buffer), 0);
					//cout<<"temp value: "<<temp<<endl;
					if(temp < 0){
						cout<<"error socket: from "<<get_socket<<" to "<<put_socket<<endl;
						perror("recv file failed.\n");
						return 1;
					}
					//uploader closed, clean up
					else if(temp == 0){
						//find downloader
						multimap<string, pair<int,int> >::iterator it2;
						for(it2=look_up.begin();it2!=look_up.end();it2++){
							if((it2->second).first == put_socket) break;
						}
						if(it2 == look_up.end()){
							perror("error uploader closed, downloader not found.\n");
							return 1;
						}
						//cout<<"finished transmit: "<<fds.size()<<endl;
						look_up.erase(it);
						look_up.erase(it2);
						fds.erase(fds.begin()+i);
						nfds--;
						current_size--;
						close(get_socket);
						close(put_socket);
						i--;
						//cout<<"finished cleanup: "<<fds.size()<<endl;
						//last set of fd and NON_ACCEPT, terminate
						if(fds.size() == 1 && NON_ACCEPT == 1){
							//cout<<"terminate"<<endl;
							close(fds[0].fd);
							return 0;
						}
					}
					else{
						i = send(put_socket, buffer, temp, 0);
						if(i < 0){
							perror("send file failed.\n");
							return 1;
						}
					}
				}
			}
		}
	}
}
