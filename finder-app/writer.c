#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>

#define DEBUG 0

int main(int argc, char *argv[]) {

#if DEBUG
	printf ("argc = %d\n",argc);
	
	for (int i=0; i<argc; i++) {
		printf ("argv[%d] = %s\n",i,argv[i]);
	}
#endif
	openlog("writer.c", LOG_PID | LOG_CONS, LOG_USER);
	
	if (argc != 3) {
    		syslog(LOG_NOTICE, "%s", "No of arguments are more/less then define");
		return 1;
	}
	char *writefile = argv[1];
	char *writestr = argv[2];
	
	FILE *file = fopen(writefile,"w");

	if (file == NULL) {
    		syslog(LOG_ERR, "%s", "Error opening file!!");
		return 1;
	}


	unsigned int total_len = strlen(writefile)+strlen(writestr)+12;
	char *debug_log = (char *)malloc(total_len);
	if (file == NULL) {
		syslog(LOG_WARNING,"Dynamic memory allocation failed!");
	}
	else {
		strcpy(debug_log,"Writing ");
		strcat(debug_log,writestr);
		strcat(debug_log," to ");
		strcat(debug_log,writefile);	
		syslog(LOG_DEBUG, "%s", debug_log);
	}
	
	fprintf(file, "%s", writestr);
	
	free(debug_log);	
	fclose(file);
	closelog();
	return 0;
}
