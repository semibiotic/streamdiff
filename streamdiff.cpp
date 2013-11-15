#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

int64_t    start_line[2]   = {0, 0};
int64_t    cur_line[2]     = {0, 0};

char * filename[2]    = {NULL, "-"};
char * prevContext[5] = {NULL, NULL, NULL, NULL, NULL};

char * postContext[2][5] = { {NULL, NULL, NULL, NULL, NULL}, {NULL, NULL, NULL, NULL, NULL} };

char linebuf1[1024];
char linebuf2[1024];

bool isCycled = false;
bool diffOnly = false;

void usage()
{
    fprintf(stderr, "Usage: streamdiff [-sN[,M]] [-c] FILE1 [FILE2]\n");
}

int main(int argc, char ** argv)
{   char * ptr = NULL;

    int64_t i;
    int opt;
    int meet[2];
    int n;

    off64_t stored_offsets[2]  = {0, 0};
    int64_t stored_linenums[2] = {0, 0};

    while ((opt = getopt(argc, argv, "s:cd")) != -1) {
        switch (opt) {
        case 's':
            start_line[0] = strtol(optarg, &ptr, 10);
            if (ptr && *ptr == ',')
                start_line[1] = strtol(ptr+1, NULL, 10);
            else
                start_line[1] = start_line[0];
            break;
        case 'c':
            isCycled = true;
            break;
        case 'd':
            diffOnly = true;
            break;
        default: /* '?' */
            usage();
            exit(EXIT_FAILURE);
        }
    }

    argv += optind;
    argc -= optind;

    if (argv[0]) {
        filename[0] = argv[0];
        if (argv[1]) {
            filename[1] = argv[1];
        }
    }

    if (!filename[0]) {
        usage();
        exit(EXIT_FAILURE);
    }

    while (true) {

        int     fds[2];

        FILE   * file[2] = {NULL, NULL};

    // Open files, seek to stored lines
        for (i=0; i < 2; i++) {
            if (strcmp(filename[i], "-") != 0) {
                fds[i] = open(filename[i], O_RDONLY);
                if (fds[i] >= 0) {
                    if (stored_offsets[i] == lseek64(fds[i], stored_offsets[i], SEEK_SET)) {
                        lseek64(fds[i], stored_offsets[i], SEEK_SET);
                        cur_line[i] = stored_linenums[i];
                    }
                    else {
                        lseek64(fds[i], 0, SEEK_SET);
                        cur_line[i] = 0;
                    }

                    file[i] = fdopen(fds[i], "r");
                }
            }
            else {
                file[i] = stdin;
            }

            if (!file[i]) {
                perror("fopen");
                exit(EXIT_FAILURE);
            }
        }

        if (file[0] == file[1]) {
            usage();
            exit(EXIT_FAILURE);
        }

        for (i=0; i<5; i++) {
            if (prevContext[i] != NULL) {
                free(prevContext[i]);
                prevContext[i] = NULL;
            }
            if (postContext[0][i] != NULL) {
                free(postContext[0][i]);
                postContext[0][i] = NULL;
            }
            if (postContext[1][i] != NULL) {
                free(postContext[1][i]);
                postContext[1][i] = NULL;
            }
        }

    // Skip until start positions
        while (start_line[0] > cur_line[0]) {
            if (!fgets(linebuf1, sizeof(linebuf1), file[0])) {
                perror("skip");
                exit(EXIT_FAILURE);
            }
#ifdef DEBUG
            printf("%lld: %s%s", cur_line[0], ">", linebuf1);
#endif
            cur_line[0]++;
        }

        while (start_line[1] > cur_line[1]) {
            if (!fgets(linebuf1, sizeof(linebuf1), file[1])) {
                perror("skip");
                exit(EXIT_FAILURE);
            }
#ifdef DEBUG
            printf("%lld: %s%s", cur_line[1], "<", linebuf1);
#endif
            cur_line[1]++;
        }

    // Search for difference
        while (true) {
            linebuf1[0] = '\0';
            linebuf2[0] = '\0';

            stored_offsets[0]  = ftello(file[0]);
            stored_offsets[1]  = ftello(file[1]);
            stored_linenums[0] = cur_line[0];
            stored_linenums[1] = cur_line[1];

            fgets(linebuf1, sizeof(linebuf1), file[0]);
            fgets(linebuf2, sizeof(linebuf2), file[1]);

#ifdef DEBUG
            printf("%lld,%lld: %s%s", cur_line[0], cur_line[1], ">", linebuf1);
            printf("%lld,%lld: %s%s", cur_line[0], cur_line[1], "<", linebuf2);
#endif

            if (!strlen(linebuf1) && !strlen(linebuf2)) {
                return(EXIT_SUCCESS);
            }

            if (start_line[0] <= cur_line[0]
                    && strcmp(linebuf1, linebuf2) != 0) {
                break;
            }

            if (prevContext[0] != NULL)
                free(prevContext[0]);

            prevContext[0] = prevContext[1];
            prevContext[1] = prevContext[2];
            prevContext[2] = prevContext[3];
            prevContext[3] = prevContext[4];
            prevContext[4] = strdup(linebuf1);

            cur_line[0]++;
            cur_line[1]++;
        }

        if (!diffOnly)
            printf("found difference in %lld:%lld:\n", cur_line[0], cur_line[1]);

    // display context before difference
        if (!diffOnly) {
            for (i=0; i<5; i++) {
                if (prevContext[i]) {
                    printf("%lld,%lld:  %s", cur_line[0] - (5-i), cur_line[1] - (5-i), prevContext[i]);
                }
            }
        }

        postContext[0][0] = strdup(linebuf1);
        postContext[1][0] = strdup(linebuf2);

    // load context after difference
        for(i=1; i<5; i++) {
            linebuf1[0] = '\0';
            linebuf2[0] = '\0';
            fgets(linebuf1, sizeof(linebuf1), file[0]);
            fgets(linebuf2, sizeof(linebuf2), file[1]);

            postContext[0][i] = strdup(linebuf1);
            postContext[1][i] = strdup(linebuf2);
        }

        fclose(file[0]);
        fclose(file[1]);

     // find difference end
        meet[0] = 5;
        meet[1] = 5;

        for (i=0; i<5; i++) {
            for (n=0; i+n < 5; n++) {
                if (strcmp(postContext[0][i], postContext[1][i+n]) == 0) {
                    meet[0] = i;
                    meet[1] = i+n;
                    goto out;
                }

                if (strcmp(postContext[0][i+n], postContext[1][i]) == 0) {
                    meet[0] = i;
                    meet[1] = i+n;
                    goto out;
                }
            }
        }

    out:

    // display difference
        for (i=0; i < 2; i++) {
            for (n=0; n < meet[i] && n < 5; n++) {
                printf("%lld,%lld: %s%s", cur_line[0]+n, cur_line[1]+n, i ? "+":"-", postContext[i][n]);
            }
        }

    // display context after difference
        if (meet[0] < 5) {
            if (!diffOnly)
                printf("%lld,%lld:  %s", cur_line[0] + meet[0], cur_line[1] + meet[1], postContext[0][meet[0]]);

        // restart search just after difference
            if (isCycled) {
                start_line[0] = cur_line[0] + meet[0];
                start_line[1] = cur_line[1] + meet[1];

#ifdef DEBUG
                printf("\n debug: \n");

                printf("meet = %lld,%lld\n", cur_line[0] + meet[0], cur_line[1] + meet[1]);

                for (n=0; n<5; n++) {
                    printf("%lld@0: %s%s", cur_line[0]+n, "-", postContext[0][n]);
                }

                printf("\n");

                for (n=0; n<5; n++) {
                    printf("%lld@1: %s%s", cur_line[1]+n, "+", postContext[1][n]);
                }
#endif

                continue;
            }
        }

    // difference end is not found, stop
        break;
    } // search restart loop

    printf("fail to find difference end");

    return 0;
}

