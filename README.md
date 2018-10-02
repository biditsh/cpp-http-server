HTTP Server Project

Bidit Sharma 4/17/2018

==External Documentation==

Installation:
- Dowload/Clone the repository
- Using the terminal/bash shell, go the the repository
- type 'make' on the terminal, the program will be compiled

To run the server:
- Entre ./server <port_number> when the while in the directory where the program was compilesd

To terminate the server:
- Enter X when it is running. The program will return 503 Service Unavailable to clients that are connected and request after termination has been called, and will proceed to termination

Logs: 
- The server log is stored in log.txt file in the same directory as the executable
	Log file is in the following format per request/response:
				request-id thread-id date
				request-id protocol(from client)
				request-id protocol(response from server)



==Some Features==

Handling of HEAD method:
- The server handles HEAD HTTP request - it only sends the header and filesize information of the file requested using HEAD

Handling of DELETE method:
- The server handles DETELE method. If the requested file is successfully deleted, the header contains and OK response, otherwise contains a NOT_IMPLEMENTED response.

(Not extra-feature but additional info:)
- The termination command is listened to throughout the life of the program in a different thread.
- Clients that seek connection and send request to threads between the time span of termination call from user and termination of thread get a 503 SERVICE_UNAVAILABLE response, and the threads proceed to safe termination. 
