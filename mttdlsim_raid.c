// - ������ϴ Τ���ֲ� ����Ǳ���������� ������ϴ
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_statistics.h>
#include <time.h>

#define MAX_DISKS 100000

#define MAX_ROW 40

#define DISK_FAIL 0
#define DAILY_REPAIR 1
#define DISK_REPAIRED 2
#define DISK_WARN 3
#define DISK_LD 4
#define DISK_SCRUB 5

#define RAID0 0
#define RAID1 1
#define RAID5 2
#define RAID6 3
#define D2        4
#define FULL2    5
#define MPDC    6
#define MPPDC    7

typedef struct {
	int type;
	long time;
	int disk_no;
} event;

event event_heap[MAX_DISKS * 2];
int events;

int failed_disks[MAX_DISKS];
int front, end, rend;
int warn_disks[MAX_DISKS];
int front_warn, end_warn, rend_warn;
int ld_disks[MAX_DISKS];
int front_ld, end_ld, rend_ld;

const gsl_rng_type *T;
gsl_rng *r;

typedef int (*array_failed)(int n);

typedef struct{
	int first;
	int second;
}volelem;

volelem vol_matrix[MAX_DISKS];
int row_matrix[2*MAX_ROW][MAX_ROW];

int failed_set[MAX_DISKS];
int send;

int lend[2*MAX_ROW];

double result[10000];
int RAID0_failed(int n)
{
	if (((end + MAX_DISKS - front) % MAX_DISKS) > 0)
		return 1;
	return 0;
}
int RAID1_failed(int n)
{
	int i,j,ie,je;
	if (((end + MAX_DISKS - front) % MAX_DISKS) < 2)
		return 0;
	je = (end + MAX_DISKS - 1) % MAX_DISKS;
	ie = (je + MAX_DISKS - 1) % MAX_DISKS;
	for (i = front; i != ie; i++, i %= MAX_DISKS)
		for (j = (i + 1) % MAX_DISKS; j != je; j++, j %= MAX_DISKS) {
			if(((failed_disks[i]%2==0)&&(failed_disks[j]==(failed_disks[i]+1)))||((failed_disks[i]%2==1)&&(failed_disks[j]==(failed_disks[i]-1))))
				return 1;
		}
	return 0;    
}
int RAID5_failed(int n)
{
	int fail_no, ld_no;
	int i;

	if (((end + MAX_DISKS - front) % MAX_DISKS) > 1)
	{  // printf("dataloss failure + failure fail_no %d....fail_no %d...\n",failed_disks[front],failed_disks[front+1]);
		return 1;
	}

	if ((((end + MAX_DISKS - front) % MAX_DISKS) == 1) && (end_ld != front_ld))
	{
		fail_no=failed_disks[front];
		for (i=front_ld; i!=end_ld; i++, i%= MAX_DISKS) 
		{  
			ld_no=ld_disks[i];
			if (fail_no != ld_no)
			{// printf("dataloss failure + ld fail_no %d....ld_no %d...\n",fail_no,ld_no);
				return 1; 
			}
		}
	}

	return 0;
}

int RAID6_failed(int n)
{
	int fail_no1, fail_no2, ld_no;
	int i, j;

	if (((end + MAX_DISKS - front) % MAX_DISKS) > 2)
		return 1;

	
	if (((end + MAX_DISKS - front) % MAX_DISKS) == 2 && (end_ld != front_ld))
	{ 
		j=front;	
		fail_no1=failed_disks[j];	 
		fail_no2=failed_disks[(j+1)% MAX_DISKS];
		for (i=front_ld; i!=end_ld; i++, i%= MAX_DISKS) 
		{  
			ld_no=ld_disks[i];
			if ((fail_no1 != ld_no) && (fail_no2 != ld_no))
			{
				return 1; 
			}
		}
	}

	return 0;
}


array_failed func[] = { RAID0_failed, RAID1_failed, RAID5_failed, RAID6_failed};

extern int __checkheap();

