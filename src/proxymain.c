/*
   3APA3A simpliest proxy server
   (c) 2002-2021 by Vladimir Dubrovin <3proxy@3proxy.org>

   please read License Agreement

*/

#include "proxy.h"

#define param ((struct clientparam *)p)
#ifdef _WIN32
DWORD WINAPI threadfunc(LPVOID p)
{
#else
void *threadfunc(void *p)
{
#endif
	int i = -1;

	if (!i)
	{
		param->res = 13;
		freeparam(param);
	}
	else
	{
		((struct clientparam *)p)->srv->pf((struct clientparam *)p);
	}
#ifdef _WIN32
	return 0;
#else
	return NULL;
#endif
}
#undef param

struct socketoptions sockopts[] = {
#ifdef TCP_NODELAY
	{TCP_NODELAY, "TCP_NODELAY"},
#endif
#ifdef TCP_CORK
	{TCP_CORK, "TCP_CORK"},
#endif
#ifdef TCP_DEFER_ACCEPT
	{TCP_DEFER_ACCEPT, "TCP_DEFER_ACCEPT"},
#endif
#ifdef TCP_QUICKACK
	{TCP_QUICKACK, "TCP_QUICKACK"},
#endif
#ifdef TCP_TIMESTAMPS
	{TCP_TIMESTAMPS, "TCP_TIMESTAMPS"},
#endif
#ifdef USE_TCP_FASTOPEN
	{USE_TCP_FASTOPEN, "USE_TCP_FASTOPEN"},
#endif
#ifdef SO_REUSEADDR
	{SO_REUSEADDR, "SO_REUSEADDR"},
#endif
#ifdef SO_REUSEPORT
	{SO_REUSEPORT, "SO_REUSEPORT"},
#endif
#ifdef SO_PORT_SCALABILITY
	{SO_PORT_SCALABILITY, "SO_PORT_SCALABILITY"},
#endif
#ifdef SO_REUSE_UNICASTPORT
	{SO_REUSE_UNICASTPORT, "SO_REUSE_UNICASTPORT"},
#endif
#ifdef SO_KEEPALIVE
	{SO_KEEPALIVE, "SO_KEEPALIVE"},
#endif
#ifdef SO_DONTROUTE
	{SO_DONTROUTE, "SO_DONTROUTE"},
#endif
#ifdef IP_TRANSPARENT
	{IP_TRANSPARENT, "IP_TRANSPARENT"},
#endif
#ifdef TCP_FASTOPEN
	{TCP_FASTOPEN, "TCP_FASTOPEN"},
#endif
#ifdef TCP_FASTOPEN_CONNECT
	{TCP_FASTOPEN_CONNECT, "TCP_FASTOPEN_CONNECT"},
#endif
	{0, NULL}};

char optsbuf[1024];

char *printopts(char *sep)
{
	int i = 0, pos = 0;
	for (; sockopts[i].optname; i++)
		pos += sprintf(optsbuf + pos, "%s%s", i ? sep : "", sockopts[i].optname);
	return optsbuf;
}

int getopts(const char *s)
{
	int i = 0, ret = 0;
	for (; sockopts[i].optname; i++)
		if (strstr(s, sockopts[i].optname))
			ret |= (1 << i);
	return ret;
}

void setopts(SOCKET s, int opts)
{
	int i, opt, set;
	for (i = 0; opts >= (opt = (1 << i)); i++)
	{
		set = 1;
		if (opts & opt)
			setsockopt(s, *sockopts[i].optname == 'T' ? IPPROTO_TCP :
#ifdef SOL_IP
						  *sockopts[i].optname == 'I' ? SOL_IP
													  :
#endif
													  SOL_SOCKET,
					   sockopts[i].opt, (char *)&set, sizeof(set));
	}
}

#ifndef MODULEMAINFUNC
#define MODULEMAINFUNC main
#define STDMAIN

#ifndef _WINCE

int main(int argc, char **argv)
{

#else

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	int argc;
	char **argv;
	WNDCLASS wc;
	HWND hwnd = 0;

#endif

#else
extern int linenum;
extern int haveerror;

int MODULEMAINFUNC(int argc, char **argv)
{

#endif

	SOCKET sock = INVALID_SOCKET, new_sock = INVALID_SOCKET;
	int i = 0;
	SASIZETYPE size;
	pthread_t thread;
	struct clientparam defparam;
	struct srvparam srv;
	struct clientparam *newparam;
	int error = 0;
	unsigned sleeptime;
	unsigned char buf[256];
	char *hostname = NULL;
	int opt = 1;
#ifndef NOIPV6
	struct sockaddr_in6 cbsa;
#else
	struct sockaddr_in cbsa;
#endif
	FILE *fp = NULL;
	struct linger lg;
	int nlog = 5000;
	char loghelp[] =
#ifdef STDMAIN
#ifndef _WIN32
		" -I inetd mode (requires real socket, doesn't work with TTY)\n"
		" -l@IDENT log to syslog IDENT\n"
#endif
		" -d go to background (daemon)\n"
#else
		" -u never ask for username\n"
		" -u2 always ask for username\n"
#endif
#if defined SO_BINDTODEVICE || defined IP_BOUND_IF
		" -Di(DEVICENAME) bind internal interface to device, e.g. eth1\n"
		" -De(DEVICENAME) bind external interface to device, e.g. eth1\n"
#endif
#ifdef WITHSLICE
		" -s Use slice() - faster proxing, but no filtering for data\n"
#endif
		"-g(GRACE_TRAFF,GRACE_NUM,GRACE_DELAY) - delay GRACE_DELAY milliseconds before polling if average polling size below  GRACE_TRAFF bytes and GRACE_NUM read operations in single directions are detected within 1 second to minimize polling\n"
		" -fFORMAT logging format (see documentation)\n"
		" -l log to stderr\n"
		" -lFILENAME log to FILENAME\n"
		" -b(BUFSIZE) size of network buffer (default 4096 for TCP, 16384 for UDP)\n"
		" -S(STACKSIZE) value to add to default client thread stack size\n"
		" -t be silent (do not log service start/stop)\n"
		"\n"
		" -iIP ip address or internal interface (clients are expected to connect)\n"
		" -eIP ip address or external interface (outgoing connection will have this)\n"
		" -rHOST:PORT Use IP:port for connect back proxy instead of listen port\n"
		" -RHOST:PORT Use PORT to listen connect back proxy connection to pass data to\n"
		" -4 Use IPv4 for outgoing connections\n"
		" -6 Use IPv6 for outgoing connections\n"
		" -46 Prefer IPv4 for outgoing connections, use both IPv4 and IPv6\n"
		" -64 Prefer IPv6 for outgoing connections, use both IPv4 and IPv6\n"
		" -ocOPTIONS, -osOPTIONS, -olOPTIONS, -orOPTIONS -oROPTIONS - options for\n"
		" to-client (oc), to-server (os), listening (ol) socket, connect back client\n"
		" (or) socket, connect back server (oR) listening socket\n"
		" where possible options are: ";

#ifdef _WIN32
	unsigned long ul = 1;
#else
	pthread_attr_t pa;
#ifdef STDMAIN
	int inetd = 0;
#endif
#endif
#ifdef _WIN32
	HANDLE h;
#endif
#ifdef STDMAIN

#ifdef _WINCE
	argc = ceparseargs((char *)lpCmdLine);
	argv = ceargv;
	if (FindWindow(lpCmdLine, lpCmdLine))
		return 0;
	ZeroMemory(&wc, sizeof(wc));
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpfnWndProc = DefWindowProc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpszClassName = lpCmdLine;
	RegisterClass(&wc);

	hwnd = CreateWindowEx(WS_EX_TOOLWINDOW, lpCmdLine, lpCmdLine, WS_VISIBLE | WS_POPUP, 0, 0, 0, 0, 0, 0, hInstance, 0);
#endif

#ifdef _WIN32
	WSADATA wd;
	WSAStartup(MAKEWORD(1, 1), &wd);

#endif

#endif

	srvinit(&srv, &defparam);
	srv.pf = childdef.pf;
	srv.service = defparam.service = childdef.service;

	srv.needuser = 0;
	pthread_mutex_init(&log_mutex, NULL);

	for (i = 1; i < argc; i++)
	{
		if (*argv[i] == '-')
		{
			switch (argv[i][1])
			{
			case 'd':
				if (!conf.demon)
					daemonize();
				conf.demon = 1;
				break;
#if defined SO_BINDTODEVICE || defined IP_BOUND_IF
			case 'D':
				if (argv[i][2] == 'i')
					srv.ibindtodevice = mystrdup(argv[i] + 3);
				else
					srv.obindtodevice = mystrdup(argv[i] + 3);
				break;
#endif
			case 'l':
				srv.logfunc = logstdout;
				if (srv.logtarget)
					myfree(srv.logtarget);
				srv.logtarget = (unsigned char *)mystrdup(argv[i] + 2);
				if (argv[i][2])
				{
					if (argv[i][2] == '@')
					{

#ifdef STDMAIN
#ifndef _WIN32
						openlog(argv[i] + 3, LOG_PID, LOG_DAEMON);
						srv.logfunc = logsyslog;
#endif
#endif
					}
					else
					{
						fp = fopen(argv[i] + 2, "a");
						if (fp)
						{
							srv.stdlog = fp;
						}
					}
				}
				break;
			case 'i':
				getip46(46, (unsigned char *)argv[i] + 2, (struct sockaddr *)&srv.intsa);
				break;
			case 'e':
			{
#ifndef NOIPV6
				struct sockaddr_in6 sa6;
				memset(&sa6, 0, sizeof(sa6));
				error = !getip46(46, (unsigned char *)argv[i] + 2, (struct sockaddr *)&sa6);
				if (!error)
				{
					if (*SAFAMILY(&sa6) == AF_INET)
						srv.extsa = sa6;
					else
						srv.extsa6 = sa6;
				}
#else
				error = !getip46(46, (unsigned char *)argv[i] + 2, (struct sockaddr *)&srv.extsa);
#endif
			}
			break;
			case 'N':
				getip46(46, (unsigned char *)argv[i] + 2, (struct sockaddr *)&srv.extNat);
				break;
			case 'p':
				*SAPORT(&srv.intsa) = htons(atoi(argv[i] + 2));
				break;
			case '4':
			case '6':
				srv.family = atoi(argv[i] + 1);
				break;
			case 'b':
				srv.bufsize = atoi(argv[i] + 2);
				break;
			case 'n':
				srv.usentlm = atoi(argv[i] + 2);
				break;
#ifdef STDMAIN
#ifndef _WIN32
			case 'I':
				size = sizeof(defparam.sincl);
				if (so._getsockname(0, (struct sockaddr *)&defparam.sincl, &size) ||
					*SAFAMILY(&defparam.sincl) != AF_INET)
					error = 1;

				else
					inetd = 1;
				break;
#endif
#endif
			case 'f':
				if (srv.logformat)
					myfree(srv.logformat);
				srv.logformat = (unsigned char *)mystrdup(argv[i] + 2);
				break;
			case 't':
				srv.silent = 1;
				break;
			case 'h':
				hostname = argv[i] + 2;
				break;
			case 'u':
				srv.needuser = 0;
				if (*(argv[i] + 2))
					srv.needuser = atoi(argv[i] + 2);
				break;
			case 'T':
				srv.transparent = 1;
				break;
			case 'S':
				srv.stacksize = atoi(argv[i] + 2);
				break;
			case 'a':
				srv.anonymous = 1 + atoi(argv[i] + 2);
				break;
			case 'g':
				sscanf(argv[i] + 2, "%d,%d,%d", &srv.gracetraf, &srv.gracenum, &srv.gracedelay);
				break;
			case 's':
#ifdef WITHSPLICE
				if (srv.service == S_ADMIN)
#endif
					srv.singlepacket = 1 + atoi(argv[i] + 2);
#ifdef WITHSPLICE
				else if (*(argv[i] + 2))
					srv.usesplice = atoi(argv[i] + 2);
#endif
				break;
			case 'o':
				switch (argv[i][2])
				{
				case 's':
					srv.srvsockopts = getopts(argv[i] + 3);
					break;
				case 'c':
					srv.clisockopts = getopts(argv[i] + 3);
					break;
				case 'l':
					srv.lissockopts = getopts(argv[i] + 3);
					break;
				case 'r':
					srv.cbcsockopts = getopts(argv[i] + 3);
					break;
				case 'R':
					srv.cbcsockopts = getopts(argv[i] + 3);
					break;
				default:
					error = 1;
				}
				if (!error)
					break;
			default:
				error = 1;
				break;
			}
		}
		else
			break;
	}

#ifndef PORTMAP
		if (error || i != argc)
		{

			fprintf(stderr, "%s of %s\n"
							"Usage: %s options\n"
							"Available options are:\n"
							"%s\n"
							"\t%s\n"
							" -pPORT - service port to accept connections\n"
							"%s"
							"\tExample: %s -i127.0.0.1\n\n"
							"%s",
					argv[0],
					conf.stringtable ? (char *)conf.stringtable[3] : VERSION " (" BUILDDATE ")",
					argv[0], loghelp, printopts("\n\t"), childdef.helpmessage, argv[0],
#ifdef STDMAIN
					copyright
#else
					""
#endif
			);

			return (1);
		}
#endif

#ifndef NOPORTMAP
		if (error || argc != i + 3 || *argv[i] == '-' || (*SAPORT(&srv.intsa) = htons((unsigned short)atoi(argv[i]))) == 0 || (srv.targetport = htons((unsigned short)atoi(argv[i + 2]))) == 0)
		{

			fprintf(stderr, "%s of %s\n"
							"Usage: %s options"
							" [-e<external_ip>] <port_to_bind>"
							" <target_hostname> <target_port>\n"
							"Available options are:\n"
							"%s\n"
							"\t%s\n"
							"%s"
							"\tExample: %s -d -i127.0.0.1 6666 serv.somehost.ru 6666\n\n"
							"%s",
					argv[0],
					conf.stringtable ? (char *)conf.stringtable[3] : VERSION " (" BUILDDATE ")",
					argv[0], loghelp, printopts("\n\t"), childdef.helpmessage, argv[0],
#ifdef STDMAIN
					copyright
#else
					""
#endif
			);
			return (1);
		}
		srv.target = (unsigned char *)mystrdup(argv[i + 1]);
#endif

#ifndef _WIN32
	if (inetd)
	{
		fcntl(0, F_SETFL, O_NONBLOCK | fcntl(0, F_GETFL));
		so._setsockopt(0, SOL_SOCKET, SO_LINGER, (unsigned char *)&lg, sizeof(lg));
		so._setsockopt(0, SOL_SOCKET, SO_OOBINLINE, (unsigned char *)&opt, sizeof(int));
		defparam.clisock = 0;
		if (!(newparam = myalloc(sizeof(defparam))))
		{
			return 2;
		};
		*newparam = defparam;
		return ((*srv.pf)((void *)newparam) ? 1 : 0);
	}
#endif


	srvinit2(&srv, &defparam);
	if (!*SAFAMILY(&srv.intsa))
		*SAFAMILY(&srv.intsa) = AF_INET;
	if (!*SAPORT(&srv.intsa))
		*SAPORT(&srv.intsa) = htons(childdef.port);
	*SAFAMILY(&srv.extsa) = AF_INET;
#ifndef NOIPV6
	*SAFAMILY(&srv.extsa6) = AF_INET6;
#endif
	if (hostname)
		parsehostname(hostname, &defparam, childdef.port);

	if (srv.srvsock == INVALID_SOCKET)
	{

		lg.l_onoff = 1;
		lg.l_linger = conf.timeouts[STRING_L];
		sock = so._socket(SASOCK(&srv.intsa), SOCK_STREAM, IPPROTO_TCP);
		if (sock == INVALID_SOCKET)
		{
			perror("socket()");
			return -2;
		}
		setopts(sock, srv.lissockopts);
#ifdef _WIN32
		ioctlsocket(sock, FIONBIO, &ul);
#else
		fcntl(sock, F_SETFL, O_NONBLOCK | fcntl(sock, F_GETFL));
#endif
		srv.srvsock = sock;
		opt = 1;
		if (so._setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(int)))
			perror("setsockopt()");
#ifdef SO_REUSEPORT
		opt = 1;
		so._setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (char *)&opt, sizeof(int));
#endif
#if defined SO_BINDTODEVICE
		if (srv.ibindtodevice && so._setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, srv.ibindtodevice, strlen(srv.ibindtodevice) + 1))
		{
			dolog(&defparam, "failed to bind device");
			return -12;
		}
#elif defined IP_BOUND_IF
		if (srv.ibindtodevice)
		{
			int idx;
			idx = if_nametoindex(srv.ibindtodevice);
			if (!idx || (*SAFAMILY(&srv.intsa) == AF_INET && setsockopt(sock, IPPROTO_IP, IP_BOUND_IF, &idx, sizeof(idx))))
			{
				dolog(&defparam, (unsigned char *)"failed to bind device");
				return -12;
			}
#ifndef NOIPV6
			if ((*SAFAMILY(&srv.intsa) == AF_INET6 && so._setsockopt(sock, IPPROTO_IPV6, IPV6_BOUND_IF, &idx, sizeof(idx))))
			{
				dolog(&defparam, (unsigned char *)"failed to bind device");
				return -12;
			}
#endif
		}
#endif
	}
	size = sizeof(srv.intsa);
	for (sleeptime = SLEEPTIME * 100; so._bind(sock, (struct sockaddr *)&srv.intsa, SASIZE(&srv.intsa)) == -1; usleep(sleeptime))
	{
		sprintf((char *)buf, "bind(): %s", strerror(errno));
		if (!srv.silent)
			dolog(&defparam, buf);
		sleeptime = (sleeptime << 1);
		if (!sleeptime)
		{
			so._closesocket(sock);
			return -3;
		}
	}
	if (so._listen(sock, srv.backlog ? srv.backlog : 1 + (srv.maxchild >> 3)) == -1)
	{
		sprintf((char *)buf, "listen(): %s", strerror(errno));
		if (!srv.silent)
			dolog(&defparam, buf);
		return -4;
	}

	if (!srv.silent)
	{
		sprintf((char *)buf, "Accepting connections [%u/%u]", (unsigned)getpid(), (unsigned)pthread_self());
		dolog(&defparam, buf);
	}

	srv.fds.fd = sock;
	srv.fds.events = POLLIN;

