/**
为什么要 mmap？ 不直接 read() 取数据？
为什么申请4个buffer？ 1个不行吗？
QBUF 和 DQBUF 的关系是什么？ 数据怎么流转的
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>  // V4L2 标准头文件，所有ioctl命令定义在这里

#define DEVICE      "/dev/video11"  // 摄像头设备节点，昨天 Bring-up 确认的
#define WIDTH       1920            // 分辨率宽
#define HEIGHT      1080            // 分辨率高
#define BUF_COUNT   4               // 申请4个buffer，形成环形队列

// 用来保存 mmap 映射后的 buffer 信息
// start: 映射到用户态的地址（你能直接读的内存地址）
// length: buffer 大小
struct buffer {
    void   *start;
    size_t  length;
};

int main(){
    int fd ;
    struct v4l2_capability cap; //设备能力信息
    struct v4l2_format fmt;     //视频数据格式（分辨率、像素格式等）
    struct v4l2_requestbuffers req; //申请 buffer 时的参数
    struct v4l2_buffer buf;      //每个 buffer 的信息（索引、类型、内存类型等）
    struct buffer buffers[BUF_COUNT]; //保存 mmap 映射后的 buffer 信息
    enum v4l2_buf_type type; //buffer 类型

    // 1. 打开设备
    fd = open(DEVICE, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return -1;  
    }
    printf("Device opened successfully\n");

    // 2. 查询设备能力 VIDIOC_QUERYCAP , 确认是否支持视频采集 
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        perror("Failed to query device capabilities");
        return -1;
    }
    printf("Driver Name: %s\n", cap.driver);//驱动名 比如rkisp_v6
    printf("Card Name: %s\n", cap.card);    //设备名 比如rkisp_mainpath

    // 3. 设置视频格式 VIDIOC_S_FMT: 分辨率、像素格式等 
    //NV12 是 YUV420 的一种，Y平面+UV交错平面?
    // MPLANE 是多平面格式，RK ISP 输出用这个?
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE; //多平
    fmt.fmt.pix_mp.width = WIDTH;
    fmt.fmt.pix_mp.height = HEIGHT;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
    fmt.fmt.pix_mp.num_planes = 1 ; // NV12 只有1个平面（Y和UV打包在一起
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        perror("S_FMT failed");
        return -1;
    }
    printf("✓ 设置格式成功: %dx%d NV12\n", WIDTH, HEIGHT);

    // 4. 申请 buffer VIDIOC_REQBUFS
    /// 让内核分配物理内存用来存放图像数据
    // 用 MMAP 模式：内核分配，用户态通过mmap访问
    // 为什么申请4个？形成环形队列，硬件填这个、你读那个，不阻塞
    memset(&req, 0, sizeof(req));//清零
    req.count = BUF_COUNT; //申请4个buffer
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE; //多平
    req.memory = V4L2_MEMORY_MMAP; //MMAP模式
    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        perror("REQBUFS failed");
        return -1;
    }
    printf("✓ 申请 %d 个 buffer 成功\n", req.count);

    // 5. mmap 映射 buffer
    // 内核的 buffer 在物理内存里，用户态程序看不到
    // mmap 把它映射到你的进程地址空间，你就能直接读了
    // 不用把数据从内核拷贝到用户态，这就是"零拷贝"
    for (int i = 0; i < BUF_COUNT; i++) {
        struct v4l2_plane planes[1];//?
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;//?
        buf.m.planes = planes;//?
        buf.length   = 1;//?
         // QUERYBUF: 查询这个buffer的具体信息（大小、偏移量）
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            perror("QUERYBUF failed"); return -1;
        }
        buffers[i].length = buf.m.planes[0].length;//?
        // mmap 把内核buffer映射到用户态
        // mem_offset 是内核buffer的偏移量，mmap用它找到物理内存
        /*
            虚拟地址（你能用的）        物理内存（实际数据）
            buffers[0].start ──────────────→ 内核buf[0]
            buffers[1].start ──────────────→ 内核buf[1]
            buffers[2].start ──────────────→ 内核buf[2]
            buffers[3].start ──────────────→ 内核buf[3]
        */
        buffers[i].start = mmap(NULL,// 让OS帮你选虚拟地址
                                buf.m.planes[0].length,// 映射多大
                                PROT_READ | PROT_WRITE,// 可读可写
                                MAP_SHARED,// 和内核共享同一块物理内存
                                fd,// 通过这个设备文件找到驱动
                                buf.m.planes[0].m.mem_offset);// 内核中那块buffer的偏移
        if (buffers[i].start == MAP_FAILED) {
            perror("mmap failed"); return -1;
        }
    }
    printf("mmap 映射成功\n");

    // 6.buffer 入队 VIDIOC_QBUF
    // 把所有buffer交给驱动/硬件
    // 硬件会把采集到的图像数据填进这些buffer
    for (int i = 0; i < BUF_COUNT; i++) {
        struct v4l2_plane planes[1];
        memset(&buf, 0, sizeof(buf));
        memset(planes, 0, sizeof(planes));
        buf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory   = V4L2_MEMORY_MMAP;
        buf.index    = i;
        buf.m.planes = planes;
        buf.length   = 1;
        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            perror("QBUF failed"); return -1;
        }
    }
    printf("✓ buffer 入队成功\n");

    // =============================================
    // 第七步：开始采集 VIDIOC_STREAMON
    // 这一步才真正让硬件开始工作
    // 摄像头开始曝光，数据开始往buffer里写
    // =============================================
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        perror("STREAMON"); return -1;
    }
    printf("✓ 开始采集\n");

    // =============================================
    // 第八步：取帧 VIDIOC_DQBUF
    // 从队列里取出一个填满数据的buffer
    // 这是阻塞调用：如果硬件还没填完，就等着
    // =============================================
    struct v4l2_plane planes[1];
    memset(&buf, 0, sizeof(buf));
    memset(planes, 0, sizeof(planes));
    buf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory   = V4L2_MEMORY_MMAP;
    buf.m.planes = planes;
    buf.length   = 1;
    if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
        perror("DQBUF"); return -1;
    }
    printf("✓ 取帧成功，大小: %d bytes\n", buf.m.planes[0].bytesused);

    // =============================================
    // 第九步：保存数据
    // buffers[buf.index].start 就是图像数据的起始地址
    // 直接写文件，这就是零拷贝的好处，数据从来没有被复制过
    //零拷贝体现在这里
    // =============================================
    FILE *fp = fopen("capture.yuv", "wb");
    fwrite(buffers[buf.index].start, buf.m.planes[0].bytesused, 1, fp);
    fclose(fp);
    printf("✓ 保存到 capture.yuv\n");

    // =============================================
    // 第十步：停止采集 + 清理
    // STREAMOFF 停止硬件采集
    // munmap 解除内存映射
    // close 关闭设备
    // =============================================
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    ioctl(fd, VIDIOC_STREAMOFF, &type);

    for (int i = 0; i < BUF_COUNT; i++)
        munmap(buffers[i].start, buffers[i].length);
    close(fd);
    printf("✓ 完成\n");
    return 0;

}