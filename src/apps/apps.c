#include "apps.h"
#include "utils/log.h"
#include "utils/timer.h"
#include <apps/apps_cfg_parse.h>
#include <apps/extmap.h>
#include <config.h>
#include <errno.h>
#include <signal.h>
#include <ui/actions_warning.h>
#include <unistd.h>
#include <utils/misc.h>

static void apps_bg_app_check_timer_cb(void *userdata, bool is_last){
    apps_t *apps = (apps_t *)userdata;
    for(int i = 0; i < apps->app_count; i++){
        if(apps->apps[i].type == APP_TYPE_BACKGROUND){
            if(apps->apps[i].pid != -1){
                // if pid still alive
                if(kill(apps->apps[i].pid, 0) == 0){
                    continue;
                }
                else{
                    // pid is not alive
                    apps->apps[i].pid = -1;
                }
            }
        }
    }
}

int apps_init(apps_t *apps,bool use_sd){
    apps->app_count = 0;

    apps->parse_log_f = fopen(APPS_PARSE_LOG, "w");
    if(apps->parse_log_f == NULL){
        log_error("failed to open parse log file: %s", APPS_PARSE_LOG);
    }

    int errcnt = apps_cfg_scan(apps, APPS_DIR,APP_SOURCE_NAND);

    if(use_sd){
        log_info("==> Apps will scan SD directory: %s", APPS_DIR_SD);
        errcnt += apps_cfg_scan(apps, APPS_DIR_SD,APP_SOURCE_SD);
    }

    if(errcnt != 0){
        log_warn("failed to load apps, error count: %d", errcnt);
        ui_warning(UI_WARNING_APP_LOAD_ERROR);
    }

#ifndef APP_RELEASE
    for(int i = 0; i < apps->app_count; i++){
        apps_cfg_log_entry(&apps->apps[i]);
    }
    apps_extmap_log_entry(&apps->extmap);
#endif // APP_RELEASE

    prts_timer_create(
        &apps->bg_app_check_timer, 
        0, APPS_BG_APP_CHECK_PERIOD, -1, 
        apps_bg_app_check_timer_cb, apps);
    return 0;
}

int apps_destroy(apps_t *apps){
    apps->app_count = 0;
    fclose(apps->parse_log_f);
    return 0;
}

extern int g_exitcode;
extern int g_running;

static int launch_app_foreground(const char* basename, const char* working_dir,const char* args){
    if(!basename || !working_dir) return -1;

    char exec[128];
    join_path(exec, sizeof(exec), working_dir, basename);
    if(!file_exists_readable(exec)){
        log_error("executable file not found: %s", exec);
        return -1;
    }

    FILE * fp = fopen("/tmp/appstart", "w");
    if(!fp) {
        log_error("unable to write to /tmp/appstart???: %s", strerror(errno));
        return -1;
    }

    fprintf(fp, "#!/bin/sh\n");
    fprintf(fp, "chmod +x %s\n", exec);
    fprintf(fp, "cd %s\n", working_dir);
    if(args && args[0] != '\0'){
        fprintf(fp, "%s %s\n", exec, args);
    }
    else{
        fprintf(fp, "%s\n", exec);
    }
 
    fflush(fp);
    fsync(fileno(fp));
    fclose(fp);

    log_info("/tmp/appstart written: %s %s %s", exec, working_dir, args);
    g_exitcode = EXITCODE_APPSTART;
    g_running = 0;
    return 0;
}

int apps_try_launch_by_file(apps_t *apps,const char* working_dir,const char *basename){
    if(!working_dir || !basename) return -1;

    char ext[10];
    // 提取文件扩展名
    const char *dot = strrchr(basename, '.');
    if(dot && *(dot + 1) != '\0'){
        strncpy(ext, dot, sizeof(ext) - 1);
        ext[sizeof(ext) - 1] = '\0';
    } else {
        ext[0] = '\0';
    }

    if(ext[0] == '\0'){
        log_info("no extension found, launch app foreground: %s", basename);
        return launch_app_foreground(basename, working_dir, NULL);
    }

    app_entry_t *app = NULL;
    apps_extmap_get(&apps->extmap, ext, &app);
    if(!app){
        log_info("no extension found, launch app foreground: %s", basename);
        return launch_app_foreground(basename, working_dir, NULL);
    }

    const char *app_basename = path_basename(app->executable_path);
    if(!app_basename){
        log_error("unable to get basename of %s", app->executable_path);
        return -1;
    }
    char file_full_path[128];
    join_path(file_full_path, sizeof(file_full_path), working_dir, basename);
    if(!file_exists_readable(file_full_path)){
        log_error("file not found: %s", file_full_path);
        return -1;
    }

    log_info("launch app foreground: %s,file: %s", app->executable_path, file_full_path);
    return launch_app_foreground(app_basename, app->app_dir, file_full_path);
}
