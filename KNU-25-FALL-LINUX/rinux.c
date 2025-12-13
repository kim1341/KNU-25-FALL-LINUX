#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>

#define N 10
#define TQ 10

#define READY 0
#define SLEEP 1
#define DONE 2

typedef struct {
    int pid;
    int id;
    int remaining_tq;
    int state;
    int io_timer;
    int remaining_burst;
    
    int fd_to_child[2];
    int fd_from_child[2];
    
    int arrival_time;
    int finish_time;
    int waiting_time;
} PCB;

PCB proc[N];

void do_child_work(int id) {
    srand(time(NULL) ^ getpid()); 

    int cpu_burst = (rand() % 10) + 1; 
    
    int cmd;
    int report[2];

    while (1) {
        read(proc[id].fd_to_child[0], &cmd, sizeof(int));

        cpu_burst--;

        if (cpu_burst <= 0) {
            report[0] = 3;
        } 
        else {
            if (rand() % 10 < 3) { 
                report[0] = 2;
            } else {
                report[0] = 1;
            }
        }
        
        report[1] = cpu_burst; 
        
        write(proc[id].fd_from_child[1], report, sizeof(report));

        if (report[0] == 3) exit(0);
    }
}

int main() {
    srand(time(NULL));

    printf("=== OS 시뮬레이션 시작 (TQ: %d) ===\n", TQ);

    for (int i = 0; i < N; i++) {
        pipe(proc[i].fd_to_child);
        pipe(proc[i].fd_from_child);

        int pid = fork();

        if (pid == 0) {
            close(proc[i].fd_to_child[1]);
            close(proc[i].fd_from_child[0]);
            do_child_work(i);
        } 
        else {
            proc[i].pid = pid;
            proc[i].id = i;
            proc[i].remaining_tq = TQ;
            proc[i].state = READY;
            proc[i].io_timer = 0;
            proc[i].remaining_burst = -1;
            
            proc[i].arrival_time = 0;
            proc[i].waiting_time = 0;
            proc[i].finish_time = 0;

            close(proc[i].fd_to_child[0]);
            close(proc[i].fd_from_child[1]);
        }
    }

    int active_count = N;
    int current_idx = 0;
    int global_time = 0;

    while (active_count > 0) {
        sleep(1);
        global_time++;
        
        for(int i=0; i<N; i++) {
            if(proc[i].state == SLEEP) {
                proc[i].io_timer--;
                if(proc[i].io_timer <= 0) proc[i].state = READY;
            }
            else if(proc[i].state == READY) {
                proc[i].waiting_time++;
            }
        }

        printf("[시간 %2d] Ready 큐: [ ", global_time);
        int print_cnt = 0;
        for(int i=0; i<N; i++) {
            int idx = (current_idx + i) % N;
            if (proc[idx].state == READY && proc[idx].remaining_tq > 0) {
                printf("%d ", proc[idx].id);
                print_cnt++;
            }
        }
        if(print_cnt == 0) printf("비어있음 ");
        printf("] ");

        int target = -1;
        for(int i=0; i<N; i++) {
            int idx = (current_idx + i) % N;
            if(proc[idx].state == READY && proc[idx].remaining_tq > 0) {
                target = idx;
                current_idx = idx;
                break;
            }
        }

        if (target == -1) {
            int still_alive = 0;
            for(int i=0; i<N; i++) {
                if(proc[i].state != DONE) {
                    proc[i].remaining_tq = TQ;
                    still_alive++;
                }
            }
            if(still_alive == 0) {
                printf("\n");
                break;
            }
            printf("-> (IDLE) 실행 대기 중...\n");
            continue;
        }

        int cmd = 1;
        write(proc[target].fd_to_child[1], &cmd, sizeof(int));
        proc[target].remaining_tq--;
        
        int report[2];
        read(proc[target].fd_from_child[0], report, sizeof(report));
        
        int status = report[0];
        proc[target].remaining_burst = report[1];

        if (status == 3) {
            printf("-> P%d 종료 (남은Burst: 0)\n", target);
            proc[target].state = DONE;
            proc[target].finish_time = global_time;
            
            if (proc[target].waiting_time > 0) proc[target].waiting_time--; 
            
            active_count--;
        }
        else if (status == 2) {
            printf("-> P%d I/O 요청 (남은Burst:%d, 남은TQ:%d)\n", 
                   target, proc[target].remaining_burst, proc[target].remaining_tq);
            proc[target].state = SLEEP;
            proc[target].io_timer = (rand() % 4) + 1;
            
            if (proc[target].waiting_time > 0) proc[target].waiting_time--;
        }
        else {
            printf("-> P%d 실행 (남은Burst:%d, 남은TQ:%d)\n", 
                   target, proc[target].remaining_burst, proc[target].remaining_tq);
            
            if (proc[target].waiting_time > 0) proc[target].waiting_time--;

            if(proc[target].remaining_tq == 0) {
                printf("          (P%d TQ 만료! 순서 변경)\n", target);
                current_idx = (current_idx + 1) % N;
            }
        }
    }

    for(int i=0; i<N; i++) wait(NULL);

    printf("\n\n");
    printf("=========================================================\n");
    printf("                 성능 분석표 (TQ = %d)\n", TQ);
    printf("=========================================================\n");
    printf(" ID (Real PID) | 종료시간 | 대기시간(WT) | 반환시간(TT)\n");
    printf("---------------+----------+--------------+---------------\n");

    double total_wait = 0;
    double total_turnaround = 0;

    for(int i=0; i<N; i++) {
        int turnaround = proc[i].finish_time - proc[i].arrival_time;
        printf("  %d (%d)  |    %3d   |      %3d     |      %3d\n", 
            proc[i].id, proc[i].pid, 
            proc[i].finish_time, proc[i].waiting_time, turnaround);
        total_wait += proc[i].waiting_time;
        total_turnaround += turnaround;
    }

    printf("=========================================================\n");
    printf("평균 대기 시간: %.2f\n", total_wait / N);
    printf("평균 반환 시간: %.2f\n", total_turnaround / N);
    printf("=========================================================\n");

    return 0;
}
