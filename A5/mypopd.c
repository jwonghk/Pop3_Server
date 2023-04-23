#include "netbuffer.h"
#include "mailuser.h"
#include "server.h"
#include "util.h"


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 1024

typedef enum state {
    Undefined,
    Authorized,
    Transactioned,
    UserChecked,
    PasswordChecked,
    Updated,
    Quitted,
    // TODO: Add additional states as necessary
} State;

typedef struct serverstate {
    int fd;
    net_buffer_t nb;
    char recvbuf[MAX_LINE_LENGTH + 1];
    char *words[MAX_LINE_LENGTH];
    int nwords;
    State state;
    struct utsname my_uname;
    char USERName_Entered[MAX_LINE_LENGTH+1];
    // TODO: Add additional fields as necessary

    mail_list_t listOfMails;
    int lengthOfListOfMails;
} serverstate;

static void handle_client(int fd);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Invalid arguments. Expected: %s <port>\n", argv[0]);
        return 1;
    }
    run_server(argv[1], handle_client);
    return 0;
}

// syntax_error returns
//   -1 if the server should exit
//    1 otherwise
int syntax_error(serverstate *ss) {
    if (send_formatted(ss->fd, "-ERR %s\r\n", "Syntax error in parameters or arguments") <= 0) return -1;
    return 1;
}

// checkstate returns
//   -1 if the server should exit
//    0 if the server is in the appropriate state
//    1 if the server is not in the appropriate state
int checkstate(serverstate *ss, State s) {
    if (ss->state != s) {
        if (send_formatted(ss->fd, "-ERR %s\r\n", "Bad sequence of commands") <= 0) return -1;
        return 1;
    }
    return 0;
}

// All the functions that implement a single command return
//   -1 if the server should exit
//    0 if the command was successful
//    1 if the command was unsuccessful

int do_quit(serverstate *ss) {
    dlog("Executing quit\n");
    // TODO: Implement this function
    if(ss->state == PasswordChecked) {
        ss->state = Updated;
        //if(ss->listOfMails != NULL) 
        //printf("here in quit\r\n");
        mail_list_destroy(ss->listOfMails);
        //printf("here in quit 22\r\n");
        send_formatted(ss->fd,"+OK Service closing connection\r\n");
        return -1;

        // here the server teminates right away if client enter the QUIT command 
        // when still in the Authorized state
    } else if (ss->state == Authorized) {
        send_formatted(ss->fd, "+OK %s POP3 server signing off\r\n", ss->my_uname.nodename);
        return -1;
    }
    return -1;
}

int do_user(serverstate *ss) {
    dlog("Executing user\n");
    // TODO: Implement this function
    if(strcasecmp(ss->words[0], "USER") != 0 || ss->nwords != 2) {
        send_formatted(ss->fd, "-ERR %s\r\n", "Invalid Arguments");
        return 0;
    } 
    
    int current;
    current = checkstate(ss, Authorized);
    if(current == 0){
        if(is_valid_user(ss->words[1], NULL) != 0) {
            
            strcpy(ss->USERName_Entered, ss->words[1]);
            printf("Here User is %s\r\n", ss->USERName_Entered);
            send_formatted(ss->fd, "+OK User is valid, procced with the Password\r\n");
            //ss->state = UserChecked; 
            ss->state = Authorized;
        } else {
            send_formatted(ss->fd, "-ERR sorry, no mailbox found for %s \r\n", ss->words[1]);
        }
    }
    return 1;
}


int do_pass(serverstate *ss) {
    dlog("Executing pass\n");
    // TODO: Implement this function
    //printf("Uername %s\r\n", ss->USERName_Entered);

    // First check if we are allowed to do Password Check
    // since we must have done User command first by loggin in before the Pass command
    if(checkstate(ss, Authorized) == 0) {
        
        // check if the arguments are valid for Pass after already
        // making sure we already pass the UserChecked stage i.e. already call User command
        if(strcasecmp(ss->words[0], "Pass") !=0 || ss->nwords !=2) {
            send_formatted(ss->fd, "-ERR Invalid arguments of Password \r\n");
            return 0;
        }
        
        
        // if username and password matched:
        if(is_valid_user(ss->USERName_Entered , ss->words[1]) != 0) {
            mail_list_t currentUserMailList = load_user_mail(ss->USERName_Entered);
            int totalNumberMessage = mail_list_length(currentUserMailList, 1);
            ss->lengthOfListOfMails = totalNumberMessage;
            //size_t totalSizeMessage = mail_list_size(currentUserMailList);
            //send_formatted(ss->fd, "+OK %d messages (%ld octets) \r\n", 
            send_formatted(ss->fd, "+OK password is valid, mailed loaded\r\n");
            //totalNumberMessage, totalSizeMessage );
            ss->listOfMails = load_user_mail(ss->USERName_Entered);
            ss->state = PasswordChecked;
        
        // if password is Incorrect
        } else {
            send_formatted(ss->fd, "-ERR Invalid password \r\n");
        }
    }
    // send_formatted(ss->fd , "-ERR \r\n");
    return 1;
}

