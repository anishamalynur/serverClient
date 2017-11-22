//  Inspiration drawn from:
//      socket programming:  https://www.cs.rutgers.edu/~pxk/417/notes/sockets/udp.html
//      get ip addr from channel: http://www.binarytides.com/hostname-to-ip-address-c-sockets-linux/
//      simultaneous input from both server & client: https://linux.die.net/man/3/fd_set

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "duckchat.h"
#include "raw.h"

#define MAX_USER 1000
#define MAX_CHANNEL 1000
#define MAX_SERVER 1000

struct aServer{
	sockaddr_in srv;
}

struct aUser{
	char username[32];
	sockaddr_in cli;
};

struct aChannel{
	char channelName[32];
	int subscribedNum;
	aUser* subscribedClients[100]; //max of 100 users per channel
	aServer* adjServers[MAX_SERVER] //to keep track of the adjacent servers for a particular channel
};

aUser* theUsers[MAX_USER]; // keeping track of users and their channels, server supports 1000 users
aChannel* theChannels[MAX_CHANNEL]; // keeping track of channels and users on them
aServer* theServers[MAX_SERVER]; //keeping track of the adjacent servers

int userIndex = 0;
int channelIndex = 0;

////////////////////////////////////////////////////// HELPER FUNCTIONS //////////////////////////////////////////////////////

int isSame( sockaddr_in ad1, sockaddr_in ad2){
	if(ad1.sin_addr.s_addr == ad2.sin_addr.s_addr && ad1.sin_port == ad2.sin_port){
		return 1;
	}
	return 0;
}

aUser* findUser(sockaddr_in* client){
	int i;
	for(i = 0; i < userIndex; i++){
		if(theUsers[i] -> cli.sin_addr.s_addr == client->sin_addr.s_addr && theUsers[i]->cli.sin_port == client->sin_port){
			//printf( "found user!! %s\n", theUsers[i] -> username);
			return theUsers[i];
		}
	}
	return NULL;	
}	

int findIndexUsers(aUser* user){
	int i;
	for(i = 0; i < userIndex; i++){
		if(isSame(theUsers[i]-> cli, user-> cli)){
			return i;
		}
	}
	return -1;
}

void removeChannelReindex(aChannel* removeCh, int removeIndex){
	if (removeCh -> subscribedNum == 0){
			int i;
		for(i = removeIndex; i < channelIndex; i++){
			theChannels[i] = theChannels[i+1];
		}
		channelIndex --;
		free(theChannels[removeIndex]);
	}
}

int userInChannel(aUser* checkUser, aChannel* channel){
	int i;
	for (i = 0; i < channel -> subscribedNum; i++){
		if(isSame(checkUser-> cli, channel->subscribedClients[i] ->cli)){
			return 1;
		}
	}
	return 0;
}


//////////////////////////////////////////////////////////  MAIN  //////////////////////////////////////////////////////////