#ifndef _WIN32
	pthread_attr_init(&pa);
	pthread_attr_setstacksize(&pa, PTHREAD_STACK_MIN + (32768 + srv.stacksize));
	pthread_attr_setdetachstate(&pa, PTHREAD_CREATE_DETACHED);
#endif

	for (;;)
	{
		for (;;)
		{
			while ((conf.paused == srv.paused && srv.childcount >= srv.maxchild))
			{
				nlog++;
				if (!srv.silent && nlog > 5000)
				{
					sprintf((char *)buf, "Warning: too many connected clients (%d/%d)", srv.childcount, srv.maxchild);
					dolog(&defparam, buf);
					nlog = 0;
				}
				usleep(SLEEPTIME);
			}
			if (conf.paused != srv.paused)
				break;
			if (srv.fds.events & POLLIN)
			{
				error = so._poll(&srv.fds, 1, 1000);
			}
			else
			{
				usleep(SLEEPTIME);
				continue;
			}
			if (error >= 1)
				break;
			if (error == 0)
				continue;
			if (errno != EAGAIN && errno != EINTR)
			{
				sprintf((char *)buf, "poll(): %s/%d", strerror(errno), errno);
				if (!srv.silent)
					dolog(&defparam, buf);
				break;
			}
		}
		if ((conf.paused != srv.paused) || (error < 0))
			break;
		error = 0;
		size = sizeof(defparam.sincr);
		new_sock = so._accept(sock, (struct sockaddr *)&defparam.sincr, &size);
		if (new_sock == INVALID_SOCKET)
		{
#ifdef _WIN32
			switch (WSAGetLastError())
			{
			case WSAEMFILE:
			case WSAENOBUFS:
			case WSAENETDOWN:
				usleep(SLEEPTIME * 10);
				break;
			case WSAEINTR:
				error = 1;
				break;
			default:
				break;
			}

#else
			switch (errno)
			{
#ifdef EMFILE
			case EMFILE:
#endif
#ifdef ENFILE
			case ENFILE:
#endif
#ifdef ENOBUFS
			case ENOBUFS:
#endif
#ifdef ENOMEM
			case ENOMEM:
#endif
				usleep(SLEEPTIME * 10);
				break;

			default:
				break;
			}
#endif
			nlog++;
			if (!srv.silent && (error || nlog > 5000))
			{
				sprintf((char *)buf, "accept(): %s", strerror(errno));
				dolog(&defparam, buf);
				nlog = 0;
			}
			continue;
		}
		setopts(new_sock, srv.clisockopts);
		size = sizeof(defparam.sincl);
		if (so._getsockname(new_sock, (struct sockaddr *)&defparam.sincl, &size))
		{
			sprintf((char *)buf, "getsockname(): %s", strerror(errno));
			if (!srv.silent)
				dolog(&defparam, buf);
			continue;
		}
#ifdef _WIN32
		ioctlsocket(new_sock, FIONBIO, &ul);
#else
		fcntl(new_sock, F_SETFL, O_NONBLOCK | fcntl(new_sock, F_GETFL));
#endif
		so._setsockopt(new_sock, SOL_SOCKET, SO_LINGER, (char *)&lg, sizeof(lg));
		so._setsockopt(new_sock, SOL_SOCKET, SO_OOBINLINE, (char *)&opt, sizeof(int));
		if (!(newparam = myalloc(sizeof(defparam))))
		{
			so._closesocket(new_sock);
			defparam.res = 21;
			if (!srv.silent)
				dolog(&defparam, (unsigned char *)"Memory Allocation Failed");
			usleep(SLEEPTIME);
			continue;
		};
		*newparam = defparam;
		if (defparam.hostname)
			newparam->hostname = (unsigned char *)mystrdup((char *)defparam.hostname);
		clearstat(newparam);
		newparam->clisock = new_sock;

		newparam->prev = newparam->next = NULL;
		error = 0;
		pthread_mutex_lock(&srv.counter_mutex);
		if (!srv.child)
		{
			srv.child = newparam;
		}
		else
		{
			newparam->next = srv.child;
			srv.child = srv.child->prev = newparam;
		}
#ifdef _WIN32
#ifndef _WINCE
		h = (HANDLE)_beginthreadex((LPSECURITY_ATTRIBUTES)NULL, (unsigned)(16384 + srv.stacksize), (void *)threadfunc, (void *)newparam, 0, &thread);
#else
		h = (HANDLE)CreateThread((LPSECURITY_ATTRIBUTES)NULL, (unsigned)(16384 + srv.stacksize), (void *)threadfunc, (void *)newparam, 0, &thread);
#endif
		srv.childcount++;
		if (h)
		{
			newparam->threadid = (unsigned)thread;
			CloseHandle(h);
		}
		else
		{
			sprintf((char *)buf, "_beginthreadex(): %s", _strerror(NULL));
			if (!srv.silent)
				dolog(&defparam, buf);
			error = 1;
		}
#else

		error = pthread_create(&thread, &pa, threadfunc, (void *)newparam);
		srv.childcount++;
		if (error)
		{
			sprintf((char *)buf, "pthread_create(): %s", strerror(error));
			if (!srv.silent)
				dolog(&defparam, buf);
		}
		else
		{
			newparam->threadid = (unsigned)thread;
		}
#endif
		pthread_mutex_unlock(&srv.counter_mutex);
		if (error)
			freeparam(newparam);

		memset(&defparam.sincl, 0, sizeof(defparam.sincl));
		memset(&defparam.sincr, 0, sizeof(defparam.sincr));
	}

	if (!srv.silent)
		srv.logfunc(&defparam, (unsigned char *)"Exiting thread");
	srvfree(&srv);

