//韦布分布 潜在扇区错误  磁盘周期清洗
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_statistics.h>

// Total max disk numbers
#define MAX_DISKS 50000

// Disk event type
#define DISK_FAIL 0  // disk failure
#define DAILY_REPAIR 1 // disk daily repair
#define DISK_REPAIRED 2 // disk repaired over 
#define DISK_WARN 3 // disk is predicted to fail
#define DISK_LD 4 // disk latent section failure
#define DISK_SCRUB 5 // disk scrub
#define NODE_FAIL 6 // node failure

// data coding mode
#define Twocopy 0
#define Threecopy 1

// event and disk structure
typedef struct
{
    int type;
    long time;
    int disk_no;
    int node_no;
    int rack_no;
} event;
event event_heap[MAX_DISKS * 2];
int events;

typedef struct
{
    int disk_no;
    int node_no;
    int rack_no;
} disk;

// event-triggered disk arrays and indexes
disk failed_disks[MAX_DISKS];
int front, end;
disk warn_disks[MAX_DISKS];
int front_warn, end_warn;
disk ld_disks[MAX_DISKS];
int front_ld, end_ld, rend_ld;

// gsl variables
const gsl_rng_type *T;
gsl_rng *r;

// extern functions 
typedef int (*array_failed)(int d, int n, int r);
extern int __checkheap();
extern void heap_insert(long time, int type, int disk_no, int node_no, int rack_no);
extern int heap_search(int type, int disk_no, int node_no, int rack_no);
extern void heap_delete(int i);

// check whether data loss happened
double result[10000];
int pro;
int Twocopy_failed(int d, int n, int r)
{
    int i, j;
    int rack_no1, rack_no2;
    pro = ((r - 1) * n * d);
    if (((end + MAX_DISKS - front) % MAX_DISKS) > 1)
    {
        for (i = front; i != end; i++, i %= MAX_DISKS)
        {
            rack_no1 = failed_disks[i].rack_no;
            for (j = (i + 1) % MAX_DISKS; j != end; j++, j %= MAX_DISKS)
            {
                rack_no2 = failed_disks[j].rack_no;
                if (rack_no1 != rack_no2)
                {
                    //printf("when dataloss induced by op+op, the number of failure is %d....\n",((end + MAX_DISKS - front) % MAX_DISKS));
                    return 1;
                }
            }
        }
    }

    if ((((end + MAX_DISKS - front) % MAX_DISKS) >= 1) && (end_ld != front_ld))
    {
        for (i = front; i != end; i++, i %= MAX_DISKS)
        {
            rack_no1 = failed_disks[i].rack_no;
            for (j = front_ld; j != end_ld; j++, j %= MAX_DISKS)
            {
                rack_no2 = ld_disks[j].rack_no;
                if (rack_no1 != rack_no2 && (rand() % pro) == 0)
                {
                    //       			 printf("when dataloss induced by op+ld, then nubmber of failure is %d...., number of ld is %d...\n",((end + MAX_DISKS - front) % MAX_DISKS),((end_ld + MAX_DISKS - front_ld) % MAX_DISKS));
                    return 1;
                }
            }
        }
    }

    return 0;
}
int Threecopy_failed(int d, int n, int r)
{
    int i, j, k;
    int rack_no1, rack_no2, rack_no3;
    int node_no1, node_no2, node_no3;
    int disk_no1, disk_no2, disk_no3;
    pro = ((r - 1) * (n - 1) * n * d * d);

    if (((end + MAX_DISKS - front) % MAX_DISKS) > 2)
        for (i = front; i != (end + MAX_DISKS - 2) % MAX_DISKS; i++, i %= MAX_DISKS)
        {
            rack_no1 = failed_disks[i].rack_no;
            node_no1 = failed_disks[i].node_no;
            for (j = (i + 1) % MAX_DISKS; j != (end + MAX_DISKS - 1) % MAX_DISKS; j++, j %= MAX_DISKS)
            {
                rack_no2 = failed_disks[j].rack_no;
                node_no2 = failed_disks[j].node_no;
                for (k = (j + 1) % MAX_DISKS; k != end; k++, k %= MAX_DISKS)
                {
                    rack_no3 = failed_disks[k].rack_no;
                    node_no3 = failed_disks[k].node_no;
                    if ((rack_no1 == rack_no2 && node_no1 != node_no2 && rack_no1 != rack_no3) || (rack_no1 == rack_no3 && node_no1 != node_no3 && rack_no1 != rack_no2) || (rack_no2 == rack_no3 && node_no2 != node_no3 && rack_no2 != rack_no1))
                        return 1;
                }
            }
        }
    return 0;

    if ((((end + MAX_DISKS - front) % MAX_DISKS) >= 2) && (end_ld != front_ld))
    {
        for (i = front; i != end; i++, i %= MAX_DISKS)
        {
            rack_no1 = failed_disks[i].rack_no;
            node_no1 = failed_disks[i].node_no;
            disk_no1 = failed_disks[i].disk_no;
            for (j = (i + 1) % MAX_DISKS; j != end; j++, j %= MAX_DISKS)
            {
                rack_no2 = failed_disks[j].rack_no;
                node_no2 = failed_disks[j].node_no;
                disk_no2 = failed_disks[j].disk_no;
                if (rack_no1 == rack_no2 && node_no1 != node_no2)
                    for (k = front_ld; k != end_ld; k++, k %= MAX_DISKS)
                    {
                        rack_no3 = ld_disks[k].rack_no;
                        if (rack_no1 != rack_no3 && (rand() % pro) == 0)
                        { // printf("dataloss failure + ld fail_no %d....ld_no %d...\n",fail_no,ld_no);
                            return 1;
                        }
                    }
                else if (rack_no1 != rack_no2)
                {
                    for (k = front_ld; k != end_ld; k++, k %= MAX_DISKS)
                    {
                        rack_no3 = ld_disks[k].rack_no;
                        node_no3 = ld_disks[k].node_no;
                        disk_no3 = ld_disks[k].disk_no;
                        if ((rack_no1 == rack_no3 && node_no1 != node_no3) || (rack_no2 == rack_no3 && node_no2 != node_no3))
                            if ((rand() % pro) == 0)
                            { // printf("dataloss failure + ld fail_no %d....ld_no %d...\n",fail_no,ld_no);
                                return 1;
                            }
                    }
                }
            }
        }
    }

    return 0;
}

