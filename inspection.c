/* LIBRARIES */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <ncurses.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>

/* Defining CHECK() tool. By using this methid the code results ligher and cleaner. */
#define CHECK(X) ({int __val = (X); (__val == -1 ? ({fprintf(stderr,"ERROR (" __FILE__ ":%d) -- %s\n",__LINE__,strerror(errno)); exit(-1);-1;}) : __val); })

/* COLORS */
#define RESET "\033[0m"
#define BHRED "\e[1;91m"
#define BHGRN "\e[1;92m"
#define BHYEL "\e[1;93m"
#define BHMAG "\e[1;95m"

/* GLOBAL VARIABLES */
int last_row = 20;
int last_col = 20;
FILE *log_file;										// Log file. 
time_t start_time;                                  // Starting time.
char * fifo_est_pos_x = "/tmp/fifo_est_pos_x"; 		// File path.
char * fifo_est_pos_z = "/tmp/fifo_est_pos_z";		// File path.
char * fifo_inspection = "/tmp/command_to_in_pid";	// File path.

/* FUNCTIONS HEADERS */
void signal_handler( int sig );
void setup_console();
void printer( float x, float z );
void logPrint ( char * string );

/* FUNCTIONS */
void signal_handler( int sig ) {
    /* Function to handle the SIGWINCH signal. The OS send this signal to the process when the size of
    the terminal changes. */

    if (sig == SIGWINCH) {
    /* If the size of the terminal changes, clear and restart the grafic interface. */
    	endwin();
        setup_console();
    }
}

void setup_console() {
	/* Function to initialize the console.
	To refresh the same console, the ncurses library is used. */

	initscr(); // Init the console screen.
	refresh();
	clear();

	/* Print the base structure of the GUI. */
	addstr("This is the INSPECTION console.\n");
	addstr("Press the 'r' for resetting the hoist position\n");
	addstr("Press the 's' for stopping the hoist movement\n");
	addstr("\n");
	addstr("||===================================================||---> x\n");
	addstr("||                                                   ||\n");
	addstr("||                                                   ||\n");
	addstr("||                                                   ||\n");
	addstr("||                                                   ||\n");
	addstr("||                                                   ||\n");
	addstr("||                                                   ||\n");
	addstr("||                                                   ||\n");
	addstr("||                                                   ||\n");
	addstr("||                                                   ||\n");
	addstr("||                                                   ||\n");
	addstr("||                                                   ||\n");
	addstr("||                                                   ||\n");
	addstr("||                                                   ||\n");
	addstr("||                                                   ||\n");
	addstr("||                                                   ||\n");
	addstr("||                                                   ||\n");
	addstr("||                                                   ||\n");
	addstr("|\n");
	addstr("v\n");
	addstr("z\n");
	addstr("\n");

	refresh(); // Send changes to the console.
}

void printer( float x, float z ) {
	/* Function to print dinamically the position of the hoist in the console. */

	int row = floor((z / 0.625) + 0.2) + 5; // Row of the hoist.
	int col = floor((x / 0.2) + 0.2) + 2;   // Column of the hoist.
	char str[80];                           // String to print on log file.

	curs_set(0); // Hide the cursor.

	for (int i = 5; i <= last_row; i++) { // Overwrite with blank spaces the old characters.
		move(i, last_col);
		addch(' ');
	}

	for (int i = 5; i < row; i++) { // Write the new chain on the screen.
		move(i, col);
		addch('|');
	}

	move(row, col); // Write the hoist.
	addch('V');

	last_row = row; // Update position.
	last_col = col;

	/* Print on screen dynamically with a fixed format. */
	move(26, 0);
	if (x >= 0 && z >= 0)
	{
		printw("Estimated position (X, Z) = ( %.3f, %.3f)\n", x, z);
	}
	else if (x < 0 && z >= 0)
	{
		printw("Estimated position (X, Z) = (%.3f, %.3f)\n", x, z);
	}
	else if (x >= 0 && z < 0)
	{
		printw("Estimated position (X, Z) = ( %.3f,%.3f)\n", x, z);
	}
	else if (x < 0 && z < 0)
	{
		printw("Estimated position (X, Z) = (%.3f,%.3f)\n", x, z);
	}

	time_t ltime = time(NULL);
	printw("Execution time = %ld\n", ltime - start_time); // Print the execution time in seconds.

	refresh(); // Send changes to the console.
}

