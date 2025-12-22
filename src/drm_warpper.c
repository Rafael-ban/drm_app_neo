#include "drm_warpper.h"
#include <drm_fourcc.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <sys/mman.h>
#include <unistd.h>
#include "log.h"

static inline int DRM_IOCTL(int fd, unsigned long cmd, void *arg) {
  int ret = drmIoctl(fd, cmd, arg);
  return ret < 0 ? -errno : ret;
}

static void page_flip_handler(int fd, unsigned int sequence, unsigned int tv_sec,
    unsigned int tv_usec, void *user_data)
{
    drm_warpper_t *drm_warpper = (drm_warpper_t *)user_data;
    log_debug("flip happened here....");
    if(drm_warpper->req) {
        drmModeAtomicFree(drm_warpper->req);
        drm_warpper->req = NULL;
    }
}

static void drm_warpper_wait_for_vsync(drm_warpper_t *drm_warpper){
    int ret;
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(drm_warpper->fd, &fds);

    do {
		ret = select(drm_warpper->fd + 1, &fds, NULL, NULL, NULL);
	} while (ret == -1 && errno == EINTR);

    if (ret < 0) {
		log_error("select failed: %s", strerror(errno));
		drmModeAtomicFree(drm_warpper->req);
		drm_warpper->req = NULL;
		return;
	}

	if (FD_ISSET(drm_warpper->fd, &fds)){   
         // block here until vsync
		drmHandleEvent(drm_warpper->fd, &drm_warpper->drm_event_ctx);
    }

	drmModeAtomicFree(drm_warpper->req);
	drm_warpper->req = NULL;
}

static void* drm_warpper_display_thread(void *arg){
    drm_warpper_t *drm_warpper = (drm_warpper_t *)arg;
    bool need_flip = false;
    int ret,i;

    log_info("display thread started");
    buffer_object_display_list_t switch_list;
    while(drm_warpper->thread_running){
        need_flip = false;
        switch_list.display_cnt = 0;
        switch_list.free_cnt = 0;
        drm_warpper->req = drmModeAtomicAlloc();
        for(int layer_id = 0; layer_id < 4; layer_id++){
            if(drm_warpper->plane[layer_id].used){
                buffer_object_t* buf;
                bool res = try_dequeue(&drm_warpper->plane[layer_id].ready_queue, &buf);
                if(res){
                    need_flip = true;
                    drmModeAtomicAddProperty(
                        drm_warpper->req, 
                        drm_warpper->plane_res->planes[layer_id], 
                        drm_warpper->plane[layer_id].fb_prop_id, 
                        buf->fb_id
                    );

                    switch_list.display_buf[switch_list.display_cnt++] = buf;

                    for(int i = 0; i < 2; i++){
                        if(&drm_warpper->plane[layer_id].buf[i] != buf){
                            switch_list.free_buf[switch_list.free_cnt++] = &drm_warpper->plane[layer_id].buf[i];
                        }
                    }

                }
            }
        }
        if(need_flip){
            ret = drmModeAtomicCommit(drm_warpper->fd, drm_warpper->req, DRM_MODE_PAGE_FLIP_EVENT,drm_warpper);
            if(ret < 0){
                log_error("drmModeAtomicCommit failed %s(%d)", strerror(errno), errno);
                drmModeAtomicFree(drm_warpper->req);
                drm_warpper->req = NULL;
            }
            
            drm_warpper_wait_for_vsync(drm_warpper);
            for(i = 0; i < switch_list.display_cnt; i++){
                switch_list.display_buf[i]->state = DRM_WARPPER_BUFFER_STATE_DISPLAYING;
            }
            for(i = 0; i < switch_list.free_cnt; i++){
                switch_list.free_buf[i]->state = DRM_WARPPER_BUFFER_STATE_FREE;
                sem_post(switch_list.free_buf[i]->related_free_sem);
            }
        }
        else{
            drmModeAtomicFree(drm_warpper->req);
            drm_warpper->req = NULL;
        }
        usleep(10000);
    }
    return NULL;
}

