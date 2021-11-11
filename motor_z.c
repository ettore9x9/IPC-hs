#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#define Z_UB 9.9
#define Z_LB 0
#define STEP 0.01

float z_position = Z_LB;
float est_pos_z = Z_LB;
int command = 0;

void signal_handler(int sig) {

    if(sig==SIGUSR1){
        command=5;
    }   
    if(sig==SIGUSR2){
        z_position=0;
        command=5;
    }
}

float float_rand( float min, float max ) {
    // Function to generate a randomic error.

    float scale = rand() / (float) RAND_MAX; /* [0, 1.0] */
    return min + scale * ( max - min );      /* [min, max] */
}

int main(){

    int fd_z,fd_inspection_z, ret;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler =&signal_handler;
    sa.sa_flags=SA_RESTART;

    struct sigaction sa2;
    memset(&sa2, 0, sizeof(sa2));
    sa2.sa_handler =&signal_handler;
    sa2.sa_flags=SA_RESTART;

    sigaction(SIGUSR1,&sa,NULL);
    sigaction(SIGUSR2,&sa2,NULL);

    fd_set rset;

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    fd_z = open("fifo_command_to_mot_z", O_RDONLY);
    fd_inspection_z=open("fifo_est_pos_z", O_WRONLY);
    
    while(1){    
        FD_ZERO(&rset);
        FD_SET(fd_z, &rset);
        ret = select(FD_SETSIZE, &rset, NULL, NULL, &tv);

        if(ret == -1){
            perror("select() on motor z\n");
            fflush(stdout);
        }
        else if(FD_ISSET(fd_z, &rset) != 0){
            read(fd_z, &command, sizeof(int));
        }
        if(command == 1){
            //printf("Motor Z received: increase\n");
            if (z_position > Z_UB){
                command = 5;
               // printf("\rUpper Z limit of the work envelope reached.");
            } else {
                z_position += STEP;
            }
        }
        if(command == 2){
            //Motor Z received: decrease
            if (z_position < Z_LB){
                command = 5;
                //Lower Z limit of the work envelope reached
                } else {
                    z_position -= STEP;
                }
            }
        if(command == 5){
            //do nothing
        }


        // Sleeps. If the command does not change, than repeats again the same command.
        est_pos_z=z_position + float_rand(-0.005,0.005); //compute the estimated position
        write(fd_inspection_z, &est_pos_z, sizeof(float)); //send to inspection konsole
        usleep(20000);

    } // End of the while cycle.

    close(fd_z);
    close(fd_inspection_z);

    return 0;
}