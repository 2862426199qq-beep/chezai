#include "camera_thread.h"
#include <QDebug>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>


CameraThread::CameraThread(QObject *parent)
    : QThread(parent), fd(-1), running(false)
{
}

void CameraThread::stop()
{
    running = false;  // 设置标志，run()循环会退出
}

//打开设备
bool CameraThread::initDevice(){
    fd = open(DEVICE, O_RDWR);
    if(fd < 0){
        qDebug() << "open fail"; 
        return false;
    }

    /*设置格式*/
    /*V4L2核心-rkisp*/
    struct v4l2_format fmt;//设置格式,v4l2_format来源于linux/videodev2.h
    memset(&fmt,0,sizeof(fmt));//清零
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;//多平面
    fmt.fmt.pix_mp.width = WIDTH;//宽
    fmt.fmt.pix_mp.height = HEIGHT;//高
    //imx415 是多平面格式（Y平面+UV平面分开），要用 VIDEO_CAPTURE_MPLANE 而不是普通的 VIDEO_CAPTURE。
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;//这是NV12格式：前 W*H 字节是 Y 平面，后 W*H/2 字节是 UV 交错平面
    fmt.fmt.pix_mp.num_planes = 1;//平面数
    if(ioctl(fd, VIDIOC_S_FMT, &fmt) < 0){
        qDebug() << "S_FMT fail"; 
        return false;
    }

    /*申请4个buff*/
    /*v4l2 -- vediobuf2 内存框架*/
    struct v4l2_requestbuffers req;//申请buffer：buffer是内核用来存放采集到的图像数据的内存块，用户空间通过 mmap 映射来访问这些 buffer。
    memset(&req,0,sizeof(req));
    req.count = BUF_COUNT;//申请4个buffer
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;//多平面
    req.memory = V4L2_MEMORY_MMAP;//内存映射方式
    if(ioctl(fd, VIDIOC_REQBUFS, &req) < 0){
        qDebug() << "REQBUFS fail";
        return false;
    }

    /*maap映射*/
    /*映射到刚刚申请的buffer*/
    for(int i = 0; i < BUF_COUNT; i++){
        struct v4l2_plane planes[1];
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        memset(planes, 0, sizeof(planes));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;//多平面
        buf.memory = V4L2_MEMORY_MMAP;//内存映射方式
        buf.index = i;//第i个buffer
        buf.m.planes = planes;//平面信息
        buf.length = 1;//平面数
        if(ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0){
            qDebug() << "QUERYBUF fail";
            return false;
        }
        buffers[i].length = buf.m.planes[0].length;//buffer长度,panles[0]是Y平面，长度是W*H，panles[1]是UV平面，长度是W*H/2
        buffers[i].start = mmap(NULL,//映射地址由内核选择,NULL表示让内核自动选取地址
             buffers[i].length, //映射长度
             PROT_READ | PROT_WRITE,//映射权限，读写
              MAP_SHARED, //共享映射，多个进程可以看到同一块内存
              fd, //文件描述符
              buf.m.planes[0].m.mem_offset);//buf.m.planes[0].m.mem_offset是内核分配的buffer在物理内存中的偏移地址，mmap需要这个偏移地址来建立映射关系
        if(buffers[i].start == MAP_FAILED){
            qDebug() << "mmap fail";
            return false;
        }



    }

    /*所有buffer入队*/
    /*v4l2 -- vediobuf2_qbuf -- rkisp驱动的buf_queue 回调*/
   // ✅ 准确的描述：
    // VIDIOC_QBUF：告诉内核"这个buffer我处理完了，还给硬件继续填"
    // 内核把这个buffer重新加入硬件的DMA目标队列
    // 零拷贝体现在：硬件填的物理内存 和 我们mmap读的虚拟地址 是同一块，没有复制
    
    for(int i = 0; i <BUF_COUNT; i++){
        struct v4l2_plane planes[1];
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        memset(planes, 0, sizeof(planes));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;//多平面
        buf.memory = V4L2_MEMORY_MMAP;//内存映射方式
        buf.index = i;//第i个buffer
        buf.m.planes = planes;//平面信息
        buf.length = 1;//平面数
        if(ioctl(fd, VIDIOC_QBUF, &buf) < 0){
            qDebug() << "QBUF fail";
            return false;
        }


    }
    
    /*启动采集*/
    // v4l2 -- vb2_streamon -- rkisp驱动的stream_on回调start_streaming()
    //start_streaming():是RKISP驱动中的一个函数，负责启动视频流的采集。当用户空间调用VIDIOC_STREAMON ioctl命令时，内核会调用这个函数来启动采集过程。
    //在start_streaming()函数中，RKISP驱动会配置硬件寄存器，启动DMA传输，让RKISP硬件开始采集图像数据并写入之前通过QBUF入队的buffer中。这样就完成了从摄像头到
    //内存的图像数据流动，用户空间可以通过之前mmap映射的地址访问这些数据了。
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;//多平面
    if(ioctl(fd, VIDIOC_STREAMON, &type) < 0){
        qDebug() << "STREAMON fail";
        return false;
    }
    qDebug() << "Camera init success";
    return true;
}