void heap_insert(long  time, int type, int disk_no)
{
	/*if(!__checkheap()){

	  printf("checkheap failed before heap_insert\n");
	  exit(1);
	  }*/
    
    /*
    if(type == DISK_LD || type == DISK_SCRUB){
        return;
    }
    */
    
    
	int i, p;
	int j;
	//	if (type != DISK_LD )
	for (j=1; j <=events; j++) 
		if (event_heap[j].type == type && event_heap[j].disk_no == disk_no)
			return;

	events++;
	i = events;
	p = i / 2;
	while (p > 0 && event_heap[p].time > time) {
		event_heap[i] = event_heap[p];
		i = p;
		p = i / 2;
	}
	event_heap[i].time = time;
	event_heap[i].type = type;
	event_heap[i].disk_no = disk_no;
	/*if(!__checkheap()) {
	  printf("checkheap failed after heap_insert. time %ld, disk_no %d, type %d\n", time, disk_no, type);
	  exit(1);
	  }*/
}

int heap_search(int type, int disk_no)
{
	int i;
	for (i=1; i<=events ; i++)
	{
		if (event_heap[i].type == type && event_heap[i].disk_no == disk_no)
		{
			return i;
		}
	}
	return -1;
}

static void __MinHeapIfy(int i)
{
	int min=i;
	int left=2*i;
	int right=left+1;

	if(left <= events && event_heap[i].time > event_heap[left].time)
	{
		min=left;
	}
	else
	{
		min=i;
	}
	if(right <= events && event_heap[min].time > event_heap[right].time)
	{
		min=right;
	}
	if(i != min)
	{
		event tmp = event_heap[i];
		event_heap[i] = event_heap[min];
		event_heap[min] = tmp;
		__MinHeapIfy(min);
	}
}

//x�ڵ��time��С����С��ֵ
static void __HeapDecreaseKey(int x)
{
	event tmp;
	while(x > 1 && event_heap[x>>1].time > event_heap[x].time)
	{
		tmp = event_heap[x];
		event_heap[x] = event_heap[x>>1];
		event_heap[x>>1] = tmp;
		x >>= 1;
	}
}

int __checkheap()
{
	int i, left, right;
	int end = events>>1;
	for(i=1; i<=end; ++i)
	{
		if(event_heap[i].time<0)
			return 0;
		left = 2*i;
		right = left + 1;
		if(left <= events && event_heap[i].time > event_heap[left].time)
			return 0;
		if(right <= events && event_heap[i].time > event_heap[right].time)
			return 0;        
	}
	return 1;
}

//ɾ������ָ��Ԫ��i
void heap_delete(int i)
{
	/*if(!__checkheap()) {
	  printf("checkheap failed before heap_delete\n");
	  exit(1);
	  }*/
	event_heap[0] = event_heap[i];
	event tmp = event_heap[events];

	if(event_heap[i].time == tmp.time)
	{
		event_heap[i]=tmp;
		events--;
	}
	else if(event_heap[i].time < tmp.time)//iΪ���ڵ�ķ�֧����С�����ʿ����⵽�ƻ���Ҫ���е���
	{
		event_heap[i]=tmp;
		events--;
		__MinHeapIfy(i);
	}
	else //i�ڵ��ֵ��С����С��tmp
	{
		event_heap[i]=tmp;
		events--;
		__HeapDecreaseKey(i);
	}
	/*if(!__checkheap()) {
	  printf("checkheap failed after heap_delete\n");
	  exit(1);
	  }*/
}

/*void new_failure(long now_time, int disk_no, double Lyita_op, double Lbita_op, double Lyita_tia, double Lbita_tia, double fdr)
  {
  long time;
  long time_tia;
  struct timeval tp;

  gsl_rng_env_setup();
  T = gsl_rng_default;
  r = gsl_rng_alloc(T);
  gettimeofday(&tp, NULL);
  gsl_rng_set(r, tp.tv_usec);

  time = now_time + gsl_ran_weibull(r, Lyita_op, Lbita_op);
  heap_insert(time, DISK_FAIL, disk_no);
  printf("time....%lf\n",time);

  if( (rand()%10000) < fdr * 100 )
  {
  time_tia = 1+gsl_ran_weibull(r, Lyita_tia, Lbita_tia);
  heap_insert((time - time_tia)>=0 ? (time - time_tia) : 0, DISK_WARN, disk_no);  
  printf("time_tia....%lf\n",time_tia);
  }

  } */

