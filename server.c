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
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <uuid/uuid.h>
#include "duckchat.h"
#include "raw.h"

#define MAX_USER 1000
#define MAX_CHANNEL 1000
#define MAX_SERVER 1000
#define MAX_MESSAGE 1000
#define MAX_HOSTNAME 100

struct aServer{
    char srv_name[MAX_HOSTNAME+32]; //keep track of ip:port
    sockaddr_in srv;
};

struct aUser{
	char username[32];
	sockaddr_in cli;
};

struct aChannel{
	char channelName[32];
	int subscribedNum;
	aUser* subscribedClients[100]; //max of 100 users per channel
	aServer* adjServers[MAX_SERVER]; //keep track of adjacent servers for a particular channel
	int adjServersNum; //keep track of number of asjacent servers for a particular channel
};

aUser* theUsers[MAX_USER]; //keep track of users and their channels, server supports 1000 users
aChannel* theChannels[MAX_CHANNEL]; //keep track of channels and users on them
aServer* theServers[MAX_SERVER]; //keep track of adjacent servers

int userIndex = 0;
int channelIndex = 0;
int serverIndex = 0;

int sockfd;
aServer *this_srv; 
char msg_nums[MAX_MESSAGE][8]; //keep track of unique message number identifiers
int index_msg_num = 0; //what msg_num program is on^


/////////////////////////////////////////  HELPER FUNCTIONS  /////////////////////////////////////////

int isSame(sockaddr_in ad1, sockaddr_in ad2){
	if(ad1.sin_addr.s_addr == ad2.sin_addr.s_addr && ad1.sin_port == ad2.sin_port){
		return 1;
	}
	return 0;
}

aUser* findUser(sockaddr_in* client){
    int i;
    for(i = 0; i < userIndex; i++){
        if(isSame(theUsers[i]->cli, *client)){
            return theUsers[i];
        }
    }
    return NULL;
}

aChannel* findChannel(char* ch){
    int i;
    for(i = 0; i < channelIndex; i++){
        if(strcmp(theChannels[i]->channelName, ch)==0){
            return theChannels[i];
        }
    }
    return NULL;
}

