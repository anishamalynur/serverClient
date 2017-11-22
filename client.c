
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
