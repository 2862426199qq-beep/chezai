#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#define DEVICE "/dev/video11"
#define W 1920
#define H 1080
#define NBUF 4
#define WARM 30  // 预热帧数
struct buf { void *start; size_t len; };
int main(){
    int fd = open(DEVICE, O_RDWR);
    struct v4l2_format fmt = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE };
    fmt.fmt.pix_mp.width = W; fmt.fmt.pix_mp.height = H;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12; fmt.fmt.pix_mp.num_planes = 1;
    ioctl(fd, VIDIOC_S_FMT, &fmt);
    struct v4l2_requestbuffers req = {.count=NBUF,.type=V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,.memory=V4L2_MEMORY_MMAP};
    ioctl(fd, VIDIOC_REQBUFS, &req);
    struct buf bufs[NBUF];
    for(int i=0;i<NBUF;i++){
        struct v4l2_plane p[1]; struct v4l2_buffer b={.type=req.type,.memory=req.memory,.index=i,.m.planes=p,.length=1};
        ioctl(fd,VIDIOC_QUERYBUF,&b); bufs[i].len=p[0].length;
        bufs[i].start=mmap(NULL,p[0].length,PROT_READ|PROT_WRITE,MAP_SHARED,fd,p[0].m.mem_offset);
        ioctl(fd,VIDIOC_QBUF,&b);
    }
    enum v4l2_buf_type t=V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE; ioctl(fd,VIDIOC_STREAMON,&t);
    printf("Warming up %d frames...\n",WARM);
    for(int n=0;n<WARM;n++){
        struct v4l2_plane p[1]; struct v4l2_buffer b={.type=req.type,.memory=req.memory,.m.planes=p,.length=1};
        ioctl(fd,VIDIOC_DQBUF,&b);
        if(n==WARM-1){ FILE*f=fopen("warm.yuv","wb"); fwrite(bufs[b.index].start,p[0].bytesused,1,f); fclose(f); printf("Saved warm.yuv (%d bytes)\n",p[0].bytesused); }
        ioctl(fd,VIDIOC_QBUF,&b);
    }
    ioctl(fd,VIDIOC_STREAMOFF,&t); close(fd); return 0;
}
