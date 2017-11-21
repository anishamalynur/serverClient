raw.c                                                                                               0000664 0001750 0001750 00000001251 13174521222 012255  0                                                                                                    ustar   amalynur                        amalynur                                                                                                                                                                                                               #include <termios.h>
#include <unistd.h>
#include "raw.h"

/* See raw.h for usage information */

static struct termios oldterm;

/* Returns -1 on error, 0 on success */
int raw_mode (void)
{
        struct termios term;

        if (tcgetattr(STDIN_FILENO, &term) != 0) return -1;
    
        oldterm = term;     
        term.c_lflag &= ~(ECHO);    /* Turn off echoing of typed charaters */
        term.c_lflag &= ~(ICANON);  /* Turn off line-based input */
        term.c_cc[VMIN] = 1;
        term.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSADRAIN, &term);

        return 0;
}

void cooked_mode (void)    
{   
        tcsetattr(STDIN_FILENO, TCSANOW, &oldterm);
}
                                                                                                                                                                                                                                                                                                                                                       server.c                                                                                            0000664 0001750 0001750 00000034454 13177003207 013006  0                                                                                                    ustar   amalynur                        amalynur                                                                                                                                                                                                               
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

struct aUser{
	char username[32];
	sockaddr_in cli;
};

struct aChannel{
	char channelName[32];
	int subscribedNum;
	aUser* subscribedClients[100]; //max of 100 users per channel
};

