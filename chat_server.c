#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include <sys/shm.h>

#define MAX_CLIENTS 4
#define MAX_STRING_LENGTH 256
#define MAX_ID_LENGTH 10
#define SERVER_PORT_PARENT 9079
#define SERVER_PORT_CHILD SERVER_PORT_PARENT+2

struct SharedData {
    char clients_ids[MAX_CLIENTS][MAX_ID_LENGTH+1];
    char clients_names[MAX_CLIENTS][MAX_STRING_LENGTH];
};

struct SharedData *data;
int server_socket_parent, server_socket_child, clients_in_chat_sockets[MAX_CLIENTS];
struct sockaddr_in server_address_parent, server_address_child;
socklen_t client_address_length;
int connected_clients;
int connected_in_chat_clients;
pthread_t chat_client[MAX_CLIENTS];
volatile sig_atomic_t signal_received = 0;
int shmid;


int find_string_index(int max_clients, int max_id_length, char array[][max_id_length], char *target) {
    for (int i = 0; i < max_clients; ++i) {
        if (strcmp(array[i], target) == 0) {
            return i; // строка найдена, возвращаем индекс
        }
    }
    return -1; // строка не найдена, возвращаем -1
}

void insert_char_and_shift_right(char *str, char ch) {
    if (str == NULL || strlen(str) == 0) {
        // Проверка на корректность входных данных
        printf("Invalid input.\n");
        return;
    }

    // Сдвигаем символы вправо начиная с конца строки
    for (int i = strlen(str); i > 0; --i) {
        str[i] = str[i - 1];
    }

    // Вставляем новый символ на место первого элемента
    str[0] = ch;
}

void remove_last_newline(char* str) {
    int len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') {
        str[len - 1] = '\0'; // Заменить последний символ на нулевой символ
    }
}

char* generate_random_id(char *random_id) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    srand(time(NULL)); // Инициализация генератора случайных чисел

    if (random_id) {
        for (int i = 0; i < MAX_ID_LENGTH; i++) {
            random_id[i] = charset[rand() % (sizeof(charset) - 1)];
        }

        random_id[MAX_ID_LENGTH] = '\0'; // Завершающий нуль для создания строки C
    }

    return random_id;
}

char* sockaddr_in_to_string(struct sockaddr_in *address) {
    char *result = (char*)malloc(INET_ADDRSTRLEN + 6); // Длина строки для IP + ':' + порт
    if (!result) {
        perror("Ошибка выделения памяти");
        exit(EXIT_FAILURE);
    }

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(address->sin_addr), ip, INET_ADDRSTRLEN);

    snprintf(result, INET_ADDRSTRLEN + 6, "%s:%d", ip, ntohs(address->sin_port));

    return result;
}

void write_string(int client_socket, const char* format, ...) {
    va_list args;
    va_start(args, format);

    char buffer[256]; // Предполагаем, что не более 255 символов
    vsnprintf(buffer, sizeof(buffer), format, args);

    va_end(args);

    char* result = malloc(strlen(buffer) + 1);
    if (result != NULL) {
        strcpy(result, buffer);
    }

    write(client_socket, result, strlen(result));
}

void send_online_names(int except) {
    // Создание буфера для конкатенации слов
    char buffer[MAX_STRING_LENGTH*(MAX_CLIENTS-1)+MAX_CLIENTS-2];  // Предполагаем максимальный размер буфера

    // Заполнение буфера словами с разделителями (вместо сетевых функций используйте свои)
    bzero(buffer, MAX_STRING_LENGTH*(MAX_CLIENTS-1)+MAX_CLIENTS-3);
    int n = connected_in_chat_clients;
    for (int i = 0; i < connected_in_chat_clients; ++i) {
        if(i!=except) {
            strcat(buffer, data->clients_names[i]);
            // Добавление разделителя между словами
            if (--n>0) {
                strcat(buffer, "$");
            }
        }
    }
    insert_char_and_shift_right(buffer, 'O');
    write_string(clients_in_chat_sockets[except], buffer);
    printf("%d: %s\n", except, buffer);
}

