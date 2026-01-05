#include "utils/storage.h"
#include "utils/log.h"
#include "config.h"
#include <sys/statvfs.h>

bool is_sdcard_inserted(){
    FILE *f = fopen(SD_DEV_PATH, "r");
    if(f == NULL){
        return false;
    }
    fclose(f);
    return true;
}

uint32_t get_nand_available_size(){
    struct statvfs stat;
    if(statvfs(NAND_MOUNT_POINT, &stat) != 0){
        log_error("failed to get NAND available size");
        return 0;
    }
    return stat.f_bavail * stat.f_bsize;
}
uint32_t get_sd_available_size(){
    struct statvfs stat;
    if(statvfs(SD_MOUNT_POINT, &stat) != 0){
        log_error("failed to get SD available size");
        return 0;
    }
    return stat.f_bavail * stat.f_bsize;
}
uint32_t get_nand_total_size(){
    struct statvfs stat;
    if(statvfs(NAND_MOUNT_POINT, &stat) != 0){
        log_error("failed to get NAND total size");
        return 0;
    }
    return stat.f_blocks * stat.f_bsize;
}
uint32_t get_sd_total_size(){
    struct statvfs stat;
    if(statvfs(SD_MOUNT_POINT, &stat) != 0){
        log_error("failed to get SD total size");
        return 0;
    }
    return stat.f_blocks * stat.f_bsize;
}
int format_sd_card(){
    log_info("formatting SD card");
    return 0;
}