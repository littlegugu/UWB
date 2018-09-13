#include <stdio.h>
#include <stdlib.h>

#include <winsock2.h>
#include <unistd.h>	
#include <string.h>
#include <time.h>
#include <math.h>
#include <windef.h>
#include <windows.h>

#define LINE 100
#define COR_X 0.69
#define COR_Y 0.94
#define GEOLEN 20

typedef struct wall{
    double top_x;
	double low_x;
	double top_y;
	double low_y;
    int wall_num;
};


typedef struct room{
    int room_num;
	int door_num;
    double door_top_x;
    double door_low_x;
    double door_top_y;
    double door_low_y;
    double top_x;
    double low_x;
    double top_y;
    double low_y;
    wall walls[6];
};

typedef struct area{
    double top_x;
    double low_x;
    double top_y;
    double low_y;
};


room rooms[5];//声明五个房间
area areas;
// double area[4];
double door_x = 0.0;
double door_y = 0.0;
double wall_x = 0.0;
double wall_y = 0.0;

void split(char * data,double *list);
double * dot(double * a_mat, double * b_mat,int a_row,int b_row);
double * mat_pow(double * a_mat,int a_row,int power);
// void init(void);
double max_f(double max_p,double a_num);
double min_f(double min_p,double a_num);
void geohash_grid(void);
int dichotomy(double * range,double top,double low);
int init(void);
char* byGeohash(char* str, int count,int alpha);
char* geohash(char *str,int xCount,int yCount,int alpha);
int main(int argc, char const *argv[])
{

    init();
    char s[10];
    geohash(s,3,4,4);
    printf("%s\n",s);
    // geohash_grid();//通过geohash全图画栅格
    // printf("%d\n",hh);
    
    // itoa(15, s, 2);
    // printf("%p\n",s);
    // byGeohash(s,3,4);
    // printf("%s\n",s);
    
    getchar();
}


/*二分法*/
int dichotomy(double * range,double top,double low)
{
    double grid_size = (top-low)/2;
    int count = 1;
    while(*(range)>grid_size || grid_size>*(range+1))
    {
        // printf("%f\n",grid_size);
        // Sleep(100000);
        grid_size = grid_size/2;
        count++;
    }
    return count;
}



void geohash_grid(void)
{
    //动态计算精度范围
    double x_range[2]={0,0};//0:下界；1:上界;
    double y_range[2]={0,0};
    int alpha=0;//二分次数
    int xlim,ylim;
    x_range[0] = wall_x;
    y_range[0] = wall_y;
    if(door_x>door_y)
    {
        // puts("x");
        x_range[1] = door_x;
        //通过x范围计算二分次数
        alpha = dichotomy(x_range,areas.top_x,areas.low_x);
    }
    else
    {
        // puts("y");
        y_range[1] = door_y;
        //通过y范围计算二分次数
        alpha = dichotomy(y_range,areas.top_y,areas.low_y);
    }
    // return alpha;    
    printf("alpha:%d\n",alpha);
    double xSize = (areas.top_x-areas.low_x)/pow(2,alpha);
    double ySize = (areas.top_y-areas.low_y)/pow(2,alpha);
    printf("size:%f,%f\n",xSize,ySize);
    printf("%f,%f,%f,%f\n",areas.top_x,areas.low_x,areas.top_y,areas.low_y);
    double x,y;
    int xCount,yCount;
    for( yCount = 0; yCount < pow(2,alpha); yCount++)
    {
        y = areas.low_y + ySize * yCount;
        for( xCount = 0; xCount < pow(2,alpha); xCount++)
        {
            x = areas.low_x + xSize * xCount;
            // printf("x:%f,y:%f\n",x,y);
            // printf("x:%d,y:%d\n",xCount,yCount);

        }
        
    }
    
}

char* geohash(char *str,int xCount,int yCount,int alpha)
{
    char xStr[GEOLEN],yStr[GEOLEN];
    byGeohash(xStr,xCount,alpha);
    byGeohash(yStr,yCount,alpha);
    // char str[GEOLEN];
    int j;
    for(int i = 0; i < alpha*2; i++)
    {
        str[i]=i/2;
        j = i/2;
        if (i%2==0) 
            str[i]=xStr[j];/* 偶x */
        else 
            str[i]=yStr[j];/* 奇 */
    }
    int end = alpha*2;
    str[end]='\0';
    return str;

}

char* byGeohash(char* str, int count,int alpha)
{
    // char str[GEOLEN];
    itoa(count, str, 2);
    if (strlen(str)<alpha)
    {   
        char tmp[GEOLEN];
        strcpy(tmp,str);
        int diff=alpha-strlen(str);
        printf("%d\n",diff);
        for(int j = 0; j < diff; j++)
        {
            str[j]='0';
        }
        
        for(int i = diff; i < alpha; i++)
            str[i] = tmp[i-diff];
    }
    str[alpha]='\0';
    return str;
}


