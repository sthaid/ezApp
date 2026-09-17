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

#define MB 0x100000L
#define GB (1024 * MB)

#define DEFAULT_PORT 9000

#define NOT_A_SPECIAL_CMD -9999

#define MAX_ALIAS 500

#define STATUS_UNKNOWN 99

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

// used to communicate to server process running on android
FILE   *sockfp;
int     sockfd = -1;
bool    recon_needed;

// used when ctrl-c a running cmd
sigset_t sigset;

//
// global prototypes
//

int display_help(void);
void remove_leading_and_trailing_spaces_and_newline(char *s);
void connect_to_android(void);
void read_ezsh_alias(void);
void substitue_alias(char *cmdline);
void *sig_hndlr_thread(void *cx);

int run_cmd(char *cmdline);

int run_android_cmd(char *cmdline);
int run_android_put_cmd(char *src, char *dest);
int run_android_get_cmd(char *src, char *dest);

int run_special_cmd(char *cmdline);
int special_cmd_cd(char *path);
int special_cmd_pwd(void);
int special_cmd_alias(void);
int special_cmd_vi(char *android_path);
int special_cmd_local(char *cmdline);

int get_str(FILE *fp, char *s, int s_len);
int put_fmt(FILE *fp, char *fmt, ...);
long file_size(char *path);
void print_cmd_status(int status, char *cmdline);
int copy(FILE *dest_fp, FILE *src_fp, long data_len);

// -----------------  MAIN  -------------------------------------------------

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
- vi    : Edit a file on Android. The file is first copied to the host tmp dir,\n\
          edited there, and finally copied back to the Android.\n\
          Example: vi apps/Clock/clock.c\n\
- alias : Print the command aliases which are provided in the ezsh.alias file.\n\
- local : Execute a command on the host.\n\
- help  : Display help.\n\
- quiesced : Return success if no miniApp or miniSvc is running; used by ezbackup.\n\
\n\
The device and password can be provided using environment variables EZAPP_DEVICE and\n\
EZAPP_PASSWD, or via the ezsh -d and -p options.\n\
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
    int             rc, len;
    char            response[200], port_str[30];
    struct addrinfo hints, *result;
    unsigned char  *ssl_key;
    ssl_payload_t   ssl_payload;

    static bool first_call = true;

    // if socket has benn shutdown then close sockfp;
    // closing sockfp implicitely closes sockfd
    if (recon_needed) {
        if (sockfp) {
            shutdown(sockfd, SHUT_RDWR);
            fclose(sockfp);
        }
        sockfp = NULL;
        sockfd = -1;
        recon_needed = false;
    }

    // if already connected then return
    if (sockfp != NULL) {
        return;
    }

    // create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("ERROR: socket, %s\n", strerror(errno));
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
    rc = getaddrinfo(hostname, port_str, &hints, &result);
    if (rc != 0) {
        printf("ERROR: getaddrinfo %s:%s, %s\n", hostname, port_str, strerror(errno));
        exit(1);
    }

    // connect to android
    struct sockaddr_in *ipv4 = (struct sockaddr_in *)result[0].ai_addr;
    if (!quiet) {
        char *ipaddr_str = inet_ntoa(ipv4->sin_addr);
        if (strncmp(hostname, ipaddr_str, strlen(hostname)) != 0) {
            printf("connecting to %s - %s:%s\n", hostname, ipaddr_str, port_str);
        } else {
            printf("connecting to %s:%s\n", ipaddr_str, port_str);
        }
    }
    rc = connect(sockfd, result[0].ai_addr, result[0].ai_addrlen);
    if (rc != 0) {
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
    rc = ssl_encrypt(ssl_key, password, &ssl_payload);
    if (rc != 0) {
        printf("ERROR: ssl_encrypt failed\n");
        exit(1);
    }
    rc = fwrite(&ssl_payload, 1, sizeof(ssl_payload), sockfp);
    if (rc != sizeof(ssl_payload)) {
        printf("ERROR: failed to send encrypted password\n");
        exit(1);
    }

    // read response, verify password was accepted
    rc = get_str(sockfp, response, sizeof(response));
    if (rc != 0) {
        printf("ERROR: failed to connect\n");
        exit(1);
    }
    if (strcmp(response, "password okay") != 0) {
        printf("ERROR: %s\n", response);
        exit(1);   
    }

    if (first_call) {
        // get the ezApp current working dir
        rc = get_str(sockfp, cwd, sizeof(cwd));
        if (rc != 0) {
            printf("ERROR: failed to connect\n");
            exit(1);
        }

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
        rc = get_str(sockfp, cwd_throw_away, sizeof(cwd_throw_away));
        if (rc != 0) {
            printf("ERROR: failed to connect\n");
            exit(1);
        }
    }

    // print that connection is established
    if (!quiet) {
        printf("connected\n");
    }
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
        shutdown(sockfd, SHUT_RDWR);
        recon_needed = true;
    }

    return NULL;
}