aUser* theUsers[MAX_USER]; // keeping track of users and their channels, server supports 1000 users
aChannel* theChannels[MAX_CHANNEL]; // keeping track of channels and users on them

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

				case 0:{//login

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
				case 3:{//leave
			
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

				case 4:{// request say

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

				case 5: //list 
				{	
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

				case 6: //who
				{	
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

				default:
				{
					printf("Invalid packet was sent\n");

				}

			}
		}
	}
	close(sockfd);
	return 0;
}


		
                                                                                                                                                                                                                    client.c                                                                                            0000664 0001750 0001750 00000026616 13177003162 012757  0                                                                                                    ustar   amalynur                        amalynur                                                                                                                                                                                                               
//  Inspiration drawn from:
//      socket programming:  https://www.cs.rutgers.edu/~pxk/417/notes/sockets/udp.html
//      get ip addr from channel: http://www.binarytides.com/hostname-to-ip-address-c-sockets-linux/
//      simultaneous input from both server & client: https://linux.die.net/man/3/fd_set

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include "raw.h"
#include "duckchat.h"

// global variables
int s, connected;
struct sockaddr_in serv_addr;
char active_channel[CHANNEL_MAX];
char username[USERNAME_MAX];

// send login message (username) - returns (error) ? 1 : 0
int send_login_message(char *username) {
    struct request_login msg;
    msg.req_type = REQ_LOGIN;
    strcpy(msg.req_username, username);
    // send login message
    if (send(s, &msg, sizeof(msg), 0) < 0) return 1; // error
    // success!
    return 0;
}

// send join message (channel) - returns (error) ? 1 : 0
int send_join_message(char *channel) {
    struct request_join msg;
    msg.req_type = REQ_JOIN;
    strcpy(msg.req_channel, channel);
    // send join message
    if (send(s, &msg, sizeof(msg), 0) < 0) return 1; // error
    // success! - make cannel the active_channel
    strcpy(active_channel, channel);  
    return 0;
}

// send leave message (channel) - returns (error) ? 1 : 0
int send_leave_message(char *channel) {
    struct request_leave msg;
    msg.req_type = REQ_LEAVE;
    strcpy(msg.req_channel, channel);
    // send say message
    if (send(s, &msg, sizeof(msg), 0) < 0) return 1; // error
    // success! - leave the channel
    if (strcmp(channel,active_channel) == 0) active_channel[0] = '\0';
    return 0;
}

// send logout message () - returns (error) ? 1 : 0
int send_logout_message(void) {
    struct request_logout msg;
    msg.req_type = REQ_LOGOUT;
    // send logout message
    if (send(s, &msg, sizeof(msg), 0) < 0) return 1; // error
    // success!
    return 0;
}

// send say message (channel, text) - returns (error) ? 1 : 0
int send_say_message(char *channel, char *text) {
    struct request_say msg;
    msg.req_type = REQ_SAY;
    strcpy(msg.req_channel, channel);
    strcpy(msg.req_text, text);
    // send say message
    if (send(s, &msg, sizeof(msg), 0) < 0) return 1; // error
    // success! - display (handle_server_input)
    return 0;
}

// send list message () - returns (error) ? 1 : 0
int send_list_message(void) {
    struct request_list msg;
    msg.req_type = REQ_LIST;
    // send list message
    if (send(s, &msg, sizeof(msg), 0) < 0) return 1; // error
    // success! - display (handle_server_input)
    return 0;
}

// send who message (channel) - returns (error) ? 1 : 0
int send_who_message(char *channel) {
    struct request_who msg;
    msg.req_type = REQ_WHO;
    strcpy(msg.req_channel, channel);
    // send who message
    if (send(s, &msg, sizeof(msg), 0) < 0) return 1; // error
    // success! - display (handle_server_input)
    return 0;
}

// send switch message (channel) - returns (error) ? 1 : 0
int send_switch_message(char *channel) {
    // see if this user is in the channel
    struct request_who msg;
    msg.req_type = REQ_WHO;
    strcpy(msg.req_channel, channel);
    if (send(s, &msg, sizeof(msg), 0) < 0) 
        fprintf(stderr, "\nclient: who failed\n>");
    struct text_who name[USERNAME_MAX];
    recv(s, name, sizeof(name), 0);
    int i, user_in_channel;
    user_in_channel = 0;
    for (i = 0; i < name->txt_nusernames; i++) {
        if (strcmp(username, name->txt_users[i].us_username) == 0) 
            user_in_channel = 1;
    }
    if (!user_in_channel) return 1; // error
    // success! - change active_channel to channel
    strcpy(active_channel,channel);
    return 0;
}

// handle user input () - returns void
void handle_user_input (void) {
    // read user input
    int count = 0;
    char user_input[SAY_MAX];
    char c = '\0';
    while (c != '\n') {
        if (((c = getchar()) != '\n') && (count < SAY_MAX-1)) {
            user_input[count] = c;
            putchar(c);
            count++;
        }
    }
    user_input[count] = '\0';
    // find a special command
    if (user_input[0] == '/') {
        char *msg = strchr(user_input, '/');
        msg = &msg[1];
        // send exit message
        if (strcmp(msg, "exit") == 0) {
            if (send_logout_message()) 
                fprintf(stderr, "\nclient: logout failed\n>");
            else connected = 0;
        }
        // send list message
        else if (strcmp(msg, "list") == 0) {
            if (send_list_message()) 
                fprintf(stderr, "\nclient: list failed\n>");
        }
        else {
            char *param = strchr(msg, ' ');
            if ((param != NULL) && (strlen(param) > 1)) {
                param = &param[1];
                // send join message
                if (strncmp(msg, "join ", 5) == 0) {
                    if (send_join_message(param)) 
                        fprintf(stderr, "\nclient: join failed\n>");
                    else {
                        strcpy(active_channel, param);
                        fprintf(stdout, "\n>");
                    }
                }
                // send leave message
                else if (strncmp(msg, "leave ", 6) == 0) {
                    if (send_leave_message(param)) 
                        fprintf(stderr, "\nclient: leave failed\n>");
                    else fprintf(stdout, "\n>");
                }
                // send who message
                else if (strncmp(msg, "who ", 4) == 0) {
                    if (send_who_message(param)) 
                        fprintf(stderr, "\nclient: who failed\n>");
                }
                // send switch message
                else if (strncmp(msg, "switch ", 7) == 0) {
                    if (send_switch_message(param)) 
                        fprintf(stderr, "\nclient: switch failed\n>");
                    else {
                        strcpy(active_channel, param);
                        fprintf(stdout, "\n>");
                    }
                }
                else fprintf(stderr, "\nclient: unknown command\n>");
            }
            else fprintf(stderr, "\nclient: unknown command\n>");
        }
    }
    // not a special command
    else {
        // send say message
        if (active_channel[0] != '\0') {
            if (send_say_message(active_channel, user_input))
                fprintf(stderr, "\nclient: say failed\n>");
        }
        else fprintf(stderr, "\nclient: no active channel, cannot say\n>");
    }
    return;
}

// handle server input () - returns void
void handle_server_input(void) {
    char recv_text[9999];
    text_t msg_type;
    struct text_say *say_msg;
    struct text_list *list_msg;
    struct text_who *who_msg;
    struct text_error *error_msg;
    socklen_t serv_len = sizeof(serv_addr);
    // send server input message
    if (recvfrom(s, recv_text, sizeof(recv_text), 0,
                (struct sockaddr*)&serv_addr, &serv_len) < 0)
        fprintf(stderr, "\nclient: recvfrom failed\n>");
    else {
        // success! - handle server input...
        fflush(stdout);
        fprintf(stdout, "\b\b\b\b\b\b\b\b\b\b\b\b");
        msg_type = ((struct text *)recv_text)->txt_type;
        // say message
        if (msg_type == TXT_SAY) {
            say_msg = (struct text_say *)recv_text;
            fprintf(stdout, "\n[%s][%s]: %s\n", say_msg->txt_channel,
                            say_msg->txt_username, say_msg->txt_text);
            fflush(stdout);
        }
        // list message
        else if (msg_type == TXT_LIST) {
            list_msg = (struct text_list *)recv_text;
            fprintf(stdout, "\nExisting channels:\n");
            int nchannels = list_msg->txt_nchannels;
            struct channel_info *channel;
            channel = list_msg->txt_channels;
            int i;
            for (i=0; i<nchannels; i++) {
                fprintf(stdout, " %s\n", (channel+i)->ch_channel);
            }
        }
        // who message
        else if (msg_type == TXT_WHO) {
            who_msg = (struct text_who *)recv_text;
            fprintf(stdout, "\nUsers on channel %s:\n", who_msg->txt_channel);
            int nusers = who_msg->txt_nusernames;
            struct user_info *user;
            user = who_msg->txt_users;
            int i;
            for (i=0; i<nusers; i++) {
                fprintf(stdout, " %s\n", (user+i)->us_username);
            }
        }
        // error message
        else if (msg_type == TXT_ERROR) {
            error_msg = (struct text_error *)recv_text;
            fprintf(stderr, "\nError: %s\n", error_msg->txt_error);
        }
        else {
            fprintf(stderr, "\nclient: message type not found\n>");
        }
        fprintf(stdout, ">");
    }
    return;
}

// usage: ./client channel port_num username
int main(int argc, char **argv) {
    // usage check
    if (argc != 4) {
        fprintf(stderr, "\nclient: usage: ./client socket port username\n>");
        exit(1);
    }
    if (sizeof(argv[3]) > USERNAME_MAX) {
        fprintf(stderr, "\nclient: Username too long\n>");
        exit(1);
    }
    // get channel, port number, and username from arguments
    char channel[CHANNEL_MAX];
    strcpy(channel, argv[1]);
    int port_num;
    port_num = atoi(argv[2]);
    strcpy(username, argv[3]);
    // get ip_addr from channel
    struct hostent *he;
    if ((he = gethostbyname(channel)) == NULL) {
        fprintf(stderr, "\nclient: error resolving hostname\n>");
        exit(1);
    }
    // create a UDP socket
    if ((s=socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "\nclient: can't open stream socket\n>");
        return 0;
    }
    // connect the socket to any valid IP address and a specific port;
    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, he->h_addr_list[0], he->h_length);
    serv_addr.sin_port = htons(port_num);
    if (connect(s, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "\nclient: cannot connect to server\n>");
        close(s);
        return 0;
    }
    // now the server is ready
    else connected = 1;
    
    // send login message
    if (send_login_message(username)) {
        fprintf(stderr, "\nclient: login failed\n>");
        close(s);
        return 0;
    }
    // send join message
    if (send_join_message("Common")) {
        fprintf(stderr, "\nclient: join failed\n>");
        close(s);
        return 0;
    }
    // switch into raw (character) mode
    raw_mode();
    fflush(stdout);
    // start prompt
    fprintf(stdout, ">");
    // while continue... use the appropriate input
    connected = 1;
    while (connected) {
        // allows for synchronous multiplexing (get info from server and client)
        fflush(stdout);
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        FD_SET(s, &fds);
        int rc;
        if ((rc = select(s+1, &fds, NULL, NULL, 0)) < 0) {
            fprintf(stderr, "\nclient: error in select (multiplexing)\n>");
            close(s);
            return 0;
        }
        // get user input
        if (FD_ISSET(0, &fds)) {
            handle_user_input();
            FD_CLR(0, &fds);
        }
        // get serv input
        if (FD_ISSET(s, &fds)) {
            handle_server_input();
            FD_CLR(0, &fds);
        }
    }
    fprintf(stdout, "\n");
    // return to cooked (line) mode
    cooked_mode();
    // close the socket
    close(s);
    return 0;
}
                                                                                                                  submissionForm.txt                                                                                  0000664 0001750 0001750 00000002451 13177002773 015113  0                                                                                                    ustar   amalynur                        amalynur                                                                                                                                                                                                               Student Names: Sierra Battan and Anisha Malynur
