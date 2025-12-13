#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <stdbool.h>

#define N 10
#define DEFAULT_TQ 3

typedef enum {
    STATE_READY,
    STATE_RUNNING,
    STATE_SLEEP,
    STATE_DONE
} ProcessState;

typedef struct {
    pid_t pid;
    int id;
    int remaining_tq;
    ProcessState state;
    int io_wait_time;
    int fd_p2c[2];
    int fd_c2p[2];
    
    int arrival_time;
    int finish_time;
    int waiting_time;
} PCB;

PCB pcb_table[N];

void child_process_logic(int id, int read_fd, int write_fd) {
    srand(time(NULL) ^ getpid());
    int cpu_burst = (rand() % 10) + 1;
    int cmd;
    int report[2]; 

    while (1) {
        read(read_fd, &cmd, sizeof(int));

        cpu_burst--;

        if (cpu_burst <= 0) {
            if (rand() % 2 == 0) {
                report[0] = 2; 
                report[1] = cpu_burst;
                write(write_fd, report, sizeof(report));
                cpu_burst = (rand() % 10) + 1;
            } else {
                report[0] = 3; 
                report[1] = 0;
                write(write_fd, report, sizeof(report));
                exit(0);
            }
        } else {
            report[0] = 1; 
            report[1] = cpu_burst;
            write(write_fd, report, sizeof(report));
        }
    }
}

int main() {
    srand(time(NULL));

    printf("=== OS 스케줄링 시뮬레이션 시작 (설정된 타임 퀀텀: %d) ===\n", DEFAULT_TQ);

    for (int i = 0; i < N; i++) {
        if (pipe(pcb_table[i].fd_p2c) == -1 || pipe(pcb_table[i].fd_c2p) == -1) {
            perror("파이프 생성 실패");
            exit(1);
        }

        pid_t pid = fork();

        if (pid == 0) {
            close(pcb_table[i].fd_p2c[1]);
            close(pcb_table[i].fd_c2p[0]);
            child_process_logic(i, pcb_table[i].fd_p2c[0], pcb_table[i].fd_c2p[1]);
            exit(0);
        } else {
            close(pcb_table[i].fd_p2c[0]);
            close(pcb_table[i].fd_c2p[1]);

            pcb_table[i].pid = pid;
            pcb_table[i].id = i;
            pcb_table[i].remaining_tq = DEFAULT_TQ;
            pcb_table[i].state = STATE_READY;
            pcb_table[i].io_wait_time = 0;
            
            pcb_table[i].arrival_time = 0;
            pcb_table[i].finish_time = 0;
            pcb_table[i].waiting_time = 0;
        }
    }

    int active_processes = N;
    int current_idx = 0;
    int global_time = 0;

    while (active_processes > 0) {
        usleep(100000); 
        global_time++;
        printf("\n[시간 %d]\n", global_time);

        for (int i = 0; i < N; i++) {
            if (pcb_table[i].state == STATE_SLEEP) {
                pcb_table[i].io_wait_time--;
                if (pcb_table[i].io_wait_time <= 0) {
                    pcb_table[i].state = STATE_READY;
                    printf("프로세스 %d 대기 완료 (Ready 상태로 복귀)\n", i);
                }
            }
            else if (pcb_table[i].state == STATE_READY) {
                pcb_table[i].waiting_time++;
            }
        }

        bool all_tq_zero = true;
        for (int i = 0; i < N; i++) {
            if (pcb_table[i].state != STATE_DONE && pcb_table[i].remaining_tq > 0) {
                all_tq_zero = false;
                break;
            }
        }

        if (all_tq_zero) {
            printf("모든 프로세스 타임 퀀텀 소진. 전체 프로세스 TQ 초기화.\n");
            for (int i = 0; i < N; i++) {
                if (pcb_table[i].state != STATE_DONE) {
                    pcb_table[i].remaining_tq = DEFAULT_TQ;
                }
            }
        }

        int target_idx = -1;
        int start_idx = current_idx;
        
        for (int i = 0; i < N; i++) {
            int idx = (start_idx + i) % N;
            if (pcb_table[idx].state == STATE_READY && pcb_table[idx].remaining_tq > 0) {
                target_idx = idx;
                current_idx = idx;
                break;
            }
        }

        if (target_idx == -1) {
            printf("IDLE (대기 중인 프로세스 없음)\n");
            continue;
        }

        if (pcb_table[target_idx].waiting_time > 0) {
            pcb_table[target_idx].waiting_time--;
        }

        int cmd = 1;
        write(pcb_table[target_idx].fd_p2c[1], &cmd, sizeof(int));
        
        pcb_table[target_idx].remaining_tq--;
        
        int report[2];
        read(pcb_table[target_idx].fd_c2p[0], report, sizeof(report));
        
        int response = report[0];
        int rem_burst = report[1];

        if (response == 3) {
            printf("프로세스 %d 종료됨 (남은 버스트: 0)\n", target_idx);
            pcb_table[target_idx].state = STATE_DONE;
            pcb_table[target_idx].finish_time = global_time;
            active_processes--;
            waitpid(pcb_table[target_idx].pid, NULL, 0);
        } else if (response == 2) {
            int wait_time = (rand() % 5) + 1;
            pcb_table[target_idx].state = STATE_SLEEP;
            pcb_table[target_idx].io_wait_time = wait_time;
            printf("프로세스 %d I/O 요청 (대기시간: %d, 남은 TQ: %d, 남은 버스트: %d)\n", 
                   target_idx, wait_time, pcb_table[target_idx].remaining_tq, rem_burst);
        } else {
            printf("프로세스 %d 실행 중 (남은 TQ: %d, 남은 버스트: %d)\n", 
                   target_idx, pcb_table[target_idx].remaining_tq, rem_burst);
        }

        if (pcb_table[target_idx].state == STATE_READY && pcb_table[target_idx].remaining_tq == 0) {
             printf("프로세스 %d 타임 퀀텀 만료\n", target_idx);
             current_idx = (target_idx + 1) % N;
        }
    }

    printf("\n\n");
    printf("=========================================================\n");
    printf("                  성능 분석표 (TQ = %d)\n", DEFAULT_TQ);
    printf("=========================================================\n");
    printf(" PID | 종료시간 | 대기시간(WT) | 반환시간(TT)\n");
    printf("-----+----------+--------------+---------------\n");

    double total_wait = 0;
    double total_turnaround = 0;

    for (int i = 0; i < N; i++) {
        int turnaround = pcb_table[i].finish_time - pcb_table[i].arrival_time;
        printf(" %3d |    %3d   |      %3d     |      %3d\n", 
            pcb_table[i].pid, 
            pcb_table[i].finish_time, 
            pcb_table[i].waiting_time, 
            turnaround);
        
        total_wait += pcb_table[i].waiting_time;
        total_turnaround += turnaround;
    }

    printf("=========================================================\n");
    printf("평균 대기 시간: %.2f\n", total_wait / N);
    printf("평균 반환 시간: %.2f\n", total_turnaround / N);
    printf("=========================================================\n");

    return 0;
}
