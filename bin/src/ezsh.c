#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <setjmp.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <dirent.h>
#include <netdb.h>

#include <readline/readline.h>
#include <readline/history.h>

#include <private.h>

//
// defines
//

#define DEFAULT_PORT 9000

#define NOT_A_SPECIAL_CMD -9999

#define MAX_ALIAS 500

//
// typedefs
//

typedef struct {
    char *cmd;
    char *alias;
} alias_t;

//
// variables
//

// these are obtained from env vars or the cmdline args
char    hostname[200];
int     port;
char   *password;
bool    quiet;

// current working directory
char    cwd[200];
char    cwd_initial[200];

// obtained from the ezsh.alias file
alias_t alias_tbl[MAX_ALIAS];
int     max_alias;

// fp used to communicate to server process running on android
FILE   *sockfp;
int     sockfd = -1;
bool    socket_is_shutdown;

// used when ctrl-c a running cmd
sigset_t sigset;
jmp_buf  err_jmp_buf;

// -----------------  MAIN  -------------------------------------------------

int display_help(void);
void remove_leading_and_trailing_spaces_and_newline(char *s);
void connect_to_android(void);
char *get_str_con_to_android(FILE *fp, char *s, int s_len);
void read_ezsh_alias(void);
void substitue_alias(char *cmdline);
void *sig_hndlr_thread(void *cx);
int run_cmd(char *cmdline);

int main(int argc, char **argv)
{
    int opt;
    char *device;
    pthread_t tid;

    // set stdout to line buffered
    setlinebuf(stdout);

    // get device and password from env variables;
    // - format of device is <hostname> OR <hostname>:<port>
    // - these env variables are optional, if not provided then the
    //   ezsh -d and -p options must be supplied
    device = getenv("EZAPP_DEVICE");
    password = getenv("EZAPP_PASSWD");

    // parse options:
    //  -d <device>   : sets device string
    //  -p <password> : sets password string
    //  -h            : display help and exit
    while ((opt = getopt(argc, argv, "d:p:qh")) != -1) {
        switch (opt) {
        case 'd': {
            device = optarg;
            break; }
        case 'p':
            password = optarg;
            break;
        case 'q':
            quiet = true;
            break;
        case 'h':
        default:
            display_help();
            return 0;
        }
    }

    // device and password must have been provided,
    // either from the env variables or from getopt
    if (device == NULL || device[0] == '\0' || password == NULL || password[0] == '\0') {
        printf("ERROR: device and password are required\n");
        return 1;
    }

    // extract hostname and port from device string:
    // device string contains hostname and optional port, examples:
    //   192.168.1.2
    //   samsung
    //   samsung:9000
    char *p = strchr(device, ':');
    if (p) *p = ' ';
    port = DEFAULT_PORT;
    sscanf(device, "%s %d", hostname, &port);

    // create thread to handle ctrl-c
    // - block SIGINT in the main thread, new threads inherit the signal mask
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    pthread_sigmask(SIG_BLOCK, &sigset, NULL);
    pthread_create(&tid, NULL, sig_hndlr_thread, NULL);

    // connect to android: also validates password and gets curr-working-dir (cwd)
    connect_to_android();

    // read alias file
    read_ezsh_alias();

    // if cmd args provided then use them for cmdline, and end program
    if (argc > optind) {
        char cmdline[5000];
        char *p = cmdline;
        int status;
        for (int i = optind; i < argc; i++) {
            p += sprintf(p, "%s ", argv[i]);
        }
        substitue_alias(cmdline);
        status = run_cmd(cmdline);
        return status != 0 ? 1 : 0;
    }

    // runtime loop
    while (true) {
        char prompt[200], android_cwd[200];
        char cmdline[5000], *rl;

        // use readline to acuire the cmdline:
        // - construct prompt
        // - call readline
        // - if realine returned NULL then end program
        // - copy rl to cmdline; and cleanup the cmdline
        strcpy(android_cwd, cwd);
        snprintf(prompt, sizeof(prompt), "ezsh %s> ", basename(android_cwd));
        rl = readline(prompt);
        if (rl == NULL) {
            break;
        }
        strcpy(cmdline, rl);
        free(rl);
        remove_leading_and_trailing_spaces_and_newline(cmdline);

        // if cmdline is empty then continue;
        // if cmdline is 'q' or 'exit' then end program
        if (cmdline[0] == '\0') {
            continue;
        }
        if (strcmp(cmdline, "q") == 0 || strcmp(cmdline, "exit") == 0) {
            break;
        }

        // save cmdline in history
        add_history(cmdline);

        // substitue alias
        substitue_alias(cmdline);

        // process the cmdline
        run_cmd(cmdline);
    }
}

