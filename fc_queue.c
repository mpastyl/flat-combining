#include <stdio.h>
#include <stdlib.h>
#include "timers_lib.h"




struct node_t{
	int value;//TODO: maybe change this
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

int ERROR_VALUE=54;

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

void initialize(struct queue_t * Q,struct pub_record * pub,int n){//TODO: init count?
	int i;
    struct node_t * node = (struct node_t *) malloc(sizeof(struct node_t));
	node->next = NULL;
	Q->Head = node; //TODO: check this
	Q->Tail = node;
    Q->lock = 0;
    for(i=0; i <n ;i++){
        pub[i].pending = 0;
        pub[i].response =0;
    }
}


void enqueue(struct queue_t * Q, int val){
	
    int store = 0;
    struct node_t * node = (struct node_t *) malloc(sizeof(struct node_t));
    node->value =  val;
    node->next = NULL;
    //while(__sync_lock_test_and_set(&Q->lock,1));
    //#pragma omp critical
    
    //struct node_t * tail = Q->Tail;
    //tail->next =node;
    Q->Tail->next=node;
    Q->Tail = node;
    
    //__sync_lock_test_and_set(&Q->lock,0);

}


int dequeue(struct queue_t * Q, int * val){
    
    
    struct node_t * node = Q->Head;
    struct node_t * next = node->next;
    //struct node_t * tail = Q->Tail;
    if(next == NULL) return 0;
    else{
        *val = next->value;
        Q->Head =next;
    }
    
    free(node);
    
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
    
    curr = Q->Head;
    next = Q->Head->next;
    while (curr != Q->Tail){
        printf("%d ",curr->value);
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

    struct pub_record pub[num_threads];
    //Q->Head =  NULL;
    //Q->Tail =  NULL;
	initialize(Q,pub,num_threads);
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
    
    timer_tt * timer = timer_init();
    timer_start(timer);
    #pragma omp parallel for num_threads(num_threads) shared(Q,pub) private(res,val,i,j)
    for(i=0;i<num_threads;i++){
        for(j=0; j<count/num_threads;j++){
                try_access(Q,pub,1,i,num_threads);
                res=try_access(Q,pub,0,9,num_threads);
                if(res==ERROR_VALUE) printf("%d\n",res);
            
        }
    }
    timer_stop(timer);
    double timer_val = timer_report_sec(timer);
    printf("num_threads %d enq-deqs total %d\n",num_threads,count);
    printf("thread number %d total time %lf\n",omp_get_thread_num(),timer_val);
    printf("glob counter %ld \n",glob_counter);

    timer_tt * timer2=timer_init();
    timer_start(timer2);
    int k=0;
    for(k=0;k<glob_counter;k++){
        for(i=0;i<num_threads;i++)
            res=pub[i].pending;
    }
    timer_stop(timer2);
    printf("total delay %lf\n",timer_report_sec(timer2));
    printqueue(Q);

    printf("total enqs %ld\n",count_enqs);
    printf("total deqs %ld\n",count_deqs);

    
    //------------------------------------------------------
    /*double thread_time;
    timer_tt * timer;
    #pragma omp parallel for num_threads(num_threads) shared(Q) private(timer,j,i,thread_time)
    for(i=0;i<num_threads;i++){    
        timer = timer_init();
        timer_start(timer);
        lock_queue(Q);
        for( j=0; j<100;j++)
            printf("thread %d in critical \n",omp_get_thread_num());
        unlock_queue(Q);
        timer_stop(timer);
        thread_time = timer_report_sec(timer);
        printf("thread %d  time %lf\n",omp_get_thread_num(),thread_time);
    }
    */
	return 1;
}
	
