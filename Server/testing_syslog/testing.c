#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>

int main(){
    openlog("Logs", LOG_PID, LOG_USER);
    syslog(LOG_INFO, "Start logging");
    closelog();
    return 0;
}