array_failed func[] = {Twocopy_failed, Threecopy_failed};



void initialize(int disk, int node, int rack, double Lyita_op, double Lbita_op, double Lyita_ld, double Lbita_ld, double Lgama_scrub, double Lyita_scrub, double Lbita_scrub, double Lyita_tia, double Lbita_tia, double fdr)
{
    int i, j, k;
    long time_op;
    long time_tia;
    long time_ld;
    long time_scrub;

    events = 0;
    event_heap[0].time = 0;

    for (i = 0; i < rack; i++)
        for (k = 0; k < node; k++)
            for (j = 0; j < disk; j++)
            {
                time_op = gsl_ran_weibull(r, Lyita_op, Lbita_op);
                heap_insert(time_op, DISK_FAIL, j, k, i);
                if ((rand() % 10000) < fdr * 100)
                {
                    time_tia = 1 + gsl_ran_weibull(r, Lyita_tia, Lbita_tia);
                    heap_insert((time_op - time_tia) >= 0 ? (time_op - time_tia) : 0, DISK_WARN, j, k, i);
                }
                time_ld = gsl_ran_weibull(r, Lyita_ld, Lbita_ld);
                heap_insert(time_ld, DISK_LD, j, k, i);
                time_scrub = Lgama_scrub + gsl_ran_weibull(r, Lyita_scrub, Lbita_scrub);
                heap_insert(time_scrub, DISK_SCRUB, j, k, i);
            }
}
int test_mttdl;
double test_mttdl_cycle;