#ifndef _WIN32
	pthread_attr_destroy(&pa);
#endif
	if (defparam.hostname)
		myfree(defparam.hostname);
	if (fp)
		fclose(fp);

	return 0;
}

void srvinit(struct srvparam *srv, struct clientparam *param)
{
	memset(srv, 0, sizeof(struct srvparam));
	srv->version = conf.version + 1;
	srv->paused = conf.paused;
	srv->logfunc = havelog ? conf.logfunc : lognone;
	srv->noforce = conf.noforce;
	srv->logformat = conf.logformat ? (unsigned char *)mystrdup((char *)conf.logformat) : NULL;
	srv->authfunc = conf.authfunc;
	srv->usentlm = 0;
	srv->maxchild = conf.maxchild;
	srv->backlog = conf.backlog;
	srv->stacksize = conf.stacksize;
	srv->time_start = time(NULL);
	if (havelog && conf.logtarget)
	{
		srv->logtarget = (unsigned char *)mystrdup((char *)conf.logtarget);
	}
	srv->srvsock = INVALID_SOCKET;
	srv->logdumpsrv = conf.logdumpsrv;
	srv->logdumpcli = conf.logdumpcli;
	srv->needuser = 1;
#ifdef WITHSPLICE
	srv->usesplice = 1;
#endif
	memset(param, 0, sizeof(struct clientparam));
	param->srv = srv;
	param->version = srv->version;
	param->paused = srv->paused;
	param->remsock = param->clisock = param->ctrlsock = param->ctrlsocksrv = INVALID_SOCKET;
	*SAFAMILY(&param->req) = *SAFAMILY(&param->sinsl) = *SAFAMILY(&param->sinsr) = *SAFAMILY(&param->sincr) = *SAFAMILY(&param->sincl) = AF_INET;
	pthread_mutex_init(&srv->counter_mutex, NULL);
	srv->intsa = conf.intsa;
	srv->extsa = conf.extsa;
#ifndef NOIPV6
	srv->extsa6 = conf.extsa6;
#endif
}

