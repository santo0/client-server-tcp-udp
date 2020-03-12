#include  <stdio.h>
#include  <stdlib.h>
#include  <sys/types.h>
#include  <sys/ipc.h>
#include  <sys/shm.h>

struct Data{
    int status;
    char msg[10];
};

struct NODE{
    int shm_id;
    int status;
    char msg[10];
    struct NODE *next;
};



struct NODE* create_shm_linked_list(int num_elems){
    key_t          ShmKEY;
    int            ShmID;
    struct NODE  *ShmPTR;
    struct NODE  *tmp;
    int i;
    printf("size of node %ld\n",sizeof(struct NODE));
    ShmKEY = ftok(".", 'a');
    ShmID = shmget(ShmKEY, sizeof(struct NODE), IPC_CREAT | 0666);
    if (ShmID < 0) {
        printf("*** shmget error (server) ***\n");
        exit(1);
    }    
    ShmPTR = (struct NODE *) shmat(ShmID, NULL, 0);
    if ((long) ShmPTR == -1) {
        printf("*** shmat error (server) ***\n");
        exit(1);
    }
    ShmPTR->shm_id = ShmID;
    ShmPTR->msg[0] = 'a';
    ShmPTR->msg[1] = 'a';
    ShmPTR->msg[2] = 'a';
    ShmPTR->msg[3] = '\0';
    ShmPTR->status = 0;
    ShmPTR->next = NULL;
    tmp = ShmPTR;
    printf("Client %d created with msg [%s]\n",tmp->shm_id, tmp->msg);
    for(i = 1; i < num_elems; i++){
        ShmKEY = ftok(".", 'a' + i);
        ShmID = shmget(ShmKEY, sizeof(struct NODE), IPC_CREAT | 0666);
        if (ShmID < 0) {
            printf("*** shmget error (server) ***\n");
            exit(1);
        }
        ShmPTR = (struct NODE *) shmat(ShmID, NULL, 0);
        if ((long) ShmPTR == -1) {
            printf("*** shmat error (server) ***\n");
            exit(1);
        }
        ShmPTR->shm_id = ShmID;
        ShmPTR->msg[0] = 'a' + i;
        ShmPTR->msg[1] = 'a' + i;
        ShmPTR->msg[2] = 'a' + i;
        ShmPTR->msg[3] = '\0';
        ShmPTR->status = 0;
        ShmPTR->next = tmp;
        tmp = ShmPTR;
        printf("Client %d created with msg [%s]\n",tmp->shm_id, tmp->msg);
    }
    return ShmPTR;
}

int are_finished(struct NODE **entry){
    int finish = 0;
    struct NODE* tmp = *entry;
    while(tmp && !finish){
        if(!tmp->status){
            finish = 1;
        }
        tmp = tmp->next;
    }
    return finish;
}

void print_linked_list(struct NODE **entry){
    struct NODE* tmp = *entry;
    while(tmp){
        printf("Server has received from client %d this-> %s\n", tmp->shm_id, tmp->msg);
        tmp = tmp->next;
    }
}


int  main(int  argc, char *argv[])
{
    key_t          ShmKEY;
    int            ShmID;
    struct NODE  *ShmPTR;
    struct NODE  *tmp;
    int i;
    ShmPTR = create_shm_linked_list(3);
    printf("Please start the client in another window...\n");
    if(fork() == 0){
        for (i = 0; i < 3; i++){
            printf("Client %d has received %s\n", ShmPTR->shm_id, ShmPTR->msg);
            ShmPTR->status=1;
            ShmPTR->msg[1] = ShmPTR->msg[1] + 5;
            ShmPTR->msg[3] = '\0';
            printf("Client %d has sent %s\n", ShmPTR->shm_id, ShmPTR->msg);
            ShmPTR->status = 1;
            ShmPTR = ShmPTR->next;
        }
        printf("Clients exits...\n");
        exit(0);
    }else{
        do{
            print_linked_list(&ShmPTR);
            sleep(1);
        }while(are_finished(&ShmPTR));
    }
    print_linked_list(&ShmPTR);
    /* Tanco els shared mems */ 
    tmp = ShmPTR;
    while(tmp){
        ShmPTR = tmp;
        tmp = ShmPTR->next;
        ShmID = ShmPTR->shm_id;
        shmdt((void *) ShmPTR);
        shmctl(ShmID, IPC_RMID, NULL);
    }
    printf("Server exits...\n");
    return 0;
}