// Copyright (c) 2019 Roland Bernard

#include <termios.h>
#include <stdio.h>
#include <sys/signal.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>

#include "server.h"
#include "client.h"

#define DEF_PORT 24242
#define DEF_HOST "127.0.0.1"
#define DEF_GROUP "default"
#define DEF_NAME getlogin()

// used to restore the terminal
struct termios oldterm;

void resetTerm() {
	tcsetattr(STDIN_FILENO, 0, &oldterm);
}

// find the first occurance of the given character in the string
// return -1 if the char isn't contained
int strfndchr(const char* str, char c) {
	int i = 0;
	while(str[i]) {
		if(str[i] == c)
			return i;
		i++;
	}
	return -1;
}

int main(int argc, char** argv) {
	bool_t is_server = 0;
	config_t conf = {
		.flag = FLAG_CONF_DEF_HOST | FLAG_CONF_USE_GROUP | FLAG_CONF_UTF8 | FLAG_CONF_USE_LOG,
		.name = DEF_NAME,
		.group = DEF_GROUP,
		.host = DEF_HOST,
		.passwd = NULL,
		.port = DEF_PORT
	};

	// evaluate parameters
	for(int i = 1; i < argc; i++) {
		if(strcmp("-h", argv[i]) == 0 || strcasecmp("--ip", argv[i]) == 0) /* server ip */ {
			if(i+1 < argc) {
				conf.host = argv[i+1];
				conf.flag &= ~FLAG_CONF_DEF_HOST;
				i++;
			} else
				fprintf(stderr, "no host specified, option is ignored\n");
		} else if(strcmp("-p", argv[i]) == 0 || strcasecmp("--port", argv[i]) == 0) /* server port */ {
			if(i+1 < argc) {
				unsigned int nport = atoi(argv[i+1]);
				if(nport == 0 || nport > 0xFFFF)
					perror("illegal port number, option is ignored\n");
				else
					conf.port = nport;
				i++;
			} else
				fprintf(stderr, "no port specified, option is ignored\n");
		} else if(strcmp("-a", argv[i]) == 0 || strcasecmp("--alternet", argv[i]) == 0) /* use alternet screen buffer */ {
			conf.flag |= FLAG_CONF_USE_ALTERNET;
		} else if(strcmp("-s", argv[i]) == 0 || strcasecmp("--server", argv[i]) == 0) /* is this a server */ {
			is_server = 1;
			conf.flag |= FLAG_CONF_IS_SERVER /* not actualy used */;
		} else if(strcmp("-G", argv[i]) == 0 || strcasecmp("--no-group", argv[i]) == 0) /* don't use group */ {
			conf.flag &= ~FLAG_CONF_USE_GROUP;
			conf.group = NULL;
		} else if(strcmp("-H", argv[i]) == 0 || strcasecmp("--auto-discovery", argv[i]) == 0) /* use automatic host discovery */ {
			conf.flag |= FLAG_CONF_AUTO_DIS;
		}  else if(strcmp("-B", argv[i]) == 0 || strcasecmp("--ignore-break", argv[i]) == 0) /* use automatic host discovery */ {
			conf.flag |= FLAG_CONF_IGN_BREAK;
		} else if(strcmp("-U", argv[i]) == 0 || strcasecmp("--no-utf-8", argv[i]) == 0) /* don't use utf-8 */ {
			conf.flag &= ~FLAG_CONF_UTF8;
		} else if(strcmp("-t", argv[i]) == 0 || strcasecmp("--typing-info", argv[i]) == 0) /* send typing info */ {
			conf.flag |= FLAG_CONF_USE_TYP;
		} else if(strcmp("-L", argv[i]) == 0 || strcasecmp("--no-log-info", argv[i]) == 0) /* don't send enter and exit info */ {
			conf.flag &= ~FLAG_CONF_USE_LOG;
		} else if(strcmp("-k", argv[i]) == 0 || strcasecmp("--key", argv[i]) == 0) /* set the key */ {
			if(i+1 < argc) {
				conf.passwd = argv[i+1];
				conf.flag |= FLAG_CONF_USE_ENC;
				i++;
			} else
				fprintf(stderr, "no key specified, option is ignored\n");
		} else if(strcmp("-n", argv[i]) == 0 || strcasecmp("--name", argv[i]) == 0) /* name of the user */ {
			if(i+1 < argc) {
				if(strfndchr(argv[i+1], '@') != -1 || strfndchr(argv[i+1], '|') != -1 || strfndchr(argv[i+1], '~') != -1)
					fprintf(stderr, "name can't contain '@', '|' or '~', option is ignored\n");
				else
					conf.name = argv[i+1];
				i++;
			} else
				fprintf(stderr, "no name specified, option is ignored\n");
		} else if(strcmp("-g", argv[i]) == 0 || strcasecmp("--group", argv[i]) == 0) /* name of the group */ {
			if(i+1 < argc) {
				if(strfndchr(argv[i+1], '@') != -1 || strfndchr(argv[i+1], '|') != -1 || strfndchr(argv[i+1], '~') != -1)
					fprintf(stderr, "group name can't contain '@', '|' or '~', option is ignored\n");
				else
					conf.group = argv[i+1];
				i++;
			} else
				fprintf(stderr, "no group specified, option is ignored\n");
		}  else if(strcmp("--help", argv[i]) == 0) /* output help */ {
			fprintf(stderr,
				"Usage: %s [options]\n"
				"\n"
				"Options:\n"
				"  -h, --ip IP            set the servers ip (def: '127.0.0.1')\n"
				"  -p, --port PORT        select the servers port (def: '24242')\n"
				"  -s, --server           make this a server\n"
				"  -H, --auto-discovery   use automatic discovery\n"
				"\n"
				"Options for clients:\n"
				"  -n, --name NAME        set the name (def: username)\n"
				"  -G, --no-group         do not use the group feature\n"
				"  -B, --ignore-break     do not worry about breaking words\n"
				"  -U, --no-utf-8         avoid any utf-8 I/O processing\n"
				"  -g, --group GROUP      set the group (def: 'default')\n"
				"  -a, --alternet        *use the alternet frame buffer\n"
				"  -t, --typing-info      send typing info\n"
				"  -L, --no-log-info      don't send enter and exit info\n"
				"  -k, --key KEY          encrypt mesages with the given key\n"
				"  --help                 show this help page\n"
				"\n"
				"* This may cause problems if the terminal\n"
				"  does not support certain features\n"
			, argv[0]);
			exit(EXIT_SUCCESS);
		} else {
			fprintf(stderr, "unknown option '%s', option is ignored\n", argv[i]);
		}
	}

	// configure input to be less processed
	struct termios newterm;
	tcgetattr(STDIN_FILENO, &oldterm);
	atexit(resetTerm);
	newterm = oldterm;
	newterm.c_lflag &= ~(ICANON | ECHO | ISIG);
	//newterm.c_oflag &= ~(OPOST);
	newterm.c_cc[VMIN] = 0;
	newterm.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, 0, &newterm);

	signal(SIGPIPE, SIG_IGN); // an error on send will cause a SIGPIPE


	error_t ret = 0;
	if(is_server)
		server_main(conf);
	else
		client_main(conf);
	return ret;
}
