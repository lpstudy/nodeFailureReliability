//Τ���ֲ� Ǳ����������  ����������ϴ
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_statistics.h>

// Total max disk numbers
#define MAX_DISKS 50000

// Disk event type
#define DISK_FAIL 0  // disk failure
#define DAILY_REPAIR 1 // disk daily repair
#define DISK_REPAIRED 2 // disk failure repaired over 
#define DISK_WARN 3 // disk is predicted to fail
#define DISK_LD 4 // disk latent section failure
#define DISK_SCRUB 5 // disk scrub
#define NODE_FAIL 6 // node failure
#define NODE_REPAIRED 7 // node failure repaired over

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

struct disk_struct
{
    int disk_no;
    int node_no;
    int rack_no;
	// ��������Disk�ṹ��ıȽ�
	bool operator == (const struct disk_struct& rhs) {
		return disk_no == rhs.disk_no && node_no == rhs.node_no && rack_no == rhs.rack_no;
	}
};

typedef struct
{
    disk_struct disks[MAX_DISKS];
    int front, end;
}disk_array;

// gsl variables
const gsl_rng_type *T;
gsl_rng *r;

// extern functions 
typedef int (*array_failed)(int d, int n, int r);
extern int __checkheap();
extern void heap_insert(long time, int type, int disk_no, int node_no, int rack_no);
extern int heap_search(int type, int disk_no, int node_no, int rack_no);
extern void heap_delete(int i);

// log utility functions
#ifdef TDEBUG
#define _TRACE(d, time, str) printf("%s[%d %d %d] time=%ld\n", str, d.disk_no, d.node_no, d.rack_no, time)
#else
#define _TRACE(d, time, str) {}
#endif // DEBUG
#define TRACE_LD(d, time) _TRACE(d, time, "DISK_LD")
#define TRACE_SCRUB(d, time) _TRACE(d, time, "DISK_SCRUB")
#define TRACE_FAIL(d, time) _TRACE(d, time, "DISK_FAIL")
#define TRACE_WARN(d, time) _TRACE(d, time, "DISK_WARN")
#define TRACE_REPAIR(d, time) _TRACE(d, time, "DISK_REPAIR")
#define TRACE_DLOSS(d, time) _TRACE(d, time, "DATA_LOSS")
#define TRACE_NODE_FAIL(d, time) _TRACE(d, time, "NODE_FAIL")
#define TRACE_NODE_REPAIR(d, time) _TRACE(d, time, "NODE_REPAIR")

// data loss functions
extern int Threecopy_failed(int d, int n, int r);
array_failed func[] = { Threecopy_failed, Threecopy_failed };

/*****************************************************
 * Global variables which can be used anywhere
 ****************************************************/
int disk, node, rack, coding;
double Lyita_op,  Lbita_op, Lyita_tia, Lbita_tia, fdr; // failure & prediction
double Lyita_ld,  Lbita_ld,  Lgama_scrub, Lyita_scrub, Lbita_scrub; // ld & scrub
double Lyita_node, Lbita_node, Lgama_node_rest, Lyita_node_rest, Lbita_node_rest; // node failure & repair
double Lgama_rest, Lyita_rest, Lbita_rest; // disk repair
// warn_disks�Ĵ��ڸо�û�м�ֵ�������������ж�data_loss
disk_array failed_disks, warn_disks, ld_disks, failed_nodes;

