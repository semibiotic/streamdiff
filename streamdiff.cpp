#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

int64_t    start_line[2]   = {0, 0};
int64_t    cur_line[2]     = {0, 0};

char * filename[2]    = {NULL, "-"};
char * prevContext[5] = {NULL, NULL, NULL, NULL, NULL};

char * postContext[2][5] = { {NULL, NULL, NULL, NULL, NULL}, {NULL, NULL, NULL, NULL, NULL} };

char linebuf1[1024];
char linebuf2[1024];

bool isCycled = false;

void usage()
{
    fprintf(stderr, "Usage: showdiff [-sN[,M]] [-c] FILE1 [FILE2]\n");
}

int main(int argc, char ** argv)
{   char * ptr = NULL;

    int64_t i;
    int opt;
    int meet[2];
    int n;


    while ((opt = getopt(argc, argv, "s:c")) != -1) {
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

    FILE   * file[2] = {NULL, NULL};

    for (i=0; i < 2; i++) {
        if (strcmp(filename[i], "-") != 0)
            file[i] = fopen(filename[i], "r");
        else
            file[i] = stdin;

        if (!file[i]) {
            perror("fopen");
            exit(EXIT_FAILURE);
        }
    }

    if (file[0] == file[1]) {
        usage();
        exit(EXIT_FAILURE);
    }

    cur_line[0] = 0;
    cur_line[1] = 0;

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

    if (start_line[0] != start_line[1]) {
        int         fileInd   = start_line[0] > start_line[1] ? 0 : 1;
        int64_t     skipLines = llabs(start_line[0] - start_line[1]);

        for (i=0; i < skipLines; i++) {
            if (!fgets(linebuf1, sizeof(linebuf1), file[fileInd])) {
                perror("skip");
                exit(EXIT_FAILURE);
            }
            cur_line[fileInd]++;
        }
    }

    while (true) {
        linebuf1[0] = '\0';
        linebuf2[0] = '\0';

        fgets(linebuf1, sizeof(linebuf1), file[0]);
        fgets(linebuf2, sizeof(linebuf2), file[1]);

        if (!strlen(linebuf1) && !strlen(linebuf2)) {
            fprintf(stderr, "files are identical");
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

    printf("found difference in %lld:%lld:\n", cur_line[0], cur_line[1]);

    for (i=0; i<5; i++) {
        if (prevContext[i]) {
            printf("%lld,%lld:  %s", cur_line[0] - (5-i), cur_line[1] - (5-i), prevContext[i]);
        }
    }

    postContext[0][0] = strdup(linebuf1);
    postContext[1][0] = strdup(linebuf2);

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

    int n;

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

    for (i=0; i < 2; i++) {
        for (n=0; n < meet[i] && n < 5; n++) {
            printf("%lld,%lld: %s%s", cur_line[0]+n, cur_line[1]+n, i ? "+":"-", postContext[i][n]);
        }
    }

    if (meet[0] < 5) {
        printf("%lld,%lld:  %s", cur_line[0] + meet[0], cur_line[1] + meet[1], postContext[0][meet[0]]);

        if (isCycled) {
            start_line[0] = cur_line[0] + meet[0];
            start_line[1] = cur_line[1] + meet[1];
            continue;
        }

        printf("\n"
               "./showdiff -s%lld,%lld %s %s\n", cur_line[0] + meet[0], cur_line[1] + meet[1], filename[0], filename[1]);
    }

    break;
} // loop

    printf("\n debug: \n");

    printf("meet = %lld,%lld\n", cur_line[0] + meet[0], cur_line[1] + meet[1]);

    for (n=0; n<5; n++) {
        printf("%lld,%lld: %s%s", cur_line[0]+n, cur_line[1]+n, "-", postContext[0][n]);
    }

    printf("\n");

    for (n=0; n<5; n++) {
        printf("%lld,%lld: %s%s", cur_line[0]+n, cur_line[1]+n, "+", postContext[1][n]);
    }

    return 0;
}

