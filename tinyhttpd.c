/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */
/* This program compiles for Sparc Solaris 2.6.
 * To compile for Linux://在linux下的编译需要的操作
 *  1) Comment out the #include <pthread.h> line. 
 *  2) Comment out the line that defines the variable newthread.
 *  3) Comment out the two lines that run pthread_create().
 *  4) Uncomment the line that runs accept_request().
 *  5) Remove -lsocket from the Makefile.
 */
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>//为isspace提供函数原型
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>

#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

void accept_request(int);
void bad_request(int);
void cat(int, FILE *);
void cannot_execute(int);
void error_die(const char *);
void execute_cgi(int, const char *, const char *, const char *);
int get_line(int, char *, int);
void headers(int, const char *);
void not_found(int);
void serve_file(int, const char *);
int startup(u_short *);
void unimplemented(int);

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client  客户端的端口*/
/**********************************************************************/
void accept_request(int client)//接受请求，参数
{
 char buf[1024];//声明长度为1024的字符缓冲区
 int numchars;//读取到的字符个数
 char method[255];//数据传输的方法
 char url[255];//暂时认为这是地址
 char path[512];//
 size_t i, j;//size_t是sizeof函数的返回的数据类型
 struct stat st;//声明stat结构变量
 //struct stat这个结构体是用来描述一个linux系统文件系统中的文件属性的结构。
 int cgi = 0;      /* becomes true if server decides this is a CGI 
                    cgi变量是一个flag，用来判定是否是一个cgi的，0表示不是cgi，非零表示为cgi                    * program */
 char *query_string = NULL;
//下面这部分函数的作用是将缓冲区的字符读取到method，得到传输数据的方法
 numchars = get_line(client, buf, sizeof(buf));//从客户端读取一行字符到buf，numchars为读取到的字符数，不含\0
 i = 0; j = 0;
 while (!ISspace(buf[j]) && (i < sizeof(method) - 1))//如果字符不是空格且现在还是比method短
 {
  method[i] = buf[j];
  i++; j++;
 }
 method[i] = '\0';//最后加上\0

//strcasecmp函数的作用是忽略字符串大小比较字符串，两种方法一种是POST，以一种是GET
 if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))//如果既不是POST也不是GET
 {
  unimplemented(client);// 未实现的，那就出错了
  return;
 }

 if (strcasecmp(method, "POST") == 0)//如果是method是POST，也就是向服务器传送数据，那么cgi为真
  cgi = 1;
//看来buf包括至少两个部分，一个是method，一个是url
 i = 0;
 while (ISspace(buf[j]) && (j < sizeof(buf)))//跳过method和url之间所有空白字符
  j++;
 //--------------接下去的函数是用来读取url的，直到出现空格------------------------------------ 
 while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)))
 {
  url[i] = buf[j];
  i++; j++;
 }
 url[i] = '\0';
 //----------------------------------------------------------------------------------
 if (strcasecmp(method, "GET") == 0)//如果方法是GET，也就是从服务器获得数据
 {
        query_string = url;//就将url赋值给query也就是请求
        while ((*query_string != '?') && (*query_string != '\0'))//当query指向的字符不等于？时，且未到字符串结尾
            query_string++;//那就将指针往后移
        if (*query_string == '?')//如果query指向的为？
        {
            cgi = 1;//那就判定cgi为真
            *query_string = '\0';//将query指向的字符置为\0
            query_string++;//然后继续
        }
 }
//---------------------------------------------------
 sprintf(path, "htdocs%s", url);//将url前面加上htdocs然后输出到path
 if (path[strlen(path) - 1] == '/')//如果path的最后一个字符为'/'
  strcat(path, "index.html");//那么就把path后面加上index.html
