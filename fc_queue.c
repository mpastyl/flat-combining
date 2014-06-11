#include <stdio.h>
#include <stdlib.h>
#include "timers_lib.h"

struct pointer_t{
    __int128 both;
};




struct node_t{
	int value;//TODO: maybe change this
	struct pointer_t next;
};

struct queue_t{
    struct pointer_t  Head;
    struct pointer_t  Tail;
};


__int128 get_count(__int128 a){

    __int128 b = a >>64;
    return b;
}

__int128 get_pointer(__int128 a){
    __int128 b = a << 64;
    b= b >>64;
    return b;
}

__int128 set_count(__int128  a, __int128 count){
    __int128 count_temp =  count << 64;
    __int128 b = get_pointer(a);
    b = b | count_temp;
    return b;
}

__int128 set_pointer(__int128 a, __int128 ptr){
    __int128 b = 0;
    __int128 c = get_count(a);
    b = set_count(b,c);
    ptr = get_pointer(ptr);
    b= b | ptr;
    return b;
}

__int128 set_both(__int128 a, __int128 ptr, __int128 count){
    a=set_pointer(a,ptr);
    a=set_count(a,count);
    return a;
}


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

int ERROR_VALUE=5004;

long long int glob_counter=0; 

long long int count_enqs=0;
long long int count_deqs=0;
/*
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

*/
int locks[4];

void initialize(struct queue_t * Q,struct pub_record * pub0,struct pub_record * pub1,struct pub_record * pub2,struct pub_record * pub3,int n){//TODO: init count?
	int i;
    struct node_t * node = (struct node_t *) malloc(sizeof(struct node_t));
	node->next.both = NULL;

    Q->Head.both= set_both(Q->Head.both,node,0);
    Q->Tail.both= set_both(Q->Tail.both,node,0);

    
    //Q->lock = 0;
    for(i=0;i<4;i++) locks[i]=0;
    for(i=0; i <n ;i++){
        pub0[i].pending = 0;
        pub0[i].response =0;
        pub1[i].pending = 0;
        pub1[i].response =0;
        pub2[i].pending = 0;
        pub2[i].response =0;
        pub3[i].pending = 0;
        pub3[i].response =0;
    }
}

/*
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
*/


void enqueue(struct queue_t * Q, int val){

    struct pointer_t tail;
    __int128 new_to_set;
    int temp;
    struct node_t * node = (struct node_t *) malloc(sizeof(struct node_t));
    node->value = val;
    node->next.both = 0;
    while (1){
        tail = Q->Tail;
        struct pointer_t next_p = ((struct node_t *)get_pointer(tail.both))->next;
        if (tail.both == Q->Tail.both){
            if (get_pointer(next_p.both) == 0){
                new_to_set =set_both(new_to_set,node,get_count(next_p.both) +(__int128)1);
                if (__sync_bool_compare_and_swap(&(((struct node_t * )get_pointer(tail.both))->next.both),next_p.both,new_to_set))
                    break;
            }
            else{
                new_to_set=set_both(new_to_set,next_p.both,get_count(tail.both)+(__int128)1);
                temp = __sync_bool_compare_and_swap(&Q->Tail.both,tail.both,new_to_set);
            }
        }
    }
    new_to_set=set_both(new_to_set,node,get_count(tail.both)+(__int128)1);
    temp = __sync_bool_compare_and_swap(&Q->Tail.both,tail.both,new_to_set);
}



int dequeue(struct queue_t * Q,int * pvalue){

    struct pointer_t head;
    struct pointer_t tail;
    struct pointer_t next;
    int  temp;
    int first_val;
    __int128 new_to_set;
    while(1){
        head =  Q->Head;
        first_val=((struct node_t *)get_pointer(head.both))->value;
        tail =  Q->Tail;
        next =  ((struct node_t *)get_pointer(head.both))->next;
        if ( head.both == Q->Head.both){
            if (head.both == tail.both){
                if ( get_pointer(next.both) == 0)
                    return 0;
                new_to_set =  set_both(new_to_set,next.both,get_count(tail.both) +(__int128)1);
                temp = __sync_bool_compare_and_swap(&Q->Tail.both,tail.both,new_to_set);
            }
            else{
                //if ((head==Q->Head) &&(first_val!=((struct node_t *)get_pointer(head))->value)) printf("change detected!\n");
                *pvalue =((struct node_t *)get_pointer(next.both))->value;
                new_to_set = set_both(new_to_set,next.both,get_count(head.both)+(__int128)1);
                if( __sync_bool_compare_and_swap(&Q->Head.both,head.both,new_to_set))
                    break;
            }
        }
    }
    //printf(" about to free %p \n",head);
    free(get_pointer(head.both));
    return 1;
}