int display_help(void)
{
    char help_text[] = "\
Ezsh runs on the Linux host, simulating a shell running on the Android device.\n\
\n\
To use ezsh, the following ezApp settings must first be made on the Android device:\n\
- Devel_Mode = ON\n\
- Devel_Port = nnnn  (optional, the 9000 default should be okay)\n\
- Devel_Password\n\
\n\
Options:\n\
  -h             : display help and exit\n\
  -d <dev>       : <dev_name|dev_ipaddr>[:port]\n\
  -p <password>  : ezApp devel mode password\n\
  -q             : do not display message when connecting\n\
\n\
For security, it is recommended to enable ezApp Devel_Mode when on a trusted network.\n\
\n\
Commands entered to ezsh are first checked if they require special processing;\n\
if not then the command is passed to ezApp, which runs the command on the \n\
Android device.\n\
\n\
Commands that require special processing are:\n\
- cd    : Ezsh maintains the Android current working directory (cwd) path.\n\
          When a command is executed on Android, the Android directory is\n\
          first set to the cwd.\n\
- pwd   : Prints the current working dir\n\
- get   : Copy file from Android.\n\
          Example: get apps/Clock/clock.c\n\
- put   : Copy file to Android.\n\
          Example: put clock.c apps/Clock\n\
- vi    : Edit a file on Android. The file is fist copied to the host tmp dir,\n\
          edited there, and finally copied back to the Android.\n\
          Example: vi apps/Clock/clock.c\n\
- alias : Print the command aliases which are provided in the ezsh.alias file.\n\
- local : Execute a command on the host.\n\
- help  : Display help.\n\
\n\
The device and password can be provided using environment variables EZAPP_DEVICE and\n\
EZAPP_PASSWORD, or via the ezsh -d and -p options.\n\
\n\
Examples:\n\
- export EZAPP_DEVICE=192.168.1.101\n\
- export EZAPP_PASSWD=my-secret-password\n\
- ezsh\n\
\n\
- ezsh -d 192.168.1.101 -p my-secret-password\n\
\n\
- ezsh -d 192.168.1.101:9001 -p my-secret-password\n\
\n\
- ezsh \"ls -l\"\n\
\n\
";
    printf("%s", help_text);
    return 0;
}

void remove_leading_and_trailing_spaces_and_newline(char *s)
{
    char *p = s;
    int   len;

    // return if s is NULL
    if (s == NULL) {
        return;
    }

    // remove leading spaces
    while (*p == ' ') {
        p++;
    }
    len = strlen(p);
    memmove(s, p, len+1);

    // if length of s is 0 then return
    if (len == 0) {
        return;
    }

    // remove trailing spaces and newline chars
    p = &s[len-1];
    while (p >= s && (*p == ' ' || *p == '\n')) {
        *p = '\0';
        p--;
    }
}

