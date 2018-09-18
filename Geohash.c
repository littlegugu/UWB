#include <stdio.h>
// #include <pthread.h>
#include <winsock2.h>
#include <stdlib.h>
#include <unistd.h>	
// #include <sched.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "cJSON.c"

/*
* 函数体
* 1.房间区域储存
*/

typedef struct room{
    int roomNum;
    double xTop;
    double xLow;
    double yTop;
    double yLow;
};


/*
* 声明区
*/


/*
* 初始化
*/
room rooms[10];
main(int argc, char const *argv[])
{
    /* code */
    // areas[0].areaNum = 12;
    // printf("%d",areas[0].areaNum);
    getchar();
    return 0;
}

void init(FILE *fp){

}
