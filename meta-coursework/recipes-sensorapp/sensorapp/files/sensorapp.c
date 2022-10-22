#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
int main(int argc, char **argv)
{
	char *app_name = argv[0];
	char *dev_name = "/dev/hcsr04"; // these 2 lines store the name of the user app and device filename
	int fd = -1;
	char c;
	int d;
	char dt[30]; // Stores datetime coming from kernel module
	char *ptr;
	char infl; // Variable that stores if there are going to be infinite iterations or not
	int i = 0;
	int ch;
	char buffer[8]; // stores the float value of distance since kernel does not allow floating point operations
	switch (argc) // switchcase based on the number of arguments passed during executing application
   	{
   	case 1 : // if no argument is passed, then iterate infinitely
   		printf("No argument supplied, iterating infinitely.\n");
   		infl = 'y';
   		i = ch - 10;
   		break;
   	case 2: // if an argument is passed, then dont iterate infinitely
   		ch = strtol(argv[1],&ptr,10); // converts the argument being passed from string to integer. ptr is used to check if anything other than a number is passed as an argument.
   		(ch <= 0) ? printf("Please enter integer or non-zero values\n") : (ptr[0] == '\0')  ? printf("Displaying %d values\n", ch) : printf("Ignoring characters after integer value and Displaying %d values.\n",ch); // checks if the user has entered a valid argument
   		infl = 'n';
   		break;
   	default : // if too many arguments are passed or a bug causes the arguments passed to be less than 1
   		printf("Too many or too less arguments supplied (please enter only 1 argument after program name)\n");
   		i = ch;
   		break;
   	}	
	while (i<ch) 
	{
		fd = open(dev_name, O_RDWR); //opens the devicefile
		c = 1;
		write( fd, &c, 1 ); // Executes the if statement in the write() function of the module to trigger the sensor
		read( fd, &d, 4 ); // Executes the if statement in the read() function of the module to store the value of pulse duration
		read( fd, &dt, 30 ); // Executes the else statement in the read() function of the module to store the datetime
		printf( "%s Pulse duration : %d us Distance: %f cm\n",dt, d, d/58.0 ); // prints the datetime, pulse duration and distance on the command line
		sprintf(buffer,"%f", d/58.0); // Stores the float value of distance as a string into the buffer
		write( fd, &buffer, 8); // Sends the float value of distance to the kernel module as linux kernel does not allow floating point operations
		close( fd ); // closes the device file
		(infl == 'y') ? NULL : (infl =='n') ? i++ : NULL ; // checks if the loop should iterate infinitely or not
		sleep(1); // to let the sensor adjust and to slow down the values displayed
	}
	return 0;
}
