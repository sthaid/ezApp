#include <std_hdrs.h>

#include <utils.h>
#include <private.h>

#include <cJSON.h>
#include <lodepng.h>
#include <kiss_fftr.h>

#define PAGE_SIZE2 (getpagesize())

// ----------------- TIME --------------------

long util_microsec_timer(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC,&ts);
    return  ((long)ts.tv_sec * 1000000) + ((long)ts.tv_nsec / 1000);
}

long util_get_real_time_microsec(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME,&ts);
    return ((long)ts.tv_sec * 1000000) + ((long)ts.tv_nsec / 1000);
}

char *util_time2str(char * str, long us, int gmt, int display_ms, int display_date)
{
    struct tm tm;
    time_t secs;
    int cnt;
    char * s = str;

    secs = us / 1000000;

    if (gmt) {
        gmtime_r(&secs, &tm);
    } else {
        localtime_r(&secs, &tm);
    }

    if (display_date) {
        cnt = sprintf(s, "%02d/%02d/%02d ",
                         tm.tm_mon+1, tm.tm_mday, tm.tm_year%100);
        s += cnt;
    }

    cnt = sprintf(s, "%02d:%02d:%02d",
                     tm.tm_hour, tm.tm_min, tm.tm_sec);
    s += cnt;

    if (display_ms) {
        cnt = sprintf(s, ".%03d", (int)((us % 1000000) / 1000));
        s += cnt;
    }

    if (gmt) {
        strcpy(s, " GMT");
    }

    return str;
}

// -----------------  FILE READ/WRITE/etc  -------------------

static char *concat(char *s1, char *s2, char *result)
{
    if (s1 && s2) {
        sprintf(result, "%s/%s", s1, s2); 
    } else if (s1) {
        strcpy(result, s1);
    } else if (s2) {
        strcpy(result, s2);
    } else {
        ERROR("both s1 and s2 are null\n");
        result[0] = '\0';
    }

    return result;
}

int util_write_file(char *dir, char *fn, void *buf, int len)
{
    int fd, ret;
    char path[200];

    concat(dir, fn, path);
    INFO("writing file %s\n", path);

    fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0666);
    if (fd < 0) {
        return -1;
    }

    if (buf && len > 0) {
        ret = write(fd, buf, len);
        if (ret != len) {
            close(fd);
            return -1;
        }
    }

    close(fd);
    return 0;
}

// notes: 
// - an extra '\0' byte is added to the end of the data buffer;
//   this extra char is not included in len_ret
// - len_ret arg is optional
void *util_read_file(char *dir, char *fn, int *len_ret)
{
    struct stat statbuf;
    int         fd, ret, len_ret_dummy;
    char        *buf;
    char         path[200];

    if (len_ret == NULL) {
        len_ret = &len_ret_dummy;
    }

    concat(dir, fn, path);
    INFO("reading file %s\n", path);

    *len_ret = 0;

    ret = stat(path, &statbuf);
    if (ret < 0) {
        return NULL;
    }

    buf = malloc(statbuf.st_size+1);
    if (buf == NULL) {
        return NULL;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        free(buf);
        return NULL;
    }

    ret = read(fd, buf, statbuf.st_size);
    if (ret != statbuf.st_size) {
        free(buf);
        return NULL;
    }

    buf[statbuf.st_size] = '\0';

    close(fd);

    *len_ret = statbuf.st_size;
    return buf;
}

void util_delete_file(char *dir, char *fn)
{
    char path[200];

    concat(dir, fn, path);

    INFO("deleting file %s\n", path);
    unlink(path);
}

void util_rename_file(char *old_dir, char *old_fn, char *new_dir, char *new_fn)
{
    char old_path[200], new_path[200];

    concat(old_dir, old_fn, old_path);
    concat(new_dir, new_fn, new_path);

    INFO("renaming %s to %s\n", old_path, new_path);
    rename(old_path, new_path);
}

bool util_file_exists(char *dir, char *fn)
{
    char path[200];
    struct stat statbuf;
    int rc;

    concat(dir, fn, path);

    rc = stat(path, &statbuf);
    return rc == 0;
}

