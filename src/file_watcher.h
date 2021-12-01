#ifndef FILE_WATCHER_H
#define FILE_WATCHER_H

#ifdef __linux__

// Returns 0 on success
int create_file_watcher(int* out_fd /* On success, receives the reusable file descriptor of the inotify instance */);

// Returns 0 on success
int add_watch(int fd, const char* directory);

// Call this until it returns -1 for finished, or just once to handle between 1 and MAX_EVENTS events. Returns 0 on success which also indicates possibly more is ready to be processed with another call to this function. Can also return 1 to indicate that an error occurred.
int handle_changes(int fd, void(*processCreate)(const char* filePath), void(*processModify)(const char* filePath));

void close_file_watcher(int fd);

#elif defined(__APPLE__)
#error "Not yet implemented for this platform: " _WIN32
#elif defined(_WIN32)
#error "Not yet implemented for this platform: " _WIN32
#else
#error "Not yet implemented for this platform: " _WIN32
#endif

#endif
