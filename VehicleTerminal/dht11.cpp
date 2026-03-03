#include "dht11.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

Dht11::Dht11()
{
    printf("Dht11: constructor\n");
    fflush(stdout);
}

void Dht11::run()
{
   QString humidity, temp;

#if (defined(__arm__) || defined(__aarch64__)) && !defined(DISABLE_HARDWARE)
   unsigned char date[5]={0};
   unsigned char tmp=0;
   int ret;
   int fd = open("/dev/dht11",O_RDWR);
   if(fd<0)
   {
       qDebug()<<"can't open /dev/dht11, using simulated data";
       // 打开失败时使用模拟数据
       while(1)
       {
           humidity = "25.0";
           temp = "28.0";
           emit updateDht11Data(humidity, temp);
           usleep(500000);
       }
       return;
   }

   while (1) {
       if((ret = read(fd, date, sizeof(date))) != sizeof(date))
       {
           continue;
       }
       tmp = 0;
       tmp+=date[0];
       tmp+=date[1];
       tmp+=date[2];
       tmp+=date[3];
       if(tmp==date[4])
       {
           humidity = QString("%1.%2").arg(date[0]).arg(date[1]);
           temp = QString("%1.%2").arg(date[2]).arg(date[3]);
       }
       else
       {
           humidity = "error";
           temp = "error";
       }
       emit updateDht11Data(humidity, temp);
       usleep(500000);
   }
#else
    // DISABLE_HARDWARE模式或非ARM平台，使用模拟数据
    while(1)
    {
        humidity = "25.0";
        temp = "28.0";
        emit updateDht11Data(humidity, temp);
        usleep(500000);
    }
#endif
}
