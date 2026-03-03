#ifndef GPS_H
#define GPS_H

#ifdef __cplusplus
extern "C"{
#endif

// 变量声明（实际定义在gps.c中）
extern char dataBuf[100];
extern int OneFramFlag;
extern int OneFramStart;
extern int saveData_location;
extern int gps_fd;
extern const char* gps_device;
extern int gps_valid;

int gps_init();
//int set_gps_async_update();
int analyseRawData();
int getGpsData(char* N_S_Flag,char *E_W_Flag,double *jingdu,double *weidu);
#ifdef __cplusplus
}
#endif

#endif
