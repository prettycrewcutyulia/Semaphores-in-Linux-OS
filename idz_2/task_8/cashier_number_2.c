#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/shm.h>

int cashier_1_sem = 0, cashier_2_sem = 1, queue_sem = 2, for_cashier_1_sem = 3, for_cashier_2_sem = 4, start = 5;
int num_customers;
int* customer_count_ptr;
int shm_fd;
int sem_id;

void cashier_behaviour();
void cleanup();
void signal_handler(int nsig);
void initialize_semaphores();
void create_shared_memory();
void customer_behaviour(int customers_in_queue, int i);

// Определение функции для освобождения всех семафоров и разделяемой памяти
void cleanup() {
    semctl(sem_id, 0, IPC_RMID, 0);
    semctl(sem_id, 1, IPC_RMID, 0);
    semctl(sem_id, 2, IPC_RMID, 0);
    semctl(sem_id, 3, IPC_RMID, 0);
    semctl(sem_id, 4, IPC_RMID, 0);
    semctl(sem_id, 5, IPC_RMID, 0);
    shmdt(customer_count_ptr);
}

// Обработчик сигнала SIGINT, который будет вызван при нажатии Ctrl+C
void signal_handler(int sig) {
    cleanup(); // функция для удаления всех созданных ресурсов
    exit(0); // выход из программы
}
// Функция для инициализации семафоров
void initialize_semaphores() {
    key_t sem_key;
    char pathname[] = "../3-shr-sem";
    sem_key = ftok(pathname, 0);
    sem_id = semget(sem_key, 6, IPC_CREAT | 0666);
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

// Функция создания разделяемой памяти и её отображения в адресное пространство текущего процесса
void create_shared_memory() {
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

void wait_sem(int sem_num) {
    struct sembuf op = {sem_num, -1, 0};
    semop(sem_id, &op, 1);
}

void signal_sem(int sem_num) {
    struct sembuf op = {sem_num, 1, 0};
    semop(sem_id, &op, 1);
}

int main() {
    signal(SIGINT, signal_handler); // регистрация обработчика сигнала SIGINT

    initialize_semaphores(); // создание семафоров
    create_shared_memory(); // создание общей памяти
    wait_sem(start);
    cashier_behaviour(); // поведение первого кассира
    cleanup(); // очистка ресурсов, если они были созданы ранее
    return 0;
}

// Функция cashier_behaviour описывает поведение кассиров в магазине.
void cashier_behaviour() {
    while (customer_count_ptr[0] > 0) { // Пока в магазине есть покупатели
        wait_sem(cashier_2_sem); // Ожидание сигнала от покупателя, что он готов к оплате
        if (customer_count_ptr[0] == 0) { // Проверка, что в очереди больше нет покупателей
            exit(0);
        }
        fflush(stdout);
        printf("Кассир номер %d занят. Пробивает продукты покупателя.\n",
               2); // Вывод сообщения о том, что кассир начал обслуживание покупателя
        customer_count_ptr[0]--; // Уменьшение количества покупателей на 1

        if (customer_count_ptr[0] == 0) { // Если в очереди больше нет покупателей, то сигналится другой кассир
            signal_sem(cashier_1_sem);
        }
        signal_sem(for_cashier_2_sem); // Освобождение покупателя, что он может продолжить выполнение программы
    }
}