void write_to_all_online_users(char* message, int except) {
    for(int i=0; i<MAX_CLIENTS; i++) {
        if(i!=except && clients_in_chat_sockets[i] != -1) {
            write_string(clients_in_chat_sockets[i], message);
        }
    }
}

void * handle_new_client(void* arg) {
    int client_socket = *((int*)arg);
    free(arg); // Освободить память, выделенную для аргумента
    int num;
    char buffer[256];

    if (connected_clients < MAX_CLIENTS) {
        num = connected_clients;
        generate_random_id(data->clients_ids[connected_clients++]);
    } else {
        write_string(client_socket, "-1");
        close(client_socket);
        pthread_exit(NULL);
    }

    strcpy(buffer, data->clients_ids[num]);
    write_string(client_socket, buffer);

    printf("Подключенных %d\n", connected_clients);

    bzero(buffer, sizeof(buffer));
    read(client_socket, buffer, sizeof(buffer));

    write_string(client_socket, "Welcome to the game %s. You are redirected to the table.\n", buffer);
    strcpy(data->clients_names[num], buffer);
    kill(getppid(), SIGUSR1);
    close(client_socket);
    pthread_exit(NULL);
}

void * chat_with_client() {
    printf("создан новый поток (Ctrl+C)\n");
    struct sockaddr_in client_address;
    char name[256]; 
    int num;
    char buffer[256];
    int client_socket = accept(server_socket_parent, (struct sockaddr*)&client_address, &client_address_length);
    if (client_socket == -1) {
        perror("Ошибка при принятии клиента на родительском сервере");
    }

    printf("подключился к родительскому процессу %s\n", sockaddr_in_to_string(&client_address));

    bzero(buffer, sizeof(buffer));
    read(client_socket, buffer, sizeof(buffer));

    if(find_string_index(MAX_CLIENTS, MAX_ID_LENGTH + 1, data->clients_ids, buffer) == -1 || connected_clients >= MAX_CLIENTS){
        write_string(client_socket, "You are trying to connect to the chat without logging in. Please go through authorization first.\n");
        close(client_socket);
        pthread_exit(NULL);
    }
    strcpy(name, data->clients_names[find_string_index(MAX_CLIENTS, MAX_ID_LENGTH + 1, data->clients_ids, buffer)]);
    printf("%s connected\n", name);

    num = connected_in_chat_clients;
    clients_in_chat_sockets[connected_in_chat_clients++] = client_socket;

    send_online_names(num);

    bzero(buffer, sizeof(buffer));
    strcpy(buffer, name);
    insert_char_and_shift_right(buffer, 'o');
    write_to_all_online_users(buffer, num);

    while(1) {
        bzero(buffer, sizeof(buffer));
        read(client_socket, buffer, sizeof(buffer));
        printf("%s\n", buffer);

        insert_char_and_shift_right(buffer, 'm');
        write_to_all_online_users(buffer, num);
    }

    pthread_exit(NULL);
}

void signal_handler(int signo) {
    printf("Получен сигнал SIG1 (Ctrl+C)\n");
    if (pthread_create(&chat_client[MAX_CLIENTS], NULL, chat_with_client, NULL) != 0) {
        perror("Ошибка при создании потока");
    }
}

void sigint_handler(int signo) {
    if (signo == SIGINT) {
        printf("Получен сигнал SIGINT (Ctrl+C)\n");
        kill(SIGKILL, getpid());
        close(server_socket_parent);
        close(server_socket_child);
        for(int i=0; i<MAX_CLIENTS; i++) {
            close(clients_in_chat_sockets[i]);
        }
        shmdt(data);
        shmctl(shmid, IPC_RMID, NULL);
        exit(0);
    }
}

