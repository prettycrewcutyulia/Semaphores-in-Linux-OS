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

void cashier_behaviour();
void cleanup();
void signal_handler(int nsig);
void initialize_semaphores();
void create_shared_memory();

// Определение функции для освобождения всех семафоров и разделяемой памяти
void cleanup() {
    sem_close(cashier_1_sem); // Закрытие семафора для кассира 1
    sem_close(cashier_2_sem); // Закрытие семафора для кассира 2
    sem_close(queue_sem); // Закрытие семафора для очереди
    sem_close(for_cashier_1_sem); // Закрытие семафора для кассира 1
    sem_close(for_cashier_2_sem); // Закрытие семафора для кассира 2
    sem_close(start);
    sem_unlink(CASHIER_1_SEM_NAME); // Удаление семафора для кассира 1
    sem_unlink(CASHIER_2_SEM_NAME); // Удаление семафора для кассира 2
    sem_unlink(QUEUE_SEM_NAME); // Удаление семафора для очереди
    sem_unlink(FOR_CASHIER_1_SEM_NAME); // Удаление семафора для кассира 1
    sem_unlink(FOR_CASHIER_2_SEM_NAME); // Удаление семафора для кассира 2
    sem_unlink(START_SEM_NAME);
    munmap(customer_count_ptr, sizeof(int)); // Отключение разделяемой памяти
    close(shm_fd); // Закрытие дескриптора разделяемой памяти
    shm_unlink(SHM_NAME); // Удаление разделяемой памяти
}

// Обработчик сигнала SIGINT, который будет вызван при нажатии Ctrl+C
void signal_handler(int sig) {
    cleanup(); // функция для удаления всех созданных ресурсов
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
    start = sem_open(START_SEM_NAME, O_CREAT, 0600, 0);
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

int main() {
    signal(SIGINT, signal_handler); // регистрация обработчика сигнала SIGINT

    initialize_semaphores(); // создание семафоров
    create_shared_memory(); // создание общей памяти
    sem_wait(start);
    // процесс первого кассира
    cashier_behaviour(); // поведение первого кассира
    cleanup(); // очистка ресурсов, если они были созданы ранее
    return 0;
}

// Функция cashier_behaviour описывает поведение кассиров в магазине.
void cashier_behaviour() {
    while (customer_count_ptr[0] > 0) { // Пока в магазине есть покупатели
            sem_wait(cashier_2_sem); // Ожидание сигнала от покупателя, что он готов к оплате
        if (customer_count_ptr[0] == 0) { // Проверка, что в очереди больше нет покупателей
            exit(0);
        }
        fflush(stdout);
        printf("Кассир номер %d занят. Пробивает продукты покупателя.\n",
               2); // Вывод сообщения о том, что кассир начал обслуживание покупателя
        customer_count_ptr[0]--; // Уменьшение количества покупателей на 1

        if (customer_count_ptr[0] == 0) { // Если в очереди больше нет покупателей, то сигналится другой кассир
                sem_post(cashier_1_sem);
        }
            sem_post(for_cashier_2_sem); // Освобождение покупателя, что он может продолжить выполнение программы
    }
}
