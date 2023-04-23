# Pop3_Server

This project involves:
- TCP sockets in C
- programming and debugging skills as they relate to the use of sockets in C
- implement the server side of a protocol
- develop general networking experimental skills
- develop a further understanding of what TCP does, and how to manage TCP connections from the server perspective

The POP3 protocol, described in RFC 1939, is used by mail clients to retrieve email messages from the mailbox of a specific email user. 
The server side of this protocol is implemented here. More specifically, the following commands are supported:

    USER and PASS (plain authentication)
    STAT (message count information)
    LIST (message listing) - with and without arguments
    RETR (message content retrieval)
    DELE (delete message)
    RSET (undelete messages)
    NOOP (no operation)
    QUIT (session termination)
    
 This works together with the SMTP server which will be able to store email message. 
