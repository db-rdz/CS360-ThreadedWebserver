#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <bits/signum.h>
#include <signal.h>
#include <wait.h>
#include <errno.h>
#include <string.h>

#define DEFAULT_PORT	"8090"
#define DEFAULT_CONFIG	"http.conf"



void usage(char* name);
int init_tcp(char* path, char* port, int verbose);

void my_sigchld_handler(int sig)
{
    printf("SIGNAL REAPER CALLED \n");
    pid_t p;
    int status;

    while ((p=waitpid(-1, &status, WNOHANG)) != -1)
    {
        if(p==0){
            break;
        }
       printf("REAPED CHILD PROCESS WITH ID: %i", p);
    }
}
int main(int argc, char* argv[]) {

    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = my_sigchld_handler;

    sigaction(SIGCHLD, &sa, NULL);

	char* port = NULL;
	char* config_path = NULL;

	int verbose_flag = 0;
	port = DEFAULT_PORT;
	config_path = DEFAULT_CONFIG;

	int c;
	while ((c = getopt(argc, argv, "vp:c:")) != -1) {
		switch (c) {
			case 'v':
				verbose_flag = 1;
		 		break;
			case 'p':
				port = optarg;
				break;
			case 'c':
				config_path = optarg;

				break;
			case '?':
				if (optopt == 'p' || optopt == 'c') {
					fprintf(stderr, "Option -%c requires an argument\n", optopt);
					usage(argv[0]);
					exit(EXIT_FAILURE);
				}
			default:
				fprintf(stderr, "Unknown option encountered\n");
				usage(argv[0]);
				exit(EXIT_FAILURE);
		}
	}

    init_tcp(config_path, port, verbose_flag);

	/* Instantiate your server class or call your server run function here */
	/* example: http_server_run(config_path, port, verbose_flag); */
	return 0;
}

void usage(char* name) {
	printf("Usage: %s [-v] [-p port] [-c config-file]\n", name);
	printf("Example:\n");
        printf("\t%s -v -p 8080 -c http.conf \n", name);
	return;
}