void printqueue(struct queue_t * Q){

    struct pointer_t curr ;
    struct pointer_t next ;

    curr = Q->Head;
    //printf(" in printqueue curr = %p\n",curr);
    next = ((struct node_t * )get_pointer(Q->Head.both))->next;
    //printf(" in printqueue next = %p\n",next);
    while ((get_pointer(curr.both) != get_pointer(Q->Tail.both))&&(get_pointer(curr.both)!=0)){
    //printf(" in printqueue curr = %p\n",curr);
    //printf(" in printqueue next = %p\n",next);
        printf("%d ",((struct node_t * )get_pointer(curr.both))->value);
        curr = next;
        if (get_pointer(next.both)) next = ((struct node_t * )get_pointer(curr.both))->next;
    }
    //printf("%d ",((struct node_t * )get_pointer(curr))->value);
    printf("\n");

}



int try_access(struct queue_t * Q,struct pub_record *  pub,int operation, int val,int n){

    int tid=  omp_get_thread_num()%16;
    int i,res,count;
    //1
    int lock_indx=omp_get_thread_num()/16;


    pub[tid].op = operation;
    pub[tid].val = val;
    pub[tid].pending=1;
    while (1){
        if(locks[lock_indx]){
            count=0;
            while((!pub[tid].response)&&(count<10000000)) count++; //check periodicaly for lock
            if(pub[tid].response ==1){
                pub[tid].response=0;
                return (pub[tid].val);
            }
        }
        else{
            if(__sync_lock_test_and_set(&(locks[lock_indx]),1)) continue;// must spin backto response
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
                locks[lock_indx]=0;
                return temp_val;
            }
        }
   }
}

/*
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
*/

int main(int argc, char *argv[]){

    int res,val,i,j,num_threads,count,result; 
    
    num_threads=atoi(argv[1]);
    count =atoi(argv[2]);


	struct queue_t * Q = (struct queue_t *) malloc(sizeof(struct queue_t));

    struct pub_record pub0[num_threads];
    struct pub_record pub1[num_threads];
    struct pub_record pub2[num_threads];
    struct pub_record pub3[num_threads];
    //Q->Head =  NULL;
    //Q->Tail =  NULL;
	initialize(Q,pub0,pub1,pub2,pub3,16);
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
    int tid,c,k;
    double timer_val;
    srand(time(NULL));
    #pragma omp parallel for num_threads(num_threads) shared(Q,pub0,pub1,pub2,pub3) private(res,val,i,j,tid,c,k) reduction(+:timer_val)
    for(i=0;i<num_threads;i++){
        tid=omp_get_thread_num();
        timer_tt * timer = timer_init();
        timer_start(timer);
        for(j=0; j<count/num_threads;j++){
                c=rand()%1000;
                if(tid<16){
                    try_access(Q,pub0,1,i,16);
                    for(k=0;k<c;k++);
                    res=try_access(Q,pub0,0,9,16);
                    if(res==ERROR_VALUE) printf("%d\n",res);
                }
                else if(tid<32){
                    try_access(Q,pub1,1,i,16);
                    for(k=0;k<c;k++);
                    res=try_access(Q,pub1,0,9,16);
                    if(res==ERROR_VALUE) printf("%d\n",res);
                }
                else if(tid<48){
                    try_access(Q,pub2,1,i,16);
                    for(k=0;k<c;k++);
                    res=try_access(Q,pub2,0,9,16);
                    if(res==ERROR_VALUE) printf("%d\n",res);
                }
                else{
                    try_access(Q,pub3,1,i,16);
                    for(k=0;k<c;k++);
                    res=try_access(Q,pub3,0,9,16);
                    if(res==ERROR_VALUE) printf("%d\n",res);
                }
        }
        timer_stop(timer);
        timer_val = timer_report_sec(timer);
        printf("thread number %d total time %lf\n",omp_get_thread_num(),timer_val);
    }
    printf("average time  %lf\n",timer_val/(double)num_threads);
    printf("glob counter %ld \n",glob_counter);

    timer_tt * timer2=timer_init();
    timer_start(timer2);
    
    for(k=0;k<glob_counter;k++){
        for(i=0;i<num_threads;i++)
            res=pub0[i].pending;
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
	