inline void events_for_new_disk(disk_struct d)
{
    int j = d.disk_no, k = d.node_no, i = d.rack_no;
    long time_op, time_ld, time_scrub, time_tia;
    // disk failure and prediction
    time_op = gsl_ran_weibull(r, Lyita_op, Lbita_op);
    heap_insert(time_op, DISK_FAIL, j, k, i);
    if ((rand() % 10000) < fdr * 100)
    {
        time_tia = 1 + gsl_ran_weibull(r, Lyita_tia, Lbita_tia);
        heap_insert((time_op - time_tia) >= 0 ? (time_op - time_tia) : 0, DISK_WARN, j, k, i);
    }
    // ld and scrub will be ommited in the insert
    time_ld = gsl_ran_weibull(r, Lyita_ld, Lbita_ld);
    heap_insert(time_ld, DISK_LD, j, k, i);
    time_scrub = Lgama_scrub + gsl_ran_weibull(r, Lyita_scrub, Lbita_scrub);
    heap_insert(time_scrub, DISK_SCRUB, j, k, i);
}
inline void event_failure_for_node(disk_struct d)
{
    long time_op = gsl_ran_weibull(r, Lyita_node, Lbita_node);
    heap_insert(time_op, NODE_FAIL, -1, d.node_no, d.rack_no);
}
inline void event_repair_for_node(disk_struct d, long time)
{
    long time_mttr = Lgama_node_rest + gsl_ran_weibull(r, Lyita_node_rest, Lbita_node_rest);
    heap_insert(time + time_mttr, NODE_REPAIRED, -1, d.node_no, d.rack_no);
}
inline void event_scrub_for_disk(disk_struct d, long time) 
{
    long time_scrub = time + Lgama_scrub + gsl_ran_weibull(r, Lyita_scrub, Lbita_scrub);
    heap_insert(time_scrub, DISK_SCRUB, d.disk_no, d.node_no, d.rack_no);
}
inline void event_ld_for_disk(disk_struct d, long time)
{
    long time_ld = time + gsl_ran_weibull(r, Lyita_ld, Lbita_ld);
    heap_insert(time_ld, DISK_LD, d.disk_no, d.node_no, d.rack_no);
}
inline void event_repair_for_disk(disk_struct d, long time)
{
    long time_mttr = Lgama_rest + gsl_ran_weibull(r, Lyita_rest, Lbita_rest);
    heap_insert(time + time_mttr, DISK_REPAIRED, d.disk_no, d.node_no, d.rack_no);
} 
// C++����
inline void add_disk_to_array(disk_array& darray, disk_struct d)
{
    darray.disks[darray.end] = d;
    darray.end++;
    darray.end %= MAX_DISKS;
}
inline void remove_disk_from_array(disk_array& darray, disk_struct d)
{
	int j = 0;
    for (int i = darray.front; i != darray.end; i++, i %= MAX_DISKS)
    {
        if (d == darray.disks[i])
        {
            ++j;
        }
        else
        {
            darray.disks[(i + MAX_DISKS - j) % MAX_DISKS] = darray.disks[i];
        }
    }
    darray.end = (darray.end + MAX_DISKS - j) % MAX_DISKS;
}
// true for truly deleting
inline bool remove_event_from_heap(disk_struct d, int ev_type)
{
    int number = heap_search(ev_type, d.disk_no, d.node_no, d.rack_no);
    if (number > 0)
    {
        heap_delete(number);
        return true;
    }
    return false;
}

void initialize()
{
    int i, j, k;
    events = 0;
    event_heap[0].time = 0;

    failed_disks.front = failed_disks.end = 0;
    warn_disks.front = warn_disks.end = 0;
    ld_disks.front = ld_disks.end = 0;
    failed_nodes.front = failed_nodes.end = 0;

    for (int i = 0; i < rack; i++)
        for (int k = 0; k < node; k++)
        {
            for (int j = 0; j < disk; j++)
            {
				events_for_new_disk({ j, k, i });
            }
            event_failure_for_node({-1, k, i});
        }
}
int test_mttdl;
double test_mttdl_cycle;