// return unix epoch time in microsecs
long util_file_mtime(char *dir, char *fn)
{
    char path[200];
    struct stat statbuf;
    int rc;

    concat(dir, fn, path);

    rc = stat(path, &statbuf);
    if (rc != 0) {
        return 0;
    }

    return statbuf.st_mtim.tv_sec * 1000000L + statbuf.st_mtim.tv_nsec / 1000L;
}

long util_file_size(char *dir, char *fn)
{
    char path[200];
    struct stat statbuf;
    int rc;

    concat(dir, fn, path);

    rc = stat(path, &statbuf);
    return (rc == 0 ? statbuf.st_size : 0);
}

// -----------------  DIRECTORY UTILS  -----------------------

void util_create_dir(char *dir, char *dir_to_create)
{
    char path[200], cmd[220];

    concat(dir, dir_to_create, path);
    INFO("creating dir %s\n", path);

    sprintf(cmd, "mkdir -p %s", path);
    system(cmd);
}

void util_delete_dir(char *dir, char *dir_to_delete)
{
    char path[200], cmd[220];

    concat(dir, dir_to_delete, path);
    INFO("deleting dir %s\n", path);

    sprintf(cmd, "rm -rf %s", path);
    system(cmd);
}

// -----------------  FILE MAP -------------------------------

void *util_map_file(char *dir, char *file, int len_arg, bool create_if_needed, int *created_flag)
{
    char   path[100];
    int    fd, rc, file_len, len;
    bool   file_exists;
    void  *addr = NULL;
    struct stat statbuf;
    char   *zero;

    concat(dir, file, path);

    // round len up to multiple of pagesize
    len = (len_arg + PAGE_SIZE2 - 1) & ~(PAGE_SIZE2-1);

    // preset 'created_flag' return to false
    if (created_flag != NULL) {
        *created_flag = false;
    }

    // print message
    INFO("mapping %s len_arg=0x%x adj_len=0x%x create_if_needed=%d\n", path, len_arg, len, create_if_needed);

    // stat the file to determine if it exists, and its length
    rc = stat(path, &statbuf);
    file_exists = (rc == 0) && S_ISREG(statbuf.st_mode);
    file_len    = file_exists ? statbuf.st_size : 0;
    //INFO("file exists=%d file_len=0x%x\n", file_exists, file_len);

    // if the file doesnt exist, or has the wrong length then
    //   if create flag is set
    //     unlink the file
    //     create the file with the proper length, and fill with zero
    //     set 'created_flag'
    //   else
    //     return error
    //   endif
    // endif
    if (!file_exists || file_len != len) {
        if (create_if_needed) {
            //INFO("creating %s adj_len=0x%x\n", path, len);
            unlink(path);
            fd = open(path, O_CREAT|O_EXCL|O_RDWR, 0666);
            if (fd < 0) {
                ERROR("failed to create %s, %s\n", path, strerror(errno));
                goto done;  
            }
            zero = calloc(len, 1);
            rc = write(fd, zero, len);
            free(zero);
            if (rc != len) {
                ERROR("write zero to %s, adj_len=0x%x, failed, %s\n", path, len, strerror(errno));
                close(fd);
                goto done;  
            }
            close(fd);
            if (created_flag != NULL) {
                *created_flag = true;
            }
        } else {
            ERROR("file doesnt exist or has wrong len, file_exists=%d file_len=0x%x adj_len=0x%x\n", 
                   file_exists, file_len, len);
            goto done;  
        }
    }

    // map the file
    //INFO("mapping %s adj_len=0x%x\n", path, len);
    fd = open(path, O_RDWR);
    if (fd < 0) {
        ERROR("failed to open %s, %s\n", path, strerror(errno));
        goto done;  
    }
    addr = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == NULL) {
        ERROR("mmap %s failed, %s\n", path, strerror(errno));
        close(fd);
        goto done;  
    }
    //INFO("mmap returned addr %p\n", addr);
    close(fd);

done:
    // return mapped addr
    return addr;
}

