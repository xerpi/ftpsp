#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include "psp_functions.h"
#include "ftpsp.h"

int cmd_USER_func(struct ftpsp_client *client)
{
    client_send_ctrl_msg(client, "331 Username ok, need password");
    return 1;
}

int cmd_PASS_func(struct ftpsp_client *client)
{
    client_send_ctrl_msg(client, "230 User logged in");
    return 1;   
}

int cmd_SYST_func(struct ftpsp_client *client)
{
    client_send_ctrl_msg(client, "215 UNIX Type: L8");
    return 1;
}

int cmd_FEAT_func(struct ftpsp_client *client)
{
    client_send_ctrl_msg(client, "502 Error: command not implemented");  
    return 1;
}

int cmd_NOOP_func(struct ftpsp_client *client)
{
    client_send_ctrl_msg(client, "200 Command okay.");  
    return 1;
}


int cmd_PWD_func(struct ftpsp_client *client)
{
    char path[PATH_MAX];
    sprintf(path, "257 \"%s\" is current directory.", client->cur_path);
    client_send_ctrl_msg(client, path);
    return 1;
}

int cmd_QUIT_func(struct ftpsp_client *client)
{
    client_send_ctrl_msg(client, "221 Quit");
    return 1;
}

int cmd_TYPE_func(struct ftpsp_client *client)
{
    char data_type, format_control;
    int args = sscanf(client->rd_buffer, "%*s %c %c", &data_type, &format_control);
    
    if (args > 0) {
        switch(data_type) {
        case 'A':
            if (args < 2) {
                client_send_ctrl_msg(client, "504 Error: bad parameters?");
            } else {
                client_send_ctrl_msg(client, "200 Okay");
            }
            break;
        case 'I':
            client_send_ctrl_msg(client, "200 Okay");
            break;
        case 'E':
        case 'L':
        default:
            client_send_ctrl_msg(client, "504 Error: command not implemented with this parameter");
            break;
        }
    } else {
        client_send_ctrl_msg(client, "504 Error: bad parameters?");
    }
    return 1;
}


int cmd_PASV_func(struct ftpsp_client *client)
{
    return ftpsp_start_pasv(client);    
}


static const char *num_to_month[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static int parse_ls_format(char *out, int n, int dir, unsigned int file_size, int month_n, int day_n, int hour, int minute, char *filename)
{
    return snprintf (out, n, 
                    "%c%s 1 psp psp %d %s %-2d %02d:%02d %s\n",
                     dir?'d':'-',
                     dir?"rwxr-xr-x":"rw-r--r--",
                     file_size,
                     num_to_month[(month_n-1)%12],
                     day_n,
                     hour,
                     minute,
                     filename);
}


static int get_ms_path(char *ms_path, const char *path)
{
    return sprintf(ms_path, "ms0:%s", path);
}

static int send_LIST(struct ftpsp_client *client, const char *path)
{
    //client_send_ctrl_msg(client->data_sock, "-rw-r--r--  1 xerpi xerpi 103004 feb 10 19:33 EBOOT.PBP");
    ftpsp_open_data(client);
    client_send_ctrl_msg(client, "150 Opening ASCII mode data transfer for LIST");
    
    char ms_path[PATH_MAX+4];
    get_ms_path(ms_path, path);
    printf("ms path: %s\n", ms_path);
    
    SceUID dir = sceIoDopen(ms_path);
    SceIoDirent dirent;
    memset(&dirent, 0, sizeof(dirent));

    
    while (sceIoDread(dir, &dirent) > 0) {
        parse_ls_format(client->wr_buffer, BUF_SIZE,
                        FIO_S_ISDIR(dirent.d_stat.st_mode),
                        dirent.d_stat.st_size,
                        dirent.d_stat.st_ctime.month,
                        dirent.d_stat.st_ctime.day,
                        dirent.d_stat.st_ctime.hour,
                        dirent.d_stat.st_ctime.minute,
                        dirent.d_name);
        client_send_data_msg(client, client->wr_buffer);
        memset(&dirent, 0, sizeof(dirent));
        memset(client->wr_buffer, 0, BUF_SIZE);
    }
    
    sceIoDclose(dir);
    
    client_send_ctrl_msg(client, "226 Transfer complete");
    ftpsp_close_data(client);
    return 1;
}

int cmd_LIST_func(struct ftpsp_client *client)
{
    char path[PATH_MAX];
    int n = sscanf(client->rd_buffer, "%*[^ ] %[^\r\n\t]", path);
    if (n > 0) {  /* Client specified a path */
        send_LIST(client, path);
    } else { /* Use current path */
        send_LIST(client, client->cur_path);
    }
    return 1;   
}

int cmd_CWD_func(struct ftpsp_client *client)
{
    char path[PATH_MAX];
    int n = sscanf(client->rd_buffer, "%*[^ ] %[^\r\n\t]", path);
    if (n < 1) {
        client_send_ctrl_msg(client, "500 Syntax error, command unrecognized");
    } else {
        if (strchr(path, '/') == NULL) { //Change dir relative to current dir
            if (client->cur_path[strlen(client->cur_path) - 1] != '/')
                strcat(client->cur_path, "/");
            strcat(client->cur_path, path);
        } else {
            strcpy(client->cur_path, path);
        }
        client_send_ctrl_msg(client, "250 Requested file action okay, completed.");
    }
    return 1;
}


static int dir_up(const char *in, char *out)
{
    char *pch = strrchr(in, '/');
    if (pch && pch != in) {
        size_t s = pch - in;
        strncpy(out, in, s);
        out[s] = '\0';
    } else {
        strcpy(out, "/");
    }
    return 1;
}

int cmd_CDUP_func(struct ftpsp_client *client)
{
    int s_len = strlen(client->cur_path)+1;
    char buf[s_len];
    memcpy(buf, client->cur_path, s_len);
    dir_up(buf, client->cur_path);
    client_send_ctrl_msg(client, "200 Command okay.");
    return 1;
}


static int send_file(struct ftpsp_client *client, const char *path)
{
    char ms_path[PATH_MAX+4];
    get_ms_path(ms_path, path);
    printf("RETR ms path: %s\n", ms_path);
    SceUID fd;
    if ((fd = sceIoOpen(ms_path, PSP_O_RDONLY, 0777)) >= 0) {
        ftpsp_open_data(client);
        client_send_ctrl_msg(client, "150 Opening Image mode data transfer for LIST");
        
        unsigned int bytes_read;
        while ((bytes_read = sceIoRead (fd, client->wr_buffer, BUF_SIZE)) > 0) {
            send(client->data_sock, client->wr_buffer, bytes_read, 0);
        }

        sceIoClose(fd);
        client_send_ctrl_msg(client, "226 Transfer complete");
        ftpsp_close_data(client);
        
    } else {
        client_send_ctrl_msg(client, "550 File Not Found");
    }
    return 1;
}

int cmd_RETR_func(struct ftpsp_client *client)
{
    char path[PATH_MAX];
    char cur_path[PATH_MAX];
    sscanf(client->rd_buffer, "%*[^ ] %[^\r\n\t]", path);
    strcpy(cur_path, client->cur_path);
    if (strchr(path, '/') == NULL) { //File relative to current dir
        if (cur_path[strlen(cur_path) - 1] != '/')
            strcat(cur_path, "/");
        strcat(cur_path, path);
    } 
    send_file(client, cur_path);
    return 1;       
}