int do_stat(serverstate *ss) {
    dlog("Executing stat\n");
    // TODO: Implement this function
    if(checkstate(ss, PasswordChecked) == 0) {
        //ss->listOfMails = load_user_mail(ss->USERName_Entered);
        int numbMessage = mail_list_length(ss->listOfMails, 0);
        send_formatted(ss->fd, "+OK %d %ld\r\n", numbMessage, mail_list_size(ss->listOfMails));
    }
    return 1;
}


int do_list(serverstate *ss) {
    dlog("Executing list\n");

    // invalid argument cases
    if(strcasecmp(ss->words[0], "list") !=0 || ss->nwords > 2) {
        send_formatted(ss->fd , "-ERR error\r\n");
        return 0;
    }

    // list command without an argument
    if(ss->nwords == 1) {
        if(checkstate(ss, PasswordChecked) == 0) {
            int numbMessage = mail_list_length(ss->listOfMails, 0);
            send_formatted(ss->fd, "+OK %d messages (%ld octets)\r\n", numbMessage, mail_list_size(ss->listOfMails));

            // now loop through each message in the mail box
            dlog("Number of message here: %d \r\n", numbMessage);
            for(int i = 0; i < ss->lengthOfListOfMails; i++) {
                mail_item_t item = mail_list_retrieve(ss->listOfMails, i);
                if(item == NULL){
                    continue;
                } else {
                    send_formatted(ss->fd, "%d %ld\r\n" , i+1, mail_item_size(item));
                }
            }
            send_formatted(ss->fd, ".\r\n");
        }

        
    // this is when the list command is used with argument
    } else if(ss->nwords == 2) {

        if(mail_list_length(ss->listOfMails, 1) < atoi(ss->words[1]) || atoi(ss->words[1]) <= 0) {
            send_formatted(ss->fd, "-ERR no such message, only %d messages in maildrop\r\n", 
                                mail_list_length(ss->listOfMails, 0));
        } else {
            mail_item_t item = mail_list_retrieve(ss->listOfMails, atoi(ss->words[1])-1);
            //send_formatted(ss->fd, "%s %ld\r\n", ss->words[1], mail_item_size(item));
            
            if(item == NULL) {
                send_formatted(ss->fd, "-ERR no such message\r\n");
            } else {
                send_formatted(ss->fd, "+OK %s %ld\r\n", ss->words[1], mail_item_size(item));
                //send_formatted(ss->fd, "+OK Messages Followed\r\n");
            }
        }
    }
    return 1;
}


// this function SHOULD NOT retreieve a mail that is marked as DELETED
int do_retr(serverstate *ss) {
    dlog("Executing retr\n");

    if(strcasecmp(ss->words[0], "Retr") != 0 || ss->nwords !=2) {
        return syntax_error(ss);
    }

    // check if we already have password checked
    if(checkstate(ss, PasswordChecked) == 0) {
        //mail_item_size msize = mail_item_size();
        
        mail_item_t MailsRetrieved = mail_list_retrieve(ss->listOfMails,  atoi(ss->words[1])-1);
        if(MailsRetrieved != NULL) {
            //send_formatted(ss->fd, "+OK %s %ld octets\r\n", ss->words[1], mail_item_size(MailsRetrieved));
            FILE *pointerToFile = mail_item_contents(MailsRetrieved);
            if(pointerToFile == NULL) {
                dlog("error! File could not be opened.");
                    send_formatted(ss->fd, "-ERR\r\n");
                return 1;
            }
            //size_t mailItemSize = mail_item_size(MailsRetrieved);
            //send_formatted(ss->fd, "+OK %ld octets\r\n", mailItemSize);
            send_formatted(ss->fd, "+OK Messages follows:\r\n");
            
            while(fgets(ss->recvbuf, MAX_LINE_LENGTH, pointerToFile) != NULL) {
                send_formatted(ss->fd, "%s", ss->recvbuf);
            }
            send_formatted(ss->fd, ".\r\n");
            fclose(pointerToFile);
            return 0;
        
        } else if (MailsRetrieved == NULL) {
            send_formatted(ss->fd, "-ERR no such message\r\n");
            return 1;
        }
    }
    return 1;
}


int do_rset(serverstate *ss) {
    dlog("Executing rset\n");
    
    int checkInd = checkstate(ss, PasswordChecked);
    
    if(checkInd == 0) {
        int originalLength = mail_list_length(ss->listOfMails, 1);
        int lengthDeletedExcluded = mail_list_length(ss->listOfMails, 0);
        mail_list_undelete(ss->listOfMails);
        //send_formatted(ss->fd, "+OK\r\n");
        
        send_formatted(ss->fd, "+OK %d restored\r\n", originalLength - lengthDeletedExcluded);
        
        return 0;
    }

    return 1;
}