void util_unmap_file(void *addr, int len_arg)
{
    int len, rc;

    // round len up to multiple of pagesize
    len = (len_arg + PAGE_SIZE2 - 1) & ~(PAGE_SIZE2-1);

    // print starting msg
    INFO("unmapping addr %p, len_arg=0x%x adj_len=0x%x\n", addr, len_arg, len);

    // sanity check addr arg
    if (addr == NULL) {
        ERROR("addr is NULL\n");
        return;
    }

    // unmap
    rc = munmap(addr, len);
    if (rc != 0) {
        ERROR("munmap failed, addr=%p adj_len=0x%x, %s\n", addr, len, strerror(errno));
    }
}

void util_sync_file(void *addr, int len)
{
    int           rc;
    unsigned long first_page    = (unsigned long)addr & ~(PAGE_SIZE2-1);
    unsigned long last_page     = ((unsigned long)addr + len - 1) & ~(PAGE_SIZE2-1);
    void         *adjusted_addr = (void*)first_page;
    int           adjusted_len  = last_page - first_page + PAGE_SIZE2;

    //INFO("addr=%p len=0x%x - adj_addr=%p adj_len=0x%x\n", addr, len, adjusted_addr, adjusted_len);

    rc = msync(adjusted_addr, adjusted_len, MS_SYNC);
    if (rc != 0) {
        ERROR("msync failed, adj_addr=%p adj_len=0x%x, %s\n", adjusted_addr, adjusted_len, strerror(errno));
    }
}

// -----------------  GET / SET PARAMS  ----------------------

static pthread_mutex_t params_mutex = PTHREAD_MUTEX_INITIALIZER;
#define PARAMS_LOCK    do { pthread_mutex_lock(&params_mutex); } while (0)
#define PARAMS_UNLOCK  do { pthread_mutex_unlock(&params_mutex); } while (0)

#define MAX_PARAMS 50
static struct {
    char *name;
    char *value;
} params[MAX_PARAMS];
static int max_params;
static char params_dir[100];

static void remove_trailing_newline(char *s)
{
    int len = strlen(s);

    if (len > 0 && s[len-1] == '\n') {
        s[len-1] = '\0';
    }
}

static void read_params_file(char *dir)
{
    char s[200], name[100], params_path[100];
    int cnt, n;
    FILE *fp;

    if (strcmp(dir, params_dir) == 0) {
        return;
    }
    strcpy(params_dir, dir);

    //INFO("reading params file in dir '%s'\n", dir);

    for (int i = 0; i < max_params; i++) {
        free(params[i].name);
        free(params[i].value);
    }
    memset(params, 0, sizeof(params));
    max_params = 0;

    sprintf(params_path, "%s/params", dir);
    fp = fopen(params_path, "r");
    if (fp == NULL) {
        //INFO("params file does not exist\n");
        return;
    }

    while (fgets(s, sizeof(s), fp) != NULL) {
        remove_trailing_newline(s);
        n = 0;
        cnt = sscanf(s, "%s = %n", name, &n);
        if (cnt != 1 || n == 0) {
            ERROR("read_params_file '%s'\n", s);
            fclose(fp);
            return;
        }

        params[max_params].name = strdup(name);
        params[max_params].value = strdup(s+n);
        max_params++;
    }

    fclose(fp);

    //INFO("max_params=%d\n", max_params);
    //for (int i = 0; i < max_params; i++) {
    //    INFO("  %s = %s\n", params[i].name, params[i].value);
    //}
}

static void write_params_file(char *dir)
{
    FILE *fp;
    char params_path[100];

    if (strcmp(dir, params_dir) != 0) {
        ERROR("write_params_file, dir=%s params_dir=%s\n", dir, params_dir);
        return;
    }

    //INFO("writing params file in dir '%s'\n", dir);
    //INFO("max_params=%d\n", max_params);
    //for (int i = 0; i < max_params; i++) {
    //    INFO("  %s = %s\n", params[i].name, params[i].value);
    //}

    sprintf(params_path, "%s/params", dir);
    fp = fopen(params_path, "w");
    if (fp == NULL) {
        ERROR("write_params_file, fopen failed, %s\n", strerror(errno));
        return;
    }

    for (int i = 0; i < max_params; i++) {
        fprintf(fp, "%-24s = %s\n", params[i].name, params[i].value);
    }

    fclose(fp);
}