void connect_to_android(void)
{
    int             ret, len;
    char            response[200];
    char            port_str[30];
    struct addrinfo hints, *result;
    unsigned char  *ssl_key;
    ssl_payload_t   ssl_payload;

    static bool first_call = true;

    // if socket has benn shutdown then close sockfp;
    // closing sockfp implicitely closes sockfd
    if (socket_is_shutdown) {
        if (sockfp) fclose(sockfp);
        sockfp = NULL;
        sockfd = -1;
        socket_is_shutdown = false;
    }

    // if already connected then return
    if (sockfp != NULL) {
        return;
    }

    // create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("ERROR: socket\n");
        exit(1);
    }

    // enable SO_REUSEADDR on sockfd
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // get the inet_addr of android 
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    sprintf(port_str, "%d", port);
    ret = getaddrinfo(hostname, port_str, &hints, &result);
    if (ret != 0) {
        printf("ERROR: getaddrinfo %s:%s, %s\n", hostname, port_str, strerror(errno));
        exit(1);
    }

    // connect to android
    struct sockaddr_in *ipv4 = (struct sockaddr_in *)result[0].ai_addr;
    if (!quiet && first_call) {
        char *ipaddr_str = inet_ntoa(ipv4->sin_addr);
        if (strncmp(hostname, ipaddr_str, strlen(hostname)) != 0) {
            printf("connecting to %s - %s:%s\n", hostname, ipaddr_str, port_str);
        } else {
            printf("connecting to %s:%s\n", ipaddr_str, port_str);
        }
    }
    ret = connect(sockfd, result[0].ai_addr, result[0].ai_addrlen);
    if (ret != 0) {
        printf("ERROR: connect %s:%s, %s\n", hostname, port_str, strerror(errno));
        exit(1);
    }
    free(result);

    // create fp for socket fd, and set to unbuffered
    sockfp = fdopen(sockfd, "w+");
    setvbuf(sockfp, NULL, _IONBF, 0);

    // encrypt password to ssl_payload, and send ssl_payload
    ssl_key = ssl_keygen(password);
    if (ssl_key == NULL) {
        printf("ERROR: ssl_keygen failed\n");
        exit(1);
    }
    ret = ssl_encrypt(ssl_key, password, &ssl_payload);
    if (ret != 0) {
        printf("ERROR: ssl_encrypt failed\n");
        exit(1);
    }
    ret = fwrite(&ssl_payload, 1, sizeof(ssl_payload), sockfp);
    if (ret != sizeof(ssl_payload)) {
        printf("ERROR: failed to send encrypted password\n");
        exit(1);
    }

    // read response, verify password was accepted
    get_str_con_to_android(sockfp, response, sizeof(response));
    if (strcmp(response, "password okay") != 0) {
        printf("ERROR: %s\n", response);
        exit(1);   
    }

    if (first_call) {
        // get the ezApp current working dir
        get_str_con_to_android(sockfp, cwd, sizeof(cwd));

        // ensure cwd includes terminating '/'
        len = strlen(cwd);
        if (len == 0) {
            strcpy(cwd, "/");
        } else if (cwd[len-1] != '/') {
            strcat(cwd, "/");
        }

        // save initial cwd
        strcpy(cwd_initial, cwd);

        // clear first_call_flag
        first_call = false;
    } else {
        char cwd_throw_away[200];
        get_str_con_to_android(sockfp, cwd_throw_away, sizeof(cwd_throw_away));
    }
}

char *get_str_con_to_android(FILE *fp, char *s, int s_len)
{
    char *p;
    int len;

    s[0] = '\0';

    p = fgets(s, s_len, fp);
    if (p == NULL) {
        printf("ERROR: lost connection to android, ezsh terminating\n");
        exit(1);
    }

    len = strlen(s);
    if (len > 0 && s[len-1] == '\n') {
        s[len-1] = '\0';
    }

    return s;
}

