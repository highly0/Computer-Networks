# Computer-Networks
Various projects implemented in the "Computer Networks" course:

* TCP-Server: a TCP client and server in C
  * The protocol between the client and server will use four types of messages: Initialization, Acknowledgement, HashRequest, and HashResponse. The client starts by sending an Initialization message
  to the server, which informs the server of the number of hashing requests that the client will make (denoted by the variable
  N). Afterwards, the client sends N HashRequest messages to the server,
  where each HashRequest contains the data segment to be hashed. The server responds to the Initialization message with an Acknowledgement,
  which informs the client of the total length of the response. The
  server responds to each HashRequest with a HashResponse that contains
  the hash of the corresponding data.
* UDP-Server: UDP client and server to run a simplified version of NTP (Network Time Protocol) in C
  * The protocol implemented resembles that of NTP and will use two
types of messages: TimeRequests and TimeResponses. In this project,
the packet payloads will include the time on the machine when the
packet was sent. `clock_gettime()` was used to get the current time;
that function writes the time into a timespec structure containing two
64-bit unsigned integers (the number of seconds and nanoseconds since
the start of the epoch). These values will be referred to hereon as
the time in seconds and the time in nanoseconds. Unlike the protocol
in `tcp-server`, there is no initialization. The client sends a
TimeRequest that contains a sequence number for the request and a
timestamp of when it sent the payload. Upon receiving the TimeRequest,
the server replies with a TimeResponse that contains the same sequence
number and timestamp in the TimeRequest, as well as a timestamp of
when the server sends the TimeResponse. The formats of these messages
are shown below.
* Chatroom: Reverse engineer a provided chat client chat server to implement a suitable server.
  * In this project, I was provided a chat client and
chat server. Clients allow users to communicate with one another by
connecting to the server and interacting with it in accordance to an
application protocol. Through this protocol, the server allows clients
to engage in group chats in chat rooms and send private messages to
one another. My task was to reverse engineer the chat server and its protocol
and use this information to write a compatible replacement. 
