/**
 *  CMSC421 Project 1
 *  Author: 	Jin Hui Xu
 *  E-mail:	ac39537@umbc.edu
 *  Description: This project will implement a Linux kernel module that will "encrypt" data using a Caesar Cipher. 
 *		The encryption key is the number of letters to shift. The module will create two miscellaneous devices, 
 *		/dev/rotX and /dev/rotXctl. The user writes to /dev/rotX the encryption key. Next, the user creates 
 *		a memory map to /dev/rotX, via mmap(), to store the data to encrypt. Next, the user writes the magic 
 *		string go to /dev/rotXctl to actually perform the encryption; the resulting ciphertext is fetched from 
 *		the memory mapping. 
 *
 *  Test report (The things are being tested include):
 *	  Different inputs such as integer, double, negative integer, mix of letter and integer, and empty space for rotX_write function, 
 *	the only valid input for encryption key should be a positive integer. Otherwise, program exit with error.
 *        The rotX_write function perform correctly by writing the key into buffer, program will exit if there is any error.
 *	  The rotX_read function perform correctly by reading the key from buffer and display it, program will exit if there is any error.
 *	  Different data for rotXctl_write to encrypt, only large case and small case alphabetic characters should be convert to 
 *	corresponding shifted letter by the key. Therefore, any characters beside alphabet will be ignored.
 *	  Different begin string of @ubuf for rotXctl_write, any string begins with "go" should triggle function to perform encryption. 
 *	Also, test the case sensitive for "go", so function will consume the large case of "go" input and do nothing.
 * 	  Test the thread-safe for rotX_read, rotX_write, and rotX_read functions, call two threads to read/write to the device nodes 	    
 *	simultaneously and check the result.
 *	  Test the buffer can hold up to one page size memory, and encryption the one page size characters correctly.
 *	  Test the valid range of encryption key, valid range is 1~25.
 *	  
 *    
 */

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <pthread.h>

static void *thread(void *args)
{
	char test_buffer[80];
	char test_buffer2[80];
	size_t nbytes;
	ssize_t s;
	char str[2];

	int i = *(int *)args;
	sprintf(str, "%d", i);

	int fd = open("/dev/rotX",
		      O_RDWR | O_CREAT,
		      S_IRUSR | S_IWUSR);

	strcpy(test_buffer, str);
	nbytes = strlen(test_buffer);

	s = write(fd, test_buffer, nbytes);
	if (s != nbytes) {
		if (s == -1) {
			fprintf(stderr, "rotX_write error\n");
			exit(EXIT_FAILURE);
		}
	}
	//rotX_read function testing
	nbytes = sizeof(test_buffer2);
	s = read(fd, test_buffer2, nbytes);
	if (s < 0) {
		if (s == -1) {
			fprintf(stderr, "rotX_read error\n");
			exit(EXIT_FAILURE);
		}
	}
	printf("thread encryption key: %s\n", test_buffer2);

	strcpy(test_buffer, "go");
	nbytes = strlen(test_buffer);

	int fd2 = open("/dev/rotXctl",
		       O_RDWR | O_CREAT,
		       S_IRUSR | S_IWUSR);

	s = write(fd2, test_buffer, nbytes);
	if (s != nbytes) {
		if (s == -1) {
			fprintf(stderr, "rotXctl_write error\n");
			exit(EXIT_FAILURE);
		}
	}

	close(fd);
	close(fd2);

	pthread_exit(0);
}