void read_ezsh_alias(void)
{
    char  self_path[200], ezsh_alias_path[200], *self_dir, s[2000];
    FILE *fp;
    int   line_num=0;

    // get path to ezsh.alias file
    memset(self_path, 0, sizeof(self_path));
    readlink("/proc/self/exe", self_path, sizeof(self_path));
    self_dir = dirname(self_path);
    sprintf(ezsh_alias_path, "%s/ezsh.alias", self_dir);

    // open ezsh.alias file
    fp = fopen(ezsh_alias_path, "r");
    if (fp == NULL) {
        printf("ERROR: failed to open %s\n", ezsh_alias_path);
        exit(1);
    }

    // read and process lines from ezsh.alias file
    while (fgets(s, sizeof(s), fp) != NULL) {
        line_num++;

        // remove leading and trailing spaces and trailing newline
        remove_leading_and_trailing_spaces_and_newline(s);

        // skip blank lines and lines begining with '#'
        if (s[0] == '\0' || s[0] == '#') {
            continue;
        }

        // parse string s to extract the cmd and alias components
        char *cmd = strtok(s, " ");
        char *alias = strtok(NULL, "");
        if (cmd == NULL || alias == NULL) {
            printf("ERROR: ezsh.alias, error on line %d\n", line_num);
            continue;
        }
        remove_leading_and_trailing_spaces_and_newline(alias);

        // if alias table is full then close fp and return
        if (max_alias == MAX_ALIAS) {
            printf("ERROR: alias tbl is full, ezsh.alias line %d\n", line_num);
            fclose(fp);
            return;
        }

        // add entry to alias_tbl
        alias_tbl[max_alias].cmd = strdup(cmd);
        alias_tbl[max_alias].alias = strdup(alias);
        max_alias++;
    }
}

void substitue_alias(char *cmdline)
{
    char *p, temp[1000];
    int   i;

    p = strchr(cmdline, ' ');
    if (p) *p = '\0';

    for (i = 0; i < max_alias; i++) {
        if (strcmp(cmdline, alias_tbl[i].cmd) == 0) {
            if (p) *p = ' ';
            strcpy(temp, cmdline+strlen(alias_tbl[i].cmd));
            strcpy(cmdline, alias_tbl[i].alias);
            strcat(cmdline, temp);
            return;
        }
    }

    if (p) *p = ' ';
}

void *sig_hndlr_thread(void *cx)
{
    int sig;

    while (true) {
        sigwait(&sigset, &sig);
        socket_is_shutdown = true;
        shutdown(sockfd, SHUT_RDWR);
    }

    return NULL;
}

// -----------------  RUN CMD  ---------------------------

#define ANDROID_ERROR \
    do { \
        printf("\nERROR %s line %d\n\n", __func__, __LINE__); \
        longjmp(err_jmp_buf, 1); \
    } while (0)

int run_special_cmd(char *cmdline);
int special_cmd_put(char *src, char *dest);
int special_cmd_get(char *src, char *dest);
int special_cmd_quiesced(char *src, char *dest);
int special_cmd_cd(char *path);
int special_cmd_pwd(void);
int special_cmd_alias(void);
int special_cmd_vi(char *android_path);
int special_cmd_local(char *cmdline);

int run_cmd_on_android(char *cmdline, char *short_cmdline,
                       char *data_out, int data_out_len,
                       char **data_in, int *data_in_len);

void put_fmt(FILE *fp, char *fmt, ...);
char *get_str(FILE *fp, char *s, int s_len);
int read_file(char *fn, void **buf, int *buf_len);
int write_file(char *fn, void *buf, int len);

// retcode values:
// . = 0  : success
// . > 0  : is an exitcode from the cmdline executed on android
// . < 0  : is an errno