double max_double(double x,double y)
{
	return x>y?x:y;
}

double min_double(double x,double y)
{
    return x<y?x:y;
}

int init(void)
{
    int flags = 0;
    int count = 0;
    double * list;
    list  = (double *)malloc(sizeof(LINE));
    int num;
    double top_x  = -10000.0;
    double low_x  = 10000.0;
    double top_y  = -10000.0;
    double low_y  = 10000.0;
    double x;
    double y;
    int count_x = 0;
    int count_y = 0;
	FILE * fp = NULL;/*文件指针*/
    char * buf;/*数据缓存区，记得free*/
	buf = (char *)malloc(LINE*sizeof(char));
    fp = fopen("D:\\program\\UWB\\data\\room.txt","r+");/*房间区域数据文件*/
	if (fp!=NULL) {
		puts("file reading...");
		while (fgets(buf,LINE,fp)!=NULL)
		{
			puts(buf);
            if (strlen(buf)<=3)
            {
                rooms[flags].top_x = top_x;
                rooms[flags].low_x = low_x;
                rooms[flags].top_y = top_y;
                rooms[flags].low_y = low_y;
                flags++;
                count = 0;
                top_x = -10000.0;
                low_x = 10000.0;
                top_y = -10000.0;
                low_y = 10000.0;

            }
            else
            {
                split(buf,list);
                if (count== 0) {
                    rooms[flags].room_num = flags;
                    rooms[flags].door_num = (int) list[0];
                    rooms[flags].door_top_x = list[1]-COR_X;
                    rooms[flags].door_low_x = list[2]-COR_X;
                    rooms[flags].door_top_y = list[3]-COR_Y;
                    rooms[flags].door_low_y = list[4]-COR_Y;
                    door_x += list[1]-list[2];
                    door_y += list[3]-list[4];

                }
                else {
                    num = list[0];
                    rooms[flags].walls[num].top_x=list[1]-COR_X;
                    rooms[flags].walls[num].low_x=list[2]-COR_X;
                    rooms[flags].walls[num].top_y=list[3]-COR_Y;
                    rooms[flags].walls[num].low_y=list[4]-COR_Y;
                    rooms[flags].walls[num].wall_num = num;
                    top_x = max_double(top_x,list[1]-COR_X);
                    low_x = min_double(low_x,list[2]-COR_X);
                    top_y = max_double(top_y,list[3]-COR_Y);
                    low_y = min_double(low_y,list[4]-COR_Y);
                    x = list[1]-list[2];
                    y = list[3]-list[4];
                    // printf("x:y:%f,%f\n",x,y);
                    wall_x += x<y ? x:0;
                    wall_y += x>y ? y:0;
                    count_x +=x<y ? 1:0;
                    count_y +=x>y ? 1:0;
                    
                }
                count++;
            }            
		}
		fclose(fp);
		puts("room file close!");
	}else{
        /*文件读取错误！*/
		puts("room file error!");
		fclose(fp);
	} 
    
    door_x = door_x/flags;
    door_y = door_y/flags;
    wall_x = wall_x/count_x;
    wall_y = wall_y/count_y;   
    fp = fopen("D:\\program\\UWB\\data\\area.txt","r+");
    
    if (fp!=NULL) {
        fgets(buf,LINE,fp);
        split(buf,list);
        areas.top_x=list[0]-COR_X;
        areas.low_x=list[1]-COR_X;
        areas.top_y=list[2]-COR_Y;
        areas.low_y=list[3]-COR_Y;
        fclose(fp);
        puts("area file close!");
    }
    else {
        /*文件读取错误！*/
		puts("area file error!");
		fclose(fp);
    }
    fclose(fp);
    // free(list);
    free(buf);
    return 0;
}

// void init(void)
// {
//     FILE * fp = NULL;
// 	char * data;
// 	data = (char *)malloc(LINE*sizeof(char));
    
// 	fp = fopen("D:\\program\\UWB\\data\\room.txt","r+");
//     if (fp != NULL )
//     {
//         int flags = 0;
//         int count = 0;
//         double * list;
//         list  = (double *)malloc(sizeof(LINE));
//         int num;
//         double top_x  = -10000.0;
//         double low_x  = 10000.0;
//         double top_y  = -10000.0;
//         double low_y  = 10000.0;
//         double x;
//         double y;
//         int count_x = 0;
//         int count_y = 0;
//         while (fgets(data,LINE,fp)!=NULL)
//         {
//             puts(data);
//             if (strlen(data)<=3)
//             {
//                 rooms[flags].top_x = top_x;
//                 rooms[flags].low_x = low_x;
//                 rooms[flags].top_y = top_y;
//                 rooms[flags].low_y = low_y;
//                 flags++;
//                 count = 0;
//                 top_x = -10000.0;
//                 low_x = 10000.0;
//                 top_y = -10000.0;
//                 low_y = 10000.0;