int drm_warpper_arquire_draw_buffer(drm_warpper_t *drm_warpper,int layer_id,uint8_t **vaddr){
    sem_wait(&drm_warpper->plane[layer_id].free_sem);
    for(int i = 0; i < 2; i++){
        if(drm_warpper->plane[layer_id].buf[i].state == DRM_WARPPER_BUFFER_STATE_FREE){
            *vaddr = drm_warpper->plane[layer_id].buf[i].vaddr;
            drm_warpper->plane[layer_id].buf[i].state = DRM_WARPPER_BUFFER_STATE_DRAWING;
            return 0;
        }
    }
    log_error("failed to acquire draw buffer, but free semaphore is not 0?");
    return -1;
}

int drm_warpper_return_draw_buffer(drm_warpper_t *drm_warpper,int layer_id, uint8_t* vaddr){
    buffer_object_t *buf;

    for(int i = 0; i < 2; i++){
        buf = &drm_warpper->plane[layer_id].buf[i];
        if(buf->vaddr == vaddr){
            buf->state = DRM_WARPPER_BUFFER_STATE_READY;
            enqueue(&drm_warpper->plane[layer_id].ready_queue, &buf);
            return 0;
        }
    }
    log_error("failed to return draw buffer, but vaddr is not found?");
    return -1;
}