void srvinit2(struct srvparam *srv, struct clientparam *param)
{
	if (srv->logformat)
	{
		char *s;
		if (*srv->logformat == '-' && (s = strchr((char *)srv->logformat + 1, '+')) && s[1])
		{
			unsigned char *logformat = srv->logformat;

			*s = 0;
			srv->nonprintable = (unsigned char *)mystrdup((char *)srv->logformat + 1);
			srv->replace = s[1];
			srv->logformat = (unsigned char *)mystrdup(s + 2);
			*s = '+';
			myfree(logformat);
		}
	}
	memset(&param->sinsl, 0, sizeof(param->sinsl));
	memset(&param->sinsr, 0, sizeof(param->sinsr));
	memset(&param->req, 0, sizeof(param->req));
	*SAFAMILY(&param->sinsl) = AF_INET;
	*SAFAMILY(&param->sinsr) = AF_INET;
	*SAFAMILY(&param->req) = AF_INET;
	param->sincr = param->sincl = srv->intsa;
#ifndef NOIPV6
	if (srv->family == 6 || srv->family == 64)
		param->sinsr = srv->extsa6;
	else
#endif
		param->sinsr = srv->extsa;
}

void srvfree(struct srvparam *srv)
{
	if (srv->srvsock != INVALID_SOCKET)
		so._closesocket(srv->srvsock);
	srv->srvsock = INVALID_SOCKET;
	srv->service = S_ZOMBIE;
	while (srv->child)
		usleep(SLEEPTIME * 100);

	pthread_mutex_destroy(&srv->counter_mutex);
	if (srv->target)
		myfree(srv->target);
	if (srv->logtarget)
		myfree(srv->logtarget);
	if (srv->logformat)
		myfree(srv->logformat);
	if (srv->nonprintable)
		myfree(srv->nonprintable);
#if defined SO_BINDTODEVICE || defined IP_BOUND_IF
	if (srv->ibindtodevice)
		myfree(srv->ibindtodevice);
	if (srv->obindtodevice)
		myfree(srv->obindtodevice);
#endif
}

