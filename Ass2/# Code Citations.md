# Code Citations

## License: unknown
https://github.com/DrKrantz/snn/tree/ba7675f11207b20dac20856b1288a82d5e5966eb/cpp/client.c

```
sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>

#define PORT 5000
#define MAXLINE 1000
```


## License: unknown
https://github.com/bobrob8887/TMA2-Q3/tree/f057fcee077b2bcb047753ecfa405a77fd14841b/Geekclient.c

```
.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>

#define PORT 5000
#define MAXLINE 1000

int main(
```


## License: unknown
https://github.com/gomespe/mc833-lab-redes/tree/9ebf8d8f99dc86273b6cb5b67ef3bf3f720d0091/projeto2/UDP/client/client.c

```
<stdio.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
```


## License: unknown
https://github.com/anushaupadya7/6th_sem_resources/tree/f56146092923f7cb93a37b131f8b4191fb517995/CN_LAB/Programs/udp/client.c

```
;
    int sockfd, n;
    struct sockaddr_in servaddr;

    // clear servaddr
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);
    servaddr.
```


## License: unknown
https://github.com/disc0ba11/NetProg/tree/77034df7f78a825eabbf02d7d039a1ca535f2d5c/daytime/daytime_client.cpp

```
>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>

#define PORT 5000
#define MAXLINE 1000

int main()
{
    char buffer[100];
    char *message
```

