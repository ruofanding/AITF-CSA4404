/* Attacker host's gateway router */

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <fcntl.h>

#include "flow.h"

#define TCP_PORT 50000   // The port on which to send data
#define UDP_PORT 50002   // The arbitrary port to send UDP datagrams
#define NONCE_SIZE 64
#define R_SIZE 64

/* Prototypes */
void sendUDPDatagram(struct in_addr);
void listenGatewayRequest(void);
void notifyAttacker(void);
void handle_victim_gw_request(int, struct in_addr);

/* Send UDP Datagrams */
void sendUDPDatagram(struct in_addr raw_v_addr) {
	char *victim_addr = inet_ntoa((struct in_addr)raw_v_addr);
	int sock, length, n;
	struct sockaddr_in victim, from;
	struct hostent *hp;
	char *buffer;
	
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("Socket\n");
		exit(1);
	}
	
	victim.sin_family = AF_INET;
	victim.sin_port = htons(UDP_PORT);
	inet_pton(AF_INET, victim_addr, &victim.sin_addr);
	
	buffer = "nonce1";
	
	length = sizeof(struct sockaddr_in);
	if ((sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr *)&victim, length)) < 0) {
		perror("Sendto\n");
	}
}

/* Listens and Connects to Victim Gateway */
void listenGatewayRequest() {
	int sock, connected, bytes_recieved, true = 1;
	char send_data [1024] , recv_data[1024];
	struct sockaddr_in server_addr, client_addr;
	int sin_size;
	clock_t start, end;
	double elapsedT;
    
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Socket\n");
		exit(1);
	}
	
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &true, sizeof(int)) == -1) {
		perror("Setsockopt\n");
		exit(1);
	}
	
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(TCP_PORT);
	server_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(server_addr.sin_zero), 8);
	
	// Bind Socket
	if (bind(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
		perror("Unable to bind\n");
		exit(1);
	}
	
	// Listen to Socket
	if (listen(sock, 10) == -1) {
		perror("Listen\n");
		exit(1);
	}
	
	printf("Listening on port 50000\n");
	fflush(stdout);
	
	while (1) {
		sin_size = sizeof(struct sockaddr_in);
		
		while (1) {
			
			// Accept Connection
			connected = accept(sock, (struct sockaddr*) &client_addr, &sin_size);
			if (connected < 0) {
				printf("ERROR on accept\n");
			}
			else {
				printf("---------connected to a victim gateway-------------\n");

				printf("gateway address: %s\n", inet_ntoa((struct in_addr)client_addr.sin_addr));
				
				/* Reads the flow */
				struct flow flow;
				read(connected, &flow, sizeof(flow)); // may have issues here
				printf("Received filter request:\n");
				printf("src:  %s\n", inet_ntoa((struct in_addr)flow.src_addr));
				printf("dest: %s\n", inet_ntoa((struct in_addr)flow.dest_addr));
				
				/* Spawn child process to spam UDP at victim */
				pid_t process_id;
				process_id = fork();
				if (process_id == 0) { //child process
					int i = 0;
					
					// Send UDP Datagrams
					for (i = 0; i < 10; ++i) {
						sendUDPDatagram(flow.src_addr); // check if input is right
						printf("Sending UDP Packet to %s\n", inet_ntoa((struct in_addr)flow.src_addr));
						usleep(1000);
					}
					_Exit(0);
				}
				else { //parent_process
					handle_victim_gw_request(connected, client_addr.sin_addr);
				}
			}
		}
	}
}

/* Send TCP Message to Victim GW with nonce2 */
void handle_victim_gw_request(int sockfd, struct in_addr victim_gw_addr) {
	char *msg;
	char buf[256];
	
	read(sockfd, &buf, sizeof(buf)); // may have issues here
	printf("Received nonce1 value: %s\n", buf);
	 
	/* Check nonce1 value (2nd line of content) */
	if (1/*received_nonce1 == actual_nonce1*/) {
		/* Check R value for Attacker GW (check RR shim) */
		if (1/*received_RR == actual_RR*/) {
			
			/* Tell Attacker to stop sending traffic to flow */
			//notifyAttacker();
			printf("Notifying Attacker\n");
			
			/* Store filter in TCAM (filter table)*/
			printf("Storing filter in TCAM\n");
			
			/* Send packet with nonce2 */
			printf("Sending nonce2 to Victim Gateway\n");
			msg = "123456789";
			write(sockfd, msg, sizeof(msg));
		}
		/* Incorrect RR */
		else {
			/* Send packet with correct RR and nonce2 */
			printf("Incorrect RR - Sending nonce2 to Victim Gateway\n");
			
		}
	}
	
	close(sockfd);
}

int main() {
	listenGatewayRequest();
	return 0;
}