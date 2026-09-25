#include <stdio.h>
#include "../nbnet_udp.h"

int main(void)
{
    NBN_UDP_Init();

    if (NBN_GameServer_StartEx("test", 42042) < 0)
    {
        printf("Failed to start server\n");
        return 1;
    }
}
