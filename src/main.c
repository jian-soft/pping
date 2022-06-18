#include <stdio.h>
#include <string.h> //memcpy
#include <unistd.h> //getpid
#include <netdb.h> //getaddrinfo
#include <arpa/inet.h>
#include <netinet/ip_icmp.h>
#include <poll.h> //poll

int g_pid;
int g_sn;

int create_socket(int family)
{
    int fd = socket(family, SOCK_DGRAM, IPPROTO_ICMP);
    if (fd > 0) {
        return fd;
    }
    perror("socket DGRAM");
    printf("not support or not be allowed to use SOCK_DGRAM ICMP\n");

    fd = socket(family, SOCK_RAW, IPPROTO_ICMP);
    if (fd > 0) {
        return fd;
    }
    perror("socket RAW");
    printf("create RAW socket still error\n");

    return -1;
}

static unsigned short in_cksum(const unsigned short *addr, int len, unsigned short csum)
{
    int nleft = len;
    const unsigned short *w = addr;
    unsigned short answer = 0;
    int sum = csum;

    /*
     *  Our algorithm is simple, using a 32 bit accumulator (sum),
     *  we add sequential 16 bit words to it, and at the end, fold
     *  back all the carry bits from the top 16 bits into the lower
     *  16 bits.
     */
    while (nleft > 1) {
        sum += *w++;
        nleft -= 2;
    }

    /* mop up an odd byte, if necessary */
    if (nleft == 1)
        *(unsigned char *)(&answer) = *(unsigned char *)w;
        sum += answer;

    /*
     * add back carry outs from top 16 bits to low 16 bits
     */
    sum = (sum >> 16) + (sum & 0xffff); /* add hi 16 to low 16 */
    sum += (sum >> 16);         /* add carry */
    answer = ~sum;              /* truncate to 16 bits */
    return (answer);
}

int ping4_send_probe(int fd, struct sockaddr_in *dst)
{
    struct icmphdr *icp;
    int i;

    char packet[64] = {0};

    icp = (struct icmphdr *)packet;
    icp->type = ICMP_ECHO;
    icp->code = 0;
    icp->checksum = 0;
    icp->un.echo.sequence = htons(g_sn++);
    icp->un.echo.id = g_pid;            /* ID */

    /* compute ICMP checksum here */
    icp->checksum = in_cksum((unsigned short *)icp, sizeof(packet), 0);

    i = sendto(fd, icp, sizeof(packet), 0, (struct sockaddr *)dst, sizeof(struct sockaddr));
    printf("send icmp, return:%d\n", i);

    return (sizeof(packet) == i ? 0 : i);
}

int ping4_parse_reply(char *buffer, int len)
{
    if (len <= 0) {
        printf("recv packet length abnormal!\n");
        return -1;
    }

    struct ip *iphdr = (struct ip *)buffer;
    int iphdr_len = iphdr->ip_hl * 4;
    int icmp_pktlen = len - iphdr_len;
    printf("recv pkt's icmp_pktlen:%d\n", icmp_pktlen);
    if (icmp_pktlen < 8) {
        return -1;
    }

    struct icmp *icmp_pkt = (struct icmp *)(buffer + iphdr_len);
    printf("recv icmp pkt: type:%d, id:%d, sn:%d\n",
            icmp_pkt->icmp_type, icmp_pkt->icmp_id, icmp_pkt->icmp_seq);
    printf("expected  pkt: type:%d, id:%d, sn:%d\n",
            ICMP_ECHOREPLY, g_pid, g_sn-1); //TBD: 这里sn没有进行大小端转换，不匹配

    return 0;
}

int ping4_recv(int fd)
{
    struct sockaddr_in recv_addr;
    int addr_len = sizeof(recv_addr);
    int recv_len;
    char packet[128];

    struct pollfd pset;
    pset.fd = fd;
    pset.events = POLLIN;
    pset.revents = 0;

    int poll_ret = poll(&pset, 1, 1000);
    if (poll_ret < 0) {
        perror("poll");
        return -1;
    } else if (0 == poll_ret) {
        printf("ping time out\n");
        return 0;
    }

    recv_len = recvfrom(fd, packet, sizeof(packet), 0, (struct sockaddr *)&recv_addr, (socklen_t *)&addr_len);
    printf("recv return len:%d\n", recv_len); //recv_len包括IP头

    ping4_parse_reply(packet, recv_len);

    return recv_len;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        printf("Usage: pping hostname\n");
        return 0;
    }
    g_pid = getpid();
    printf("hostname is:%s, self pid:%d\n", argv[1], g_pid);

    int ret;
    struct addrinfo *result;

    struct addrinfo hint = {
        .ai_family = AF_INET,
        .ai_protocol = IPPROTO_UDP,
        .ai_socktype = SOCK_DGRAM,
    };

    ret = getaddrinfo(argv[1], NULL, &hint, &result);
    if (ret) {
        printf("gai return fail:ret:%d, %s\n", ret, gai_strerror(ret));
        return -1;
    }

    //result is a list, only use first here
    struct sockaddr_in whereto = {0};
    memcpy(&whereto, result->ai_addr, sizeof whereto);
    char str[INET6_ADDRSTRLEN];
    printf("whereto.sin_addr:%s\n", inet_ntop(AF_INET, &(whereto.sin_addr), str, sizeof(str)));
    freeaddrinfo(result);

    int sock_fd = create_socket(AF_INET);
    if (sock_fd < 0) return -1;

    int i;
    // send 4 icmp echo pkts
    for (i = 0; i < 4; i++) {
        ping4_send_probe(sock_fd, &whereto);
        ping4_recv(sock_fd);
    }

    return 0;
}



