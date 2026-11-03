#include "../../src/vk/lvk.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static void rec_fill(VkCommandBuffer cb, void *u) { (void)cb; (void)u; }
int main(void){
  if(!lvk_init()){printf("FAIL lvk_init\n");return 1;}
  const lvk_caps_t*c=lvk_caps();
  printf("dev=%s api=%u.%u memfd=%d semfd=%d fencefd=%d ts=%d bits=%u period=%.3f qf=%u\n",c->device_name,c->api_version>>22,(c->api_version>>12)&0x3ff,c->has_ext_mem_fd,c->has_ext_sem_fd,c->has_ext_fence_fd,c->timestamps,c->timestamp_bits,c->timestamp_period_ns,c->queue_family);
  lvk_buffer b; if(!lvk_buffer_create(&b,4096,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,LVK_BUF_HOST)){printf("FAIL buffer host\n");return 1;}
  memset(b.map,0xAB,4096); printf("buffer host OK map=%p alloc=%llu\n",b.map,(unsigned long long)b.alloc_size);
  lvk_buffer e; if(c->has_ext_mem_fd){ if(lvk_buffer_create(&e,65536,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,LVK_BUF_EXPORT)){int fd=lvk_buffer_export_fd(&e);printf("buffer export OK fd=%d alloc=%llu\n",fd,(unsigned long long)e.alloc_size);if(fd>=0)close(fd);lvk_buffer_destroy(&e);} else printf("buffer export FAIL\n"); }
  int rr=lvk_run(rec_fill,NULL,-1,NULL,1); printf("lvk_run=%d\n",rr);
  if(c->timestamps){
    int s0=lvk_ts_mark(-1); usleep(20000); int s1=lvk_ts_mark(-1);
    uint64_t t0=0,t1=0; int g0=lvk_ts_get(s0,&t0,1), g1=lvk_ts_get(s1,&t1,1);
    printf("ts slots %d %d get %d %d t0=%llu t1=%llu delta=%.3f ms\n",s0,s1,g0,g1,(unsigned long long)t0,(unsigned long long)t1,(double)(t1-t0)/1e6);
    lvk_ts_release(s0); lvk_ts_release(s1);
    if(g0!=1||g1!=1||t1<t0){printf("FAIL ts\n");return 1;}
  }
  lvk_buffer_destroy(&b); lvk_shutdown(); printf("shutdown OK; reinit=%d\n",lvk_init()); lvk_shutdown();
  printf("PASS\n"); return 0;}