// caller must free the returned string
char *util_get_str_param(char *dir, char *name, char *default_value)
{
    int i;

    PARAMS_LOCK;

    // if haven't read the params file then do so
    read_params_file(dir);

    // search for matching name
    for (i = 0; i < max_params; i++) {
        if (strcmp(name, params[i].name) == 0) {
            break;
        }
    }

    // if found then 
    //   return value
    // else
    //   add param, set to default value, and write file
    // endif
    if (i < max_params) {
        char *param_value = strdup(params[i].value);
        PARAMS_UNLOCK;
        return param_value;
    } else {
        if (max_params >= MAX_PARAMS) {
            ERROR("params tbl is full\n");
            PARAMS_UNLOCK;
            return strdup(default_value);
        }
        params[max_params].name = strdup(name);
        params[max_params].value = strdup(default_value);
        max_params++;
        write_params_file(dir);
        PARAMS_UNLOCK;
        return strdup(default_value);
    }
}

void util_set_str_param(char *dir, char *name, char *value)
{
    int i;

    PARAMS_LOCK;

    // if haven't read the params file then do so
    read_params_file(dir);

    // search for matching name
    for (i = 0; i < max_params; i++) {
        if (strcmp(name, params[i].name) == 0) {
            break;
        }
    }

    // if found then
    //   if no change then return
    //   otherwise, replace its value
    // else
    //   add param to the end
    // endif
    if (i < max_params) {
        if (strcmp(params[i].value, value) == 0) {
            PARAMS_UNLOCK;
            return;
        }
        free(params[i].value);
        params[i].value = strdup(value);
    } else {
        if (max_params >= MAX_PARAMS) {
            ERROR("params tbl is full\n");
            PARAMS_UNLOCK;
            return;
        }
        params[max_params].name = strdup(name);
        params[max_params].value = strdup(value);
        max_params++;
    }

    // write the params file
    write_params_file(dir);

    PARAMS_UNLOCK;
}

double util_get_numeric_param(char *dir, char *name, double dflt_val)
{
    char   dflt_val_str[100];
    char  *value_str;
    double value;
    int    cnt;

    // create the default value string, and
    // call util_get_str_param to get the value_str
    sprintf(dflt_val_str, "%0.3f", dflt_val);
    value_str = util_get_str_param(dir, name, dflt_val_str);

    // convert value_str, returned by util_get_str_param, to 'value'
    cnt = sscanf(value_str, "%lf", &value);
    free(value_str);

    // the conversion can fail if value_str is not a number,
    // if the conversion fails then call util_set_numeric_param, and 
    // return the default value
    if (cnt != 1) {
        util_set_numeric_param(dir, name, dflt_val);
        return dflt_val;
    }

    // return the param value
    return value;
}

void util_set_numeric_param(char *dir, char *name, double value)
{
    char value_str[100];

    // create value string, and
    // call util_set_str_param to set it
    sprintf(value_str, "%0.3f", value);
    util_set_str_param(dir, name, value_str);
}    

void util_print_params(char *dir)
{
    int i;

    PARAMS_LOCK;

    read_params_file(dir);

    INFO("max_params=%d\n", max_params);
    for (i = 0; i < max_params; i++) {
        INFO("  %s = %s\n", params[i].name, params[i].value);
    }

    PARAMS_UNLOCK;
}

// -----------------  NETWORK  -------------------------------

