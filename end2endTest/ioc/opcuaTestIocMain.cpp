#include <cstdlib>

#include "epicsExit.h"
#include "epicsThread.h"
#include "iocsh.h"

int main(int argc, char *argv[])
{
    if (argc >= 2) {
        iocsh(argv[1]);
        epicsThreadSleep(0.2);
    }

    iocsh(nullptr);

    epicsExit(0);
    return EXIT_SUCCESS;
}
