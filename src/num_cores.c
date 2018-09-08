#include <stdio.h>
#include <sys/sysinfo.h>

/* This program gets the number of cores on the current machine and prints the result to the
 * command line */
int main() {
    int number_of_cores = get_nprocs_conf();
    printf("This machine has %d cores\n", number_of_cores);
    return 0;
}