Student IDs: 951490130 and 951484260


Client Program
--------------

- Does the client compile and run? yes

- Do login and logout work? yes

- Does the client accept user commands? yes

Please specify which of the following features work

    - Join: works

    - Leave: works

    - Say: works

    - Switch: works

    - List: works

    - Who: works


- Does the client send Keep Alive message when needed (for extra credit)? no

- Does the client send Keep Alive message when not needed (for extra credit)? no

- Can the client handle messages that are out of order(e.g. /leave before a /join)? no

- Can the client redisplay the prompt and user input when a message from the server is received while the user is typing (for extra credit)? no


Server Program
-------------

- Does the server compile and run? yes

- Does the server accept client requests? yes

- Do Login and Logout work? yes

Please specify which of the following features work

    - Join: works

    - Leave: works

    - Say: works

    - List: works

    - Who: works

- Does the server timeout users correctly (for extra credit)? no

- Can the server handle messages that are out of order(e.g. /say before a client has logged in)? no                                                                                                                                                                                                                       Makefile                                                                                            0000664 0001750 0001750 00000000336 13177003356 012771  0                                                                                                    ustar   amalynur                        amalynur                                                                                                                                                                                                               CC=g++

CFLAGS=-Wall -W -g -Werror



all: client server

client: client.c raw.c
	gcc client.c raw.c $(CFLAGS) -o client

server: server.c raw.c
	$(CC) server.c raw.c  $(CFLAGS) -o server

clean:
	rm -f client server *.o

                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  