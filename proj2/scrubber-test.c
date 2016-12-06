/**
 *  I am performing the extra credit part for this project
 *  CMSC421 Project 2
 *  Author: 	Jin Hui Xu
 *  E-mail:	ac39537@umbc.edu
 *  Description: In this project is to implement a Linux driver that operates upon a virtual Internet filter. 
 *		The user stores an arbitrary list of dirty words in the driver. The driver then scans incoming 
 *		network packets for those dirty words. For each found word, censor that word by overwriting it 
 *		with asterisks. 
 *
 *  Test report (The things are being tested include):
 *	  Test the function of scrubber_write function, user can write dirty word in to the device node.
 *	  Test the function of scrubber_check and scrubber_handler functions. These two functions can handlle the 
 *	interrupts and censor the dirty words on the incoming network packets and replace it with asterisks.
 *	  Different inputs for the scrubber_write function. For example, lowercase letters, uppercase letters, integers, 
 *	and empty word. The only valid input for dirty word is alphabetic characters, the other characters input will be 
 *	ignored by the function.
 *	  Multiple line input for the scrubber_write function. Any valid input that split by newline will be stored into 
 *	the dirty word linked list.
 *	  Test the packet length can hold up to 80 bytes characters, if the input is greater than the packet length, error 
 *	will occurs and the program will exit.
 * 	  Test the happy path of the program, the program generates the output correctly.
 *	  Test the thread-safe for scrubber_write, scrubber_check, and scrubber_handler functions, call sender and listener
 *	threads to modify the device nodes simultaneously and check the result.
 * 	  Test the function of scrubber_del device, and scrubber_del_write function, delete the existing input and non existig 
 *	input from dirty word linked list. The existing one should be removed from linked list, and the non existing one should 
 *	be ignored by the function.
 *	  
 *    
 */

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#define FILTER_DPORT 4210

static sem_t sem;
static int listener_eventfd;
pthread_mutex_t lock;
pthread_cond_t cv;
int counter = 0;

/**
 * Run the 'listening' thread.
 */
static void *run_listener(void *arg __attribute__ ((unused)))
{
	int daemonfd;
	struct sockaddr_in addr;

	/* create a daemon socket */
	daemonfd = socket(AF_INET, SOCK_STREAM, 0);
	if (daemonfd < 0) {
		perror("Could not create daemon socket");
		exit(EXIT_FAILURE);
	}
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(FILTER_DPORT);
	if (bind(daemonfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("Could not bind to port");
		exit(EXIT_FAILURE);
	}
	if (listen(daemonfd, 10) < 0) {
		perror("Could not listen to port");
		exit(EXIT_FAILURE);
	}

	/* signal that listener is ready */
	sem_post(&sem);

	int epfd;
	struct epoll_event event;
	epfd = epoll_create(1);
	if (epfd < 0) {
		perror("Could not create epoll instance");
		exit(EXIT_FAILURE);
	}
	event.events = EPOLLIN;
	event.data.fd = listener_eventfd;
	epoll_ctl(epfd, EPOLL_CTL_ADD, listener_eventfd, &event);
	event.data.fd = daemonfd;
	epoll_ctl(epfd, EPOLL_CTL_ADD, daemonfd, &event);

	/* keep waiting for connections */
	while (epoll_wait(epfd, &event, 1, -1) > 0) {
		if (event.data.fd == listener_eventfd) {
			/* shutdown daemon */
			break;
		}

		int sockfd;
		socklen_t addrlen = sizeof(addr);
		sockfd = accept(daemonfd, (struct sockaddr *)&addr, &addrlen);
		if (sockfd < 0) {
			perror("Error accepting connection");
			continue;
		}

		printf("Listener: Got a connection\n");

		/* handle received messages, by calling read() upon
		   sockfd */
		/*
		 * YOUR CODE HERE
		 *
		 * HINT: This thread should validate that the strings
		 * are correct. Use synchronizations between the
		 * sending and listening thread, so that the listening
		 * thread can determine when a string is correct.
		 */

		int buff_size = 80;
		int i = 0;
		int size = 4;
		int s;

		while (i < size) {
			char test_buffer[buff_size];
			pthread_mutex_lock(&lock);
			while (counter == 0) {
				pthread_cond_wait(&cv, &lock);
			}
			s = read(sockfd, test_buffer, buff_size);
			if (s == -1) {
				fprintf(stderr, "socket read error\n");
				exit(EXIT_FAILURE);
			}
			printf("message after filtered: %s\n", test_buffer);

			counter--;
			pthread_mutex_unlock(&lock);
			i++;
		}
		close(sockfd);
	}

	close(daemonfd);
	return NULL;
}

/**
 * Run the 'sending' thread.
 *
 * Opens a socket to the listener. Then send messages over the socket.
 */
static void *run_sender(void *arg __attribute__ ((unused)))
{
	int sockfd;
	struct sockaddr_in addr;

	/* block until daemon is ready */
	sem_wait(&sem);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("Could not create sending socket");
		exit(EXIT_FAILURE);
	}
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(FILTER_DPORT);
	if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("Could not connect to server");
		exit(EXIT_FAILURE);
	}

	printf("Sender: Connected\n");

	/* send messages to the listener by calling write() to
	   sockfd */
	/*
	 * YOUR CODE HERE
	 *
	 * HINT: Not only should you send messages to the socket, you
	 * should also intermix writes to /dev/scrubber.
	 */

	int buff_size = 80;
	char test_buffer[buff_size];
	ssize_t s;
	size_t nbytes;

	int fd = open("/dev/scrubber",
		      O_RDWR | O_CREAT,
		      S_IRUSR | S_IWUSR);

	//test pass a single word for dirty words list
	//the word should be stored into the linked list
	strcpy(test_buffer, "the");
	nbytes = strlen(test_buffer);

	s = write(fd, test_buffer, nbytes);
	if (s != nbytes) {
		if (s == -1) {
			fprintf(stderr, "scrubber_write error\n");
			exit(EXIT_FAILURE);
		}
	}
	//test pass a multiple line input for dirty words list
	//the words should be stored into the linked list
	strcpy(test_buffer, "in\ner\nlo");
	nbytes = strlen(test_buffer);

	s = write(fd, test_buffer, nbytes);
	if (s != nbytes) {
		if (s == -1) {
			fprintf(stderr, "scrubber_write error\n");
			exit(EXIT_FAILURE);
		}
	}
	//test pass a empty word to the dirty word list
	//the word should be ignored by the function
	strcpy(test_buffer, "  ");
	nbytes = strlen(test_buffer);

	s = write(fd, test_buffer, nbytes);
	if (s != nbytes) {
		if (s == -1) {
			fprintf(stderr, "scrubber_write error\n");
			exit(EXIT_FAILURE);
		}
	}
	//test pass an uppercase word to the dirty word list
	//the word should be stored into the linked list
	strcpy(test_buffer, "YEAH");
	nbytes = strlen(test_buffer);

	s = write(fd, test_buffer, nbytes);
	if (s != nbytes) {
		if (s == -1) {
			fprintf(stderr, "scrubber_write error\n");
			exit(EXIT_FAILURE);
		}
	}
	//test pass the intergers to the dirty word list
	//the word should be ignored by the function
	strcpy(test_buffer, "3267");
	nbytes = strlen(test_buffer);

	s = write(fd, test_buffer, nbytes);
	if (s != nbytes) {
		if (s == -1) {
			fprintf(stderr, "scrubber_write error\n");
			exit(EXIT_FAILURE);
		}
	}
	//test if dirty word is greater than the packet length 80
	//the input should be ignored by the function
	char test_buffer2[81];

	int x;
	for (x = 0; x < 81; x++) {
		test_buffer2[x] = 'a';
	}
	s = write(fd, test_buffer2, 81);
	if (s != 81) {
		if (s == -1) {
			fprintf(stderr, "scrubber_write error\n");
			exit(EXIT_FAILURE);
		}
	}
	//print all the valid dirty words
	printf("dirty words: %s, %s, %s, %s, %s\n", "the", "in", "er", "lo",
	       "YEAH");

	int size = 4;
	int i = 0;
	char *a[size];
	a[0] = "To be, or not to be, that is the question.";
	a[1] = "Whether 'tis Nobler in the mind to suffer.";
	a[2] = "Hello world!";
	a[3] = "OH, YEAH! WE GOT IT!";

	while (i < size) {
		char *message = a[i];
		send(sockfd, message, strlen(message), 0);
		printf("message before filtered: %s\n", message);
		pthread_mutex_lock(&lock);
		counter++;
		pthread_cond_broadcast(&cv);
		pthread_mutex_unlock(&lock);
		i++;
	}

	close(fd);
	close(sockfd);

	return NULL;
}

