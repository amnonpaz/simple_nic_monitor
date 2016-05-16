#include <asm/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if.h>
#include <poll.h>

#include <string.h>
#include <errno.h>
#include <stdio.h>

#define SERVICE_NAME "NIC Monitor"

#define LOG(msg, ...) printf("[" SERVICE_NAME "] " msg "\n", ##__VA_ARGS__)
#define LOGE(msg, ...) LOG("ERROR " msg, ##__VA_ARGS__)
#define LOGI(msg, ...) LOG("INFO  " msg, ##__VA_ARGS__)

#define MAX_BUFFER_LEN 4096

#define ARRAY_LEN(arr) (sizeof(arr)/sizeof(arr[0]))
#define WAIT_FOREVER (-1)

/**
 * Create a socket and bind it to NL RT family address.
 *
 * Returns: Socket fd on success, negative value on faliure.
 */
int create_socket()
{
    struct sockaddr_nl sa;
    int fd = 0;
    int err;

    memset(&sa, 0, sizeof(sa));
    sa.nl_family = AF_NETLINK;
    sa.nl_groups = RTMGRP_LINK;
    fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (fd <= 0) {
        LOGE("Failed opening netlink socket (%s)", strerror(errno));
        return 0;
    }

    err = bind(fd, (struct sockaddr *) &sa, sizeof(sa));
    if (err < 0) {
        LOGE("Failed binding netlink socket (%s)", strerror(errno));
        return 0;
    }
    
    return fd;
}

/**
 * Browse through the RT attribute of a message
 * and find the attribute containing the if name
 *
 * Returns: On success, interface name. Null on failure.
 */
char *rtattr_get_name(struct rtattr *rta, int rta_len)
{
    char *if_name = NULL;

    for ( ; RTA_OK(rta, rta_len) ; rta = RTA_NEXT(rta, rta_len)) {
        if (rta->rta_type == IFLA_IFNAME) {
            if_name = (char*)RTA_DATA(rta);
            break;
        }
    }
    
    return if_name;
}

/**
 * Parse netlink messages buffer.
 *
 * Returns: Zero on success, negative value on failure.
 */
int handle_message(char buff[], int len)
{
    struct nlmsghdr *nh;
    struct ifinfomsg *ifi;
    char *if_name;
    int res = 0;

    for (nh = (struct nlmsghdr *)buff; NLMSG_OK(nh, len);
        nh = NLMSG_NEXT(nh, len)) {

        if (nh->nlmsg_type == NLMSG_DONE)
            break;

        if (nh->nlmsg_type == NLMSG_ERROR) {
            LOGE("Netlink error");
            res = -1;
            break;
        }

        ifi = NLMSG_DATA(nh);
        if (nh->nlmsg_type != RTM_NEWLINK || !ifi->ifi_change)
            continue;

        if_name = rtattr_get_name(IFLA_RTA(ifi), IFLA_PAYLOAD(nh));
        if (!if_name)
            continue;

        LOGI("Interface %s is now %s",
             if_name, (ifi->ifi_flags & IFF_UP) ? "up" : "down");
    }

    return res;
}

/**
 * Poll for messages and read buffer from netlink socket
 *
 * Return: Zero on success, negative value on failure
 */
int wait_message(int fd)
{
    struct pollfd fds[1];
    char buff[MAX_BUFFER_LEN];
    int len;
   
    fds[0].fd = fd;
    fds[0].events = POLLIN;

    if (poll(fds, ARRAY_LEN(fds), WAIT_FOREVER) < 0) {
        LOGE("Polling error: %s", strerror(errno));
        return -1;
    }

    len = recv(fd, buff, MAX_BUFFER_LEN, 0);
    if (len < 0) {
        LOGE("Error recieving message: %s", strerror(errno));
        return -1;
    }

    return handle_message(buff, len);
}

int main(int argv, char *argc[])
{
    int fd;

    LOGI("NIC Monitor daemon is now running");

    fd = create_socket();
    if (!fd) {
        LOGE("Initialization failed");
        return 1;
    }

    do {
        if (wait_message(fd) < 0) {
            LOGE("Terminating due to an error");
            break;
        }
    } while (1);

    return 0;
}