int main(int argc, char *argv[]){
	raw_mode();
	atexit(cooked_mode);	
	
	struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;

	int sockfd;
	socklen_t clilen;

	char buffer[256];
	int n;
	int i = argc;

	if (( sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		perror("server: can’t open stream socket");
	}

	bzero((char *)&serv_addr, sizeof(serv_addr)); // set serv_addr to all 0s

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* host to network long*/
	serv_addr.sin_port = htons(atoi(argv[2])); // port number that user specifies

	if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
		perror("server: can’t bind local address");
	}	

	listen(sockfd, 5);

	clilen = sizeof(cli_addr);

	aChannel* newChannel = (aChannel*)malloc(sizeof(aChannel));
	strcpy(newChannel -> channelName, "Common");
	newChannel -> subscribedNum = 0;
	theChannels[channelIndex]= newChannel;
	channelIndex++;

	while(1){

		bzero(buffer,256);
		
		if((n = recvfrom(sockfd,buffer,255, 0,(struct sockaddr *)&cli_addr, &clilen)) > -1){

			if (n < 0){
				perror("ERROR reading from socket");
			}

			request_t reqtype = ((request*) buffer) -> req_type;

			//printf("\nthe request type is %d\n",  reqtype);
			printf("\n");

			switch(reqtype){

				case 0:{ //login

					printf("LOGIN HANDLER\n");

					aUser* newUser = (aUser*)malloc(sizeof(aUser));

					strcpy(newUser -> username, ((request_login*)buffer)-> req_username);
					newUser -> cli = cli_addr;

					theUsers[userIndex] = newUser;
					userIndex++;
					break;
				}
                
				case 1:{ //logout
					
					printf("LOGOUT HANDLER\n");
					
					aUser* logoutUser = findUser(&cli_addr);
					int removeIndex = findIndexUsers(logoutUser);
					
					int i;

					// reindex theUsers[] for logoutUser removal
					for(i = removeIndex; i < userIndex - 1; i++){
						 theUsers[i] = theUsers[i + 1];
					}

				   userIndex --; // one less user exists
		
					int a;
					int b;
					int c;
					// go through all the channels and remove the logoutUser and adjust the index
					//  a is the iterator in theChannels, b is the iterator of subscirbedClients, c iterator "reindexes"
					for(a = 0; a < channelIndex; a++){
						for(b = 0; b < theChannels[a] -> subscribedNum ; b++){
							// the logout user is on this channel
							if(isSame(theChannels[a] -> subscribedClients[b] -> cli, logoutUser-> cli)){
								printf("matched found in channel %s, user in channel %s, mathches logoutuser %s\n", theChannels[a] -> channelName, theChannels[a] -> subscribedClients[b] -> username, logoutUser-> username);
								for(c = b; c < ((theChannels[a] -> subscribedNum)); c++){
									printf("removing %s\n", theChannels[a] -> subscribedClients[b]-> username);
	 								theChannels[a] -> subscribedClients[c] = theChannels[a] -> subscribedClients[c +1];	
								}
								// if the channel has 0 users it gets freed
								theChannels[a] -> subscribedNum -= 1;
								removeChannelReindex(theChannels[a], a);
								break;					
							}
						}
					}
					free(logoutUser);
					break;
				}
		
				case 2:{ //join

					printf("JOIN HANDLER\n");

					char channel[32];
					strcpy(channel,((request_join*)buffer)-> req_channel);

					int i;
					int channelExists = 0;

					text_error er;
					er.txt_type = TXT_ERROR;
					
					aUser* joinedUser = findUser(&cli_addr);
	
					if(joinedUser == NULL){ // a user that does not exist tries to join a channel
						strcpy(er.txt_error, "You are not logged in, restart program");	
						sendto(sockfd,&er, sizeof(er), 0,(struct sockaddr *)&cli_addr, clilen );			
						break;
					}

					for(i = 0; i < channelIndex; i++){
						//check if the channel exists in theChannels (if not create one)
						if(strcmp(theChannels[i] -> channelName, channel) == 0){
							if(userInChannel(joinedUser, theChannels[i])){
								channelExists = 1;
								strcpy(er.txt_error, "user is already on the channel");	
								sendto(sockfd,&er, sizeof(er), 0,(struct sockaddr *)&cli_addr, clilen );			
								break;
							} 
							printf("a channel that exists was found: %s\n", theChannels[i] -> channelName);
							theChannels[i] -> subscribedClients[theChannels[i] -> subscribedNum] = joinedUser;
							printf("user %s joined \n",theChannels[i] -> subscribedClients[theChannels[i] -> subscribedNum] -> username);							
							theChannels[i] -> subscribedNum += 1;
							printf("total subscribed to this channel is %d\n", theChannels[i] -> subscribedNum); 
							channelExists = 1;
							break;
						}
					}
					
					// channel does not exist create one	
					if(!channelExists){					
					aChannel* newChannel = (aChannel*)malloc(sizeof(aChannel));
					//newChannel -> channelName = {'\0'};
					strcpy(newChannel -> channelName, channel);
					newChannel -> subscribedClients[0] = joinedUser;
					printf("%s joined new channel, %s\n", joinedUser-> username, channel);
					newChannel -> subscribedNum = 1;
					theChannels[channelIndex]= newChannel;
					channelIndex++;
					}
					break;
				}
                
				case 3:{ //leave
			
					printf("LEAVE HANDLER\n");
					text_error er;
					er.txt_type = TXT_ERROR;

					aUser* leaveUser = findUser(&cli_addr);;
	
					if(leaveUser == NULL){ // a user that does not exist tries to join a channel
						strcpy(er.txt_error, "You are not logged in, restart program");	
						sendto(sockfd,&er, sizeof(er), 0,(struct sockaddr *)&cli_addr, clilen );			
						break;
					}

					char channel[32];
					strcpy(channel,((request_leave*)buffer)-> req_channel);		

					int notFound = 1;	
					int noUserExists = 1;	

					int a;
					int b;
					int c;
					// go through all the channels and remove the leaveUser and adjust the index
					//  a is the iterator in theChannels, b is the iterator of subscirbedClients, c iterator "reindexes"
					for(a = 0; a < channelIndex; a++){
						// check if this is the channel the user wants to leave
						if( strcmp(theChannels[a] -> channelName, channel) == 0){
							for(b = 0; b < theChannels[a] -> subscribedNum ; b++){
								// the logout user is on this channel
								if(isSame(theChannels[a] -> subscribedClients[b] -> cli, leaveUser-> cli)){

									printf("user %s found on channel %s\n", theChannels[a] -> channelName, leaveUser-> username);				
									notFound = 0; // found a match no error message needed
									noUserExists = 0;// a matched user exists no error message needed
									
									// need to reindex the users on channel 
									for(c = b; c < ((theChannels[a] -> subscribedNum)); c++){
										printf("switching %s\n", theChannels[a] -> subscribedClients[b]-> username);
		 								theChannels[a] -> subscribedClients[c] = theChannels[a] -> subscribedClients[c +1];	
									}
									// if the channel has 0 users it gets freed
									theChannels[a] -> subscribedNum -= 1;
									removeChannelReindex(theChannels[a], a);
									break;				
								}
							}
							if(noUserExists){
								strcpy(er.txt_error, "User does not exist on the channel");	
								notFound = 0;
								sendto(sockfd,&er, sizeof(er), 0,(struct sockaddr *)&cli_addr, clilen );
								break; 
							}
						}
					}
					if(notFound){
						strcpy(er.txt_error, "Sorry that channel does not exist");	
						if((n= sendto(sockfd,&er, sizeof(er), 0,(struct sockaddr *)&cli_addr, clilen ) < -1)){
											printf("ERROR writing to socket\n");			
						break;
						}
					}
					break;
				}

				case 4:{ // request say

					printf("SAY HANDLER\n");
					char channel[32];
					char message[64];

					aUser* userSaid = findUser(&cli_addr);
					text_error er;
					er.txt_type = TXT_ERROR;

					if(userSaid == NULL){ // a user that does not exist tries to join a channel
						strcpy(er.txt_error, "You are not logged in, restart program");	
						sendto(sockfd,&er, sizeof(er), 0,(struct sockaddr *)&cli_addr, clilen );			
						break;
					}

					printf("the user who is sending request is %s\n" , userSaid -> username);
					memcpy(channel,((request_say*)buffer)-> req_channel, 32);
					printf("the channel written to is before loop: %s\n", channel);
					memcpy(message,((request_say*)buffer)-> req_text, 64);
					printf("the message written: %s\n", message);
					
					int i;
					for(i = 0; i < channelIndex; i++){
						if(strcmp(theChannels[i] -> channelName, channel) == 0){
							//iterate through subscribed users and send the message to eachone
							int j;
							printf("the number users on this channel is %d\n",theChannels[i] -> subscribedNum );
							for(j = 0; j< (theChannels[i] -> subscribedNum); j++){
									text_say sendingSay;
									sendingSay.txt_type = 0;
									strcpy(sendingSay.txt_channel, channel);
									strcpy(sendingSay.txt_username, userSaid -> username);
									strcpy(sendingSay.txt_text, message);

									int cliLen = sizeof(theChannels[i]->subscribedClients[j]->cli);

									if((n= sendto(sockfd,&sendingSay, sizeof(sendingSay), 0,(struct sockaddr *)&theChannels[i]->subscribedClients[j]->cli, cliLen ) < -1)){
										printf("ERROR writing to socket\n");
									}
							}
						}
					}
					break;
				}

                case 5:{ // list
					printf("LIST HANDLER\n");
					
					aUser* userList = findUser(&cli_addr);
					text_error er;
					er.txt_type = TXT_ERROR;

					if(userList == NULL){ // a user that does not exist tries to join a channel
						strcpy(er.txt_error, "You are not logged in, restart program");	
						sendto(sockfd,&er, sizeof(er), 0,(struct sockaddr *)&cli_addr, clilen );			
						break;
					}

					struct text_list *sendingList = (struct text_list *) malloc(sizeof(text_list) + sizeof(channel_info) * channelIndex);
					sendingList->txt_type = TXT_LIST;
					sendingList->txt_nchannels = channelIndex;
					
					for(i = 0; i < channelIndex; i++){
						strcpy(((sendingList->txt_channels)+i)->ch_channel, theChannels[i] -> channelName);
						printf("channel to be listed %s\n", theChannels[i] -> channelName);
					}
					
					int sizeLen = sizeof(text_list) + sizeof(channel_info) * channelIndex;
					if((n= sendto(sockfd,sendingList, sizeLen, 0,(struct sockaddr *)&cli_addr, clilen ) < -1)){
										printf("ERROR writing to socket\n");
					}
					free(sendingList);	
					break;		
				}

            case 6:{ // who
					printf("WHO HANDLER\n");
					struct text_who* whoSend;
					char whoChannel[32];
					strcpy(whoChannel, ((request_who*)buffer) -> req_channel);

					int foundChannel = 0;
	
					aUser* userWho = findUser(&cli_addr);
					text_error er;
					er.txt_type = TXT_ERROR;

					if(userWho == NULL){ // a user that does not exist tries to join a channel
						strcpy(er.txt_error, "You are not logged in, restart program");	
						sendto(sockfd,&er, sizeof(er), 0,(struct sockaddr *)&cli_addr, clilen );			
						break;
					}
					int i;
					for(i = 0; i < channelIndex ; i++){
							if(strcmp(theChannels[i] -> channelName, whoChannel) == 0){
								int UsersOnChannel = theChannels[i] -> subscribedNum;
								whoSend = (struct text_who *) malloc(sizeof(text_who) + sizeof(user_info) * UsersOnChannel);
								int j;
								for(j = 0; j < UsersOnChannel; j++){	
									strcpy(((whoSend->txt_users)+j)->us_username, theChannels[i] -> subscribedClients[j] -> username);
									//printf("user to be listed %s\n", theChannels[i] -> subscribedClients[j] -> username);
								}
								whoSend->txt_nusernames = UsersOnChannel;
								printf("there are %d for who to list\n", whoSend->txt_nusernames);
								strcpy(whoSend->txt_channel, whoChannel);
								whoSend->txt_type = TXT_WHO;
								foundChannel = 1;
							}
					}
					if(!foundChannel){
							strcpy(er.txt_error, "Sorry that channel does not exist");	
							if((n= sendto(sockfd,&er, sizeof(er), 0,(struct sockaddr *)&cli_addr, clilen ) < -1)){
										printf("ERROR writing to socket\n");
						}
						break;
					}	
					int whoSize = sizeof(text_who) + sizeof(user_info) * whoSend->txt_nusernames;
					if((n= sendto(sockfd,whoSend, whoSize, 0,(struct sockaddr *)&cli_addr, clilen ) < -1)){
										printf("ERROR writing to socket\n");
					}
					free(whoSend);
					break;	
				}

				case 7:{ // S2S join
					printf("S2s join HANDLER\n");
					
				}

				case 8:{ // S2S say
					printf("S2s say HANDLER\n");
					
				}

				case 9:{ // S2S leave
					printf("S2s leave HANDLER\n");
					
				}
				default:{
					printf("Invalid packet was sent\n");
				}
			}
		}
	}
	close(sockfd);
	return 0;
}


		