int run_cmd(char *cmdline)
{
    int status;
    char cd_plus_cmdline[1000];

    // check for empty cmdline; this should never happen
    if (cmdline[0] == '\0') {
        printf("ERROR: %s, cmdline is empty\n", __func__);
        return -EINVAL;
    }

    // connect to android device, this does nothing if already connected
    connect_to_android();

    // set target for error longjmp
    if (setjmp(err_jmp_buf) == 1) {
        return -ENOTCONN;
    }

    // first try running cmdline using run_special_cmd;
    // if the cmdline is not a special cmd the NOT_A_SPECIAL_CMD status is returned
    status = run_special_cmd(cmdline);
    if (status != NOT_A_SPECIAL_CMD) {
        return status;
    }

    // it wasn't a special cmd; try running the cmd on android
    sprintf(cd_plus_cmdline, "cd %s; %s", cwd, cmdline);
    status = run_cmd_on_android(cd_plus_cmdline, cmdline, NULL, 0, NULL, 0);
    return status;
}

// - - - - - - - - -  run cmd on android - - - - - - - - - - - - 

int run_cmd_on_android(char *cmdline, char *short_cmdline,
                       char *data_out, int data_out_len,
                       char **data_in, int *data_in_len)
{
    char  s[200];
    char *p;
    int   rc;
    int   status = 99;

    // preset returned values
    if (data_in != NULL) {
        *data_in = NULL;
        *data_in_len = 0;
    }

    // send 'run' and cmdline to android
    put_fmt(sockfp, "run\n");
    put_fmt(sockfp, "%s\n", cmdline);

    // if data_out is provided then send data buffer to android 
    if (data_out != NULL) {
        put_fmt(sockfp, "data_len %d\n", data_out_len);

        rc = fwrite(data_out, 1, data_out_len, sockfp);
        if (rc != data_out_len) {
            ANDROID_ERROR;
        }

        get_str(sockfp, s, sizeof(s));
        if (strncmp(s, "CMD_COMPLETE ", 13) != 0) {
            ANDROID_ERROR;
        }
        sscanf(s+13, "%d", &status);

    // if data_in is provided then recv data buffer from adnroid
    } else if (data_in != NULL) {
        static char *buf;
        int          buf_len;

        // Prior execution of this code may have leaked buf.
        // This can happen if 2nd call to get_str had an eror, and longjmps.
        // If buf is non NULL then get_str error had previously occurred, so free buf.
        if (buf != NULL) {
            free(buf);
            buf = NULL;
        }

        // get buf_len from android
        get_str(sockfp, s, sizeof(s));
        if (sscanf(s, "data_len %d", &buf_len) != 1) {
            ANDROID_ERROR;
        }

        // allocate buf
        buf = malloc(buf_len);

        // read data from android
        rc = fread(buf, 1, buf_len, sockfp);
        if (rc != buf_len) {
            free(buf);
            buf = NULL;
            ANDROID_ERROR;
        }

        // read cmd completion string from android
        get_str(sockfp, s, sizeof(s));
        if (strncmp(s, "CMD_COMPLETE ", 13) != 0) {
            free(buf);
            buf = NULL;
            ANDROID_ERROR;
        }

        // return data to caller; caller must free data
        *data_in = buf;
        *data_in_len = buf_len;
        buf = NULL;

        // parse the status returned from android
        sscanf(s+13, "%d", &status);

    // otherwise android will have run this cmd using popen;
    // read and print the provided output from popen
    } else {
        while (true) {
            // get string from android, and print
            get_str(sockfp, s, sizeof(s));

            // check for cmd complete, and parse status
            if ((p = strstr(s, "CMD_COMPLETE "))) {
                sscanf(p+13, "%d", &status);
                *p = '\0';
                if (strlen(s) > 0) {
                    printf("%s\n", s);
                }
                break;
            }

            printf("%s\n", s);
        }
    }

    // print status
    // - status == 0 : success
    // - status > 0  : is an exitcode from the cmdline executed on android
    // - status < 0  : is an errno
    if (status == 0) {
        // success
    } else if (status > 0) {
        if (status == 127) {
            printf("ezsh: Command '%s' not found.\n", short_cmdline);
        } else {
            printf("ERROR: exit_status %d\n", status);
        }
    } else if (status < 0) {
        printf("ERROR: %s\n", strerror(-status));
    }

    // return status
    return status;
}

