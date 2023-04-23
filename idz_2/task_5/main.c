#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/shm.h>

int sem_id;
int num_customers;
int* customer_count_ptr;
int cashier_1_sem = 0, cashier_2_sem = 1, queue_sem = 2, for_cashier_1_sem = 3, for_cashier_2_sem = 4;
int customers_in_queue = 0;

void customer_behavoiur(int i);

void cashier_behaviour(int id);


void cleanup();

void signal_handle(int nsig) {

    cleanup();

    exit(0);
}

void cleanup() {
    semctl(sem_id, 0, IPC_RMID, 0);
    semctl(sem_id, 1, IPC_RMID, 0);
    semctl(sem_id, 2, IPC_RMID, 0);
    semctl(sem_id, 3, IPC_RMID, 0);
    semctl(sem_id, 4, IPC_RMID, 0);
    shmdt(customer_count_ptr);
}
void setup_signal_handling() {
    signal(SIGINT, signal_handle);
}

void setup_shared_memory() {
    const int array_size = 4;
    key_t shm_key;
    int shmid;
    char pathname[]="../3-shr-sem";
    shm_key = ftok(pathname, 0);

    if((shmid = shmget(shm_key, sizeof(int)*array_size,
                       0666 | IPC_CREAT | IPC_EXCL)) < 0)  {
        if((shmid = shmget(shm_key, sizeof(int)*array_size, 0)) < 0) {
            printf("Can\'t connect to shared memory\n");
            exit(-1);
        };
        customer_count_ptr = (int*)shmat(shmid, NULL, 0);
    } else {
        customer_count_ptr = (int*)shmat(shmid, NULL, 0);
        for(int i = 0; i < array_size; ++i) {
            customer_count_ptr[i] = 0;
        }
    }

    customer_count_ptr[0] = num_customers;
}

void setup_semaphores() {
    sem_id = semget(IPC_PRIVATE, 5, IPC_CREAT | 0666);
    if (sem_id == -1) {
        perror("Can\'t create semaphore\n");
        exit(-1);
    }

    semctl(sem_id, cashier_1_sem, SETVAL, 0);
    semctl(sem_id, cashier_2_sem, SETVAL, 0);
    semctl(sem_id, queue_sem, SETVAL, 1); // for customers queue
    semctl(sem_id, for_cashier_1_sem, SETVAL, 0);
    semctl(sem_id, for_cashier_2_sem, SETVAL, 0);
}

void run_cashier(int cashier_number) {
    pid_t cashier = fork();

    if (cashier < 0) {
        perror("Can\'t fork cashier");
        exit(-1);
    } else if (cashier == 0) {
        cashier_behaviour(cashier_number);
        exit(0);
    }
}

void run_customer(int customer_number) {
    pid_t pid = fork();

    if (pid == -1) {
        perror("Can\'t fork customer");
        exit(-1);
    } else if (pid == 0) {
        customer_behavoiur(customer_number);
        exit(0);
    }
}

void wait_for_processes() {
    for (int i = 1; i <= num_customers+ 2; i++) {
        wait(0);
    }
}

void cleanup_resources() {
    cleanup();
    printf("Все посетители обслужены. Магазин закрыт. До завтра!)\n");
}
int main(int argc, char* argv[]) {

    char *p;
    num_customers = strtol(argv[1], &p, 10);

    setup_signal_handling();

    setup_shared_memory();

    setup_semaphores();

    run_cashier(1);
    run_cashier(2);

    for (int i = 1; i <= num_customers; i++) {
        run_customer(i);
    }

    wait_for_processes();

    cleanup_resources();

    return 0;
}
void wait_sem(int sem_num) {
    struct sembuf op = {sem_num, -1, 0};
    semop(sem_id, &op, 1);
}

void signal_sem(int sem_num) {
    struct sembuf op = {sem_num, 1, 0};
    semop(sem_id, &op, 1);
}

void cashier_behaviour(int id) {
    while (customer_count_ptr[0] > 0) {
        if (id == 1) {
            wait_sem(cashier_1_sem);
        } else {
            wait_sem(cashier_2_sem);
        }
        if (customer_count_ptr[0] == 0) {
            exit(0);
        }

        fflush(stdout);
        if (id == 1) {
            printf("Кассир номер 1 занят. Пробивает продукты покупателя\n");
        } else {
            printf("Кассир номер 2 занят. Пробивает продукты покупателя\n");
        }
        customer_count_ptr[0]--;

        if (customer_count_ptr[0] == 0) {
            if (id == 1) {
                signal_sem(cashier_2_sem);
            } else {
                signal_sem(cashier_1_sem);
            }
        }
        if (id == 1) {
            signal_sem(for_cashier_1_sem);
        } else {
            signal_sem(for_cashier_2_sem);
        }
    }
}

void customer_behavoiur(int i) {
    srand(time(NULL) * i);
    sleep(rand() % 2);

    wait_sem(queue_sem); // semaphore taking a queue

    fflush(stdout);
    printf("Покупатель номер %d встал в очередь\n", i);
    customers_in_queue++;

    signal_sem(queue_sem); // semaphore for releasing a queue

    int random_cashier;
    if (customers_in_queue == 1) {
        random_cashier = rand() % 2;
        signal_sem(random_cashier); // go to random cashier
    }
    if (random_cashier == 0) {  // if cashier is available
        wait_sem(for_cashier_1_sem);
    } else {
        wait_sem(for_cashier_2_sem);
    }

    fflush(stdout);
    printf("Покупатель %d оплатил покупки у кассира номер %d\n", i, random_cashier + 1);

    wait_sem(queue_sem); // semaphore taking a queue
    fflush(stdout);
    printf("Покупатель номер %d радостный пошел домой \n", i);
    customers_in_queue--;
    signal_sem(queue_sem); // semaphore for releasing a queue
}