double sim_num_loss(double t)
{
    int disk_no, rack_no, type, number;
    int node_no, n;
    long time_ld;
    long time_scrub;
    long time_mttr;
    long time_op;
    long time_tia;
    int num_loss = 0; //���ݶ�ʧ����
    int i, j;
    //   int test_mttdl_num = 1;
    long time;

    heap_delete(1);
    while (event_heap[0].time <= t)
    {
        type = event_heap[0].type;
        time = event_heap[0].time;
        disk_struct d ={event_heap[0].disk_no, event_heap[0].node_no, event_heap[0].rack_no};

        if (type == DISK_LD)
        {
            //��¼��ǰǱ�ڴ��󣬲������´�Ǳ�ڴ�������
            TRACE_LD(d, time);
            add_disk_to_array(ld_disks, d);
            event_ld_for_disk(d, time);

            if (func[coding](disk, node, rack))
            {
                num_loss = num_loss + 1;
                TRACE_DLOSS(d, time);
            }
        }
        else if (type == DISK_SCRUB)
        {
            //����ô��̵�������������
            TRACE_SCRUB(d, time);
            remove_disk_from_array(ld_disks, d);
            //������ϴ��ɣ���������ѷ�����Ǳ�ڴ���������һ����ϴ��
            event_scrub_for_disk(d, time);
        }
        else if (type == NODE_FAIL)
        {
            // ����ýڵ��ϵ����д��̵�����¼���������ڵ���ϱ�Ǻ��޸��¼�
            TRACE_NODE_FAIL(d, time);

            // 1. ������node�µ����д��̣������array��heap�����
            for (int i = 0; i < disk; ++i ) {
                disk_struct to_delete = {i, d.node_no, d.rack_no};
                remove_disk_from_array(warn_disks, to_delete);
                remove_disk_from_array(ld_disks, to_delete);
                remove_disk_from_array(failed_disks, to_delete);

                remove_event_from_heap(to_delete, DISK_LD);
                remove_event_from_heap(to_delete, DISK_SCRUB);
                remove_event_from_heap(to_delete, DISK_WARN);
                remove_event_from_heap(to_delete, DISK_REPAIRED);
            }

            // 2. ����node���ϱ�ǣ�������node����һ���޸�
            add_disk_to_array(failed_nodes, d);
            event_repair_for_node(d, time);

            //�ж��Ƿ�dataloss
            if (func[coding](disk, node, rack))
            {
                num_loss = num_loss + 1;
            }
        }
        else if (type == NODE_REPAIRED)
        {
            // ɾ����node�µ�ȫ���¼�
            TRACE_NODE_REPAIR(d, time);
            // 1. ������node�µ����д��̣������array��heap�����
            for (int i = 0; i < disk; ++i ) {
                disk_struct to_delete = {i, d.node_no, d.rack_no};
                remove_disk_from_array(warn_disks, to_delete);
                remove_disk_from_array(ld_disks, to_delete);
                remove_disk_from_array(failed_disks, to_delete);

                remove_event_from_heap(to_delete, DISK_LD);
                remove_event_from_heap(to_delete, DISK_SCRUB);
                remove_event_from_heap(to_delete, DISK_WARN);
            }
            // 2. ɾ��node�����¼�
            remove_disk_from_array(failed_nodes, d);

            // �������ɸ�node�µ�ȫ���¼�
            for (int i = 0; i < disk; ++i ) {
                disk_struct to_new = {i, d.node_no, d.rack_no};
                events_for_new_disk(to_new);
            }
            event_failure_for_node(d);
        }
        else if (type == DISK_WARN)
        { 
            //����Ԥ���������޸��¼�
            TRACE_WARN(d, time);
            add_disk_to_array(warn_disks, d);
            event_repair_for_disk(d, time);
        }
        else if (type == DISK_REPAIRED)
        {
            // �����޸���ɣ�ɾ�������ϵ������¼�
            TRACE_REPAIR(d, time);
            remove_disk_from_array(failed_disks, d);
            remove_disk_from_array(warn_disks, d);
            remove_disk_from_array(ld_disks, d);

            // ɾ�����Ϲ���������̵������¼� ?
            remove_event_from_heap(d, DISK_LD);
            remove_event_from_heap(d, DISK_SCRUB);
            remove_event_from_heap(d, DISK_FAIL);
            remove_event_from_heap(d, DISK_WARN);

            // ���������µĴ����ϵ������¼�
            events_for_new_disk(d);
        }
        else if (type == DISK_FAIL)
        { 
            // ��¼���ϣ�����array��heap���Ƴ���ض���
            TRACE_FAIL(d, time);
            add_disk_to_array(failed_disks, d);
            // ��Ϊ��ǰ�����Ѿ����ϣ����е�LD,SCRUB,WARN��û�д��ڵ�����
            remove_disk_from_array(ld_disks, d);
            remove_disk_from_array(warn_disks, d);

            remove_event_from_heap(d, DISK_LD);
            remove_event_from_heap(d, DISK_SCRUB);
            remove_event_from_heap(d, DISK_WARN);

            // ������ǰ�̵��޸��������dataloss�޹أ�����ʲô����£���Ӧ��׼���޸��˴��̣�
            event_repair_for_disk(d, time);

            //�ж��Ƿ�dataloss
            if (func[coding](disk, node, rack))
            {
                num_loss = num_loss + 1;
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
    double result[10000];
    int i, reps1, num_dloss, reps2, add_reps1 = 0, add_loss = 0;
    double pro_loss, mttdl, t;

    if (argc < 19)
    {
        printf("Usage: mttdlsim coding (Twocopy/Threecopy) disks nodes racks  Lyita_op Lbita_op Lgama_rest Lyita_rest Lbita_rest Lyita_ld, Lbita_ld, Lgama_scrub, Lyita_scrub, Lbita_scrub Lyita_tia Lbita_tia FDR t reps\n"); //fdr 90.59
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
	//./sim_predict Threecopy 4 15 60 461386 1.1 6 12 2 9259 1 6 168 3 354 1 80 43800 10
	//  0.000897

	// new version
	//./sim_predict Threecopy 4 15 60 461386 1.1 6 12 2 9259 1 6 168 3 354 1 461386 1.1  6 12 2  80 43800 10
	//  0.000897
    disk = atoi(argv[2]); // 4
    node = atoi(argv[3]); // 15
    rack = atoi(argv[4]); // 60
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
    Lyita_tia = atof(argv[15]); // 354 14.75��
    Lbita_tia = atof(argv[16]); // 1
	Lyita_node = atof(argv[17]); // node 461386 
	Lbita_node = atof(argv[18]); // 1.1
	Lgama_node_rest = atof(argv[19]); // node repair 6
	Lyita_node_rest = atof(argv[20]); //12
	Lbita_node_rest = atof(argv[21]); //2
    fdr = atof(argv[22]); // 80
    t = atof(argv[23]); // 43800 5��
    reps2 = atof(argv[24]); // 10
    srand((unsigned)time(NULL));

    mttdl = 0.0;
    gsl_rng_env_setup();
    T = gsl_rng_default;
    r = gsl_rng_alloc(T);

    gsl_rng_set(r, time(NULL));

    for (i = 0; i < reps2; i++)
    {
        num_dloss = 0;
        reps1 = 0;
        while (num_dloss < 5)
        {
            initialize();
            num_dloss = num_dloss + sim_num_loss(t);
            reps1++; //�������ݶ�ʧ��ѭ������
            if( (reps1+1)%10 == 0){
            	printf("...repeat %d...\n",reps1);
	            printf("...num_dloss %d...\n",num_dloss);
        	    fflush(stdout);
            } 
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
 *  heap_insert: ����event
 *  heap_search: ����event
 *  heap_delete: ɾ������������event
 ****************************************************************/
inline void heap_insert(long time, int type, int disk_no, int node_no, int rack_no)
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

inline int heap_search(int type, int disk_no, int node_no, int rack_no)
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

inline void __MinHeapIfy(int i)
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

//x�ڵ��time��С����С��ֵ
inline void __HeapDecreaseKey(int x)
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

inline int __checkheap()
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

//ɾ������ָ��Ԫ��i
inline void heap_delete(int i)
{

    event_heap[0] = event_heap[i];
    event tmp = event_heap[events];

    if (event_heap[i].time == tmp.time)
    {
        event_heap[i] = tmp;
        events--;
    }
    else if (event_heap[i].time < tmp.time) //iΪ���ڵ�ķ�֧����С�����ʿ����⵽�ƻ���Ҫ���е���
    {
        event_heap[i] = tmp;
        events--;
        __MinHeapIfy(i);
    }
    else //i�ڵ��ֵ��С����С��tmp
    {
        event_heap[i] = tmp;
        events--;
        __HeapDecreaseKey(i);
    }
}


// check whether data loss happened
int Threecopy_failed(int d, int n, int r)
{
	int i, j, k, pro;
	int rack_no1, rack_no2, rack_no3;
	int node_no1, node_no2, node_no3;
	int disk_no1, disk_no2, disk_no3;
	pro = ((r - 1) * (n - 1) * n * d * d);

	if (((failed_disks.end + MAX_DISKS - failed_disks.front) % MAX_DISKS) <= 2) {
		return 0;
	}

	for (i = failed_disks.front; i != (failed_disks.end + MAX_DISKS - 2) % MAX_DISKS; i++, i %= MAX_DISKS)
	{
		rack_no1 = failed_disks.disks[i].rack_no;
		node_no1 = failed_disks.disks[i].node_no;
		for (j = (i + 1) % MAX_DISKS; j != (failed_disks.end + MAX_DISKS - 1) % MAX_DISKS; j++, j %= MAX_DISKS)
		{
			rack_no2 = failed_disks.disks[j].rack_no;
			node_no2 = failed_disks.disks[j].node_no;
			for (k = (j + 1) % MAX_DISKS; k != failed_disks.end; k++, k %= MAX_DISKS)
			{
				rack_no3 = failed_disks.disks[k].rack_no;
				node_no3 = failed_disks.disks[k].node_no;
				if ((rack_no1 == rack_no2 && node_no1 != node_no2 && rack_no1 != rack_no3) || (rack_no1 == rack_no3 && node_no1 != node_no3 && rack_no1 != rack_no2) || (rack_no2 == rack_no3 && node_no2 != node_no3 && rack_no2 != rack_no1))
					return 1;
			}
		}
	}


	return 0;
}