void initialize(int n, double Lyita_op, double Lbita_op, double Lyita_ld, double Lbita_ld, double Lgama_scrub, double Lyita_scrub, double Lbita_scrub, double Lyita_tia, double Lbita_tia, double fdr)
{
	int i;
	long time_op;
	long time_tia;
	long time_ld;
	long time_scrub;

	events = 0;
	for (i = 1; i <= n; i++) {
		time_op = gsl_ran_weibull(r, Lyita_op, Lbita_op);
		heap_insert(time_op, DISK_FAIL, i);
		if( (rand()%10000) < fdr * 100 )
		{
			time_tia = 1+gsl_ran_weibull(r, Lyita_tia, Lbita_tia);
			heap_insert((time_op - time_tia)>=0 ? (time_op - time_tia) : 0, DISK_WARN, i);  
		}
		time_ld = gsl_ran_weibull(r, Lyita_ld, Lbita_ld);
		heap_insert(time_ld, DISK_LD, i);
		time_scrub = Lgama_scrub + gsl_ran_weibull(r, Lyita_scrub, Lbita_scrub);
		heap_insert(time_scrub, DISK_SCRUB, i);
	}

	//	printf("initialize...time %d...time_scrub %d \n",time, time_scrub);

}
//ģ��һ��tʱ��Ķ�ʧ����
int sim_num_loss(int coding, int n, double Lyita_op, double Lbita_op, double Lgama_rest, double Lyita_rest, double Lbita_rest, double Lyita_ld, double Lbita_ld, double Lgama_scrub, double Lyita_scrub, double Lbita_scrub, double Lyita_tia, double Lbita_tia, double fdr, double t)
{
	// long life;
	int disk_no,type, num;
    long time;
	long time_ld;
	long time_scrub;
	long time_mttr;
	long time_op;
	long time_tia;
	struct timeval tp;
	int num_loss=0;//���ݶ�ʧ����

	int i,j,k;

	gettimeofday(&tp, NULL);
	heap_delete(1);
	while (event_heap[0].time <= t) {
        type = event_heap[0].type;
        time = event_heap[0].time;
        disk_no = event_heap[0].disk_no;
        
		//����Ǳ�ڴ���
		if (type == DISK_LD) {
			ld_disks[end_ld] = disk_no;
			end_ld++;
			end_ld %= MAX_DISKS; 
			/*			printf("when a ld, ld_disks front_ld..%d....end_ld,,%d\n",front_ld,end_ld);
						int i;
						for (i=front_ld; i!=end_ld; i++, i%=MAX_DISKS)
						printf("when a ld, ld_disks....%d\n",ld_disks[i]);
						printf("when a ld, failed_disks front..%d....end,,%d\n",front,end); 
						for (i=front; i!=end; i++, i%=MAX_DISKS)
						printf("when a ld, failed_disks....%d\n",failed_disks[i]);  */
			if (func[coding](n)) {
				//dataloss�����ˣ��Թ��ϣ�failed_disks������Ԫ�أ��̽��д������̣��������ɹ���Ԥ����Ǳ�ڴ���
				for (; front != end; front++, front %= MAX_DISKS)
				{
					num= heap_search(DISK_REPAIRED, failed_disks[front]);
					if (num > 0)  heap_delete(num);
					time_op = time + gsl_ran_weibull(r, Lyita_op, Lbita_op);
					heap_insert(time_op, DISK_FAIL, failed_disks[front]);
					if( (rand()%10000) < fdr * 100 )
					{
						time_tia = 1+gsl_ran_weibull(r, Lyita_tia, Lbita_tia);
						heap_insert((time_op - time_tia)>=time ? (time_op - time_tia) : time, DISK_WARN, failed_disks[front]);  
					}
					
					time_ld = time + gsl_ran_weibull(r, Lyita_ld, Lbita_ld);
					heap_insert(time_ld, DISK_LD, failed_disks[front]);
							
					time_scrub = time + Lgama_scrub + gsl_ran_weibull(r, Lyita_scrub, Lbita_scrub);
					heap_insert(time_scrub, DISK_SCRUB, failed_disks[front]);	
				}									
				num_loss=num_loss+1;
			}
			//����Ǳ�ڴ�����̣����ܻ��ٴη���Ǳ�ڴ���
			time_ld = time + gsl_ran_weibull(r, Lyita_ld, Lbita_ld);
			heap_insert(time_ld, DISK_LD, disk_no); 			
		}
		else if (type == DISK_SCRUB) {
            
			//�������ϴ���̵�������������
			j = 0;
			for (i=front_ld; i != end_ld; i++, i %= MAX_DISKS) {
				if (disk_no == ld_disks[i]) {	
                    ++j;
                }else{
                    ld_disks[(i+MAX_DISKS-j) % MAX_DISKS] = ld_disks[i];
                }
				}
			end_ld = (end_ld+MAX_DISKS-j) % MAX_DISKS;

			//������ϴ��ɣ���������ѷ�����Ǳ�ڴ���������һ����ϴ��
			time_scrub = time + Lgama_scrub + gsl_ran_weibull(r, Lyita_scrub, Lbita_scrub);
			heap_insert(time_scrub, DISK_SCRUB, disk_no);

			//				printf("new scrub.....time_scrub %d\n", time_scrub);
		}
		else if (type == DISK_WARN) { //����Ԥ���������޸��¼�
			warn_disks[end_warn] = disk_no;
			time_mttr = Lgama_rest + gsl_ran_weibull(r, Lyita_rest, Lbita_rest);
			heap_insert(time + time_mttr, DISK_REPAIRED, disk_no);
			end_warn++;
			end_warn %= MAX_DISKS;
		} 
		else if (type == DISK_REPAIRED) {//�޸����
			int flag=0;
			if ((end + MAX_DISKS - front) % MAX_DISKS > 0) {//����޸����ǹ����̣�failed_disks����ȥ������
				j = 0;
				for (i=front; i != end; i++, i %= MAX_DISKS) {
				if (disk_no == failed_disks[i]) {	
                    ++j;
					flag=1;
                }else{
                    failed_disks[(i+MAX_DISKS-j) % MAX_DISKS] = failed_disks[i];
                }
            }
            end = (end+MAX_DISKS-j) % MAX_DISKS;
				
			}	
			if ( (end_warn + MAX_DISKS - front_warn) % MAX_DISKS > 0 && flag != 1) {//�޸�����Ԥ����warn_disksȥ����Ԥ��
				j = 0;
				for (i=front_warn; i != end_warn; i++, i %= MAX_DISKS) {
				if (disk_no == warn_disks[i]) {	
                    ++j;
					flag=2;
                }else{
                    warn_disks[(i+MAX_DISKS-j) % MAX_DISKS] = warn_disks[i];
                }
				}
				end_warn = (end_warn+MAX_DISKS-j) % MAX_DISKS;			
			}

			if (flag == 1) {//�����̱��޸��������̣���������δ�����Ϻ�Ԥ����LD��SCRUB
				//              printf("repaired a failure...disk_no %d....time_mttr %d\n",disk_no, time);			
				time_op = time + gsl_ran_weibull(r, Lyita_op, Lbita_op);
				heap_insert(time_op, DISK_FAIL,  disk_no);    	   
				if( (rand()%10000) < fdr * 100 )
				{
					time_tia = 1+gsl_ran_weibull(r, Lyita_tia, Lbita_tia);
					heap_insert((time_op - time_tia)>=time ? (time_op - time_tia) : time, DISK_WARN,  disk_no);  
				}
				time_ld = time + gsl_ran_weibull(r, Lyita_ld, Lbita_ld);
				heap_insert(time_ld, DISK_LD, disk_no);	

				time_scrub = time + Lgama_scrub + gsl_ran_weibull(r, Lyita_scrub, Lbita_scrub);
				heap_insert(time_scrub, DISK_SCRUB, disk_no);				
			}else if (flag == 2 )   {//Ԥ�����޸���ȥ�������Ĺ��ϣ�������
				num= heap_search(DISK_FAIL, disk_no);
				if (num > 0) {
					heap_delete(num);
					time_op = time + gsl_ran_weibull(r, Lyita_op, Lbita_op);
					heap_insert(time_op, DISK_FAIL,  disk_no);       
					if( (rand()%10000) < fdr * 100 )
					{
						time_tia = 1+gsl_ran_weibull(r, Lyita_tia, Lbita_tia);
						heap_insert((time_op - time_tia)>=time ? (time_op - time_tia) : time, DISK_WARN,  disk_no);  
					}  
					i=heap_search(DISK_LD, disk_no); //�����������̵�ld
					if (i>0) {
						heap_delete(i);
						time_ld = time + gsl_ran_weibull(r, Lyita_ld, Lbita_ld);
						heap_insert(time_ld, DISK_LD, disk_no);	
					}				   
				}
				//ȥ����Ԥ�����Ѿ�������LD����
				j = 0;
				for (i=front_ld; i != end_ld; i++, i %= MAX_DISKS) {
				if (disk_no == ld_disks[i]) {	
                    ++j;
                }else{
                    ld_disks[(i+MAX_DISKS-j) % MAX_DISKS] = ld_disks[i];
                }
				}
				end_ld = (end_ld+MAX_DISKS-j) % MAX_DISKS;
				
			}
			else {
				printf("error in fail/warn repaired %d\n", disk_no);
				printf("failed number %d, warn number %d \n", (end+MAX_DISKS-front)%MAX_DISKS, (end_warn+MAX_DISKS-front_warn)%MAX_DISKS);
				for (; front != end; front++, front %= MAX_DISKS) 
					printf("failed %d \n ",failed_disks[front]);
				for (;front_warn != end_warn; front_warn++, front_warn %= MAX_DISKS)
					printf("warn %d  \n",warn_disks[front_warn]);

				exit(1);
			}
		} 
		else if (type == DISK_FAIL) {//���Ϸ���������failed_disks���飬�ж��Ƿ�dataloss
			failed_disks[end] = disk_no;
			end++;
			end %= MAX_DISKS;
			
			//���������Ƿ��Ѿ�������Ǳ�ڴ����������ld_disks������ɾ��	
            j = 0;
            for (i=front_ld; i != end_ld; i++, i %= MAX_DISKS) {
				if (disk_no == ld_disks[i]) {	
                    ++j;
                }else{
                    ld_disks[(i+MAX_DISKS-j) % MAX_DISKS] = ld_disks[i];
                }
            }
            end_ld = (end_ld+MAX_DISKS-j) % MAX_DISKS;
            /*  
			for (i=front_ld; i != end_ld; i++, i %= MAX_DISKS) {
				if (disk_no == ld_disks[i]) {			       
					for (; front_ld !=i; front_ld++, front_ld %= MAX_DISKS)	{
						ld_disks[end_ld]=ld_disks[front_ld];
						end_ld++;
						end_ld %= MAX_DISKS;
					}	
					front_ld++;
					front_ld %= MAX_DISKS;
				}
			}*/

			//�������Ƿ��й����̵�δ��Ǳ�ڴ���ʹ�����ϴ�¼������У�ȥ�������޸����ʱ�ٶ���������δ��Ǳ�ڴ���					
			num= heap_search(DISK_LD, disk_no); 	
			if (num > 0)  heap_delete(num);	  
			num= heap_search(DISK_SCRUB, disk_no); 	
			if (num > 0)  heap_delete(num);	

			if (func[coding](n)) {
				for (; front != end; front++, front %= MAX_DISKS)
				{
					num= heap_search(DISK_REPAIRED, failed_disks[front]);
					if (num > 0)  heap_delete(num);
					time_op = time + gsl_ran_weibull(r, Lyita_op, Lbita_op);
					heap_insert(time_op, DISK_FAIL, failed_disks[front]);
					if( (rand()%10000) < fdr * 100 )
					{
						time_tia = 1+gsl_ran_weibull(r, Lyita_tia, Lbita_tia);
						heap_insert((time_op - time_tia)>=time ? (time_op - time_tia) : time, DISK_WARN, failed_disks[front]);  
					}

					//���¶���������δ��Ǳ�ڴ������ϴ�¼�				
					time_ld = time + gsl_ran_weibull(r, Lyita_ld, Lbita_ld);
					heap_insert(time_ld, DISK_LD, failed_disks[front]);	

					time_scrub = time + Lgama_scrub + gsl_ran_weibull(r, Lyita_scrub, Lbita_scrub);
					heap_insert(time_scrub, DISK_SCRUB, failed_disks[front]);		 
				}	

				num_loss=num_loss+1;
			}
			else { //����û��������ݶ�ʧ�������޸��¼�
				time_mttr =Lgama_rest + gsl_ran_weibull(r, Lyita_rest, Lbita_rest);
				heap_insert(time + time_mttr, DISK_REPAIRED, disk_no);

			}
		} 
		else {
			printf("BUG\n");
			exit(-1);
		}
		heap_delete(1);
	}
	return num_loss;
}