double sim_num_loss(int coding, int disk, int node, int rack, double Lyita_op, double Lbita_op, double Lgama_rest, double Lyita_rest, double Lbita_rest, double Lyita_ld, double Lbita_ld, double Lgama_scrub, double Lyita_scrub, double Lbita_scrub, double Lyita_tia, double Lbita_tia, double fdr, double t)
{
    int disk_no, rack_no, type, number;
    int node_no, n;
    long time_ld;
    long time_scrub;
    long time_mttr;
    long time_op;
    long time_tia;
    struct timeval tp;
    int num_loss = 0; //数据丢失数量
    int i, j;
    //   int test_mttdl_num = 1;
    long time;

    gettimeofday(&tp, NULL);
    heap_delete(1);
    while (event_heap[0].time <= t)
    {
        type = event_heap[0].type;
        time = event_heap[0].time;
        disk_no = event_heap[0].disk_no;
        node_no = event_heap[0].node_no;
        rack_no = event_heap[0].rack_no;

        //发生潜在错误
        if (type == DISK_LD)
        {
            ld_disks[end_ld].disk_no = disk_no;
            ld_disks[end_ld].node_no = node_no;
            ld_disks[end_ld].rack_no = rack_no;
            end_ld++;
            end_ld %= MAX_DISKS;
            //          printf("DISK_LD[%d %d %d]\n", disk_no, node_no, rack_no);

            //发生潜在错误的盘，可能会再次发生潜在错误
            time_ld = time + gsl_ran_weibull(r, Lyita_ld, Lbita_ld);
            heap_insert(time_ld, DISK_LD, disk_no, node_no, rack_no);

            if (func[coding](disk, node, rack))
            {
                //dataloss发生了，对故障（failed_disks数组中元素）盘进行处理：换盘，重新生成故障预警及潜在错误
                for (; front != end; front++, front %= MAX_DISKS)
                {
                    n = heap_search(DISK_REPAIRED, failed_disks[front].disk_no, failed_disks[front].node_no, failed_disks[front].rack_no);
                    if (n > 0)
                        heap_delete(n);

                    time_op = time + gsl_ran_weibull(r, Lyita_op, Lbita_op);
                    heap_insert(time_op, DISK_FAIL, failed_disks[front].disk_no, failed_disks[front].node_no, failed_disks[front].rack_no);
                    if ((rand() % 10000) < fdr * 100)
                    {
                        time_tia = 1 + gsl_ran_weibull(r, Lyita_tia, Lbita_tia);
                        heap_insert((time_op - time_tia) >= time ? (time_op - time_tia) : time, DISK_WARN, failed_disks[front].disk_no, failed_disks[front].node_no, failed_disks[front].rack_no);
                    }
                    //重新对新盘生成未来潜在错误
                    time_ld = time + gsl_ran_weibull(r, Lyita_ld, Lbita_ld);
                    heap_insert(time_ld, DISK_LD, failed_disks[front].disk_no, failed_disks[front].node_no, failed_disks[front].rack_no);

                    //是否要重新启动磁盘清洗过程
                    time_scrub = time + Lgama_scrub + gsl_ran_weibull(r, Lyita_scrub, Lbita_scrub);
                    heap_insert(time_scrub, DISK_SCRUB, failed_disks[front].disk_no, failed_disks[front].node_no, failed_disks[front].rack_no);

                    //				   printf("when dataloss, replace disk[%d %d %d]\n", failed_disks[front].disk_no, failed_disks[front].node_no, failed_disks[front].rack_no);
                }
                num_loss = num_loss + 1;
                //                printf("data loss at %ld\n", time);
                /*               if(test_mttdl_num == 1){
                    test_mttdl += time;
                    test_mttdl_num++;
                    test_mttdl_cycle++;
                    printf("data loss at %ld\n", time);
                }  */
            }
        }
        else if (type == DISK_SCRUB)
        {
            //清除该磁盘的所有扇区错误
            j = 0;
            for (i = front_ld; i != end_ld; i++, i %= MAX_DISKS)
            {
                if (disk_no == ld_disks[i].disk_no && node_no == ld_disks[i].node_no && rack_no == ld_disks[i].rack_no)
                {
                    ++j;
                }
                else
                {
                    ld_disks[(i + MAX_DISKS - j) % MAX_DISKS].disk_no = ld_disks[i].disk_no;
                    ld_disks[(i + MAX_DISKS - j) % MAX_DISKS].node_no = ld_disks[i].node_no;
                    ld_disks[(i + MAX_DISKS - j) % MAX_DISKS].rack_no = ld_disks[i].rack_no;
                }
            }
            end_ld = (end_ld + MAX_DISKS - j) % MAX_DISKS;

            //磁盘清洗完成，清除所有已发生的潜在错误。启动下一次清洗。
            time_scrub = time + Lgama_scrub + gsl_ran_weibull(r, Lyita_scrub, Lbita_scrub);
            heap_insert(time_scrub, DISK_SCRUB, disk_no, node_no, rack_no);
            //				printf("new scrub.....time_scrub %d\n", time_scrub);
        }
        else if (type == DISK_WARN)
        { //发生预警，生成修复事件
            warn_disks[end_warn].disk_no = disk_no;
            warn_disks[end_warn].node_no = node_no;
            warn_disks[end_warn].rack_no = rack_no;
            time_mttr = Lgama_rest + gsl_ran_weibull(r, Lyita_rest, Lbita_rest);
            heap_insert(time + time_mttr, DISK_REPAIRED, disk_no, node_no, rack_no);
            end_warn++;
            end_warn %= MAX_DISKS;
            //			printf("new warn-repair[%d %d %d], warn-TIME%d\n", disk_no, node_no, rack_no,time);
        }
        else if (type == DISK_REPAIRED)
        {
            //          printf("DISK_RE[%d %d %d]\n", disk_no, node_no, rack_no);
            int flag = 0;
            int i;
            if (front != end)
            {
                //find failed disk with pos i
                /*			   for (i=front; i != end; i++, i %= MAX_DISKS) {
                    if (disk_no == failed_disks[i].disk_no && node_no == failed_disks[i].node_no && rack_no == failed_disks[i].rack_no) {
                        flag=1;
                        break;
                    }
                } */

                //remove i
                j = 0;
                for (i = front; i != end; i++, i %= MAX_DISKS)
                {
                    if (disk_no == failed_disks[i].disk_no && node_no == failed_disks[i].node_no && rack_no == failed_disks[i].rack_no)
                    {
                        ++j;
                        flag = 1;
                    }
                    else
                    {
                        failed_disks[(i + MAX_DISKS - j) % MAX_DISKS].disk_no = failed_disks[i].disk_no;
                        failed_disks[(i + MAX_DISKS - j) % MAX_DISKS].node_no = failed_disks[i].node_no;
                        failed_disks[(i + MAX_DISKS - j) % MAX_DISKS].rack_no = failed_disks[i].rack_no;
                    }
                }
                end = (end + MAX_DISKS - j) % MAX_DISKS;

                /*               if (i != end ) {               
                   for (; front !=i; front++, front %= MAX_DISKS)	{
                        failed_disks[end].disk_no=failed_disks[front].disk_no;
                        failed_disks[end].node_no=failed_disks[front].node_no;
                        failed_disks[end].rack_no=failed_disks[front].rack_no;
                        end++;
                        end %= MAX_DISKS;
                    }	
                    front++;
                    front %= MAX_DISKS;
               }  */
            }

            if (flag != 1 && (end_warn != front_warn))
            {

                j = 0;
                for (i = front_warn; i != end_warn; i++, i %= MAX_DISKS)
                {
                    if (disk_no == warn_disks[i].disk_no && node_no == warn_disks[i].node_no && rack_no == warn_disks[i].rack_no)
                    {
                        ++j;
                        flag = 2;
                    }
                    else
                    {
                        warn_disks[(i + MAX_DISKS - j) % MAX_DISKS].disk_no = warn_disks[i].disk_no;
                        warn_disks[(i + MAX_DISKS - j) % MAX_DISKS].node_no = warn_disks[i].node_no;
                        warn_disks[(i + MAX_DISKS - j) % MAX_DISKS].rack_no = warn_disks[i].rack_no;
                    }
                }
                end_warn = (end_warn + MAX_DISKS - j) % MAX_DISKS;

                /*		    for (i=front_warn; i != end_warn; i++, i %= MAX_DISKS) {
			         if(disk_no == warn_disks[i].disk_no && node_no == warn_disks[i].node_no && rack_no == warn_disks[i].rack_no) {
					    flag=2;
				        break;
					 }
			     }
				 if (i != end_warn ) {
                     for (; front_warn !=i; front_warn++, front_warn %= MAX_DISKS)	{
                        warn_disks[end_warn].disk_no=warn_disks[front_warn].disk_no;
                        warn_disks[end_warn].node_no=warn_disks[front_warn].node_no;
                        warn_disks[end_warn].rack_no=warn_disks[front_warn].rack_no;
                        end_warn++;
                        end_warn %= MAX_DISKS;
                      }	
                    front_warn++;
                    front_warn %= MAX_DISKS;
			    } */
            }

            if (flag == 1)
            {
                //重新生成所有事件
                time_op = time + gsl_ran_weibull(r, Lyita_op, Lbita_op);
                heap_insert(time_op, DISK_FAIL, disk_no, node_no, rack_no);
                if ((rand() % 10000) < fdr * 100)
                {
                    time_tia = 1 + gsl_ran_weibull(r, Lyita_tia, Lbita_tia);
                    heap_insert((time_op - time_tia) > time ? (time_op - time_tia) : time, DISK_WARN, disk_no, node_no, rack_no);
                }

                time_ld = time + gsl_ran_weibull(r, Lyita_ld, Lbita_ld);
                heap_insert(time_ld, DISK_LD, disk_no, node_no, rack_no);

                time_scrub = time + Lgama_scrub + gsl_ran_weibull(r, Lyita_scrub, Lbita_scrub);
                heap_insert(time_scrub, DISK_SCRUB, disk_no, node_no, rack_no);
            }
            else if (flag == 2)
            {
                number = heap_search(DISK_FAIL, disk_no, node_no, rack_no);
                if (number > 0)
                {
                    heap_delete(number);

                    time_op = time + gsl_ran_weibull(r, Lyita_op, Lbita_op);
                    heap_insert(time_op, DISK_FAIL, disk_no, node_no, rack_no);
                    if ((rand() % 10000) < fdr * 100)
                    {
                        time_tia = 1 + gsl_ran_weibull(r, Lyita_tia, Lbita_tia);
                        heap_insert((time_op - time_tia) > time ? (time_op - time_tia) : time, DISK_WARN, disk_no, node_no, rack_no);
                    }
                }
                //重新生成新盘的ld
                number = heap_search(DISK_LD, disk_no, node_no, rack_no);
                if (number > 0)
                {
                    heap_delete(number);

                    time_ld = time + gsl_ran_weibull(r, Lyita_ld, Lbita_ld);
                    heap_insert(time_ld, DISK_LD, disk_no, node_no, rack_no);
                }
                //重新生成新盘的清洗事件
                number = heap_search(DISK_SCRUB, disk_no, node_no, rack_no);
                if (number > 0)
                {
                    heap_delete(number);

                    time_scrub = time + Lgama_scrub + gsl_ran_weibull(r, Lyita_scrub, Lbita_scrub);
                    heap_insert(time_scrub, DISK_SCRUB, disk_no, node_no, rack_no);
                }
                //去除该预警盘已经发生的LD错误
                j = 0;
                for (i = front_ld; i != end_ld; i++, i %= MAX_DISKS)
                {
                    if (disk_no == ld_disks[i].disk_no && node_no == ld_disks[i].node_no && rack_no == ld_disks[i].rack_no)
                    {
                        ++j;
                    }
                    else
                    {
                        ld_disks[(i + MAX_DISKS - j) % MAX_DISKS].disk_no = ld_disks[i].disk_no;
                        ld_disks[(i + MAX_DISKS - j) % MAX_DISKS].node_no = ld_disks[i].node_no;
                        ld_disks[(i + MAX_DISKS - j) % MAX_DISKS].rack_no = ld_disks[i].rack_no;
                    }
                }
                end_ld = (end_ld + MAX_DISKS - j) % MAX_DISKS;

                /*			    for (i=front_ld; i != end_ld; i++, i %= MAX_DISKS) {
                    if (disk_no == ld_disks[i].disk_no && node_no == ld_disks[i].node_no &&rack_no == ld_disks[i].rack_no ) {			       
                        for (; front_ld !=i; front_ld++, front_ld %= MAX_DISKS)	{
                            ld_disks[end_ld].disk_no=ld_disks[front_ld].disk_no;
                            ld_disks[end_ld].node_no=ld_disks[front_ld].node_no;
                            ld_disks[end_ld].rack_no=ld_disks[front_ld].rack_no;
                            end_ld++;
                            end_ld %= MAX_DISKS;
                        }	
                        front_ld++;
                        front_ld %= MAX_DISKS;
                    }
				}  */
            }
            else
            {
                printf("error in fail/warn repaired [%d %d %d]，time %ld\n", disk_no, node_no, rack_no, time);
                printf("failed number %d, warn number %d \n", (end + MAX_DISKS - front) % MAX_DISKS, (end_warn + MAX_DISKS - front_warn) % MAX_DISKS);
                for (; front != end; front++, front %= MAX_DISKS)
                    printf("failed [%d %d %d] ,time %ld\n ", failed_disks[front].disk_no, failed_disks[front].node_no, failed_disks[front].rack_no, time);
                for (; front_warn != end_warn; front_warn++, front_warn %= MAX_DISKS)
                    printf("warn [%d %d %d] ,time %ld\n", warn_disks[front_warn].disk_no, warn_disks[front_warn].node_no, warn_disks[front_warn].rack_no, time);

                exit(1);
            }
        }
        else if (type == DISK_FAIL)
        { //故障发生，插入failed_disks数组
            //printf("DISK_FA[%d %d %d]\n", disk_no, node_no, rack_no);
            failed_disks[end].disk_no = disk_no;
            failed_disks[end].node_no = node_no;
            failed_disks[end].rack_no = rack_no;
            end++;
            end %= MAX_DISKS;
            //           printf("[DISK_FAIL fail-time=%d, d=%d, n=%d, r=%d]\n", time, disk_no, node_no, rack_no);

            //检查故障盘是否已经发生了潜在错误，如有则从ld_disks数组中删除

            j = 0;
            for (i = front_ld; i != end_ld; i++, i %= MAX_DISKS)
            {
                if (disk_no == ld_disks[i].disk_no && node_no == ld_disks[i].node_no && rack_no == ld_disks[i].rack_no)
                {
                    ++j;
                }
                else
                {
                    ld_disks[(i + MAX_DISKS - j) % MAX_DISKS].disk_no = ld_disks[i].disk_no;
                    ld_disks[(i + MAX_DISKS - j) % MAX_DISKS].node_no = ld_disks[i].node_no;
                    ld_disks[(i + MAX_DISKS - j) % MAX_DISKS].rack_no = ld_disks[i].rack_no;
                }
            }
            end_ld = (end_ld + MAX_DISKS - j) % MAX_DISKS;

            /*			i=front_ld;
			while (i != end_ld) {
			    for (i=front_ld; i != end_ld; i++, i %= MAX_DISKS) 
			        if (ld_disks[i].disk_no == disk_no && ld_disks[i].node_no == node_no && ld_disks[i].rack_no == rack_no)    
					  break;
			            
				if (i != end_ld ) {
				    for (; front_ld !=i; front_ld++, front_ld %= MAX_DISKS)	{
				        ld_disks[end_ld].disk_no=ld_disks[front_ld].disk_no;
						ld_disks[end_ld].node_no=ld_disks[front_ld].node_no;
						ld_disks[end_ld].rack_no=ld_disks[front_ld].rack_no;
				        end_ld++;
					    end_ld %= MAX_DISKS;
				     }	
				 front_ld++;
				 front_ld %= MAX_DISKS;
			    }
			} */
            //检查堆中是否有故障盘的未来潜在错误，磁盘清洗，如有，去除。等修复完成时再对新盘生成未来潜在错误
            number = heap_search(DISK_LD, disk_no, node_no, rack_no);
            if (number > 0)
                heap_delete(number);

            number = heap_search(DISK_SCRUB, disk_no, node_no, rack_no);
            if (number > 0)
                heap_delete(number);

            number = heap_search(DISK_WARN, disk_no, node_no, rack_no);
            if (number > 0)
                heap_delete(number);

            //			number= heap_search(DISK_REPAIRED, disk_no, node_no, rack_no);
            //            if (number > 0)
            //                heap_delete(number);
            //判断是否dataloss
            if (func[coding](disk, node, rack))
            {
                for (; front != end; front++, front %= MAX_DISKS)
                {
                    number = heap_search(DISK_REPAIRED, failed_disks[front].disk_no, failed_disks[front].node_no, failed_disks[front].rack_no);
                    if (number > 0)
                        heap_delete(number);
                    time_op = time + gsl_ran_weibull(r, Lyita_op, Lbita_op);
                    heap_insert(time_op, DISK_FAIL, failed_disks[front].disk_no, failed_disks[front].node_no, failed_disks[front].rack_no);
                    if ((rand() % 10000) < fdr * 100)
                    {
                        time_tia = 1 + gsl_ran_weibull(r, Lyita_tia, Lbita_tia);
                        heap_insert((time_op - time_tia) >= time ? (time_op - time_tia) : time, DISK_WARN, failed_disks[front].disk_no, failed_disks[front].node_no, failed_disks[front].rack_no);
                    }

                    //重新对新盘生成未来潜在错误，磁盘清洗事件
                    time_ld = time + gsl_ran_weibull(r, Lyita_ld, Lbita_ld);
                    heap_insert(time_ld, DISK_LD, failed_disks[front].disk_no, failed_disks[front].node_no, failed_disks[front].rack_no);

                    time_scrub = time + Lgama_scrub + gsl_ran_weibull(r, Lyita_scrub, Lbita_scrub);
                    heap_insert(time_scrub, DISK_SCRUB, failed_disks[front].disk_no, failed_disks[front].node_no, failed_disks[front].rack_no);

                    //                   printf("when dataloss, replace disk[%d %d %d]\n", failed_disks[front].disk_no, failed_disks[front].node_no, failed_disks[front].rack_no);
                }

                num_loss = num_loss + 1;
                /*              if(test_mttdl_num == 1){
                    test_mttdl += time;
                    test_mttdl_num++;
                    test_mttdl_cycle++;
                    printf("data loss at %ld\n", time);
                } */
            }
            else
            { //故障没有造成数据丢失，生成修复事件
                time_mttr = Lgama_rest + gsl_ran_weibull(r, Lyita_rest, Lbita_rest);
                heap_insert(time + time_mttr, DISK_REPAIRED, disk_no, node_no, rack_no);
                //               printf("INSE_fail-REpair[%d %d %d]\n",  disk_no, node_no, rack_no);
            }
        }
        else
        {
            printf("BUG\n");
            exit(-1);
        }
        heap_delete(1);
    }

    return num_loss;
}