char *util_get_ipaddr(char *ipaddr_str)
{
    int rc, a=0, b=0, c=0, d=0;
    unsigned int addr;
    struct ifaddrs *ifap, *ifap_orig;;
    static bool verbose = true;

    strcpy(ipaddr_str, "xxx.xxx.xxx.xxx");

    rc = getifaddrs(&ifap_orig);
    if (rc != 0) {
        ERROR("getifaddrs, %s\n", strerror(errno));
        return ipaddr_str;
    }

    ifap = ifap_orig;
    while (ifap) {
        //INFO("ifa_name = %s\n", ifap->ifa_name);
        if (ifap->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *x = (struct sockaddr_in*)ifap->ifa_addr;

            addr = htonl(x->sin_addr.s_addr);
            a = (addr >> 24) & 0xff;
            b = (addr >> 16) & 0xff;
            c = (addr >>  8) & 0xff;
            d = (addr >>  0) & 0xff;

            // skip over loopback and docker0 networks;
            // the docker0 network may be present when running the ezApp linux version
            if (a == 127 || a == 172) {
                goto next;
            }

            if (verbose) {
                INFO("getifaddrs found %d.%d.%d.%d\n", a,b,c,d);
            }
            sprintf(ipaddr_str, "%d.%d.%d.%d", a,b,c,d);
        }
            
next:
        ifap = ifap->ifa_next;
    }

    freeifaddrs(ifap_orig);

    verbose = false;
    return ipaddr_str;
}

// ----------------- JSON --------------------

// note: arg end_ptr is optional
void *util_json_parse(char *str, char **end_ptr)
{
    char *end_ptr_dummy;

    if (end_ptr == NULL) {
        end_ptr = &end_ptr_dummy;
    }

    if (str == NULL || end_ptr == NULL) {
        return NULL;
    }

    return cJSON_ParseWithOpts(str, (const char **)end_ptr, 0);
}

void util_json_free(void *json_root)
{
    if (json_root == NULL) {
        return;
    }

    cJSON_Delete((cJSON*)json_root);
}

json_value_t *util_json_get_value(void *json_item, ...)
{
    cJSON              *item = (cJSON*)json_item;
    va_list             ap;
    char               *arg;
    int                 cnt, array_idx;
    static json_value_t value;

    memset(&value, 0, sizeof(value));

    if (json_item == NULL) {
        value.type = JSON_TYPE_UNDEFINED;
        return &value;
    }

    va_start(ap, json_item);

    while ((arg = va_arg(ap, char*)) != NULL) {
        cnt = sscanf(arg, "%d", &array_idx);
        if (cnt == 0) {
            item = cJSON_GetObjectItem(item, arg);
        } else {
            item = cJSON_GetArrayItem(item, array_idx);
        }
        if (item == NULL) {
            break;
        }
    }

    va_end(ap);

    if (item == NULL) {
        value.type = JSON_TYPE_UNDEFINED;
        return &value;
    }

    switch (item->type) {
    case cJSON_False:
    case cJSON_True:
        value.type = JSON_TYPE_FLAG;
        value.u.flag = (item->type == cJSON_True);
        break;
    case cJSON_Number:
        value.type = JSON_TYPE_NUMBER;
        value.u.number = item->valuedouble;
        break;
    case cJSON_String:
        value.type = JSON_TYPE_STRING;
        value.u.string = item->valuestring;
        break;
    case cJSON_Array:
        value.type = JSON_TYPE_ARRAY;
        value.u.array = item;
        break;
    case cJSON_Object:
        value.type = JSON_TYPE_OBJECT;
        value.u.object = item;
        break;
    default:
        value.type = JSON_TYPE_UNDEFINED;
        break;
    }

    return &value;
}

// ----------------- PNG  --------------------

int util_read_png_file(char *dir, char *filename, unsigned char **pixels, int *w, int *h)
{
    char path[200];
    int rc;

    concat(dir, filename, path);
    //INFO("reading png file %s\n", path);

    rc = lodepng_decode32_file(pixels, (unsigned int*)w, (unsigned int*)h, path);
    if (rc != 0) {
        ERROR("lodepng_decode32_file %s failed, rc=%d\n", path, rc);
        return -1;
    }

    return 0;
}