void freeparam(struct clientparam *param)
{
	if (param->res == 2)
		return;
	if (param->datfilterssrv)
		myfree(param->datfilterssrv);

	if (param->clibuf)
		myfree(param->clibuf);
	if (param->srvbuf)
		myfree(param->srvbuf);
	if (param->srv)
	{
		pthread_mutex_lock(&param->srv->counter_mutex);
		if (param->prev)
		{
			param->prev->next = param->next;
		}
		else
			param->srv->child = param->next;
		if (param->next)
		{
			param->next->prev = param->prev;
		}
		(param->srv->childcount)--;
		pthread_mutex_unlock(&param->srv->counter_mutex);
	}
	if (param->hostname)
		myfree(param->hostname);
	if (param->username)
		myfree(param->username);
	if (param->password)
		myfree(param->password);
	if (param->extusername)
		myfree(param->extusername);
	if (param->extpassword)
		myfree(param->extpassword);
	if (param->ctrlsocksrv != INVALID_SOCKET && param->ctrlsocksrv != param->remsock)
	{
		so._shutdown(param->ctrlsocksrv, SHUT_RDWR);
		so._closesocket(param->ctrlsocksrv);
	}
	if (param->ctrlsock != INVALID_SOCKET && param->ctrlsock != param->clisock)
	{
		so._shutdown(param->ctrlsock, SHUT_RDWR);
		so._closesocket(param->ctrlsock);
	}
	if (param->remsock != INVALID_SOCKET)
	{
		so._shutdown(param->remsock, SHUT_RDWR);
		so._closesocket(param->remsock);
	}
	if (param->clisock != INVALID_SOCKET)
	{
		so._shutdown(param->clisock, SHUT_RDWR);
		so._closesocket(param->clisock);
	}
	myfree(param);
}

FILTER_ACTION handlepredatflt(struct clientparam *cparam)
{
	return PASS;
}

FILTER_ACTION handledatfltcli(struct clientparam *cparam, unsigned char **buf_p, int *bufsize_p, int offset, int *length_p)
{
	return PASS;
}

FILTER_ACTION handledatfltsrv(struct clientparam *cparam, unsigned char **buf_p, int *bufsize_p, int offset, int *length_p)
{
	FILTER_ACTION action;
	int i;

	for (i = 0; i < cparam->ndatfilterssrv; i++)
	{
		action = (*cparam->datfilterssrv[i]->filter->filter_data_srv)(cparam->datfilterssrv[i]->data, cparam, buf_p, bufsize_p, offset, length_p);
		if (action != CONTINUE)
			return action;
	}
	return PASS;
}