int main(int argc, char *argv[])
{
	int coding, n, i;
	double Lyita_op, Lbita_op, Lgama_rest, Lyita_rest, Lbita_rest, Lyita_ld, Lbita_ld, Lgama_scrub, Lyita_scrub, Lbita_scrub, Lyita_tia, Lbita_tia, fdr, t;
	int reps1, num_dloss,reps2;
	double pro_loss;
	int add_reps1=0, add_loss=0;
	if (argc < 17) {
		printf("Usage: mttdlsim coding(RAID0/RAID1/RAID5/RAID6) num_disks Lyita_op Lbita_op Lgama_rest Lyita_rest Lbita_rest Lyita_ld, Lbita_ld, Lgama_scrub, Lyita_scrub, Lbita_scrub Lyita_tia Lbita_tia FDR t reps"); //fdr 90.59 
		//������RAID5 12���̸��� ���������������� ������״���� �޸�λ�ò��� �޸��������� �޸���״���� Ǳ�ڴ������� Ǳ�ڴ�����״ ��ϴλ�� ��ϴ���� ��ϴ��״ ��ǰʱ������ ��ǰʱ����״ ׼ȷ��90.54 �������� �ظ�����
		return -1;
	}

	if (!strcmp(argv[1], "RAID0"))
		coding = RAID0;
	else if (!strcmp(argv[1], "RAID1"))
		coding = RAID1;
	else if (!strcmp(argv[1], "RAID5"))
		coding = RAID5;
	else if (!strcmp(argv[1], "RAID6"))
		coding = RAID6; 
	else {
		printf("coding BUG\n");
		return -1;
	}
	n = atoi(argv[2]);
	Lyita_op = atof(argv[3]);
	Lbita_op = atof(argv[4]);
	Lgama_rest = atof(argv[5]);
	Lyita_rest = atof(argv[6]);
	Lbita_rest = atof(argv[7]);
	Lyita_ld = atof(argv[8]);
	Lbita_ld = atof(argv[9]);
	Lgama_scrub = atof(argv[10]);
	Lyita_scrub = atof(argv[11]);
	Lbita_scrub = atof(argv[12]);
	Lyita_tia = atof(argv[13]);
	Lbita_tia = atof(argv[14]);
	fdr = atof(argv[15]);
	t = atof(argv[16]);
	reps2 = atof(argv[17]);

	//comment out rand seed for test
	srand( (unsigned)time( NULL ) );

	/*
	 * */

	gsl_rng_env_setup();
	T = gsl_rng_default;
	r = gsl_rng_alloc(T);


	struct timeval tp;
	gettimeofday(&tp, NULL);

	//comment out rand seed for test
	gsl_rng_set(r, tp.tv_usec);

	for (i = 0; i < reps2; i++) {
		num_dloss=0;
		reps1=0;
		while (num_dloss < 10)
		{
			//printf("%d mttdl: %d \n", i, result[i]);
			//  if( (i+1)%100 == 0){
			//      printf("...repeat %d...", i+1);
			//      fflush(stdout);
			//  }
			initialize( n, Lyita_op, Lbita_op, Lyita_ld, Lbita_ld, Lgama_scrub, Lyita_scrub, Lbita_scrub, Lyita_tia, Lbita_tia, fdr);
			front = 0;
			end = 0;
			front_warn = 0;
			end_warn = 0;
			front_ld = 0;
			end_ld = 0;

			num_dloss=num_dloss+sim_num_loss(coding, n, Lyita_op, Lbita_op, Lgama_rest, Lyita_rest, Lbita_rest, Lyita_ld, Lbita_ld, Lgama_scrub, Lyita_scrub, Lbita_scrub, Lyita_tia, Lbita_tia, fdr, t);  
			reps1++;//�������ݶ�ʧ��ѭ������
			/*	if( (reps1+1)%10 == 0){
				printf("...repeat %d...\n",reps1);
				printf("...num_dloss %d...\n",num_dloss);
				fflush(stdout);
				} */
		}
		add_loss=add_loss+num_dloss;
		add_reps1=add_reps1+reps1;
		result[i]=(double)num_dloss/(double)reps1;
		printf("...pro_dloss %f\n",result[i]);

	}
	pro_loss=gsl_stats_mean(result , 1 , reps2);
	printf("\naverage = %f\n", pro_loss);
	printf("...add_loss %d....add_reps1 %d...pro_loss %lf \n",add_loss, add_reps1,1.0*add_loss/add_reps1);
	printf("\nvariance = %f\n", gsl_stats_variance_with_fixed_mean(result,1,reps2,pro_loss));
	//  printf("\num_dataloss = %d\n", num_dloss);
	//  printf("\resp = %d\n", reps1);
	gsl_rng_free(r);
	return 0;
}
