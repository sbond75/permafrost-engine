#ifdef __linux__

#include "file_watcher.h"

// Based on https://github.com/apetrone/simplefilewatcher/blob/master/source/FileWatcherLinux.cpp , https://lynxbee.com/c-program-to-monitor-and-notify-changes-in-a-directory-file-using-inotify/ , and https://www.opensourceforu.com/2011/04/getting-started-with-inotify/

#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/inotify.h>

#define MAX_EVENTS 4 //1024 /*Max. number of events to process at one go*/
#define LEN_NAME FILENAME_MAX //16 /*Assuming that the length of the filename won't exceed 16 bytes*/
#define EVENT_SIZE  ( sizeof (struct inotify_event) ) /*size of one event*/
#define BUF_LEN     ( MAX_EVENTS * ( EVENT_SIZE + LEN_NAME )) /*buffer to store the data of events*/

// Returns 0 on success
int create_file_watcher(int* out_fd /* On success, receives the reusable file descriptor of the inotify instance */)
{
  int fd;
 
  /* Initialize Inotify*/
  fd = inotify_init();
  if ( fd < 0 ) {
    perror( "Couldn't initialize inotify");
    return 1;
  }

  *out_fd = fd;
  return 0;
}

// Returns 0 on success
int add_watch(int fd, const char* directory) {//, int* out_wd /* On success, this receives the watch descriptor, and all these should be passed to the close_file_watcher() function when freeing this resource. */) {
  int wd;
  
  /* add watch to starting directory */
  wd = inotify_add_watch(fd, directory, IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE | IN_MOVED_FROM | IN_DELETE); 
 
  if (wd == -1)
    {
      printf("Couldn't add watch to %s: %s\n",directory, strerror(errno));
      return 1;
    }
  else
    {
      printf("Watching directory: %s\n",directory);
    }

  //*out_wd = wd;
  return 0;
}

// Call this until it returns -1 for finished, or just once to handle between 1 and MAX_EVENTS events. Returns 0 on success which also indicates possibly more is ready to be processed with another call to this function. Can also return 1 to indicate that an error occurred.
int handle_changes(int fd, void(*processCreate)(const char* filePath), void(*processModify)(const char* filePath)) {
  int length, i = 0, wd;
  char buffer[BUF_LEN];
  
  /* do it once*/
    {
      i = 0;
      length = read( fd, buffer, BUF_LEN );  
 
      if ( length < 0 ) {
        perror( "read" );
	return 1;
      }  
 
      while ( i < length ) {
        struct inotify_event *event = ( struct inotify_event * ) &buffer[ i ];
        if (event->mask & IN_ISDIR) {
	  if (event->mask & IN_CREATE) {
              printf( "The directory %s was created.\n", event->name );
	  }
	  else if (event->mask & IN_MODIFY) {
	    printf( "The directory %s was modified.\n", event->name );
	  }
	  else {
	    printf("Warning: unhandled event mask (for directory %s) %ju\n", event->name, event->mask);
	  }
	}
	else {
	  // File
	  if (event->mask & IN_CREATE) {
	    printf( "The file %s was created.\n", event->name );
	    processCreate(event->name);
	  }
	  else if (event->mask & IN_MODIFY) {
	    printf( "The file %s was modified.\n", event->name );
	    processModify(event->name);
	  }
	  else {
	    printf("Warning: unhandled event mask (for file %s) %ju\n", event->name, event->mask);
	  }
	}
	i += EVENT_SIZE + event->len;
      }
    }
    
    return length == BUF_LEN ? -1 : 0;
}

void close_file_watcher(int fd) {
  /* Clean up*/
  //inotify_rm_watch( fd, wd );
  close( fd );
}

#elif defined(__APPLE__)
#error "Not yet implemented for this platform: " _WIN32
#elif defined(_WIN32)
#error "Not yet implemented for this platform: " _WIN32
#else
#error "Not yet implemented for this platform: " _WIN32
#endif
