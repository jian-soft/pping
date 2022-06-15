#include <stdio.h>
#include <netdb.h> //getaddrinfo
//#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char* argv[])
{
	if (argc < 2) {
		printf("Usage: pping hostname\n");
		return 0;
	}
	printf("hostname is:%s\n", argv[1]);
	
	int ret;
	struct addrinfo *result, *ai;

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
    ai = result;
    struct sockaddr_in *sa_in = (struct sockaddr_in *)ai->ai_addr;
	char str[INET6_ADDRSTRLEN];
	printf("ai->ai_addr.sin_addr:%s\n", inet_ntop(AF_INET, &(sa_in->sin_addr), str, sizeof(str)));

    freeaddrinfo(result);
    return 0;
}