void logPrint ( char * string ) {
    /* Function to print on log file adding time stamps. */

    time_t ltime = time(NULL);
    fprintf( log_file, "%.19s: %s", ctime( &ltime ), string );
    fflush(log_file);
}

/* MAIN */
int main(int argc, char *argv[]) {

	int fd_from_motor_x, fd_from_motor_z, fd_command_pid; //file descriptors
	int ret;											  //select() system call return value
	char str[80];                                         // String to print on log file.

	/* Signals that the process can receive. */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &signal_handler;
    sa.sa_flags = SA_RESTART;
	
    /* sigaction for SIGWINCH */
	CHECK(sigaction(SIGWINCH,&sa,NULL));

	/*process IDs*/
	pid_t pid_motor_x = atoi(argv[1]);
	pid_t pid_motor_z = atoi(argv[2]);
	pid_t pid_wd = atoi(argv[3]);
	pid_t command_pid;

	//receive command pid
	fd_command_pid = CHECK(open(fifo_inspection, O_RDONLY));

	CHECK(read(fd_command_pid, &command_pid, sizeof(int)));

	float est_pos_x, est_pos_z; // Estimate hoist X and Z positions.
	char alarm;					// Char that will contain the 'stop' or 'reset' command.

	fd_set rset;				//set of ready file descriptors

	/*the select() system call does not wait for file descriptors to be ready */
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	/*opening pipes*/
	fd_from_motor_x = CHECK(open(fifo_est_pos_x, O_RDONLY));
	fd_from_motor_z = CHECK(open(fifo_est_pos_z, O_RDONLY));

	/* Open the log file */
	log_file = fopen("Log.txt", "a"); 

	logPrint("inspection: Inspection console started.\n");

	setup_console();

	start_time = time(NULL);    // Evaluate the starting time.

	while (1) {

		/* Initialize the set of file descriptors for the select system call. */
		FD_ZERO(&rset);
		FD_SET(fd_from_motor_x, &rset);
		FD_SET(fd_from_motor_z, &rset);
		FD_SET(0, &rset);

		ret = CHECK(select(FD_SETSIZE, &rset, NULL, NULL, &tv));

		if (FD_ISSET(0, &rset) != 0)//if the standard input receives any inputs...
		{					   
			alarm = getchar(); //get keyboard input

			CHECK(kill(pid_wd, SIGTSTP)); //Send a signal to let the watchdog know that an input occurred.

			sprintf(str, "inspection: Input received = %c\n", alarm);
			logPrint(str);

			if (alarm == 's')
			{								
				CHECK(kill(command_pid, SIGUSR1)); 	// STOP command
				CHECK(kill(pid_motor_x, SIGUSR1)); 	// enable input from keyboard
				CHECK(kill(pid_motor_z, SIGUSR1));	// SIGUSR1 signal has been used for STOP command			
				alarm = '0';
			}

			if (alarm == 'r')
			{								
				CHECK(kill(pid_motor_x, SIGUSR2));	//RESET command
				CHECK(kill(pid_motor_z, SIGUSR2));	//SIGUSR2 signal has been used for RESET command
				CHECK(kill(command_pid, SIGUSR2)); 	//alarm the command console that resetting has started!
			}
		}

		if (FD_ISSET(fd_from_motor_z, &rset) != 0)
		{
			CHECK(read(fd_from_motor_z, &est_pos_z, sizeof(float))); //read the estimated position from motors...
		}

		if (FD_ISSET(fd_from_motor_x, &rset) != 0)
		{
			CHECK(read(fd_from_motor_x, &est_pos_x, sizeof(float)));
		}

		if ( (est_pos_x < 0.001) && (est_pos_z < 0.001) && (alarm == 'r') )
		{
			CHECK(kill(command_pid, SIGUSR1)); //alarm the command console that resetting has finished!
			alarm = 's'; //the hoist can now stop!
		}

		printer(est_pos_x, est_pos_z);

		sprintf(str, "inspection: est_pos_x = %f, est_pos_z = %f\n", est_pos_x, est_pos_z); //print data on LogFile
		logPrint(str);

		usleep(15000); //sleep

	} // End of while.

	/* Close pipes. */
	CHECK(close(fd_from_motor_x));
	CHECK(close(fd_from_motor_z));
	CHECK(close(fd_command_pid));

	logPrint("inspection: Inspection console ended.\n");

	fclose(log_file); // Close log file.

	return 0;
}