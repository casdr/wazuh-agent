#ifdef WIN32
#include "windows.h"
#else
#include <sys/stat.h> 
#include <fcntl.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include "shared.h"


void randombytes(void *ptr, unsigned int length)
{

    char failed = 0;

    #ifdef WIN32

    static HCRYPTPROV prov = 0;
    if (prov == 0) {
        if (!CryptAcquireContext(&prov, NULL, NULL, PROV_RSA_FULL, 0)) {
            failed = 1;
        }
    }
    if (!failed && !CryptGenRandom(prov, length, ptr)) {
        failed = 1;
    }

    #else

    int fh;
    if ((fh = open("/dev/urandom", O_RDONLY)) >= 0 || (fh = open("/dev/random", O_RDONLY)) >= 0) {
        if (read(fh, ptr, length) == 0) {
            failed = 1;
        }
        close(fh);
    } else {
        failed = 1;
    }

    #endif

    if (failed) {
        ErrorExit("%s: Error in randombytes failed on all possible methods for accessing random data", __local_name);
    }
}


void srandom_init(void)
{

    #ifndef WIN32
    unsigned int seed; 
    #ifdef __OpenBSD__
    srandomdev();
    #else
    randombytes(&seed, sizeof seed);
    srandom(seed);
    #endif  // __OpenBSD__
    #endif  // Win32

}