// -----------------  RUN CMD  ---------------------------

// retcode values:
// . = 0  : success
// . > 0  : is an exitcode from the cmdline executed on android
// . < 0  : is an errno

int run_cmd(char *cmdline)
{
    int  status;
    char cd_plus_cmdline[500];

    // check for empty cmdline; this should never happen
    if (cmdline[0] == '\0') {
        printf("ERROR: cmdline is empty\n");
        return -EINVAL;
    }

    // connect to android device, this does nothing if already connected
    connect_to_android();

    // first try running cmdline using run_special_cmd;
    // if the cmdline is not a special cmd the NOT_A_SPECIAL_CMD status is returned
    status = run_special_cmd(cmdline);
    if (status != NOT_A_SPECIAL_CMD) {
        return status;
    }

    // it wasn't a special cmd, run the cmd on Android
    sprintf(cd_plus_cmdline, "cd %s; %s", cwd, cmdline);
    status = run_android_cmd(cd_plus_cmdline);
    return status;
}

// -----------------  RUN CMDS ON ANDROID  ---------------

// This routine is used to run all commands on Android, except for the
// get and put cmds. The get and put cmds are used to transfer files to/from
// the Android device, and they have unique requirements.
int run_android_cmd(char *cmdline)
{
    int  rc, status;
    char s[500], *p;

    // send cmdline to Android
    rc = put_fmt(sockfp, "run\n%s\n", cmdline);
    if (rc != 0) {
        printf("ERROR: failed to send cmd to Android, %s\n", strerror(errno));
        recon_needed = true;
        return -EINVAL;
    }

    // get cmd output from android, and print;
    // check for cmd completion, break out of loop when CMD_COMPLETE received
    status = STATUS_UNKNOWN;
    while (true) {
        rc = get_str(sockfp, s, sizeof(s));
        if (rc != 0) {
            printf("ERROR: failed to receive response from Android, %s\n", strerror(errno));
            recon_needed = true;
            return -EINVAL;
        }

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

    // print response status from Android
    print_cmd_status(status, cmdline);

    // return status
    return status;
}

// copy file to android:
// - src: path to src file on devel computer
// - dest: path to dest file or dest dir on android,
//   . will be prepended with cwd
//   . may be empty str
int run_android_put_cmd(char *src, char *dest)
{
    char  temp[200], dest_path[200], *src_filename, s[100];
    long  data_len;
    int   rc, status;
    FILE *filefp = NULL;

    // src is required
    if (src[0] == '\0') {
        printf("ERROR: src arg required\n");
        return -EINVAL;
    }

    // get size of src file
    data_len = file_size(src);
    if (data_len == 0) {
        printf("ERROR: failed to get file_size of %s, %s\n", src, strerror(errno));
        return -EINVAL;
    }

    // open src file for reading
    filefp = fopen(src, "r");
    if (filefp == NULL) {
        printf("ERROR: failed to open %s for reading, %s\n", src, strerror(errno));
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

    // send the cmd and data_len to Android
    rc = put_fmt(sockfp,
                 "run\n"
                 "put %s %s\n"
                 "data_len %ld\n",
                 dest_path, src_filename, data_len);
    if (rc != 0) {
        printf("ERROR: failed to send cmd to Android, %s\n", strerror(errno));
        fclose(filefp);
        recon_needed = true;
        return -EINVAL;
    }

    // copy the file data to Android
    rc = copy(sockfp, filefp, data_len);
    fclose(filefp);
    filefp = NULL;
    if (rc != 0) {
        printf("ERROR: failed to copy file data to Android, %s\n", strerror(errno));
        recon_needed = true;
        return -EINVAL;
    }
    
    // get response from Android
    rc = get_str(sockfp, s, sizeof(s));
    if (rc != 0) {
        printf("ERROR: failed to receive status from Android, %s\n", strerror(errno));
        recon_needed = true;
        return -EINVAL;
    }
    if (strncmp(s, "CMD_COMPLETE ", 13) != 0) {
        printf("ERROR: failed to receive status from Android, %s\n", strerror(errno));
        recon_needed = true;
        return -EINVAL;
    }
    status = STATUS_UNKNOWN;
    sscanf(s+13, "%d", &status);

    // print response from Android
    print_cmd_status(status, "put");

    // return status
    return status;
}

// copy file from android:
// - src: path to src file on android
// - dest: path to dest file or dest dir on develsys,
//   . if empty str then will be replaced with "."
int run_android_get_cmd(char *src, char *dest)
{
    char  src_path[200], dest_path[200], s[100];
    DIR  *dir;
    int   status, rc;
    long  data_len;
    FILE *filefp;

    // src is required
    if (src[0] == '\0') {
        printf("ERROR: src arg required\n");
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
    //printf("src_path  '%s'\n", src_path);
    //printf("dest_path '%s'\n", dest_path);

    // open dest path for writing
    filefp = fopen(dest_path, "w");
    if (filefp == NULL) {
        printf("ERROR: failed to open %s for writing, %s\n", dest_path, strerror(errno));
        return -EINVAL;
    }

    // send the cmd to Android
    rc = put_fmt(sockfp,
                 "run\n"
                 "get %s\n",
                 src_path);
    if (rc != 0) {
        printf("ERROR: failed to send cmd to Android, %s\n", strerror(errno));
        fclose(filefp);
        recon_needed = true;
        return -EINVAL;
    }

    // read data_len from Android
    s[0] = '\0';
    rc = get_str(sockfp, s, sizeof(s));
    if (rc != 0) {
        printf("ERROR: failed to recv data_len from Android, %s\n", strerror(errno));
        fclose(filefp);
        recon_needed = true;
        return -EINVAL;
    }
    if (sscanf(s, "data_len %ld", &data_len) != 1) {
        printf("ERROR: failed to recv data_len from Android\n");
        fclose(filefp);
        recon_needed = true;
        return -EINVAL;
    }

    // copy the file data provided from Android to the dest_path file on devel PC
    rc = copy(filefp, sockfp, data_len);
    fclose(filefp);
    filefp = NULL;
    if (rc != 0) {
        printf("ERROR: failed to copy file data to Android, %s\n", strerror(errno));
        recon_needed = true;
        return -EINVAL;
    }

    // get response from Android
    rc = get_str(sockfp, s, sizeof(s));
    if (rc != 0) {
        printf("ERROR: failed to recv status from Android, %s\n", strerror(errno));
        recon_needed = true;
        return -EINVAL;
    }
    if (strncmp(s, "CMD_COMPLETE ", 13) != 0) {
        printf("ERROR: failed to recv status from Android, %s\n", strerror(errno));
        recon_needed = true;
        return -EINVAL;
    }
    status = STATUS_UNKNOWN;
    sscanf(s+13, "%d", &status);

    // print response from Android
    print_cmd_status(status, "get");

    // return status
    return status;
}

// -----------------  RUN SPECIAL CMD --------------------

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
    } else if (strcmp(cmd, "local") == 0) {
        return special_cmd_local(cmdline);
    } else if (strcmp(cmd, "vi") == 0) {
        return special_cmd_vi(arg1);

    } else if (strcmp(cmd, "put") == 0) {
        return run_android_put_cmd(arg1, arg2);
    } else if (strcmp(cmd, "get") == 0) {
        return run_android_get_cmd(arg1, arg2);
    } else if (strcmp(cmd, "quiesced") == 0) {
        return run_android_cmd(cmd);

    } else if (strcmp(cmd, "help") == 0) {
        return display_help();

    } else {
        return NOT_A_SPECIAL_CMD;
    }
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

    // run_andorid_cmd to verify new_cwd is an android directory;
    // if so then the new_cwd will be used
    char cmd[300];
    int  status;
    sprintf(cmd, "if [ ! -d %s ]; then exit 1; else exit 0; fi", new_cwd);
    status = run_android_cmd(cmd);
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
        printf("ERROR: android_path required\n");
        return -EINVAL;
    }

    // construct /tmp path
    strcpy(temp, android_path);
    sprintf(tmp_path, "/tmp/%s", basename(temp));
    unlink(tmp_path);

    // copy android file to tmp on devel computer
    status = run_android_get_cmd(android_path, tmp_path);
    if (status != 0) {
        printf("ERROR: failed to get file %s\n", android_path);
        return status;
    }

    // edit tmp_path file on devel computer
    sprintf(vi_cmd, "vi %s", tmp_path);
    system(vi_cmd);

    // copy editted file back to android
    status = run_android_put_cmd(tmp_path, android_path);
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

// - - - - - - - - -  support routines - - - - - - - - - - - - - 

int put_fmt(FILE *fp, char *fmt, ...)
{
    va_list ap;
    int rc;

    va_start(ap, fmt);
    rc = vfprintf(fp, fmt, ap);
    va_end(ap);

    if (rc < 0) {
        printf("ERROR: put_fmt failed, '%s'\n", fmt);
        return -EINVAL;
    }

    return 0;
}

int get_str(FILE *fp, char *s, int s_len)
{
    char *p;
    int len;

    s[0] = '\0';

    p = fgets(s, s_len, fp);
    if (p == NULL) {
        printf("ERROR: failed to get string from Android\n");
        return -EINVAL;
    }

    len = strlen(s);
    if (len > 0 && s[len-1] == '\n') {
        s[len-1] = '\0';
    }

    return 0;
}

long file_size(char *path)
{
    struct stat statbuf;
    int rc;

    rc = stat(path, &statbuf);
    return (rc == 0 ? statbuf.st_size : 0);
}

void print_cmd_status(int status, char *cmdline)
{
    char *short_cmdline, *p=NULL;

    // print status
    // - status == 0 : success
    // - status > 0  : is an exitcode from the cmdline executed on android
    // - status < 0  : is an errno

    if (status == 0) {
        // success
    } else if (status > 0) {
        if ((strncmp(cmdline, "cd ", 3) == 0) && ((p = strstr(cmdline, "; ")) != NULL)) {
            short_cmdline = p+2;
        } else {
            short_cmdline = cmdline;
        }
        if (status == 127) {
            printf("ERROR: cmd '%s' not found.\n", short_cmdline);
        } else if (status == STATUS_UNKNOWN) {
            printf("ERROR: cmd '%s' status unknown\n", short_cmdline);
        } else {
            printf("ERROR: cmd '%s' status %d\n", short_cmdline, status);
        }
    } else if (status < 0) {
        printf("ERROR: %s\n", strerror(-status));
    }
}

int copy(FILE *dest_fp, FILE *src_fp, long total_len)
{
    char *buff;
    long xfer_len, rc, total_xfered, len_remaining;

    #define MAX_BUFF (16 * MB)

    buff = malloc(MAX_BUFF);
    if (buff == NULL) {
        return -EINVAL;
    }

    total_xfered = 0;
    len_remaining = total_len;
    while (true) {
        xfer_len = (len_remaining > MAX_BUFF ? MAX_BUFF : len_remaining);

        rc = fread(buff, 1, xfer_len, src_fp);
        if (rc != xfer_len) {
            printf("ERROR: fread failed, xfer_len=%ld rc=%ld, %s\n", 
                  xfer_len, rc, strerror(errno));
            free(buff);
            return -EINVAL;
        }

        rc = fwrite(buff, 1, xfer_len, dest_fp);
        if (rc != xfer_len) {
            printf("ERROR: write failed, xfer_len=%ld rc=%ld, %s\n", 
                  xfer_len, rc, strerror(errno));
            free(buff);
            return -EINVAL;
        }

        total_xfered += xfer_len;
        len_remaining -= xfer_len;

        printf("\r%0.3f GB / %0.3f GB (%0.0f%%)",
               (double)total_xfered / GB,
               (double)total_len / GB,
               (double)total_xfered / total_len * 100);
        fflush(stdout);

        if (len_remaining == 0) {
            printf("\n");
            free(buff);
            return 0;
        }
    }
}