int main(int argc, char *argv[])
{
    int coding, i;
    double Lyita_op, Lbita_op, Lgama_rest, Lyita_rest, Lbita_rest, Lyita_ld, Lbita_ld, Lgama_scrub, Lyita_scrub, Lbita_scrub, Lyita_tia, Lbita_tia, fdr, t;
    int reps1, num_dloss, reps2;
    double pro_loss, mttdl;
    int disk, node, rack;
    int add_reps1 = 0, add_loss = 0;

    if (argc < 19)
    {
        printf("Usage: mttdlsim coding (Twocopy/Threecopy) disks nodes racks  Lyita_op Lbita_op Lgama_rest Lyita_rest Lbita_rest Lyita_ld, Lbita_ld, Lgama_scrub, Lyita_scrub, Lbita_scrub Lyita_tia Lbita_tia FDR t reps\n"); //fdr 90.59
                                                                                                                                                                                                                               //参数：系统配置 磁盘数/node 结点数/机架 机架数/系统 故障特征生命参数 故障形状参数 修复位置参数 修复生命参数 修复形状参数 潜在错误生命 潜在错误形状 清洗位置 清洗生命 清洗形状 提前时间生命 提前时间形状 准确率90.54 测试周期 重复次数
        return -1;
    }

    if (!strcmp(argv[1], "Twocopy"))
        coding = Twocopy;
    else if (!strcmp(argv[1], "Threecopy"))
        coding = Threecopy;
    else
    {
        printf("coding BUG\n");
        return -1;
    }
    disk = atoi(argv[2]);
    node = atoi(argv[3]);
    rack = atoi(argv[4]);
    Lyita_op = atof(argv[5]);
    Lbita_op = atof(argv[6]);
    Lgama_rest = atof(argv[7]);
    Lyita_rest = atof(argv[8]);
    Lbita_rest = atof(argv[9]);
    Lyita_ld = atof(argv[10]);
    Lbita_ld = atof(argv[11]);
    Lgama_scrub = atof(argv[12]);
    Lyita_scrub = atof(argv[13]);
    Lbita_scrub = atof(argv[14]);
    Lyita_tia = atof(argv[15]);
    Lbita_tia = atof(argv[16]);
    fdr = atof(argv[17]);
    t = atof(argv[18]);
    reps2 = atof(argv[19]);
    srand((unsigned)time(NULL));

    mttdl = 0.0;
    gsl_rng_env_setup();
    T = gsl_rng_default;
    r = gsl_rng_alloc(T);

    struct timeval tp;
    gettimeofday(&tp, NULL);
    gsl_rng_set(r, tp.tv_usec);

    for (i = 0; i < reps2; i++)
    {
        num_dloss = 0;
        reps1 = 0;
        while (num_dloss < 5)
        {
            initialize(disk, node, rack, Lyita_op, Lbita_op, Lyita_ld, Lbita_ld, Lgama_scrub, Lyita_scrub, Lbita_scrub, Lyita_tia, Lbita_tia, fdr);
            front = 0;
            end = 0;
            front_warn = 0;
            end_warn = 0;
            front_ld = 0;
            end_ld = 0;

            num_dloss = num_dloss + sim_num_loss(coding, disk, node, rack, Lyita_op, Lbita_op, Lgama_rest, Lyita_rest, Lbita_rest, Lyita_ld, Lbita_ld, Lgama_scrub, Lyita_scrub, Lbita_scrub, Lyita_tia, Lbita_tia, fdr, t);
            reps1++; //出现数据丢失的循环次数
            /*	if( (reps1+1)%10 == 0){
            printf("...repeat %d...\n",reps1);
            printf("...num_dloss %d...\n",num_dloss);
            fflush(stdout);
            } */
        }
        add_loss = add_loss + num_dloss;
        add_reps1 = add_reps1 + reps1;
        result[i] = (double)num_dloss / (double)reps1;
        printf("...pro_dloss %f\n", result[i]);
    }

    pro_loss = gsl_stats_mean(result, 1, reps2);
    printf("\naverage = %f\n", pro_loss);
    printf("...add_loss %d....add_reps1 %d...pro_loss %lf \n", add_loss, add_reps1, 1.0 * add_loss / add_reps1);
    printf("\nvariance = %f\n", gsl_stats_variance_with_fixed_mean(result, 1, reps2, pro_loss));
    //    printf("\n mttdl = %f\n", test_mttdl/test_mttdl_cycle);

    //printf("\num_dataloss = %d\n", num_dloss);
    //printf("\resp = %d\n", reps1);
    gsl_rng_free(r);

    return 0;
}