int do_noop(serverstate *ss) {
    dlog("Executing noop\n");

    if(checkstate(ss, PasswordChecked) != -1) {
        send_formatted(ss->fd, "+OK (noop)\r\n");
    }
    return 0;
}


typedef struct mail_item *mail_item_t;

struct mail_item {
    char file_name[2 * 1024];
    size_t file_size;
    int deleted;
};


int do_dele(serverstate *ss) {
    dlog("Executing dele\n");
    
    int totalNumberMessage = mail_list_length(ss->listOfMails, 1);


    // check of invalid argument
    if(strcasecmp(ss->words[0], "dele") != 0 || ss->nwords !=2 || 
            (atoi(ss->words[1]) > totalNumberMessage) || atoi(ss->words[1]) <= 0) {
        return syntax_error(ss);
    }

    dlog("before mail_list_retrieve!!!!!\r\n");
    mail_item_t itemToBeDeleted = mail_list_retrieve(ss->listOfMails, atoi(ss->words[1]) - 1 );
    
    if(itemToBeDeleted == NULL) {
        send_formatted(ss->fd, "-ERR message %s already deleted\r\n", ss->words[1]);
    } else {
        dlog("Right before mail_item_delete being executed\r\n");
        mail_item_delete(itemToBeDeleted);
        dlog("Here being deleted item is: %s %ld\r\n", itemToBeDeleted->file_name, itemToBeDeleted->file_size);
        send_formatted(ss->fd, "+OK message %s deleted\r\n", ss->words[1]);
        return 0;
    }       
    return 1;
}


void handle_client(int fd) {
    size_t len;
    serverstate mstate, *ss = &mstate;
    ss->fd = fd;
    ss->nb = nb_create(fd, MAX_LINE_LENGTH);
    ss->state = Undefined;
    uname(&ss->my_uname);
    if (send_formatted(fd, "+OK POP3 Server on %s ready\r\n", ss->my_uname.nodename) <= 0) 
        {   
            return;
        }

    // the state will become Authorized once the connection was established successfully
    ss->state = Authorized;

    
    while ((len = nb_read_line(ss->nb, ss->recvbuf)) >= 0) {
        if (ss->recvbuf[len - 1] != '\n') {
            // command line is too long, stop immediately
            send_formatted(fd, "-ERR Syntax error, command unrecognized\r\n");
            break;
        }
        if (strlen(ss->recvbuf) < len) {
            // received null byte somewhere in the string, stop immediately.
            send_formatted(fd, "-ERR Syntax error, command unrecognized\r\n");
            break;
        }
        // Remove CR, LF and other space characters from end of buffer
        while (isspace(ss->recvbuf[len - 1])) ss->recvbuf[--len] = 0;
        dlog("Command is %s\n", ss->recvbuf);
        if (strlen(ss->recvbuf) == 0) {
            send_formatted(fd, "-ERR Syntax error, blank command unrecognized\r\n");
            break;
        }
        // Split the command into its component "words"
        ss->nwords = split(ss->recvbuf, ss->words);
        char *command = ss->words[0];
        if (!strcasecmp(command, "QUIT")) {
            if (do_quit(ss) == -1) break;
        } else if (!strcasecmp(command, "USER")) {
            if (do_user(ss) == -1) break;
        } else if (!strcasecmp(command, "PASS")) {
            if (do_pass(ss) == -1) break;
        } else if (!strcasecmp(command, "STAT")) {
            if (do_stat(ss) == -1) break;
        } else if (!strcasecmp(command, "LIST")) {
            if (do_list(ss) == -1) break;
        } else if (!strcasecmp(command, "RETR")) {
            if (do_retr(ss) == -1) break;
        } else if (!strcasecmp(command, "RSET")) {
            if (do_rset(ss) == -1) break;
        } else if (!strcasecmp(command, "NOOP")) {
            if (do_noop(ss) == -1) break;
        } else if (!strcasecmp(command, "DELE")) {
            if (do_dele(ss) == -1) break;
        } else if (!strcasecmp(command, "TOP") ||
                   !strcasecmp(command, "UIDL") ||
                   !strcasecmp(command, "APOP")) {
            dlog("Command not implemented %s\n", ss->words[0]);
            if (send_formatted(fd, "-ERR Command not implemented\r\n") <= 0) break;
        } else {
            // invalid command
            if (send_formatted(fd, "-ERR Syntax error, command unrecognized\r\n") <= 0) break;
        }
    }
    nb_destroy(ss->nb);
}
