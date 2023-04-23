#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <sys/fcntl.h>

#define CASHIER_1_SEM_NAME "/cashier_1"
#define CASHIER_2_SEM_NAME "/cashier_2"
#define QUEUE_SEM_NAME "/queue"
#define FOR_CASHIER_1_SEM_NAME "/for_cashier_1"
#define FOR_CASHIER_2_SEM_NAME "/for_cashier_2"
#define START_SEM_NAME "/start"
#define SHM_NAME "/customer_count"

sem_t *cashier_1_sem, *cashier_2_sem, *queue_sem, *for_cashier_1_sem, *for_cashier_2_sem, *start;
int num_customers;
int* customer_count_ptr;
int shm_fd;

void cashier_behaviour(int id);
void cleanup();
void signal_handler(int nsig);
void initialize_semaphores();
void create_shared_memory();

void customer_behaviour(int customers_in_queue, int i);

// Определение функции для освобождения всех семафоров и разделяемой памяти
void cleanup() {
    sem_close(cashier_1_sem); // Закрытие семафора для кассира 1
    sem_close(cashier_2_sem); // Закрытие семафора для кассира 2
    sem_close(queue_sem); // Закрытие семафора для очереди
    sem_close(for_cashier_1_sem); // Закрытие семафора для кассира 1
    sem_close(for_cashier_2_sem); // Закрытие семафора для кассира 2
    sem_unlink(CASHIER_1_SEM_NAME); // Удаление семафора для кассира 1
    sem_unlink(CASHIER_2_SEM_NAME); // Удаление семафора для кассира 2
    sem_unlink(QUEUE_SEM_NAME); // Удаление семафора для очереди
    sem_unlink(FOR_CASHIER_1_SEM_NAME); // Удаление семафора для кассира 1
    sem_unlink(FOR_CASHIER_2_SEM_NAME); // Удаление семафора для кассира 2
    munmap(customer_count_ptr, sizeof(int)); // Отключение разделяемой памяти
    close(shm_fd); // Закрытие дескриптора разделяемой памяти
    shm_unlink(SHM_NAME); // Удаление разделяемой памяти
}

// Обработчик сигнала SIGINT, который будет вызван при нажатии Ctrl+C
void signal_handler(int sig) {
    cleanup(); // функция для удаления всех созданных ресурсов
    sem_close(start); // Закрытие семафора для старта
    sem_unlink(START_SEM_NAME); // Удаление семафора для старта
    exit(0); // выход из программы
}
// Функция для инициализации семафоров
void initialize_semaphores() {
// Создание семафора для кассы 1, начальное значение 0
    if ((cashier_1_sem = sem_open(CASHIER_1_SEM_NAME, O_CREAT, 0600, 0)) == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
// Создание семафора для кассы 2, начальное значение 0
    if ((cashier_2_sem = sem_open(CASHIER_2_SEM_NAME, O_CREAT, 0600, 0)) == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
// Создание семафора для очереди, начальное значение 1
    if ((queue_sem = sem_open(QUEUE_SEM_NAME, O_CREAT, 0600, 1)) == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
// Создание семафора для кассы 1 для сигнала, что следующий покупатель готов обслуживаться, начальное значение 0
    if ((for_cashier_1_sem = sem_open(FOR_CASHIER_1_SEM_NAME, O_CREAT, 0600, 0)) == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
// Создание семафора для кассы 2 для сигнала, что следующий покупатель готов обслуживаться, начальное значение 0
    if ((for_cashier_2_sem = sem_open(FOR_CASHIER_2_SEM_NAME, O_CREAT, 0600, 0)) == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
// Создание семафора для старта, начальное значение 0
    if ((start = sem_open(START_SEM_NAME, O_CREAT, 0600, 0)) == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
}

// Функция создания разделяемой памяти и её отображения в адресное пространство текущего процесса
void create_shared_memory() {
// Создание объекта разделяемой памяти и получение файлового дескриптора
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0644);
//    if (shm_fd == -1) {
//        perror("shm_open");
//        exit(EXIT_FAILURE);
//    }
// Изменение размера объекта разделяемой памяти
    if (ftruncate(shm_fd, 4) == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }
// Отображение объекта разделяемой памяти в адресное пространство текущего процесса и получение указателя на него
    customer_count_ptr = mmap(NULL, 4, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
//    if (customer_count_ptr == MAP_FAILED) {
//        perror("mmap");
//        exit(EXIT_FAILURE);
//    }

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

    sem_post(start);
    sem_post(start);

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

    sem_wait(queue_sem); // Семафор, чтобы взять очередь
    fflush(stdout);
    printf("Покупатель номер %d встал в очередь\n", i);
    customers_in_queue++; // Увеличиваем количество покупателей в очереди
    sem_post(queue_sem); // Семафор, чтобы отпустить очередь

    int random_cashier; // Случайный кассир
    if (customers_in_queue == 1) { // Если первый в очереди
        random_cashier = rand() % 2; // Выбирается случайный кассир
        if (random_cashier == 0) {
            sem_post(cashier_1_sem); // Покупатель идет к первому кассиру
        } else {
            sem_post(cashier_2_sem); // Покупатель идет ко второму кассиру
        }
    }
    if (random_cashier == 0) {
        sem_wait(for_cashier_1_sem); // Семафор ожидания обслуживания покупателя первым кассиром
    } else {
        sem_wait(for_cashier_2_sem); // Семафор ожидания обслуживания покупателя вторым кассиром
    }

    fflush(stdout);
    printf("Покупатель номер %d оплатил свои покупки у кассира номер %d.\n", i, random_cashier + 1); // Покупатель оплатил свои покупки

    sem_wait(queue_sem); // Семафор, чтобы взять очередь
    fflush(stdout);
    printf("Покупатель номер %d радостный пошел домой.\n", i);
    customers_in_queue--; // Уменьшаем количество покупателей в очереди
    sem_post(queue_sem ); // Семафор, чтобы отпустить очередь

    exit(0);
}