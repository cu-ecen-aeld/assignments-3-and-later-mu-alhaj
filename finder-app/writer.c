#include <stdio.h>
#include <syslog.h>

int main( int argc, char *argv[] ) 
{
    openlog( "writer_utility", LOG_PID | LOG_CONS, LOG_USER );
    
    // check for arguments
    if ( argc < 3 )
    {
        printf( "writer utility needs 2 args\n");
        syslog(LOG_ERR, "writer utility needs 2 args");
        closelog();
        return 1; 
    }

    char* path = argv[1];
    char* str  = argv[2];

    printf ("path %s\n", path);
    printf ("path %s\n", str);


    FILE *f = fopen(path, "w");
    if(f)
    {
        syslog(LOG_DEBUG, "Writing%sto%s",str,path);
        if( fprintf(f, "%s", str) < 0 )
        {
            syslog(LOG_ERR, "Failed to write %s to file %s", str, path);
        }
        fclose(f);
    }
    else
    {
        syslog(LOG_ERR, "Failed to open file %s", path);
        closelog();
        return 1;
    }



    closelog();
    return 0;
}