/*****************************************************************
 *            The Heap structure functions
 *  heap_insert: 插入event
 *  heap_search: 搜索event
 *  heap_delete: 删除给定索引的event
 ****************************************************************/
void heap_insert(long time, int type, int disk_no, int node_no, int rack_no)
{
    int i, p;
    int j;
    if (type == DISK_LD || type == DISK_SCRUB)
    {
        return;
    }

    for (j = 1; j <= events; j++)
        if (event_heap[j].type == type && event_heap[j].disk_no == disk_no && event_heap[j].node_no == node_no && event_heap[j].rack_no == rack_no)
            return;

    events++;
    i = events;
    p = i / 2;
    while (p > 0 && event_heap[p].time > time)
    {
        event_heap[i] = event_heap[p];
        i = p;
        p = i / 2;
    }
    event_heap[i].time = time;
    event_heap[i].type = type;
    event_heap[i].disk_no = disk_no;
    event_heap[i].node_no = node_no;
    event_heap[i].rack_no = rack_no;
}

int heap_search(int type, int disk_no, int node_no, int rack_no)
{
    int i;
    for (i = 1; i <= events; i++)
    {
        if (event_heap[i].type == type && event_heap[i].disk_no == disk_no && event_heap[i].node_no == node_no && event_heap[i].rack_no == rack_no)
        {
            return i;
        }
    }
    return -1;
}