int main(void)
{
	pthread_mutex_init(&lock, NULL);
	pthread_cond_init(&cv, NULL);

	/* create a semaphore, to block the sender until listener is
	   ready */
	sem_init(&sem, 0, 0);

	/* create an eventfd object, used to notify the listener when
	   shutting down */
	listener_eventfd = eventfd(0, 0);
	if (listener_eventfd < 0) {
		perror("Could not create eventfd");
		exit(EXIT_FAILURE);
	}

	/* spawn listening and sending threads */
	pthread_t listener_pth, sender_pth;
	pthread_create(&listener_pth, NULL, run_listener, NULL);
	pthread_create(&sender_pth, NULL, run_sender, NULL);

	/* after sending thread has terminated, send a message to
	   listening thread to terminate itself */
	pthread_join(sender_pth, NULL);
	uint64_t val = 1;
	if (write(listener_eventfd, &val, sizeof(val)) < 0) {
		perror("Could not send kill signal to listener");
		exit(EXIT_FAILURE);
	}
	pthread_join(listener_pth, NULL);

	pthread_cond_destroy(&cv);
	pthread_mutex_destroy(&lock);

	char test_buffer2[80];
	size_t s;
	ssize_t nbytes;

	//test the functionality of scrubber_del_write 
	int fd2 = open("/dev/scrubber_del",
		       O_RDWR | O_CREAT,
		       S_IRUSR | S_IWUSR);

	//delete the matching input in the dirty word linked list
	//the matching word in the linked list should be removed
	strcpy(test_buffer2, "the");
	nbytes = strlen(test_buffer2);

	s = write(fd2, test_buffer2, nbytes);
	if (s != nbytes) {
		if (s == -1) {
			fprintf(stderr, "scrubber_write error\n");
			exit(EXIT_FAILURE);
		}
	}
	//delete the non matching input in the dirty word linked list
	//the input should be ignored by the function
	strcpy(test_buffer2, "THE");
	nbytes = strlen(test_buffer2);

	s = write(fd2, test_buffer2, nbytes);
	if (s != nbytes) {
		if (s == -1) {
			fprintf(stderr, "scrubber_write error\n");
			exit(EXIT_FAILURE);
		}
	}

	close(fd2);

	return 0;
}