//             }
//             else
//             {
//                 split(data,list);
//                 if (count== 0) {
//                     rooms[flags].room_num = flags;
//                     rooms[flags].door_num = (int) list[0];
//                     rooms[flags].door_top_x = list[1]-COR_X;
//                     rooms[flags].door_low_x = list[2]-COR_X;
//                     rooms[flags].door_top_y = list[3]-COR_Y;
//                     rooms[flags].door_low_y = list[4]-COR_Y;
//                     door_x += list[1]-list[2];
//                     door_y += list[3]-list[4];

//                 }
//                 else {
//                     num = list[0];
//                     rooms[flags].walls[num].top_x=list[1]-COR_X;
//                     rooms[flags].walls[num].low_x=list[2]-COR_X;
//                     rooms[flags].walls[num].top_y=list[3]-COR_Y;
//                     rooms[flags].walls[num].low_y=list[4]-COR_Y;
//                     rooms[flags].walls[num].wall_num = num;
//                     top_x = max_double(top_x,list[1]-COR_X);
//                     low_x = min_double(low_x,list[2]-COR_X);
//                     top_y = max_double(top_y,list[3]-COR_Y);
//                     low_y = min_double(low_y,list[4]-COR_Y);
//                     x = list[1]-list[2];
//                     y = list[3]-list[4];
//                     // printf("x:y:%f,%f\n",x,y);
//                     wall_x += x<y ? x:0;
//                     wall_y += x>y ? y:0;
//                     count_x +=x<y ? 1:0;
//                     count_y +=x>y ? 1:0;
                    
//                 }
//                 count++;
//             }
//         }
//         fclose(fp);
//         door_x = door_x/flags;
//         door_y = door_y/flags;
//         wall_x = wall_x/count_x;
//         wall_y = wall_y/count_y;
//         //区域
//         fp = fopen("D:\\program\\UWB\\area1.txt","r+");
//         if(fp != NULL)
//         {
//             fgets(data,LINE,fp);
//             split(data,list);
//             area[0]=list[0]-COR_X;
//             area[1]=list[1]-COR_X;
//             area[2]=list[2]-COR_Y;
//             area[3]=list[3]-COR_Y;
//             // fclose(fp);
//         }
//     }
// }

/*
* 分割字符串
*/
void split(char * data,double *list)
{
    char * re;
    re = strtok(data,",");
    int count = 0;
    while(re != NULL)
    {
        *(list + count++ ) = atof(re);
        // puts(re);
        re = strtok(NULL,",");
    }
    // return list;
}

double * dot(double * a_mat, double * b_mat,int a_row,int b_row)
{
    int a_size  = sizeof(a_mat);
    int b_size  = sizeof(b_mat);
    int a_col   = (int)a_size/a_row; 
    int b_col   = (int)b_size/b_row; 
    if(a_col != b_row)
    {
        printf("Error:Two matrix can not be be multiplied!");
        return NULL;
    }else{
        // double * ap = (double *)a;//获取矩阵
        // double * bp = (double *)b;//获取矩阵a的首地址 
        double value;
        int i,j,k;
        const int  c_size = a_row * b_col;
        double *c_mat;

        // printf("c_size:%d\n",c_size);
        c_mat = (double *)malloc(sizeof(c_size));
        for( i = 0; i < a_row; i++)
        {
            for( j = 0; j < b_col; j++)
            {
                value = 0.0;
                for( k = 0; k < b_row; k++)
                {
                    value += ((*(a_mat+(j*a_row)+k))*(*(b_mat+(k*a_row)+i)));
                }
                *(c_mat+i+(j*a_row))=value;
            }
        }
        return c_mat;
    }
}

double * mat_pow(double * a_mat,int a_row,int power)
{
    // double * ap = (double *)a;//获取矩阵
    int a_size = sizeof(a_mat);
    int a_col = (int) sizeof(a_mat)/a_row;
    int i,j;
    double *c_mat;

    if(power<=0){
        puts("Error: wrong power!");
        return NULL;
    }
    else
    {
        double * b_mat;
        b_mat = (double *)malloc(a_size*sizeof(double));
        for( i = 0; i < a_size; i++)
        {
            *(b_mat+i)=*(a_mat+i);
        }
        if (power!=1)
        {
            for (j = 0; j < power-1; j++)
            {
                c_mat = dot(a_mat,b_mat,a_row,a_row);
                b_mat = c_mat;
            }
        }
        return b_mat;
    }
}