int util_write_png_file(char *dir, char *filename, unsigned char *pixels, int w, int h)
{
    char           pathname[200];
    LodePNGState   state;
    int            rc = -1;
    unsigned char *png_buffer;
    size_t         png_size;

    // create pathname
    concat(dir, filename, pathname);
    
    // init lodepng state
    lodepng_state_init(&state);
    // - tell LodePNG your raw input buffer is 8-bit RGBA
    state.info_raw.colortype = LCT_RGBA;
    state.info_raw.bitdepth = 8;
    // - tell LodePNG your desired output file must be 8-bit RGBA
    state.info_png.color.colortype = LCT_RGBA;
    state.info_png.color.bitdepth = 8;
    // - stop LodePNG from automatically changing the output color format
    state.encoder.auto_convert = 0; 
    
    // encode pixels to png_buffer
    rc = lodepng_encode(&png_buffer, &png_size, pixels, w, h, &state);
    if (rc != 0) {
        ERROR("lodepng_encode failed, rc=%d, %s\n", rc, lodepng_error_text(rc));
        goto done;
    }

    // write png_buffer to pathname
    rc = lodepng_save_file(png_buffer, png_size, pathname);
    if (rc != 0) {
        ERROR("lodepng_save_file failed, rc=%d, %s\n", rc, lodepng_error_text(rc));
        goto done;
    }
    
    // set success return value
    rc = 0;

done:
    // cleanup and return status
    free(png_buffer);
    lodepng_state_cleanup(&state);
    return rc;
}

// ----------------- FFT ---------------------

void util_fft_real_to_real(int n_fft, float *input, float *output, bool scale_by_n_fft)
{
    kiss_fftr_cfg  cfg;
    int            n_output;
    complex_t     *cpx_output;

    // verify n_fft is multiple of 2, if not then decrement
    if (n_fft & 1) {
        ERROR("n_fft=%d is not multiple of 2\n", n_fft);
        n_fft--;
    }

    // determine number of elements in output
    n_output = n_fft / 2 + 1;

    // alloc fft complex output buffer
    cpx_output = calloc(n_output, sizeof(complex_t));

    // alloc kissfft cfg, and perform fft
    cfg = kiss_fftr_alloc(n_fft, false, NULL, NULL);
    kiss_fftr(cfg, input, (kiss_fft_cpx*)cpx_output);

    // convert complex ouput buffer to caller supplied real value buffer;
    // apply scaling if requested
    if (scale_by_n_fft) {
        float scale_factor = (1.0 / n_fft);
        for (int i = 0; i < n_output; i++) {
            output[i] = sqrtf(cpx_output[i].r * cpx_output[i].r + cpx_output[i].i * cpx_output[i].i) * scale_factor;
        }
    } else {
        for (int i = 0; i < n_output; i++) {
            output[i] = sqrtf(cpx_output[i].r * cpx_output[i].r + cpx_output[i].i * cpx_output[i].i);
        }
    }

    // free allocated complex output buffer
    free(cpx_output);

    // free kissfft cfg
    kiss_fft_free(cfg);
}

void util_fft_real_to_complex(int n_fft, float *input, complex_t *cpx_output, bool scale_by_n_fft)
{
    kiss_fftr_cfg  cfg;
    int            n_output;

    // verify n_fft is multiple of 2, if not then decrement
    if (n_fft & 1) {
        ERROR("n_fft=%d is not multiple of 2\n", n_fft);
        n_fft--;
    }

    // determine number of elements in output
    n_output = n_fft / 2 + 1;

    // alloc kissfft cfg, and perform fft
    cfg = kiss_fftr_alloc(n_fft, false, NULL, NULL);
    kiss_fftr(cfg, input, (kiss_fft_cpx*)cpx_output);

    // if scaling is requested then do so
    if (scale_by_n_fft) {
        float scale_factor = 1.0 / n_fft;
        for (int i = 0; i < n_output; i++) {
            cpx_output[i].r *= scale_factor;
            cpx_output[i].i *= scale_factor;
        }
    }

    // free kissfft cfg
    kiss_fft_free(cfg);
}

void util_fft_inverse_complex_to_real(int n_fft, complex_t *cpx_input, float *output, bool scale_by_n_fft)
{
    kiss_fftr_cfg cfg;

    // verify n_fft is multiple of 2, if not then decrement
    if (n_fft & 1) {
        ERROR("n_fft=%d is not multiple of 2\n", n_fft);
        n_fft--;
    }

    // alloc kissfft inverse cfg, and perform inverse fft
    cfg = kiss_fftr_alloc(n_fft, true, NULL, NULL);
    kiss_fftri(cfg, (kiss_fft_cpx*)cpx_input, output);

    // if scaling is requested then do so
    if (scale_by_n_fft) {
        float scale_factor = 1.0 / n_fft;
        for (int i = 0; i < n_fft; i++) {
            output[i] *= scale_factor;
        }
    }

    // free kissfft cfg
    kiss_fft_free(cfg);
}