// - - - - - - - - -  run special cmd  - - - - - - - - - - - - - 

int run_special_cmd(char *cmdline)
{
    char cmd[200], arg1[200], arg2[200];

    // extract cmd, arg1, and arg2 from cmdline
    cmd[0] = arg1[0] = arg2[0] = '\0';
    sscanf(cmdline, "%s %s %s", cmd, arg1, arg2);

    // process special cmds
    if (strcmp(cmd, "cd") == 0) {
        return special_cmd_cd(arg1);
    } else if (strcmp(cmd, "pwd") == 0) {
        return special_cmd_pwd();
    } else if (strcmp(cmd, "alias") == 0) {
        return special_cmd_alias();
    } else if (strcmp(cmd, "put") == 0) {
        return special_cmd_put(arg1, arg2);
    } else if (strcmp(cmd, "get") == 0) {
        return special_cmd_get(arg1, arg2);
    } else if (strcmp(cmd, "quiesced") == 0) {
        return special_cmd_quiesced(arg1, arg2);
    } else if (strcmp(cmd, "vi") == 0) {
        return special_cmd_vi(arg1);
    } else if (strcmp(cmd, "local") == 0) {
        return special_cmd_local(cmdline);
    } else if (strcmp(cmd, "help") == 0) {
        return display_help();
    } else {
        return NOT_A_SPECIAL_CMD;
    }
}

// copy file to android:
// - src: path to src file on devel computer
// - dest: path to dest file or dest dir on android,
//   . will be prepended with cwd
//   . may be empty str
int special_cmd_put(char *src, char *dest)
{
    char  cmdline[1000];
    char  temp[200];
    char  dest_path[200];
    char *src_filename;
    void *data;
    int   data_len;
    int   status;

    // src is required
    if (src[0] == '\0') {
        printf("ERROR: %s, src arg required\n", __func__);
        return -EINVAL;
    }

    // extract src_filename from src
    strcpy(temp, src);
    src_filename = basename(temp);

    // construct dest_path
    if (dest[0] == '/') {
        strcpy(dest_path, dest);
    } else {
        sprintf(dest_path, "%s%s", cwd, dest);
    }

    // read src file
    status = read_file(src, &data, &data_len);
    if (status != 0) {
        printf("ERROR: %s, failed to read_file %s\n", __func__, src);
        return status;
    }

    // run 'put <dest_path> <src_filename>' on android
    sprintf(cmdline, "put %s %s", dest_path, src_filename);
    status = run_cmd_on_android(cmdline, cmdline, data, data_len, NULL, 0);

    // free data
    free(data);

    // return status
    return status;
}

// copy file from android:
// - src: path to src file on android
// - dest: path to dest file or dest dir on develsys,
//   . if empty str then will be replaced with "."
int special_cmd_get(char *src, char *dest)
{
    char  src_path[200], dest_path[200], cmdline[1000];
    DIR  *dir;
    int   status, data_len;
    char *data;

    // src is required
    if (src[0] == '\0') {
        printf("ERROR: %s, src arg required\n", __func__);
        return -EINVAL;
    }

    // construct src_path by prepending src with cwd
    if (src[0] == '/') {
        strcpy(src_path, src);
    } else {
        sprintf(src_path, "%s%s", cwd, src);
    }

    // construct dest_path
    if (dest[0] != '\0') {
        strcpy(dest_path, dest);
    } else {
        strcpy(dest_path, ".");
    }
    dir = opendir(dest_path);
    if (dir != NULL) {
        char *src_filename, temp[200];
        int   len;

        len = strlen(dest_path);
        if (len > 0 && dest_path[len-1] != '/') {
            strcat(dest_path, "/");
        }
        strcpy(temp, src_path);
        src_filename = basename(temp);
        strcat(dest_path, src_filename);
        closedir(dir);
    }

    // debug print
    //printf("src_path '%s'\n", src_path);
    //printf("dest_path '%s'\n", dest_path);

    // run 'get <src_path>' on android
    sprintf(cmdline, "get %s", src_path);
    status = run_cmd_on_android(cmdline, cmdline, NULL, 0, &data, &data_len);
    if (status != 0) {
        printf("ERROR: %s, failed to get file %s from Android\n", __func__, src_path);
        free(data);
        return status;
    }

    // write file to dest_path on devel computer
    status = write_file(dest_path, data, data_len);
    if (status != 0) {
        printf("ERROR: %s, failed to write file %s to devel computer\n", __func__, dest_path);
        free(data);
        return status;
    }

    // free data, and return status
    free(data);
    return status;
}

