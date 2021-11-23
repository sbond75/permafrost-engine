//
//  main.c
//  Defenders of Paradise
//
//  Created by sbond75 on 11/20/21.
//  Copyright © 2021 sbond75. All rights reserved.
//

#include <main.h>

#include <limits.h> /* PATH_MAX */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int main(int argc, char * argv[]) {
    //char* args[] = {argv[0], ".", "./scripts/editor/main.py", NULL};
    char* args[] = {argv[0], ".", "./scripts/pong.py", NULL};
    int numArgs = sizeof(args)/sizeof(args[0]) - 1;

    // https://www.educative.io/edpresso/two-dimensional-arrays-in-c
    char buf[numArgs-1][PATH_MAX]; /* PATH_MAX incudes the \0 so +1 is not required */
    for (size_t i = 1; i < numArgs; i++) {
        // https://stackoverflow.com/questions/1563168/example-of-realpath-function-in-c
        char *res = realpath(args[i], buf[i-1]);
        if (res) { // or: if (res != NULL)
            printf("This source is at %s.\n", buf);
        } else {
            char* errStr = strerror(errno);
            printf("error string: %s\n", errStr);

            perror("realpath");
            exit(EXIT_FAILURE);
        }
        args[i] = res;
    }
    pf_main(numArgs, args);
    
    return 0;
}