int main() {
    // Объявление переменных
    pid_t new_client_waiting;
    pthread_t authorize_client[MAX_CLIENTS];
    char server_ip[] = "127.0.0.1";
    struct sockaddr_in client_address;
    client_address_length = sizeof(client_address);
    connected_clients = 0;
    connected_in_chat_clients = 0; 

    // Создание сокета для родительского сервера
    server_socket_parent = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_parent == -1) {
        perror("Ошибка при создании сокета для родительского сервера");
        exit(1);
    }

    // Настройка родительского сервера
    server_address_parent.sin_family = AF_INET;
    server_address_parent.sin_port = htons(SERVER_PORT_PARENT);
    if (inet_pton(AF_INET, server_ip, &server_address_parent.sin_addr) <= 0) {
        perror("Ошибка при настройке адреса сервера");
        exit(1);
    }

    // Привязка сокса к IP и порту для родительского сервера
    if (bind(server_socket_parent, (struct sockaddr*)&server_address_parent, sizeof(server_address_parent)) == -1) {
        perror("Ошибка при привязке сокса для родительского сервера");
        exit(1);
    }

    // Создание сокета для дочернего сервера
    server_socket_child = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_child == -1) {
        perror("Ошибка при создании сокета для дочернего сервера");
        exit(1);
    }

    // Настройка дочернего сервера
    server_address_child.sin_family = AF_INET;
    server_address_child.sin_port = htons(SERVER_PORT_CHILD);
    if (inet_pton(AF_INET, server_ip, &server_address_child.sin_addr) <= 0) {
        perror("Ошибка при настройке адреса сервера");
        exit(1);
    }

    // Привязка сокса к IP и порту для дочернего сервера
    if (bind(server_socket_child, (struct sockaddr*)&server_address_child, sizeof(server_address_child)) == -1) {
        perror("Ошибка при привязке сокса для дочернего сервера");
        exit(1);
    }

    // Ожидание подключения клиентов на родительском сервере
    if (listen(server_socket_parent, MAX_CLIENTS ) == -1) {
        perror("Ошибка при ожидании клиентов для родительского сервера");
        exit(1);
    }

    // Ожидание подключения клиентов на дочернем сервере
    if (listen(server_socket_child, MAX_CLIENTS + 1) == -1) {
        perror("Ошибка при ожидании клиентов для дочернего сервера");
        exit(1);
    }

    for(int i=0; i<MAX_CLIENTS; i++) {
        clients_in_chat_sockets[i] = -1;
    }

    printf("Родительский сервер ожидает подключений на порту %d\n", SERVER_PORT_PARENT);
    printf("Дочерний сервер ожидает подключений на порту %d\n", SERVER_PORT_CHILD);

    // Создаем сегмент разделяемой памяти
    shmid = shmget(IPC_PRIVATE, sizeof(struct SharedData), IPC_CREAT | 0666);

    if (shmid < 0) {
        perror("Ошибка создания сегмента разделяемой памяти");
        return 1;
    }

    // Присоединяем сегмент разделяемой памяти к адресному пространству процесса
    data = (struct SharedData *)shmat(shmid, NULL, 0);

    new_client_waiting = fork();

    if(new_client_waiting < 0) {
        printf("Ошибка при создании дочернего процесса \"new_client_waiting\"\n");
        exit(1);
    } else if(new_client_waiting == 0) {
        int client_socket;
        while (1)
        {
            client_socket = accept(server_socket_child, (struct sockaddr*)&client_address, &client_address_length);
            if (client_socket == -1) {
                perror("Ошибка при принятии клиента");
                continue;
            }

            printf("подключился к дочернему процессу %s\n", sockaddr_in_to_string(&client_address));
            int* client_socket_ptr = (int*)malloc(sizeof(int));
            *client_socket_ptr = client_socket;
            if (pthread_create(&authorize_client[MAX_CLIENTS], NULL, handle_new_client, client_socket_ptr) != 0) {
                perror("Ошибка при создании потока");
                close(client_socket);
            }
        }
        close(client_socket);
        close(server_socket_child);
    } else {
        signal(SIGUSR1, signal_handler);
        signal(SIGINT, sigint_handler);
        printf("Ожидание сигнала SIGUSR1...\n");

        while(1) {
            sleep(1);
        }


        close(server_socket_parent);
        shmdt(data);
        shmctl(shmid, IPC_RMID, NULL);
    }

    return 0;
}