aServer* findServer(sockaddr_in* client){
    int i;
    for(i = 0; i < serverIndex; i++){
        if(isSame(theServers[i]->srv, *client)){
            return theServers[i];
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
	if(removeCh -> subscribedNum == 0){
			int i;
		for(i = removeIndex; i < channelIndex; i++){
			theChannels[i] = theChannels[i+1];
		}
		channelIndex --;
		free(theChannels[removeIndex]);
	}
}

void removeAdjServerReindex(aServer* removeSrv, aChannel* ch){
	int i;
	for(i = 0; i < ch->adjServersNum; i++){
		if(isSame(removeSrv->srv, ch->adjServers[i]->srv)){
            printf("found a matching srv in remove adjserverReindex\n");
            int j;
            for(j = i; j < ch ->adjServersNum; j++){
                ch->adjServers[j] = ch->adjServers[j +1];
            }
            ch -> adjServersNum -= 1;
		}
	}
}

int userInChannel(aUser* checkUser, aChannel* channel){
	int i;
	for (i = 0; i < channel->subscribedNum; i++){
		if(isSame(checkUser->cli, channel->subscribedClients[i]->cli)){
			return 1;
		}
	}
	return 0;
}

int serverInChannel(aServer* checkServer, aChannel* channel){
    int i;
    for (i = 0; i < channel->adjServersNum; i++){
        if(isSame(checkServer->srv, channel->adjServers[i]->srv)){
            return 1;
        }
    }
    return 0;
}

void broadcast_join_message(aChannel* channel) {
    printf("BROADCASTING JOIN\n");
    struct request_s2s_join join_msg;
    join_msg.req_type = REQ_S2S_JOIN;
    strcpy(join_msg.req_channel, channel->channelName);
    int i, f;
    f=0;
    for(i=0; i< channel->adjServersNum; i++){
        // Naieve Broadcast
        if (sendto(sockfd, &join_msg, sizeof(join_msg), 0, (struct sockaddr*)&(channel->adjServers[i]->srv), sizeof(channel->adjServers[i]->srv)) < 0 ) {
            perror("Message failed");
        }
        else {
            f=1;
            printf("%s %s send s2s_join on %s\n", this_srv->srv_name, channel->adjServers[i]->srv_name, channel->channelName);
        }
    }
    if (f) printf("Broadcasted Join!\n");
    else printf("Nowhere to forward Join\n");
}

void broadcast_say_message(aServer* origin_server, char* channel, char* message, char* username) {
    printf("BROADCASTING SAY\n");
	struct request_s2s_say say_msg;
	say_msg.req_type = REQ_S2S_SAY;
	//long long uni[8] = {'\0'};
	char uni[8] = {'\0'};
	memcpy(uni, msg_nums[index_msg_num-1], 8);
	memcpy(say_msg.uni_num, uni, 8);
		//strcpy(say_msg.uni_num, uni);
	strcpy(say_msg.req_channel,channel);
	strcpy(say_msg.req_text, message);
	strcpy(say_msg.req_username, username);
	aChannel* chan = findChannel(channel);
	int f=0;
 	//printf("adj server num is %d\n", chan-> adjServersNum);
    int i;
    for(i=0; i<chan->adjServersNum; i++){
        if (!isSame(chan->adjServers[i]->srv, origin_server->srv)) {
            //Broadcast
            if (sendto(sockfd, &say_msg, sizeof(say_msg), 0, (struct sockaddr*)&(chan->adjServers[i]->srv), sizeof(chan->adjServers[i]->srv)) < 0 ) {
                    // need to check if 0th server does not match this_srv
                	perror("Message failed");
            	}
            else {
                f=1;
                 printf("%s %s send s2s_say on %s\n", this_srv->srv_name, chan->adjServers[i]->srv_name, chan->channelName);
            }
        }
    }
    if (f) printf("Broadcasted SAY!\n");
    else{
		printf("Nowhere to forward SAY\n");
		if(chan->subscribedNum == 0 && chan->adjServersNum == 1){
            //removeAdjServerReindex(origin_server,chan); <- NO!
            struct request_s2s_leave leave_msg;
            leave_msg.req_type = REQ_S2S_LEAVE;
            strcpy(leave_msg.req_channel, chan->channelName);
            int n;
            if((n= sendto(sockfd,&leave_msg, sizeof(leave_msg), 0,(struct sockaddr *)&origin_server->srv, sizeof(origin_server->srv) ) < -1)){
                printf("ERROR writing to socket\n");
            }
            printf("%s %s send s2s_leave on %s\n", this_srv->srv_name, chan->adjServers[i]->srv_name, chan->channelName);
		}
	} 
}

/*void signal_handler(int num) {
    // handle the logic for soft state
    // send a s2s_join every 60 seconds
    // remove servers that haven't sent you a join in more than 120 seconds
    //alarm(60);
}*/

//////////////////////////////////////////  MAIN FUNCTION  //////////////////////////////////////////


int main(int argc, char *argv[]){
	raw_mode();
	atexit(cooked_mode);
	struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;
	socklen_t clilen;
	char buffer[256];
	int n;
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		perror("server: can’t open stream socket");
	}
	bzero((char *)&serv_addr, sizeof(serv_addr)); // set serv_addr to all 0s
    if (argc<3) {
        printf("usage: ./server domain_name port_num\n");
    }
    // get ip address from the host name
    struct hostent *he;
    char host[MAX_HOSTNAME];
    strcpy(host, argv[1]);
    if ((he = gethostbyname(host)) == NULL){
        perror("server: error resolving hostname");
    }
    struct in_addr **serv_list;
    serv_list = (struct in_addr**) he->h_addr_list;
    // set up this server's "name" for printing purposes
    this_srv = (aServer*)malloc(sizeof(aServer));
    char serv_name[MAX_HOSTNAME];
    inet_ntop(AF_INET, serv_list[0], serv_name, MAX_HOSTNAME+32);
    strcat(serv_name, ":");
    strcat(serv_name, argv[2]);
    strcpy(this_srv->srv_name, serv_name);
    printf("created server %s\n", this_srv->srv_name);
    // set up this server's sockaddr_in
    serv_addr.sin_family = AF_INET;
	memcpy(&serv_addr.sin_addr.s_addr, serv_list[0], he->h_length);
	serv_addr.sin_port = htons(atoi(argv[2])); // port number that user specifies
	if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
		perror("server: can’t bind local address");
	}
	listen(sockfd, 5);
	clilen = sizeof(cli_addr);
	aChannel* newChannel = (aChannel*)malloc(sizeof(aChannel));
	strcpy(newChannel -> channelName, "Common");
	newChannel -> subscribedNum = 0;
    newChannel -> adjServersNum = 0;
	// Parse the variable length input
    if (argc > 3) {
        if (((argc-3)%2) != 0) {
            printf("usage: command line server descripting arguments must be of even length\n");
        }
        else {
            // iterate through the command line to find neighbor servers
            int s;
            for (s = 3; s < argc; s += 2) {
                char n_host_name[MAX_HOSTNAME];
                strcpy(n_host_name, argv[s]);
                if ((he = gethostbyname(n_host_name)) == NULL){
                    perror("server: error resolving hostname");
                }
                serv_list = (struct in_addr**) he->h_addr_list;
                // set up neighbor server's name
                char n_srv_name[MAX_HOSTNAME+32];
                inet_ntop(AF_INET, serv_list[0], n_srv_name, MAX_HOSTNAME+32);
                strcat(n_srv_name, ":");
                strcat(n_srv_name, argv[s+1]);
                printf("'created' neighbor server %s\n", n_srv_name);
                aServer* n_server = (aServer*)malloc(sizeof(aServer));
                strcpy(n_server->srv_name, n_srv_name);
                // set up neighbor server's port sockaddr_in
                n_server->srv.sin_family = AF_INET;
                n_server->srv.sin_port = htons(atoi(argv[s+1]));
                memcpy(&n_server->srv.sin_addr.s_addr, serv_list[0], he->h_length);
                // put it in theServers and theChannels[Common]->adjServers
                theServers[serverIndex] = n_server;
                serverIndex++;
                newChannel->adjServers[newChannel->adjServersNum] = n_server;
                newChannel->adjServersNum++;
            }
        }
    }
	theChannels[channelIndex]= newChannel;
    channelIndex++;
    //add timing for soft state...
    /*
    signal(SIGALRM, signal_handler(1));
    alarm(60;)
     */
    while(1){
		bzero(buffer,256);
		if((n = recvfrom(sockfd,buffer,255, 0,(struct sockaddr *)&cli_addr, &clilen)) > -1){
			if (n < 0){
				perror("ERROR reading from socket");
			}
			request_t reqtype = ((request*) buffer) -> req_type;
			printf("\n");
			switch(reqtype){
				case 0:{ //login
					printf("LOGIN HANDLER\n");
					aUser* newUser = (aUser*)malloc(sizeof(aUser));
					strcpy(newUser -> username, ((request_login*)buffer)-> req_username);
                    newUser -> cli = cli_addr;
					theUsers[userIndex] = newUser;
					userIndex++;
                    printf("%s recv login %s\n", this_srv->srv_name, newUser->username);
					break;
				}
				case 1:{ //logout
					printf("LOGOUT HANDLER\n");
					aUser* logoutUser = findUser(&cli_addr);
                    printf("%s recv logout %s\n", this_srv->srv_name, logoutUser->username);
					int removeIndex = findIndexUsers(logoutUser);
					int i;
					// reindex theUsers[] for logoutUser removal
					for(i = removeIndex; i < userIndex - 1; i++){
						 theUsers[i] = theUsers[i + 1];
					}
                    userIndex--; // one less user exists
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
								//removeChannelReindex(theChannels[a], a);
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
                    printf("%s recv join %s\n", this_srv->srv_name, joinedUser->username);
					for(i = 0; i < channelIndex; i++){
						//check if the channel exists in theChannels
						if(strcmp(theChannels[i] -> channelName, channel) == 0){
							if(userInChannel(joinedUser, theChannels[i])){
								channelExists = 1;
								strcpy(er.txt_error, "user is already on the channel");	
								sendto(sockfd,&er, sizeof(er), 0,(struct sockaddr *)&cli_addr, clilen );			
								break;
							}
							theChannels[i]->subscribedClients[theChannels[i]->subscribedNum] = joinedUser;
                            printf("user %s joined %s\n", theChannels[i]-> subscribedClients[theChannels[i]->subscribedNum]->username, theChannels[i]->channelName);
							theChannels[i] -> subscribedNum += 1;
							//printf("total subscribed to this channel is %d\n", theChannels[i] -> subscribedNum);
							channelExists = 1;
							break;
						}
					}
					// channel does not exist create one + BROADCAST
					if(!channelExists){					
                        aChannel* newChannel = (aChannel*)malloc(sizeof(aChannel));
                        //newChannel -> channelName = {'\0'};
                        strcpy(newChannel -> channelName, channel);
                        newChannel -> subscribedClients[0] = joinedUser;
                        printf("%s joined new channel, %s\n", joinedUser->username, channel);
                        newChannel -> subscribedNum = 1;
                        int i;
                        for( i= 0; i< serverIndex; i++){
                            newChannel -> adjServers[i] = theServers[i];
                        }
                        newChannel -> adjServersNum = i;
                        theChannels[channelIndex]= newChannel;
                        channelIndex++;
                        broadcast_join_message(newChannel); // BROADCAST
					}
					break;
				}
				case 3:{ //leave
					printf("LEAVE HANDLER\n");
					text_error er;
                    er.txt_type = TXT_ERROR;
					aUser* leaveUser = findUser(&cli_addr);
					if(leaveUser == NULL){ // a user that does not exist tries to join a channel
						strcpy(er.txt_error, "You are not logged in, restart program");	
						sendto(sockfd,&er, sizeof(er), 0,(struct sockaddr *)&cli_addr, clilen );
						break;
					}
                    printf("%s recv leave %s\n", this_srv->srv_name, leaveUser->username);
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
									//removeChannelReindex(theChannels[a], a);
									break;				
								}
							}
							if(noUserExists){
								strcpy(er.txt_error, "User does not exist on the channel");	
								notFound = 0;
								sendto(sockfd,&er, sizeof(er), 0, (struct sockaddr *)&cli_addr, clilen);
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
				case 4:{ //say
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
					memcpy(channel,((request_say*)buffer)-> req_channel, 32);
					memcpy(message,((request_say*)buffer)-> req_text, 64);
                    printf("%s recv say on %s from %s: %s\n", this_srv->srv_name, channel, userSaid-> username, message);
					//long long uniChar[16];
					unsigned char uniChar[16];
					//uuid_generate(uniChar);	
					FILE* fp;
			
					fp = fopen("/dev/urandom", "r");
					if (fp== NULL){

						fprintf(stderr, "opening URANDOM failed");
						return 0;
					}
					fprintf(stderr, "opening urandom was successful)");
					fread(&uniChar, 1, 8, fp);
					//close(fp);
					//fprintf(stderr, "uniCHAR AFTER GENERATE %s\n", uniChar);
					memcpy(msg_nums[index_msg_num], uniChar, 8);
					index_msg_num ++;
					printf("done with file\n");
                    int i;
                    for(i = 0; i < channelIndex; i++){
						if(strcmp(theChannels[i] -> channelName, channel) == 0){
							//iterate through subscribed users and send the message to each one
							int j;
							//printf("the number users on this channel is %d\n",theChannels[i] -> subscribedNum );
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
					// send the s2s say
                    printf("about to broadcast say\n");
                    broadcast_say_message(this_srv, channel, message, userSaid->username); // BROADCAST
					/*
                    struct request_s2s_say say_msg;
    				say_msg.req_type = REQ_S2S_SAY;
					msg_nums[index_msg_nums] = index_msg_nums;
					say_msg.uni_num = msg_nums[index_msg_num];
					index_msg_num++;
					strcpy( say_msg.req_channel,channel);
					strcpy(say_msg.req_text, message);
					strcpy(say_msg.req_username, userSaid -> username);
					if (sendto(sockfd, &say_msg, sizeof(say_msg), 0, (struct sockaddr*)&(theServers[0]->srv), sizeof(theServers[0])) < 0 ) { // need to check if 0th server does not match this_srv
                        perror("Message failed");
                    }*/
					break;
				}
                case 5:{ //list
					printf("LIST HANDLER\n");
					aUser* userList = findUser(&cli_addr);
					text_error er;
					er.txt_type = TXT_ERROR;
					if(userList == NULL){ // a user that does not exist tries to join a channel
						strcpy(er.txt_error, "You are not logged in, restart program");	
						sendto(sockfd,&er, sizeof(er), 0,(struct sockaddr *)&cli_addr, clilen);
						break;
					}
                    printf("%s recv list %s\n", this_srv->srv_name, userList->username);
					struct text_list *sendingList = (struct text_list *) malloc(sizeof(text_list) + sizeof(channel_info) * channelIndex);
					sendingList->txt_type = TXT_LIST;
					sendingList->txt_nchannels = channelIndex;
					int i;
					for(i = 0; i < channelIndex; i++){
						strcpy(((sendingList->txt_channels)+i)->ch_channel, theChannels[i] -> channelName);
						printf("channel to be listed %s\n", theChannels[i] -> channelName);
					}
					int sizeLen = sizeof(text_list) + sizeof(channel_info) * channelIndex;
					if((n= sendto(sockfd,sendingList, sizeLen, 0,(struct sockaddr *)&cli_addr, clilen) < -1)){
                        printf("ERROR writing to socket\n");
					}
					free(sendingList);	
					break;		
				}
                case 6:{ //who
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
                    printf("%s recv who %s\n", this_srv->srv_name, userWho->username);
					int i;
					for(i = 0; i < channelIndex ; i++){
                        if(strcmp(theChannels[i] -> channelName, whoChannel) == 0){
                            int UsersOnChannel = theChannels[i] -> subscribedNum;
                            whoSend = (struct text_who *) malloc(sizeof(text_who) + sizeof(user_info) * UsersOnChannel);
                            int j;
                            for(j = 0; j < UsersOnChannel; j++){
                                strcpy(((whoSend->txt_users)+j)->us_username, theChannels[i] -> subscribedClients[j] -> username);
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
				case 8:{ //S2S join
					printf("S2S JOIN HANDLER\n");
                    char channel[32];
                    strcpy(channel,((request_s2s_join*)buffer)->req_channel);
                    int channelExists = 0;
                    aServer* joinedServer = findServer(&cli_addr);
                    // need to understand what the cli_addr is
                    printf("%s %s recv s2s_join on %s\n", this_srv->srv_name, joinedServer->srv_name, channel);
                    int i;
                    for(i = 0; i < channelIndex; i++){
                        //check if the channel exists in theChannels
                        if(strcmp(theChannels[i]->channelName, channel) == 0){
                            if(serverInChannel(joinedServer, theChannels[i])){
                                channelExists = 1;
                                break;
                            }
                            theChannels[i]->adjServers[theChannels[i]->adjServersNum] = joinedServer;
                            printf("server %s joined %s\n",theChannels[i]->adjServers[theChannels[i]->adjServersNum]->srv_name, theChannels[i]->channelName);
                            theChannels[i] -> adjServersNum++;
                            channelExists = 1;
                            break;
                        }
                    }
                    //channel does not exist create one
                    if(!channelExists){
                        aChannel* newChannel = (aChannel*)malloc(sizeof(aChannel));
                        strcpy(newChannel -> channelName, channel);
                        newChannel -> subscribedNum = 0;
                        int i;
                        for( i= 0; i< serverIndex; i++){
                            newChannel -> adjServers[i] = theServers[i];
                        }
                        newChannel -> adjServersNum = i;
                        printf("%s joined new channel, %s\n", joinedServer->srv_name, channel);
                        theChannels[channelIndex]= newChannel;
                        channelIndex++;
                        broadcast_join_message(newChannel); // BROADCAST
                    }
                    break;
				}

				case 11:{ //S2S say
					printf("S2S SAY HANDLER\n");
					char channel[32];
					char message[64];
					char username[32]; 
					char uniNum[8];
					int inLeave = 0;
					aServer* serverSaid = findServer(&cli_addr);
					//fprintf(stderr, "the user who is sending request is %s\n", userSaid -> username);
					memcpy(channel,((request_s2s_say*)buffer)-> req_channel, 32);
					memcpy(message,((request_s2s_say*)buffer)-> req_text, 64);
					memcpy(username,((request_s2s_say*)buffer)-> req_username, 32);
					memcpy( uniNum,((request_s2s_say*)buffer)-> uni_num,8);
               fprintf(stderr, "%s %s recv s2s_say on %s: %s\n", this_srv->srv_name, serverSaid->srv_name, channel, message);
					fprintf(stderr, "the uniNum is %s\n", uniNum);
					int i;
					for(i = 0; i < channelIndex; i++){
						if(strcmp(theChannels[i] -> channelName, channel) == 0){ //channel match
							//iterate through subscribed users and send the message to eachone
							/*if(uniNum == msg_nums[index_msg_num]){//duplicate message
							}
							else{*/
                            int a;
                            printf("the number of index_msg_num %d", index_msg_num);
                            for(a = 0; a < index_msg_num; a ++){
                                if(strncmp(uniNum, msg_nums[a],8)==0){
											//if(uniNum == msg_nums[a]){
                                    //LEAVE
                                    // find serverSaid in channels adjlist and remove
                                    removeAdjServerReindex(serverSaid,theChannels[a]);
                                    struct request_s2s_leave leave_msg;
                                    leave_msg.req_type = REQ_S2S_LEAVE;
                                    strcpy(leave_msg.req_channel, channel);
                                    if((n= sendto(sockfd,&leave_msg, sizeof(leave_msg), 0,(struct sockaddr *)&cli_addr, clilen ) < -1)){
                                        printf("ERROR writing to socket\n");
                                    }
                                    printf("%s %s send s2s_leave on %s\n", this_srv->srv_name, serverSaid->srv_name, channel);
												inLeave = 1;
                                    break;
                                }
                            }
								if(!inLeave){
                            memcpy(msg_nums[index_msg_num], uniNum, 8);
                            index_msg_num++;
                            int j;
                            printf("the number users on this channel is %d\n",theChannels[i] -> subscribedNum );
                            for(j = 0; j< (theChannels[i] -> subscribedNum); j++){
                                text_say sendingSay;
                                sendingSay.txt_type = 0;
                                strcpy(sendingSay.txt_channel, channel);
                                strcpy(sendingSay.txt_username, username);
                                strcpy(sendingSay.txt_text, message);
                                int cliLen = sizeof(theChannels[i]->subscribedClients[j]->cli);
                                if((n= sendto(sockfd,&sendingSay, sizeof(sendingSay), 0,(struct sockaddr *)&theChannels[i]->subscribedClients[j]->cli, cliLen ) < -1)){
                                    printf("ERROR writing to socket\n");
                                }
							}
						}
					}
					}
					// send the s2s say
					if(!inLeave){
                    broadcast_say_message(serverSaid, channel, message, username);} // BROADCAST
                    /*if(!f){
                    printf("nowhere to forward in say 2s2\n");
                    struct request_s2s_leave leave_msg;
                    leave_msg.req_type = REQ_S2S_LEAVE;
                    strcpy(leave_msg.req_channel, channel);
                    if((n= sendto(sockfd,&leave_msg, sizeof(leave_msg), 0,(struct sockaddr *)&serverSaid->srv, sizeof(serverSaid->srv) ) < -1)){
                        printf("ERROR writing to socket\n");
                    }
                    }*/
					break;
				}
				case 9:{ //S2S leave
					printf("S2S LEAVE HANDLER\n");
					aServer* serverLeave = findServer(&cli_addr);
					char channel[32];
					memcpy(channel,((request_s2s_leave*)buffer)-> req_channel, 32);
                    printf("%s %s recv s2s_leave on %s\n", this_srv->srv_name, serverLeave->srv_name, channel);
					//find the appropriate channel					
					int i;
					for(i = 0; i < channelIndex; i++){
						if(strcmp(theChannels[i] -> channelName, channel) == 0){ //channel match
							if(theChannels[i] -> adjServersNum <= 1){ //&& theChannels[i] -> subscribedNum == 0?? << why is this needed?
							removeAdjServerReindex(serverLeave,theChannels[i]);
							printf("a\n");
							}
						}
					}
					break;
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

/*aServer* createServerFromCLI_ADDR(sockaddr_in srv_addr){
 aServer* new_server = (aServer*)malloc(sizeof(aServer));
 new_server->srv = srv_addr;
 char n_srv_name[MAX_HOSTNAME+32];
 inet_ntop(AF_INET, &(srv_addr.sin_addr.s_addr), n_srv_name, MAX_HOSTNAME+32);
 strcat(n_srv_name, ":");
 char n_srv_port[32];
 inet_ntop(AF_INET, &(srv_addr.sin_port), n_srv_port, 32);
 strcat(n_srv_name, n_srv_port);
 strcpy(new_server->srv_name, n_srv_name);
 printf("created server %s\n", new_server->srv_name);
 theServers[serverIndex] = new_server;
 serverIndex++;
 return new_server;
 }*/