// returns 0 if ezApp currently has no app and no svcs running
int special_cmd_quiesced(char *src, char *dest)
{
    char *cmdline = "quiesced";
    int status;

    status = run_cmd_on_android(cmdline, cmdline, NULL, 0, NULL, 0);
    return status;
}

// this routine updates cwd; and will always terminate cwd with '/'.
int special_cmd_cd(char *path)
{
    int   len;
    char  new_cwd[200];
    char *token;

    // sanity check that cwd begins and ends with '/'
    len = strlen(cwd);
    if (cwd[0] != '/' || cwd[len-1] != '/') {
        printf("ERROR: invalid cwd '%s'\n", cwd);
        exit(1);
    }

    // use new_cwd as work area, until it is validated
    strcpy(new_cwd, cwd);

    // update new_cwd, using the supplied path
    if (path[0] == '\0') {
        strcpy(new_cwd, cwd_initial);
    } else if (path[0] == '/') {
        strcpy(new_cwd, path);
    } else {
        // ensure path terminates with '/'
        len = strlen(path);
        if (path[len-1] != '/') {
            strcat(path, "/");
        }

        // tokenize path using '/' separator;
        // and update new_cwd using the value of each token
        while ((token = strtok(path, "/"))) {
            path = NULL;
            if (strcmp(token, ".") == 0) {
                // do nothing
            } else if (strcmp(token, "..") == 0) {
                if (strcmp(new_cwd, "/") != 0) {
                    int idx = strlen(new_cwd) - 2;
                    while (new_cwd[idx] != '/') idx--;
                    new_cwd[idx+1] = '\0';
                }
            } else {
                strcat(new_cwd, token);
                strcat(new_cwd, "/");
            }
        }
    }

    // ensure new_cwd terminates with '/'
    len = strlen(new_cwd);
    if (len > 0 && new_cwd[len-1] != '/') {
        strcat(new_cwd, "/");
    }

    // run_cmd_on_android to verify new_cwd is an android directory;
    // if so then the new_cwd will be used
    char cmd[300];
    int  status;
    sprintf(cmd, "if [ ! -d %s ]; then exit 1; else exit 0; fi", new_cwd);
    status = run_cmd_on_android(cmd, cmd, NULL, 0, NULL, 0);
    if (status == 0) {
        strcpy(cwd, new_cwd);
    }

    // return status
    return status;
}

int special_cmd_pwd(void)
{
    printf("%s\n", cwd);
    return 0;
}

int special_cmd_alias(void)
{
    int i;
    
    for (i = 0; i < max_alias; i++) {
        printf("%-16s %s\n", alias_tbl[i].cmd, alias_tbl[i].alias);
    }
    return 0; 
}

