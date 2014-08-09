#include <stdio.h>
#include <stdlib.h>
#include "timers_lib.h"
#include <sched.h>


int MAX_VALUES=16;

struct node_t{
    int enq_count;
    int deq_count;
	int value[16];//TODO: maybe change this
	struct node_t * next;
};

struct queue_t{
	struct node_t * Head;
	struct node_t * Tail;
    int lock;
};


struct pub_record{ //TODO: pad?? to avoid false sharing.
    long long int temp;
    int pending; //1 = waiting for op
    long long int temp2;
    int op; // operation 1= enqueue , 0= dequeue
    long long int temp3;
    int val;
    long long int temp4;
    int response; //1 means reposponse is here!
    long long int temp5;
};

int ERROR_VALUE=500004;

long long int glob_counter=0; 

long long int count_enqs=0;
long long int count_deqs=0;
void lock_queue (struct queue_t * Q){


    while (1){
        if (!Q->lock){
            if(!__sync_lock_test_and_set(&(Q->lock),1)) break;
        }
    }   
}

void unlock_queue(struct queue_t * Q){

    Q->lock = 0;
}

void initialize(struct queue_t * Q,struct pub_record * pub_enq,struct pub_record * pub_deq,int n){//TODO: init count?
	int i;
    struct node_t * node = (struct node_t *) malloc(sizeof(struct node_t));
	node->next = NULL;
    node->enq_count=0;
    node->deq_count=0;
	Q->Head = node; //TODO: check this
	Q->Tail = node;
    Q->lock = 0;
    for(i=0; i <n ;i++){
        pub_enq[i].pending = 0;
        pub_enq[i].response =0;
        pub_deq[i].pending = 0;
        pub_deq[i].response =0;
    }
}


void enqueue(struct queue_t * Q, int val){

    struct node_t *  tail=Q->Tail;
    if(tail->enq_count!=MAX_VALUES){// fat node not full
        tail->value[tail->enq_count]=val;
        tail->enq_count++;
    }
    else{
        struct node_t * node = (struct node_t *) malloc(sizeof(struct node_t));
        node->deq_count=0;
        node->value[0]=val;
        node->next=NULL;
        node->enq_count=1;
        Q->Tail->next=node;
        Q->Tail=node;
    }
}


int dequeue(struct queue_t * Q, int * val){

    struct node_t * head=Q->Head; //TODO: fix empty queue!!
    struct node_t * next=head->next;
    if((head->deq_count>=head->enq_count)&&(next==NULL))return 0;//queue is empty
    if(head->deq_count<MAX_VALUES){
        *val=head->value[head->deq_count];
        head->deq_count++;
    }
    else{
        Q->Head=Q->Head->next;
        free(head);
        head=Q->Head;
        next=head->next;
        //if((next==NULL)&&(head->val_count==0)) return 0;
        head->deq_count=1;
        *val=head->value[0];
    }
    return 1;
}

int try_access(struct queue_t * Q,struct pub_record *  pub,int operation, int val,int n){

    int tid=  omp_get_thread_num();
    int i,res,count;
    //1
    
    
    pub[tid].op = operation;
    pub[tid].val = val;
    pub[tid].pending=1;
    while (1){
        if(Q->lock){
            count=0;
            while((!pub[tid].response)&&(count<10000000)) count++; //check periodicaly for lock
            if(pub[tid].response ==1){
                pub[tid].response=0;
                return (pub[tid].val);
            }
        }
        else{
            if(__sync_lock_test_and_set(&(Q->lock),1)) continue;// must spin backto response
            else{
                glob_counter++;
                for(i=0 ;i<n; i++){
                    if(pub[i].pending){
                        if (pub[i].op ==1) {count_enqs++; enqueue(Q,pub[i].val);}
                        else if(pub[i].op==0){
                           count_deqs++;
                           res=dequeue(Q,&pub[i].val);
                           if(!res) 
                                    pub[i].val = ERROR_VALUE;
                        }
                        else printf("wtf!!  %d \n",pub[i].op);
                        pub[i].pending = 0;
                        pub[i].response = 1;
                    }
                }
                int temp_val=pub[tid].val;
                pub[tid].response=0;
                Q->lock=0;
                return temp_val;
            }
        }
   }
}

void printqueue(struct queue_t * Q){
    
    struct node_t * curr ;
    struct node_t * next ;
    int i;
    curr = Q->Head;
    next = Q->Head->next;
    while (curr != Q->Tail){
        for(i=curr->deq_count;i<curr->enq_count;i++){
            printf("%d ",curr->value[i]);
        }
        curr = next;
        next = curr ->next;
    }
    printf("%d ",curr->value);
    printf("\n");
    
}


