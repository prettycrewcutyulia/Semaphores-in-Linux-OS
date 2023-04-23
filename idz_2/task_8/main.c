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
    shmdt(customer_count_ptr);
}

// Обработчик сигнала SIGINT, который будет вызван при нажатии Ctrl+C
void signal_handler(int sig) {
    cleanup(); // функция для удаления всех созданных ресурсов
    semctl(sem_id, 5, IPC_RMID, 0);
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

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler); // регистрация обработчика сигнала SIGINT
    int customers_in_queue = 0;
    char *p;

    if (argc != 2) {
        printf("Неверное количество аргументов.\n");
        exit(0);
    }

    num_customers = strtol(argv[1], &p, 10); // количество посетителей из аргумента командной строки

    if (num_customers <= 0) {
        printf("Количество посетителей должно быть больше 0.\n");
        exit(0);
    }
    initialize_semaphores(); // создание семафоров
    create_shared_memory(); // создание общей памяти

    customer_count_ptr[0] = num_customers; // количество посетителей в общей памяти

    signal_sem(start);
    signal_sem(start);

    for (int i = 1; i <= num_customers; i++) { // создание процессов для каждого посетителя
        pid_t pid = fork();
        if (pid == -1) {
            perror("Can\'t fork customer");
            exit(-1);
        } else if (pid == 0) { // процесс посетителя
            customer_behaviour(customers_in_queue, i); // поведение посетителя
        }
    }

    for (int i = 1; i <= num_customers; i++) {
        wait(0); // ожидание завершения всех процессов посетителей
    }

    cleanup(); // удаление всех созданных ресурсов
    printf("Все посетители обслужены. Магазин закрыт. До завтра!)\n");
    exit(0); // выход из программы
}
// функция customer_behaviour описывает поведение покупателей в магазине.
void customer_behaviour(int customers_in_queue, int i) {
    srand(time(NULL) * i);
    sleep(rand() % 3 + (i / 2)); // Покупатель засыпает на случайное количество секунд

    wait_sem(queue_sem); // Семафор, чтобы взять очередь
    fflush(stdout);
    printf("Покупатель номер %d встал в очередь\n", i);
    customers_in_queue++; // Увеличиваем количество покупателей в очереди
    signal_sem(queue_sem); // Семафор, чтобы отпустить очередь

    int random_cashier; // Случайный кассир
    if (customers_in_queue == 1) { // Если первый в очереди
        random_cashier = rand() % 2; // Выбирается случайный кассир
        if (random_cashier == 0) {
            signal_sem(cashier_1_sem); // Покупатель идет к первому кассиру
        } else {
            signal_sem(cashier_2_sem); // Покупатель идет ко второму кассиру
        }
    }
    if (random_cashier == 0) {
        wait_sem(for_cashier_1_sem); // Семафор ожидания обслуживания покупателя первым кассиром
    } else {
        wait_sem(for_cashier_2_sem); // Семафор ожидания обслуживания покупателя вторым кассиром
    }

    fflush(stdout);
    printf("Покупатель номер %d оплатил свои покупки у кассира номер %d.\n", i, random_cashier + 1); // Покупатель оплатил свои покупки

    wait_sem(queue_sem); // Семафор, чтобы взять очередь
    fflush(stdout);
    printf("Покупатель номер %d радостный пошел домой.\n", i);
    customers_in_queue--; // Уменьшаем количество покупателей в очереди
    signal_sem(queue_sem ); // Семафор, чтобы отпустить очередь

    exit(0);
}