static void __MinHeapIfy(int i)
{
    int min = i;
    int left = 2 * i;
    int right = left + 1;

    if (left <= events && event_heap[i].time > event_heap[left].time)
    {
        min = left;
    }
    else
    {
        min = i;
    }
    if (right <= events && event_heap[min].time > event_heap[right].time)
    {
        min = right;
    }
    if (i != min)
    {
        event tmp = event_heap[i];
        event_heap[i] = event_heap[min];
        event_heap[min] = tmp;
        __MinHeapIfy(min);
    }
}

//x节点的time减小到更小的值
static void __HeapDecreaseKey(int x)
{
    event tmp;
    while (x > 1 && event_heap[x >> 1].time > event_heap[x].time)
    {
        tmp = event_heap[x];
        event_heap[x] = event_heap[x >> 1];
        event_heap[x >> 1] = tmp;
        x >>= 1;
    }
}

int __checkheap()
{
    int i, left, right;
    int end = events >> 1;
    for (i = 1; i <= end; ++i)
    {
        if (event_heap[i].time < 0)
            return 0;
        left = 2 * i;
        right = left + 1;
        if (left <= events && event_heap[i].time > event_heap[left].time)
            return 0;
        if (right <= events && event_heap[i].time > event_heap[right].time)
            return 0;
    }
    return 1;
}

//删除堆中指定元素i
void heap_delete(int i)
{

    event_heap[0] = event_heap[i];
    event tmp = event_heap[events];

    if (event_heap[i].time == tmp.time)
    {
        event_heap[i] = tmp;
        events--;
    }
    else if (event_heap[i].time < tmp.time) //i为根节点的分支的最小堆性质可能遭到破坏，要进行调整
    {
        event_heap[i] = tmp;
        events--;
        __MinHeapIfy(i);
    }
    else //i节点的值减小到更小的tmp
    {
        event_heap[i] = tmp;
        events--;
        __HeapDecreaseKey(i);
    }
}