int main(void)
{
	char test_buffer[80];
	char test_buffer2[80];
	char test_buffer3[80];
	size_t nbytes;
	ssize_t s;
	int i;

	int fd = open("/dev/rotX",
		      O_RDWR | O_CREAT,
		      S_IRUSR | S_IWUSR);

	//rotX_write function testing
	strcpy(test_buffer, "13");
	nbytes = strlen(test_buffer);

	s = write(fd, test_buffer, nbytes);
	if (s != nbytes) {
		if (s == -1) {
			fprintf(stderr, "rotX_write error\n");
			exit(EXIT_FAILURE);
		}
	}
	//rotX_read function testing
	nbytes = sizeof(test_buffer2);
	s = read(fd, test_buffer2, nbytes);
	if (s < 0) {
		if (s == -1) {
			fprintf(stderr, "rotX_read error\n");
			exit(EXIT_FAILURE);
		}
	}
	printf("encryption key: %s\n", test_buffer2);

	char *dest =
	    mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if (dest == MAP_FAILED) {
		fprintf(stderr, "mmap() error\n");
		exit(1);
	}
	//rotXclt_write function testing
	//set our data with mix of small and large case of letters with some punctuations
	strcpy(dest, "127ABCDE,#$(abcde!");
	printf("data before encryption: %s\n", dest);

	int fd2 = open("/dev/rotXctl",
		       O_RDWR | O_CREAT,
		       S_IRUSR | S_IWUSR);

	//set @ubuf to go and start encrypt data
	strcpy(test_buffer3, "go");
	nbytes = strlen(test_buffer3);

	s = write(fd2, test_buffer3, nbytes);
	if (s != nbytes) {
		if (s == -1) {
			fprintf(stderr, "rotXctl_write error\n");
			exit(EXIT_FAILURE);
		}
	}
	//see results, punctuations should not be encrypted
	//the letters should be encrypted to the corresponding large or small letter
	printf("data after encryption: %s\n", dest);

	//test with capital "google" as ubuf, the data should be encrypted
	strcpy(test_buffer3, "google");
	nbytes = strlen(test_buffer3);

	s = write(fd2, test_buffer3, nbytes);
	if (s != nbytes) {
		if (s == -1) {
			fprintf(stderr, "rotXctl_write error\n");
			exit(EXIT_FAILURE);
		}
	}
	printf("data after encryption: %s\n", dest);

	//test with capital "GO" as @ubuf, the data should not be encrypted
	strcpy(test_buffer3, "GO");
	nbytes = strlen(test_buffer3);

	s = write(fd2, test_buffer3, nbytes);
	if (s != nbytes) {
		if (s == -1) {
			fprintf(stderr, "rotXctl_write error\n");
			exit(EXIT_FAILURE);
		}
	}
	printf("data after encryption: %s\n", dest);

	//test with capital "Go" as @ubuf, the data should not be encrypted
	strcpy(test_buffer3, "Go");
	nbytes = strlen(test_buffer3);

	s = write(fd2, test_buffer3, nbytes);
	if (s != nbytes) {
		if (s == -1) {
			fprintf(stderr, "rotXctl_write error\n");
			exit(EXIT_FAILURE);
		}
	}
	printf("data after encryption: %s\n", dest);

	//test with capital "Go" as @ubuf, the data should not be encrypted
	strcpy(test_buffer3, "gO");
	nbytes = strlen(test_buffer3);

	s = write(fd2, test_buffer3, nbytes);
	if (s != nbytes) {
		if (s == -1) {
			fprintf(stderr, "rotXctl_write error\n");
			exit(EXIT_FAILURE);
		}
	}
	printf("data after encryption: %s\n", dest);

	//test with capital "  " as @ubuf, the data should not be encrypted
	strcpy(test_buffer3, "  ");
	nbytes = strlen(test_buffer3);

	s = write(fd2, test_buffer3, nbytes);
	if (s != nbytes) {
		if (s == -1) {
			fprintf(stderr, "rotXctl_write error\n");
			exit(EXIT_FAILURE);
		}
	}
	printf("data after encryption: %s\n", dest);

	close(fd);
	close(fd2);

	//test the valid key for encryption, valid range is 1~25
	fd = open("/dev/rotX", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

	strcpy(test_buffer, "26");
	nbytes = strlen(test_buffer);

	s = write(fd, test_buffer, nbytes);
	if (s != nbytes) {
		if (s == -1) {
			fprintf(stderr, "rotX_write error\n");
			exit(EXIT_FAILURE);
		}
	}
	//rotX_read function testing
	nbytes = sizeof(test_buffer2);
	s = read(fd, test_buffer2, nbytes);
	if (s < 0) {
		if (s == -1) {
			fprintf(stderr, "rotX_read error\n");
			exit(EXIT_FAILURE);
		}
	}
	printf("encryption key: %s\n", test_buffer2);

	fd2 = open("/dev/rotXctl", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

	strcpy(test_buffer3, "go");
	nbytes = strlen(test_buffer3);

	s = write(fd2, test_buffer3, nbytes);
	if (s != nbytes) {
		if (s == -1) {
			fprintf(stderr, "rotXctl_write error\n");
			exit(EXIT_FAILURE);
		}
	}
	//the data should not be encrypted because of the invalid key
	printf("data after encryption: %s\n", dest);

	close(fd);
	close(fd2);

	//thread safe testing
	//test the read/write functions are thread safe
	pthread_t tids[10];
	//int num[10];
	for (i = 0; i < 10; i++) {
		if (pthread_create(&tids[i], NULL, thread, &i) != 0)
			exit(EXIT_FAILURE);
	}

	for (i = 0; i < 10; i++) {
		pthread_join(tids[i], NULL);
	}

	//the result data should be shifted by sum of encryption key of all threads
	printf("data after encryption: %s\n", dest);

	//test the buffer can hold up to one page size memory
	fd = open("/dev/rotX", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

	strcpy(test_buffer, "1");
	nbytes = strlen(test_buffer);

	s = write(fd, test_buffer, nbytes);
	if (s != nbytes) {
		if (s == -1) {
			fprintf(stderr, "rotX_write error\n");
			exit(EXIT_FAILURE);
		}
	}

	nbytes = sizeof(test_buffer2);
	s = read(fd, test_buffer2, nbytes);
	if (s < 0) {
		if (s == -1) {
			fprintf(stderr, "rotX_read error\n");
			exit(EXIT_FAILURE);
		}
	}
	printf("encryption key: %s\n", test_buffer2);

	//set our data maximum size + 1
	char string[4096];
	//set up all char in string as 'a'
	for (i = 0; i < 4096; i++)
		string[i] = 'a';
	strcpy(dest, string);
	//the last char in buffer should be 'a'
	printf("last char in max size data before encryption: %c\n",
	       dest[4095]);

	fd2 = open("/dev/rotXctl", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

	strcpy(test_buffer3, "go");
	nbytes = strlen(test_buffer3);

	s = write(fd2, test_buffer3, nbytes);
	if (s != nbytes) {
		if (s == -1) {
			fprintf(stderr, "rotXctl_write error\n");
			exit(EXIT_FAILURE);
		}
	}
	//see results, the last character in the buffer should be encrypted
	//the last char in buffer should be 'b'
	printf("last char in max size data after encryption: %c\n", dest[4095]);

	close(fd);
	close(fd2);

	//rotX_write function input testing
	fd = open("/dev/rotX", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

	//set double as key should cause error
	strcpy(test_buffer, "12.5");

	//set negative integer, non integer, and empty space as key should cause error
	//uncomment following codes for testing these three case 
	//strcpy(test_buffer, "-12");
	//strcpy(test_buffer, "acb1d");
	//strcpy(test_buffer, " ");

	nbytes = strlen(test_buffer);
	s = write(fd, test_buffer, nbytes);
	if (s != nbytes) {
		if (s == -1) {
			fprintf(stderr, "rotX_write error\n");
			exit(EXIT_FAILURE);
		}
	}

	nbytes = sizeof(test_buffer2);
	s = read(fd, test_buffer2, nbytes);
	if (s < 0) {
		if (s == -1) {
			fprintf(stderr, "rotX_read error\n");
			exit(EXIT_FAILURE);
		}
	}
	printf("encryption key: %s\n", test_buffer2);
	close(fd);

	return 0;
}