int drm_warpper_init(drm_warpper_t *drm_warpper){
    int ret;

    memset(drm_warpper, 0, sizeof(drm_warpper_t));

    drm_warpper->fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (drm_warpper->fd < 0) {
        log_error("open /dev/dri/card0 failed");
        return -1;
    }

    ret = drmSetClientCap(drm_warpper->fd, DRM_CLIENT_CAP_ATOMIC, 1);
    if(ret) {
        log_error("No atomic modesetting support: %s", strerror(errno));
        return -1;
    }
    
    drm_warpper->res = drmModeGetResources(drm_warpper->fd);
    drm_warpper->crtc_id = drm_warpper->res->crtcs[0];
    drm_warpper->conn_id = drm_warpper->res->connectors[0];
    
    drmSetClientCap(drm_warpper->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    ret = drmSetClientCap(drm_warpper->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    if (ret) {
      log_error("failed to set client cap\n");
      return -1;
    }
    drm_warpper->plane_res = drmModeGetPlaneResources(drm_warpper->fd);
    log_info("Available Plane Count: %d", drm_warpper->plane_res->count_planes);

    drm_warpper->conn = drmModeGetConnector(drm_warpper->fd, drm_warpper->conn_id);

    log_info("Connector Name: %s, %dx%d, Refresh Rate: %d",
        drm_warpper->conn->modes[0].name, drm_warpper->conn->modes[0].vdisplay, drm_warpper->conn->modes[0].hdisplay,
        drm_warpper->conn->modes[0].vrefresh);

    drm_warpper->drm_event_ctx.version = DRM_EVENT_CONTEXT_VERSION;
    drm_warpper->drm_event_ctx.page_flip_handler = page_flip_handler;

    drm_warpper->thread_running = true;
    pthread_create(&drm_warpper->display_thread, NULL, drm_warpper_display_thread, drm_warpper);
    return 0;
}

int drm_warpper_destroy(drm_warpper_t *drm_warpper){
    drmModeFreeConnector(drm_warpper->conn);
    drmModeFreePlaneResources(drm_warpper->plane_res);
    drmModeFreeResources(drm_warpper->res);
    close(drm_warpper->fd);
    drm_warpper->thread_running = false;
    pthread_join(drm_warpper->display_thread, NULL);
    return 0;
}

static int drm_warpper_create_buffer_object(int fd,buffer_object_t* bo,int width,int height,drm_warpper_layer_mode_t mode){
    struct drm_mode_create_dumb creq;
    struct drm_mode_map_dumb mreq;
    uint32_t handles[4], pitches[4], offsets[4];
    uint64_t modifiers[4];
    int ret;
 
    memset(&creq, 0, sizeof(struct drm_mode_create_dumb));
    if(mode == DRM_WARPPER_LAYER_MODE_MB32_NV12){
        creq.width = width;
        creq.height = height * 3 / 2;
        creq.bpp = 8;
    }
    else if(mode == DRM_WARPPER_LAYER_MODE_RGB565){
        creq.width = width;
        creq.height = height;
        creq.bpp = 16;
    }
    else if(mode == DRM_WARPPER_LAYER_MODE_ARGB8888){
        creq.width = width;
        creq.height = height;
        creq.bpp = 32;
    }
    else{
        log_error("invalid layer mode");
        return -1;
    }


    ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
    if (ret < 0) {
      log_error("cannot create dumb buffer (%d): %m", errno);
      return -errno;
    }
  
    memset(&offsets, 0, sizeof(offsets));
    memset(&handles, 0, sizeof(handles));
    memset(&pitches, 0, sizeof(pitches));
    memset(&modifiers, 0, sizeof(modifiers));

    if(mode == DRM_WARPPER_LAYER_MODE_MB32_NV12){
        offsets[0] = 0;
        handles[0] = creq.handle;
        pitches[0] = creq.pitch;
        modifiers[0] = DRM_FORMAT_MOD_ALLWINNER_TILED;
      
        offsets[1] = creq.pitch * height;
        handles[1] = creq.handle;
        pitches[1] = creq.pitch;
        modifiers[1] = DRM_FORMAT_MOD_ALLWINNER_TILED;
    }
    else{
        offsets[0] = 0;
        handles[0] = creq.handle;
        pitches[0] = creq.pitch;
        modifiers[0] = 0;
    }

    if(mode == DRM_WARPPER_LAYER_MODE_MB32_NV12){
        ret = drmModeAddFB2WithModifiers(fd, width, height, DRM_FORMAT_NV12, handles,
                                     pitches, offsets, modifiers, &bo->fb_id,
                                     DRM_MODE_FB_MODIFIERS);
    }
    else if(mode == DRM_WARPPER_LAYER_MODE_RGB565){
        ret = drmModeAddFB2(fd, width, height, DRM_FORMAT_RGB565, handles, pitches, offsets,&bo->fb_id, 0);
    }
    else if(mode == DRM_WARPPER_LAYER_MODE_ARGB8888){
        ret = drmModeAddFB2(fd, width, height, DRM_FORMAT_ARGB8888, handles, pitches, offsets,&bo->fb_id, 0);
    }
  
    if (ret) {
      log_error("drmModeAddFB2 return err %d", ret);
      return -1;
    }
    
    /* prepare buffer for memory mapping */
    memset(&mreq, 0, sizeof(mreq));
    mreq.handle = creq.handle;
    ret = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
    if (ret) {
      log_error("1st cannot map dumb buffer (%d): %m\n", errno);
      return -1;
    }
    /* perform actual memory mapping */
    bo->vaddr = mmap(0, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);   

    if (bo->vaddr == MAP_FAILED) {
        log_error("2nd cannot mmap dumb buffer (%d): %m\n", errno);
      return -1;
    }

    return 0;
}

static int drm_warpper_get_prop_id(drm_warpper_t *drm_warpper,int layer_id,const char *prop_name){
    drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(drm_warpper->fd, drm_warpper->plane_res->planes[layer_id], DRM_MODE_OBJECT_PLANE);
    int i = 0;
    for (i = 0; i < (int)props->count_props; ++i) {
        drmModePropertyPtr prop = drmModeGetProperty(drm_warpper->fd, props->props[i]);
        log_info("property %s, id: %d", prop->name, prop->prop_id);
        if (strcmp(prop->name, prop_name) == 0) {
            log_info("found property %s, id: %d", prop_name, prop->prop_id);
            return prop->prop_id;
        }
    }
    log_error("failed to find property %s", prop_name);
    return -1;
}

int drm_warpper_init_layer(drm_warpper_t *drm_warpper,int layer_id,int width,int height,drm_warpper_layer_mode_t mode){

    plane_t* plane = &drm_warpper->plane[layer_id];

    plane->buf[0].width = width;
    plane->buf[0].height = height;
    plane->buf[1].width = width;
    plane->buf[1].height = height;
    
    drm_warpper_create_buffer_object(drm_warpper->fd, &plane->buf[0], width, height, mode);
    drm_warpper_create_buffer_object(drm_warpper->fd, &plane->buf[1], width, height, mode);

    plane->fb_prop_id = drm_warpper_get_prop_id(drm_warpper, layer_id, "FB_ID");

    sem_init(&plane->free_sem, 0, 2);
    plane->buf[0].related_free_sem = &plane->free_sem;
    plane->buf[1].related_free_sem = &plane->free_sem;
    plane->used = true;

    memset(&plane->ready_queue, 0, sizeof(queue_t));

    return 0;
}


int drm_warpper_destroy_layer(drm_warpper_t *drm_warpper,int layer_id){
    struct drm_mode_destroy_dumb destroy;

    memset(&destroy, 0, sizeof(struct drm_mode_destroy_dumb));

    drmModeRmFB(drm_warpper->fd, drm_warpper->plane[layer_id].buf[0].fb_id);
    munmap(drm_warpper->plane[layer_id].buf[0].vaddr, drm_warpper->plane[layer_id].buf[0].size);

    destroy.handle = drm_warpper->plane[layer_id].buf[0].handle;
    drmIoctl(drm_warpper->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);

    drmModeRmFB(drm_warpper->fd, drm_warpper->plane[layer_id].buf[1].fb_id);
    munmap(drm_warpper->plane[layer_id].buf[1].vaddr, drm_warpper->plane[layer_id].buf[1].size);

    destroy.handle = drm_warpper->plane[layer_id].buf[1].handle;
    drmIoctl(drm_warpper->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);

    sem_destroy(&drm_warpper->plane[layer_id].free_sem);
    drm_warpper->plane[layer_id].used = false;

    return 0;
}

int drm_warpper_get_layer_buffer(drm_warpper_t *drm_warpper,int layer_id,uint8_t **vaddr){
    sem_wait(&drm_warpper->plane[layer_id].free_sem);
    for(int i = 0; i < 2; i++){
        if(drm_warpper->plane[layer_id].buf[i].state == DRM_WARPPER_BUFFER_STATE_FREE){
            *vaddr = drm_warpper->plane[layer_id].buf[i].vaddr;
            drm_warpper->plane[layer_id].buf[i].state = DRM_WARPPER_BUFFER_STATE_DRAWING;
            return 0;
        }
    }
    log_error("failed to get layer buffer, but free semaphore is not 0?");
    return -1;
}

int drm_warpper_return_layer_buffer(drm_warpper_t *drm_warpper,int layer_id,int buf_id){
    drm_warpper->plane[layer_id].buf[buf_id].state = DRM_WARPPER_BUFFER_STATE_READY;
    enqueue(&drm_warpper->plane[layer_id].ready_queue, &buf_id);
    return 0;
}


int drm_warpper_mount_layer(drm_warpper_t *drm_warpper,int layer_id,int x,int y){
    int ret;
    sem_wait(&drm_warpper->plane[layer_id].free_sem);
    drm_warpper->plane[layer_id].buf[0].state = DRM_WARPPER_BUFFER_STATE_DISPLAYING;
    ret = drmModeSetPlane(drm_warpper->fd, 
        drm_warpper->plane_res->planes[layer_id], 
        drm_warpper->crtc_id, 
        drm_warpper->plane[layer_id].buf[0].fb_id, 
        0,
        x, y, 
        drm_warpper->plane[layer_id].buf[0].width, drm_warpper->plane[layer_id].buf[0].height, 
        0, 0,
        (drm_warpper->plane[layer_id].buf[0].width) << 16, (drm_warpper->plane[layer_id].buf[0].height) << 16
    );
    if (ret < 0)
        log_error("drmModeSetPlane err %d", ret);
    return 0;
}


int drm_warpper_get_layer_buffer_legacy(drm_warpper_t *drm_warpper,int layer_id){
    return drm_warpper->plane[layer_id].buf[0].vaddr;
}