// - - - - fft test - - - -

static int fft_test_check(char *test_name, int n, float *array1, float *array2);

int util_fft_test(long *duration_usec)
{
    #define TWO_PI (2 * M_PI)
    #define N 1000

    float     input[N];
    complex_t cpx_output[N/2+1];
    float     real_output[N/2+1];
    float     expected_output[N/2+1];
    float     reconstructed_input[N];
    int       rc1=-1, rc2=-1, rc3=-1;

    //INFO("starting ...\n");

    // -------------
    // --- Test1 ---
    // -------------

    // init fft_input with sin wave, and zero other buffers
    for (int i = 0; i < N; i++) {
        input[i] = sin(TWO_PI * i / N * 10);
    }
    memset(cpx_output, 0, sizeof(cpx_output));
    memset(reconstructed_input, 0, sizeof(reconstructed_input));

    // perform fft, and inverse fft
    util_fft_real_to_complex(N, input, cpx_output, false);
    util_fft_inverse_complex_to_real(N, cpx_output, reconstructed_input, true);
    rc1 = fft_test_check("Test1", N, input, reconstructed_input);

    // -------------
    // --- Test2 ---
    // -------------

    // init fft_input with sin wave, and zero other buffers
    for (int i = 0; i < N; i++) {
        input[i] = sin(TWO_PI * i / N * 10);
    }
    memset(cpx_output, 0, sizeof(cpx_output));
    memset(reconstructed_input, 0, sizeof(reconstructed_input));

    // perform fft, and inverse fft
    util_fft_real_to_complex(N, input, cpx_output, true);
    util_fft_inverse_complex_to_real(N, cpx_output, reconstructed_input, false);
    rc2 = fft_test_check("Test2", N, input, reconstructed_input);

    // -------------
    // --- Test3 ---
    // -------------

    // init fft_input with sin wave, and zero other buffers
    for (int i = 0; i < N; i++) {
        input[i] = sin(TWO_PI * i / N * 10);
    }
    memset(real_output, 0, sizeof(real_output));
    memset(expected_output, 0, sizeof(expected_output));
    expected_output[10] = 0.5;

    // perform real to real fft
    util_fft_real_to_real(N, input, real_output, true);
    rc3 = fft_test_check("Test3", N/2+1, expected_output, real_output);

    // ----------------------
    // --- Test4 - Timing ---
    // ----------------------

    long start_usec = util_microsec_timer();
    util_fft_real_to_real(N, input, real_output, true);
    *duration_usec = util_microsec_timer() - start_usec;
    //INFO("Test4 PASSED - duration = %ld usec\n", *duration_usec);

    //INFO("done\n");
    return (rc1 == 0 && rc2 == 0 && rc3 == 0) ? 0 : -1;
}

static int fft_test_check(char *test_name, int n, float *array1, float *array2)
{
    #define EPSILON .001
    bool different;
    int  num_different = 0;

    for (int i = 0; i < n; i++) {
        different = (fabsf(array1[i] - array2[i]) > EPSILON);
        if (different) num_different++;
    }

    if (num_different == 0) {
        //INFO("%s PASSED\n", test_name);
        return 0;
    }

    ERROR("%s FAILED, num_different=%d\n", test_name, num_different);
    for (int i = 0; i < n; i++) {
        different = (fabsf(array1[i] - array2[i]) > EPSILON);
        ERROR("%3d: %8.4f %8.4f   %s\n", 
              i, array1[i], array2[i],
              different ? "DIFFERENT" : "");
    }

    return -1;
}

// - - - - calculate rms - - - -

double util_rms_float(float *x, int n)
{       
    double sum = 0;
            
    for (int i = 0; i < n; i++) {
        sum += x[i] * x[i];
    }
    return sqrt(sum / n);
}