// copies file fron Android to devel PC,
// runs vi on devel PC,
// copies editted file back to Android
int special_cmd_vi(char *android_path)
{
    char temp[200], tmp_path[200], vi_cmd[1000];
    int status;

    // android_path is required
    if (android_path[0] == '\0') {
        printf("ERROR: %s, android_path required\n", __func__);
        return -EINVAL;
    }

    // construct /tmp path
    strcpy(temp, android_path);
    sprintf(tmp_path, "/tmp/%s", basename(temp));

    // copy android file to tmp on devel computer
    status = special_cmd_get(android_path, tmp_path);
    if (status != 0) {
        if (status == -ENOENT) {
            unlink(tmp_path);
        } else {
            return status;
        }
    }

    // edit tmp_path file on devel computer
    sprintf(vi_cmd, "vi %s", tmp_path);
    system(vi_cmd);

    // copy editted file back to android
    status = special_cmd_put(tmp_path, android_path);
    if (status != 0) {
        return status;
    }

    // success
    return 0;
}

// run a cmd on the devel PC
int special_cmd_local(char *cmdline)
{
    char *p, *local_cmd, *home;
    int   status = 0;
    char  dir[200];

    // cmdline contains:
    // - "local"
    // - "local <cmd_to_execute_on_devel_pc>"

    // if cmd_to_execute_on_devel_pc is not provided then execute bash
    p = strchr(cmdline, ' ');
    if (p == NULL) {
        printf("ezsh: executing bash ...\n");
        system("bash");
        return 0;
    } 
    local_cmd = p+1;

    // process local cmd "cd"
    if (strcmp(local_cmd, "cd") == 0) {
        home = getenv("HOME");
        if (home == NULL) {
            home = "/";
        }
        status = chdir(home);
        if (status != 0) {
            status = -errno;
        }

    // process local cmd "cd <dir>"
    } else if (sscanf(local_cmd, "cd %s", dir) == 1) {
        status = chdir(dir);
        if (status != 0) {
            status = -errno;
            printf("ERROR: cd %s, %s\n", dir, strerror(-status));
        }

    // use system() to execute local_cmd
    } else {
        status = system(local_cmd);
        status = WEXITSTATUS(status);
    }

    // return status
    return status;
}

// - - - - - - - - -  run cmd support routines - - - - - - - - - 

void put_fmt(FILE *fp, char *fmt, ...)
{
    va_list ap;
    int rc;

    va_start(ap, fmt);

    rc = vfprintf(fp, fmt, ap);
    if (rc < 0) {
        ANDROID_ERROR;
    }

    va_end(ap);
}

char *get_str(FILE *fp, char *s, int s_len)
{
    char *p;
    int len;

    s[0] = '\0';

    p = fgets(s, s_len, fp);
    if (p == NULL) {
        ANDROID_ERROR;
    }

    len = strlen(s);
    if (len > 0 && s[len-1] == '\n') {
        s[len-1] = '\0';
    }

    return s;
}

int read_file(char *fn, void **buf_arg, int *buf_len_arg)
{
    int fd, ret;
    struct stat statbuf;
    char *buf;

    *buf_len_arg = 0;
    *buf_arg = NULL;

    ret = stat(fn, &statbuf);
    if (ret < 0) {
        return errno ? -errno : -EINVAL;
    }
    
    fd = open(fn, O_RDONLY);
    if (fd < 0) {
        return errno ? -errno : -EINVAL;
    }

    buf = malloc(statbuf.st_size);
    if (buf == NULL) {
        close(fd);
        return errno ? -errno : -EINVAL;
    }
    
    ret = read(fd, buf, statbuf.st_size);
    if (ret != statbuf.st_size) {
        free(buf);
        close(fd);
        return errno ? -errno : -EINVAL;
    }

    close(fd);

    *buf_arg = buf;
    *buf_len_arg = statbuf.st_size;
    return 0;  
}

int write_file(char *fn, void *buf, int len)
{
    int fd, ret;

    fd = open(fn, O_CREAT | O_TRUNC | O_WRONLY, 0666);
    if (fd < 0) {
        return errno ? -errno : -EINVAL;
    }

    ret = write(fd, buf, len);
    if (ret != len) {
        return errno ? -errno : -EINVAL;
    }

    close(fd);
    return 0;
}

