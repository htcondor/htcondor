#include <stdio.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <linux/sockios.h>
#include <linux/if.h>
#include <linux/ethtool.h>
#include <string.h>
#include <stdlib.h>

int main (int argc, char **argv)
{
    int sock;
    struct ifreq ifr;
    struct ethtool_cmd edata;
    int rc;

    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        perror("socket");
        exit(1);
    }

    strncpy(ifr.ifr_name, "eth0", sizeof(ifr.ifr_name));
    ifr.ifr_data = &edata;

    edata.cmd = ETHTOOL_GSET;

    rc = ioctl(sock, SIOCETHTOOL, &ifr);
    if (rc < 0) {
        perror("ioctl");
        exit(1);
    }
    switch (edata.speed) {
        case SPEED_10: printf("DetectedBandwidth = 10\n"); break;
        case SPEED_100: printf("DetectedBandwidth = 100\n"); break;
        case SPEED_1000: printf("DetectedBandwidth = 1000\n"); break;
        case SPEED_2500: printf("DetectedBandwidth = 2500\n"); break;
        case SPEED_10000: printf("DetectedBandwidth = 10000\n"); break;
        default: printf("Speed returned is %d\n", edata.speed);
    }

    return (0);
}
