/*-------------------------------------------------------------------------
 *    文 件 名 :  echo_client.c
 *    用     途 :
 *    创 建 者 :  weideng
 *    创建时间:  2019年1月31日 下午3:25:11
 *    修订记录:
 *          1) 
 *-------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <time.h>
#include "st.h"

char *ip = "127.0.0.1";
int port = 80;
int num = 1000;

#define __FILENAME__ (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)

//ULS_LOG_DEBUG("[pid=%d|tid=%d|st_id=%d|info=%s]" msg, getpid(), get_thread_index(), _srs_context->get_id(),_srs_context->get_user_info().c_str(), ##__VA_ARGS__)
#define ST_DEBUG(f_, ...)\
{\
    static char str[32]; \
    static time_t lastt = 0; \
    struct tm *tmp;\
    time_t currt = st_time();\
    tmp = localtime(&currt);\
    sprintf(str, "[%02d/%d:%02d:%02d:%02d] ", tmp->tm_mday,\
          1900 + tmp->tm_year, tmp->tm_hour,\
          tmp->tm_min, tmp->tm_sec);\
    printf(("[%s][%s][%d]"f_), str, __FILENAME__, __LINE__, __VA_ARGS__);\
    fflush(stdout);\
}

void* echo_client(void *arg)
{

    int ret = 0;

    st_netfd_t pstfd;
    st_netfd_t stfd;

    struct sockaddr_in addr;

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    stfd = st_netfd_open_socket(sock);
    if (stfd == NULL) {
        ST_DEBUG("st_netfd_open_socket failed. ret=%d", 0);
        exit(0);
    }


    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    if (st_connect(stfd, (const struct sockaddr*) &addr, sizeof(struct sockaddr_in),
            1*1000*1000) == -1)
    {
        ST_DEBUG("connect to server error. ip=%s, port=%d, ret=%d", ip,
                port, 0);
        exit(0);
    }


    char buf[10] = {'f', 'u', 'c', 'k', ' ', 'w', 'o', 'r', 'd', '!'};

//    static char resp[] = "HTTP/1.0 200 OK\r\nContent-type: text/html\r\n"
//                           "Connection: close\r\n\r\n<H2>It worked222!</H2>\n";

    int n, m;

    while( (n = st_write(stfd, buf, sizeof(buf), 3 * 1000 * 1000)) > 0 )
    {
        ST_DEBUG("write %s, n:%d\n", buf, n);
        if((m = st_read(stfd, buf, n, 3*1000*1000)) != n)
        {
            ST_DEBUG("write error m:%d\n", m);
        }
        st_usleep(20*1000);
    }
    ST_DEBUG("echo write failed:%d\n", n);
}

int main()
{
    int i;
    if (st_init() < 0)
    {
        ST_DEBUG("ERROR: initialization failed: st_init%s", "");
        exit(0);
    }


    for(i = 0; i < num; ++i)
    {
        st_thread_create(echo_client, NULL, 0, 0);
    }

    st_sleep(-1);
}


