/*-------------------------------------------------------------------------
 *    文 件 名 :  echo.c
 *    用     途 :
 *    创 建 者 :  weideng
 *    创建时间:  2019年1月31日 上午9:12:31
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

int sk_count = 1;

#define MAX_BIND_ADDRS 1

#define __FILENAME__ (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)

//get_srs_log_instance()->warn(__FILENAME__, __FUNCTION__, __LINE__, _srs_context->get_id(), "[lay=%d|tid=%d|info=%s]" msg,   g_lay_id, get_thread_index(), _srs_context->get_user_info().c_str(), ##__VA_ARGS__);\

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

struct socket_info
{
    st_netfd_t nfd; /* Listening socket                     */
    char *addr; /* Bind address                         */
    unsigned int port; /* Port                                 */
    int wait_threads; /* Number of threads waiting to accept  */
    int busy_threads; /* Number of threads processing request */
    int rqst_count; /* Total number of processed requests   */
} srv_socket[MAX_BIND_ADDRS]; /* Array of listening sockets           */

static void create_listeners(void)
{
    int i, n, sock;
    char *c;
    struct sockaddr_in serv_addr;
    struct hostent *hp;
    unsigned short port;

    for (i = 0; i < sk_count; i++)
    {
        port = 0;
        if ((c = strchr(srv_socket[i].addr, ':')) != NULL)
        {
//            *c++ = '\0';
            port = (unsigned short) atoi(c);
        }
        if (srv_socket[i].addr[0] == '\0')
            srv_socket[i].addr = "0.0.0.0";
        if (port == 0)
            port = 80;

        /* Create server socket */
        if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
        {
            ST_DEBUG("ERROR: can't create socket: socket%s", "");
            exit(0);
        }
        n = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &n, sizeof(n)) < 0)
        {
            ST_DEBUG("ERROR: can't set SO_REUSEADDR: setsockopt%s", "");
            exit(0);
        }
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);
        serv_addr.sin_addr.s_addr = inet_addr(srv_socket[i].addr);
//        if (serv_addr.sin_addr.s_addr == INADDR_NONE)
//        {
//            /* not dotted-decimal */
//            if ((hp = gethostbyname(srv_socket[i].addr)) == NULL)
//            {
//                ST_DEBUG("ERROR: can't resolve address: %s", srv_socket[i].addr);
//                exit(0);
//            }
//            memcpy(&serv_addr.sin_addr, hp->h_addr, hp->h_length);
//        }
        srv_socket[i].port = port;

        /* Do bind and listen */
        if (bind(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        {
            ST_DEBUG("ERROR: can't bind to address %s, port %hu",
            srv_socket[i].addr, port);
            exit(0);
        }

        if (listen(sock, 125) < 0)
        {
            ST_DEBUG("ERROR: listen%s", "");
            exit(0);
        }

        /* Create file descriptor object from OS socket */
        if ((srv_socket[i].nfd = st_netfd_open_socket(sock)) == NULL)
        {
            ST_DEBUG("ERROR: st_netfd_open_socket%s", "");
            exit(0);
        }

        ST_DEBUG("success bind to address %s, port %hu\n",
                    srv_socket[i].addr, port);
    }
}

static void* echo_thread(void *arg);
static void echo(st_netfd_t cli_nfd);

void echo_accpet()
{
    st_netfd_t srv_nfd, cli_nfd;
    struct sockaddr_in from;
    int fromlen;

    srv_nfd = srv_socket[0].nfd;
    fromlen = sizeof(from);

    while (1)
    {
        ST_DEBUG("listen to accept:%s\n", "");
        cli_nfd = st_accept(srv_socket[0].nfd, (struct sockaddr *) &from, &fromlen,
            -1);
        if (cli_nfd == NULL)
        {
            ST_DEBUG("ERROR: can't accept connection: st_accept%s\n", "");
            continue;
        }

        ST_DEBUG("handle_connections accept fd:%d\n", st_netfd_fileno(cli_nfd));

        /* Save peer address, so we can retrieve it later */
        st_netfd_setspecific(cli_nfd, &from.sin_addr, NULL);

        st_thread_create(echo_thread, cli_nfd, 0, 0);
//        echo(cli_nfd);
//        st_netfd_close(cli_nfd);
    }
}

void* echo_thread(void *arg)
{
    st_netfd_t cli_nfd = arg;
    echo(cli_nfd);
    st_netfd_close(cli_nfd);
    return arg;
}

void echo(st_netfd_t cli_nfd)
{
    char buf[10];

//    static char resp[] = "HTTP/1.0 200 OK\r\nContent-type: text/html\r\n"
//                           "Connection: close\r\n\r\n<H2>It worked222!</H2>\n";

    int n, m;
    while( (n = st_read(cli_nfd, buf, sizeof(buf), 3 * 1000 * 1000)) > 0 )
    {
        ST_DEBUG("%s", buf);
        if((m = st_write(cli_nfd, buf, n, 3*1000*1000)) != n)
        {
            ST_DEBUG("write error m:%d\n", m);
        }
    }
    ST_DEBUG("echo read failed:%d\n", n);
}

int main()
{
    srv_socket[0].addr = "\0";

    if (st_init() < 0)
    {
        ST_DEBUG("ERROR: initialization failed: st_init%s", "");
        exit(0);
    }
    create_listeners();
    echo_accpet();
}