//关闭设备
void CameraThread::closeDevice(){
    /*停止采集*/
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;//多平面
    ioctl(fd, VIDIOC_STREAMOFF, &type);//停止采集，内核会调用RKISP驱动的stream_off回调函数stop_streaming()，停止DMA传输和采集过程
    for(int i = 0; i < BUF_COUNT; i++){
        munmap(buffers[i].start, buffers[i].length);//解除mmap映射，释放内核分配的物理内存
    }
    //为什么要解除mmap映射？因为之前我们通过mmap建立了用户空间和内核空间的映射关系，
    // 映射的内存区域是内核分配的物理内存。现在设备关闭了，我们不再需要访问这些内存了，所以要解除映射，释放资源。
    close(fd);//关闭设备文件描述符，释放系统资源
    fd = -1;//重置文件描述符
}

//NV12转QImage
//NV12格式：前 W*H 字节是 Y 平面，后 W*H/2 字节是 UV 交错平面
//什么是NV12格式？NV12是一种常见的YUV420格式，适用于视频采集和编码。它由两个平面组成：
// 第一个平面是Y平面，包含每个像素的亮度信息，占用W*H字节；
// 第二个平面是UV平面，包含每个像素的色度信息，占用W*H/2字节。
// UV平面中的U和V分量交错存储，每两个像素共享一组UV值。
QImage CameraThread::nv12ToQImage(void *data, int width, int height){
    QImage img(width, height, QImage::Format_RGB888);//创建一个RGB888格式的QImage对象
    uint8_t *nv12 = (uint8_t *)data;
    uint8_t *y_plane = nv12;
    uint8_t *uv_plane = nv12 + width * height;  // UV紧跟在Y后面

    for(int row = 0; row < height ;row++){
        uint8_t *line = img.scanLine(row);  // 拿到这行的原始指针
        for (int col = 0; col < width; col++) // 遍历每个像素
        {
            int y = y_plane[row * width + col]; // Y值
            int uv_index = (row / 2) * width + (col & ~1); // UV索引，注意UV是2x2下采样的
            int u = uv_plane[uv_index] - 128; // U值，中心化到[-128,127]
            int v = uv_plane[uv_index + 1] - 128; // V值，中心化到[-128,127]

            // YUV转RGB公式
            //RGB是一种颜色空间，表示红绿蓝三种基色的强度。YUV是一种颜色空间，表示亮度（Y）和色度（U和V）。要将NV12格式的YUV数据转换为RGB格式，需要使用以下公式：
            // R = Y + 1.402 * V
            // G = Y - 0.344 * U - 0.714 * V
            // B = Y + 1.772 * U
            // 这里使用qBound函数确保RGB值在[0,255]范围内
            //R是红色分量，G是绿色分量，B是蓝色分量。
            // 通过这个公式，我们可以根据每个像素的Y、U、V值计算出对应的RGB值，然后存储到QImage中，最终得到一帧可显示的图像。
            line[col * 3]     = qBound(0, (int)(y + 1.402f * v), 255); // R
            line[col * 3 + 1] = qBound(0, (int)(y - 0.344f * u - 0.714f * v), 255); // G
            line[col * 3 + 2] = qBound(0, (int)(y + 1.772f * u), 255); // B
        }

    }
    return img;
}

//线程主函数
void CameraThread::run(){
    if(!initDevice()) return; // 初始化设备失败，退出线程
    running = true; // 设置运行标志,running是一个volatile bool类型的成员变量，用于控制线程的运行状态。
                    //当running为true时，线程继续采集和处理图像；当running被外部调用stop()函数设置为false时，
                    // run()函数中的循环会检测到这个变化并退出，从而停止线程的执行。
    while(running){
        struct v4l2_buffer buf;//定义一个v4l2_buffer结构体变量，用于存储从内核获取的缓冲区信息
        struct v4l2_plane planes[1];//定义一个v4l2_plane数组，用于存储多平面格式的缓冲区信息，这里只需要一个平面，因为我们使用的是NV12格式，只有一个UV平面
        memset(&buf, 0, sizeof(buf));//清零
        memset(planes, 0, sizeof(planes));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;//多平面
        buf.memory = V4L2_MEMORY_MMAP;//内存映射方式
        buf.m.planes = planes;//平面信息
        buf.length = 1;//平面数
        
        // 1. 取一帧（阻塞等待硬件填满）
        // ioctl(fd, VIDIOC_DQBUF, &buf) 是一个阻塞调用
        if(ioctl(fd, VIDIOC_DQBUF, &buf) < 0){
            qDebug() << "DQBUF fail";
            break;
        }
        // 2. 转换格式并发信号UI
        QImage img = nv12ToQImage(buffers[buf.index].start, WIDTH, HEIGHT);
        emit frameReady(img); // 通过信号传给主线程显示
        // 3. 处理完，把buffer还回去继续采集
        if(ioctl(fd, VIDIOC_QBUF, &buf) < 0){
            qDebug() << "QBUF fail";
            break;
        }
        //VIDIOC_DQBUF和VIDIOC_QBUF区别是
        //VIDIOC_DQBUF：从内核队列中取出一个已经填充好数据的缓冲区，供用户空间处理。这个调用会阻塞，直到有数据可用。
        //VIDIOC_QBUF：将一个已经处理完的缓冲区重新放回内核队列，供内核再次使用。这个调用不会阻塞。
    }   
    closeDevice(); // 关闭设备，释放资源
}