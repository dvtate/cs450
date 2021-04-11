#include "types.h"
#include "stat.h"
#include "user.h"
#include "pstat.h"
#include "fcntl.h"

int spin()
{
    int k = 0;
    for(int i = 0; i < 50; ++i)
        for(int j = 0; j < 400000; ++j) {
            k = j % 10;
            k = k + 1;
        }

    return k;
}

int main(int argc, char *argv[]){

    const int tickets[] = { 10, 20, 30 };
    int child_pids[3];

    for (int i = 0; i < 3 ; i++) {
        // Make child
        child_pids[i] = fork();
        if (child_pids[i] == 0) {
            printf(1, "spawn: %d", i);
            // Set prio and spin
            settickets(tickets[i]);
            for (;;)
                spin();
        }
    }

    printf(1, "pid#%d: %d, pid#%d: %d, pid#%d: %d\n",
        child_pids[0], tickets[0],
        child_pids[1], tickets[1],
        child_pids[2], tickets[2]);

    // Get 5 datapoints
    for (int n = 0; n < 100; n++) {
        // Spin like the children for a bit
        for (int i = 0; i < 20000; i++)
            spin(); // Should really do something else here but /shrug

        // Get ticks
        int ticks[3] = { 0, 0, 0 };
        struct pstat ps;
        getpinfo(&ps);
        for (int i = 1; i < 3; i++)
            for (int j = 0; j < NPROC; j++)
                if (ps.pid[j] == child_pids[i])
                    ticks[i] = ps.ticks[j];

        // Show ticks
        printf(1, "pid#%d: %d, pid#%d: %d, pid#%d: %d\n",
            child_pids[0], ticks[0],
            child_pids[1], ticks[1],
            child_pids[2], ticks[2]);
    }
}