int main(int argc, char *argv[]){

    int res,val,i,j,num_threads,count,result; 
    
    num_threads=atoi(argv[1]);
    count =atoi(argv[2]);


	struct queue_t * Q = (struct queue_t *) malloc(sizeof(struct queue_t));

    struct pub_record pub_enq[num_threads];
    struct pub_record pub_deq[num_threads];
    //Q->Head =  NULL;
    //Q->Tail =  NULL;
	initialize(Q,pub_enq,pub_deq,num_threads);
    /*result = try_access(Q,pub,1,5,num_threads);
    result = try_access(Q,pub,1,7,num_threads);
    result = try_access(Q,pub,0,5,num_threads);
    printf("asdadsa %d\n",result);
    result = try_access(Q,pub,1,12,num_threads);
    printqueue(Q);
    */
    /*enqueue(Q,5);
    enqueue(Q,7);
    enqueue(Q,4);
    //res =  dequeue(Q,&val);
    //if (res) printf("Dequeued %d \n",val);
    enqueue(Q,1);
    printqueue(Q)*/

    //try_access(Q,pub,1,5,num_threads);
    //try_access(Q,pub,1,MAX_VALUES,num_threads);

    /*try_access(Q,pub,1,2,num_threads);
    try_access(Q,pub,1,4,num_threads);
    try_access(Q,pub,1,8,num_threads);
    try_access(Q,pub,1,9,num_threads);
    try_access(Q,pub,1,12,num_threads);
    res = try_access(Q,pub,0,9,num_threads);
    printf("res = %d\n",res);
    res = try_access(Q,pub,0,9,num_threads);
    printf("res = %d\n",res);
    res = try_access(Q,pub,0,9,num_threads);
    printf("res = %d\n",res);
    res = try_access(Q,pub,0,9,num_threads);
    printf("res = %d\n",res);
    res = try_access(Q,pub,0,9,num_threads);
    printf("res = %d\n",res);
    res = try_access(Q,pub,0,9,num_threads);
    printf("res = %d\n",res);
    printqueue(Q);
    */
srand(time(NULL));
    timer_tt * timer;
    int c,k;
    timer_tt * glob_timer=timer_init();
    timer_start(glob_timer); 
    long int sum=0;
    double total_time=0;


    #pragma omp parallel for num_threads(num_threads) shared(Q) private(res,val,i,j,c,timer,k) reduction(+:total_time) reduction(+:sum) 
    for(i=0;i<num_threads;i++){
        c=50;
        //printf("thread number %d cpuid %d\n",omp_get_thread_num(),sched_getcpu());
        timer=timer_init();
        timer_start(timer);
        sum=0;
         for (j=0;j<count/num_threads;j++){
                try_access(Q,pub_enq,1,i,num_threads);
                sum+=c;
                for(k=0;k<c;k++);
                res = try_access(Q,pub_deq,0,9,num_threads);
                if(res==ERROR_VALUE) printf("%d\n",res);
                //else printf("thread %d  dequeued --> %d\n",omp_get_thread_num(),res);
         }
         timer_stop(timer);
         total_time=timer_report_sec(timer);
         //printf("threads number %d total time %lf\n",omp_get_thread_num(),total_time);
         //printf("sum =  %ld\n",sum);
         /*timer_tt * timer2=timer_init();
         timer_start(timer2);
         for(k=0;k<sum;k++);
         timer_stop(timer2);
         total_time=timer_res-timer_report_sec(timer2);
         printf("delay time %lf\n",timer_report_sec(timer2));
         //printf("threads number %d  time %lf\n",omp_get_thread_num(),total_time);
         */
    }
   // printf("new total_time %lf\n",total_time);
   // printf("new sum %ld\n",sum);
    double avg_total_time=total_time/(double)num_threads;
    printf("avg total time %lf\n",avg_total_time);
    timer_stop(glob_timer);
    printf("glob timer %lf\n",timer_report_sec(glob_timer));
/*    long int avg_sum=sum/num_threads;
    printf("avg sum %ld\n",avg_sum);
    timer_tt * timer2=timer_init();
    timer_start(timer2);
    for(k=0;k<avg_sum;k++);
    timer_stop(timer2);
    double avg_delay=timer_report_sec(timer2);
   // printf("average delay time %lf\n",avg_delay);
    //printf("threads number %d  time %lf\n",omp_get_thread_num(),total_time);

*/
    //printf("average time %lf\n",avg_total_time - avg_delay);
    /*timer_stop(glob_timer);
    printf("global time %lf\n",timer_report_sec(glob_timer));

    glob_timer=timer_init();
    timer_start(glob_timer);
    for(i=0;i<sum;i++);
    timer_stop(glob_timer);
    printf("test delay %lf/n",timer_report_sec(glob_timer));
    */
    printqueue(Q);
    //timer_stop(timer);
    //printf("num_threasd %d  enq-deqs total %d \n",num_threads,count);
    //printf("Total time  %lf \n",time_res);

	return 1;
}
	