//stat函数取得peth指向的文件文件状态，复制到st指向的结构体，返回值：执行成功则返回0，失败返回-1，错误代码存于errno。

 if (stat(path, &st) == -1)//获取文件信息失败的话
{
  while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers 读取并丢弃头部 */
   numchars = get_line(client, buf, sizeof(buf));//从客户端读取一行字符到buf，numchars为读取到的字符数，不含\0
  not_found(client);//找不到的操作
 }
 else//读取成功
 {
  if ((st.st_mode & S_IFMT) == S_IFDIR) //mode_t st_mode; //protection 文件的类型和存取的权限, S_IFMT 0170000 文件类型的位遮罩
   strcat(path, "/index.html");//还是再加上一个index.html
  if ((st.st_mode & S_IXUSR) ||(st.st_mode & S_IXGRP) ||(st.st_mode & S_IXOTH))//如果这一串不知道是什么东西成立
     cgi = 1;//那它是一个cgi
  if (!cgi)//如果不是一个cgi
   serve_file(client, path);
  else//如果是cgi
   execute_cgi(client, path, method, query_string);//执行cgi
 }

 close(client);//关闭客户端
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
/**********************************************************************/
void bad_request(int client)
{
 char buf[1024];

 sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
 send(client, buf, sizeof(buf), 0);
 sprintf(buf, "Content-type: text/html\r\n");
 send(client, buf, sizeof(buf), 0);
 sprintf(buf, "\r\n");
 send(client, buf, sizeof(buf), 0);
 sprintf(buf, "<P>Your browser sent a bad request, ");
 send(client, buf, sizeof(buf), 0);
 sprintf(buf, "such as a POST without a Content-Length.\r\n");
 send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
void cat(int client, FILE *resource)
{
 char buf[1024];

 fgets(buf, sizeof(buf), resource);
 while (!feof(resource))
 {
  send(client, buf, strlen(buf), 0);
  fgets(buf, sizeof(buf), resource);
 }
}

/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
/**********************************************************************/
void cannot_execute(int client)
{
 char buf[1024];

 sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "Content-type: text/html\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
 send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
/**********************************************************************/
void error_die(const char *sc)//意外终止，接受一个字符串，表示出错的原因
{
 perror(sc);//perror(s) 用来将上一个函数发生错误的原因输出到标准设备(stderr)。
 exit(1);
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
/**********************************************************************/
void execute_cgi(int client, const char *path,const char *method, const char *query_string)//执行cgi
{
 char buf[1024];
 int cgi_output[2];
 int cgi_input[2];
 pid_t pid;
 int status;
 int i;
 char c;
 int numchars = 1;
 int content_length = -1;

 buf[0] = 'A'; buf[1] = '\0';
 if (strcasecmp(method, "GET") == 0)
  while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
   numchars = get_line(client, buf, sizeof(buf));
 else    /* POST */
 {
  numchars = get_line(client, buf, sizeof(buf));
  while ((numchars > 0) && strcmp("\n", buf))
  {
   buf[15] = '\0';
   if (strcasecmp(buf, "Content-Length:") == 0)
    content_length = atoi(&(buf[16]));
   numchars = get_line(client, buf, sizeof(buf));
  }
  if (content_length == -1) {
   bad_request(client);
   return;
  }
 }

 sprintf(buf, "HTTP/1.0 200 OK\r\n");
 send(client, buf, strlen(buf), 0);

 if (pipe(cgi_output) < 0) {
  cannot_execute(client);
  return;
 }
 if (pipe(cgi_input) < 0) {
  cannot_execute(client);
  return;
 }

 if ( (pid = fork()) < 0 ) {
  cannot_execute(client);
  return;
 }
 if (pid == 0)  /* child: CGI script */
 {
  char meth_env[255];
  char query_env[255];
  char length_env[255];

  dup2(cgi_output[1], 1);
  dup2(cgi_input[0], 0);
  close(cgi_output[0]);
  close(cgi_input[1]);
  sprintf(meth_env, "REQUEST_METHOD=%s", method);
  putenv(meth_env);
  if (strcasecmp(method, "GET") == 0) {
   sprintf(query_env, "QUERY_STRING=%s", query_string);
   putenv(query_env);
  }
  else {   /* POST */
   sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
   putenv(length_env);
  }
  execl(path, path, NULL);
  exit(0);
 } else {    /* parent */
  close(cgi_output[1]);
  close(cgi_input[0]);
  if (strcasecmp(method, "POST") == 0)
   for (i = 0; i < content_length; i++) {
    recv(client, &c, 1, 0);
    write(cgi_input[1], &c, 1);
   }
  while (read(cgi_output[0], &c, 1) > 0)
   send(client, &c, 1, 0);

  close(cgi_output[0]);
  close(cgi_input[1]);
  waitpid(pid, &status, 0);
 }
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in 储存输入数据的缓冲区
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
int get_line(int sock, char *buf, int size)//从端口读入一个字符串，返回字符串长度
{
 int i = 0;
 char c = '\0';
 int n;//n指的是 是否读取到数据

 while ((i < size - 1) && (c != '\n'))//只要c为\n或者\r都会结束
 {
  n = recv(sock, &c, 1, 0);//读取数据，长度为1，将其赋给c，读取模式为0
  /* DEBUG printf("%02X\n", c); */
  if (n > 0)//接收到数据
  {
        if (c == '\r')//如果c是一个换行符
        {
            n = recv(sock, &c, 1, MSG_PEEK);//MSG_PEEK 查看当前数据。数据将被复制到缓冲区中，但并不从输入队列中删除。
            /* DEBUG printf("%02X\n", c); */
            if ((n > 0) && (c == '\n'))//如果接收到数据，而c是一个换行符
                recv(sock, &c, 1, 0);//那就在接着读一个字符
            else
                c = '\n';//将c强制置为\n实际上是为了退出
        }
    buf[i] = c;//将接收到的数据存入到buf字符数组当中
    i++;
  }
  else//如果没有接收到数据，那就将c置为\ne而退出
   c = '\n';
 }
 buf[i] = '\0';//在字符串的结尾加上\0
 
 return(i);//返回储存的字符个数
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/
void headers(int client, const char *filename)
{
 char buf[1024];
 (void)filename;  /* could use filename to determine file type */

 strcpy(buf, "HTTP/1.0 200 OK\r\n");
 send(client, buf, strlen(buf), 0);
 strcpy(buf, SERVER_STRING);
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "Content-Type: text/html\r\n");
 send(client, buf, strlen(buf), 0);
 strcpy(buf, "\r\n");
 send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
void not_found(int client)//找不到就输出一系列错误信息
{
 char buf[1024];

 sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, SERVER_STRING);
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "Content-Type: text/html\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "your request because the resource specified\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "is unavailable or nonexistent.\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "</BODY></HTML>\r\n");
 send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/**********************************************************************/
void serve_file(int client, const char *filename)//接受文件名
{
 FILE *resource = NULL;
 int numchars = 1;
 char buf[1024];

 buf[0] = 'A'; buf[1] = '\0';
 while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
  numchars = get_line(client, buf, sizeof(buf));

 resource = fopen(filename, "r");
 if (resource == NULL)
  not_found(client);
 else
 {
  headers(client, filename);
  cat(client, resource);
 }
 fclose(resource);
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/
int startup(u_short *port)
{
 int httpd = 0;
 struct sockaddr_in name;

 httpd = socket(PF_INET, SOCK_STREAM, 0);
 /*
    socket()函数用于根据指定的地址族、数据类型和协议来分配一个套接口的描述字及其所用的资源。如果协议protocol未指定（等于0），则使用缺省的连接方式。
    int socket( int af, int type, int protocol);
    
    af：一个地址描述。目前仅支持AF_INET格式，也就是说ARPA Internet地址格式。
    
    type：指定socket类型。新套接口的类型描述类型，如TCP（SOCK_STREAM）和UDP（SOCK_DGRAM）。
    常用的socket类型有，SOCK_STREAM、SOCK_DGRAM、SOCK_RAW、SOCK_PACKET、SOCK_SEQPACKET等等。
    
    protocol：顾名思义，就是指定协议。套接口所用的协议。如调用者不想指定，可用0。
    常用的协议有，IPPROTO_TCP、IPPROTO_UDP、IPPROTO_STCP、IPPROTO_TIPC等，它们分别对应TCP传输协议、UDP传输协议、STCP传输协议、TIPC传输协议。
    
    若无错误发生，socket()返回引用新套接口的描述字。否则的话，返回INVALID_SOCKET错误
 */
 if (httpd == -1)//如果分配失败，就执行意外终止，并输出终止原因
  error_die("socket");
 memset(&name, 0, sizeof(name));
 name.sin_family = AF_INET;
 name.sin_port = htons(*port);
 name.sin_addr.s_addr = htonl(INADDR_ANY);
 if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
  error_die("bind");
 if (*port == 0)  /* if dynamically allocating a port 动态分配端口*/
 {
  int namelen = sizeof(name);
  if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)//获取失败的话
    {
    /*
    getcock()
    获取一个套接口的本地名字。
    s：标识一个已捆绑套接口的描述字。(httpd)
    name：接收套接口的地址（名字）。
    namelen：名字缓冲区长度。
    */
        error_die("getsockname");
    }
  *port = ntohs(name.sin_port);//将一个无符号短整形数从网络字节顺序转换为主机字节顺序。ntohs()返回一个以主机字节顺序表达的数。将值赋给port
 }
 if (listen(httpd, 5) < 0)//如果监听失败
    error_die("listen");
 return(httpd);//上述步骤都成功的话，最终返回分配到的套接口的描述字
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
/**********************************************************************/
void unimplemented(int client)//无法实现的，输出一系列错误信息
{
 char buf[1024];

 sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, SERVER_STRING);
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "Content-Type: text/html\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "</TITLE></HEAD>\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "</BODY></HTML>\r\n");
 send(client, buf, strlen(buf), 0);
}

/**********************************************************************/

int main(void)
{
 int server_sock = -1;//server_sock是一个flag变量，用以指示是否成功调用服务器端口，初始化为-1
 u_short port = 0;
 int client_sock = -1;//client_sock是一个flag变量，用以指示是否成功调用客户端端口初始化为-1
 struct sockaddr_in client_name;//这个神奇的结构体是个啥我也看不懂
 int client_name_len = sizeof(client_name);//客户端名字的大小
 pthread_t newthread;//用于声明线程ID。

 server_sock = startup(&port);//打开端口，应该会修改port的值，所以才要传入地址，sever_sock现在是分配到的套接口的描述字
 printf("httpd running on port %d\n", port);

 while (1)
 {
    client_sock = accept(server_sock,(struct sockaddr *)&client_name,&client_name_len);
    /*
    int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
    在一个套接口接受一个连接。
    参数:
        sockfd：套接字描述符，该套接口在listen()后监听连接。
        addr：（可选）指针，指向一缓冲区，其中接收为通讯层所知的连接实体的地址。Addr参数的实际格式由套接口创建时所产生的地址族确定。
        addrlen：（可选）指针，输入参数，配合addr一起使用，指向存有addr地址长度的整型数。
    如果没有错误产生，则accept()返回一个描述所接受包的SOCKET类型的值。
    否则的话，返回INVALID_SOCKET错误，应用程序可通过调用WSAGetLastError()来获得特定的错误代码。
    */
    if (client_sock == -1)//如果连接出错
        error_die("accept");
 /* accept_request(client_sock); */
 if (pthread_create(&newthread , NULL, accept_request, client_sock) != 0)//如果进程创建失败
 /*
    pthread_create是类Unix操作系统（Unix、Linux、Mac OS X等）的创建线程的函数。
    第一个参数为指向线程标识符的指针。
    第二个参数用来设置线程属性。
    第三个参数是线程运行函数的起始地址。
    最后一个参数是运行函数的参数。
    若线程创建成功，则返回0。若线程创建失败，则返回出错编号，并且*thread中的内容是未定义的。
 */
   perror("pthread_create");
 }

 close(server_sock);//关闭服务器端口

 return(0);
}