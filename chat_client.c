#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <pthread.h>
#include <ncurses.h>

#define SERVER_PORT 9079
#define MAX_ID_LENGTH 10
#define MAX_STRING_LENGTH 256
#define WIDTH 120
#define HEIGHT 35
#define MAX_CLIENTS 5
#define MAX_COUNT_MESSAGE 30

char name[MAX_STRING_LENGTH];
char buffer[256];
int client_socket;
int inputIndex;
int outputIndex;
int max_y, max_x;
WINDOW *input_win, *output_win, *table_win;
volatile sig_atomic_t flag = 0;
volatile int shouldExit = 0;
char* names_online[MAX_CLIENTS];
char* messages[MAX_COUNT_MESSAGE];
int clients_online = 0;
int messages_counter = 0;

void draw_table();
void draw_input();
void draw_message(const char* format, ...);
void draw_all_buffer_messages();
void write_message(char* message);
void draw_struct();
void draw_output();
void draw_close_cards(int start_y, int start_x);

void draw_output() {
    wclear(output_win);
    box(output_win,0,0);
    wrefresh(output_win);
}

void remove_char_and_shift_left(char *str) {
    if (str == NULL || strlen(str) == 0) {
        // Проверка на корректность входных данных
        printf("Invalid input.\n");
        return;
    }

    // Сдвигаем символы влево начиная с первого символа
    for (int i = 0; i < strlen(str) - 1; ++i) {
        str[i] = str[i + 1];
    }

    // Заменяем последний символ нулевым символом завершения строки
    str[strlen(str) - 1] = '\0';
}

void draw_table() {
    box(table_win,0,0);
    mvwprintw(table_win ,1, 1, "users online:");
    overlay(table_win, output_win);
    for(int i=0; i<clients_online; i++) {
        mvwprintw(table_win , 2+i, 1, names_online[i]);
        overlay(table_win, output_win);
        wrefresh(table_win);
    }
}

void draw_input() {
    wclear(input_win);
    mvwprintw(input_win ,0, 0, "Type your message and press Enter. Press 'q' to quit");
    wrefresh(input_win);
}

void draw_message(const char* format, ...) {
    va_list args;
    va_start(args, format);

    char message[MAX_STRING_LENGTH]; // Предполагаем, что не более 255 символов
    vsnprintf(message, sizeof(buffer), format, args);

    va_end(args);
    if(messages_counter == MAX_COUNT_MESSAGE) {
        for(int i=0; i<MAX_COUNT_MESSAGE; i++) {
            strcpy(messages[i], "");
        }
        messages_counter = 0;
    }
    strcpy(messages[messages_counter++], message);
    inputIndex = 0; // Сбрасываем индекс для следующего ввода
    if(outputIndex > max_y - 5) {
        draw_output();
        outputIndex=0;
    }
    mvwprintw(output_win , 1+outputIndex++, 1, "%s", message);
    draw_input();
    draw_table();
    move(max_y-1, inputIndex);
    wrefresh(output_win);
}

void draw_all_buffer_messages() {
    for(int i=0; i<outputIndex; i++) {
        mvwprintw(output_win , 1+i, 1, "%s", messages[messages_counter-outputIndex+i]);
        move(max_y-1, inputIndex);
        wrefresh(output_win);
    }
}

void write_message(char* message) {
    char *answer = (char *) malloc(sizeof(char) * (MAX_STRING_LENGTH*2+2));
    snprintf(answer, snprintf(NULL, 0, "%s: %s", name, buffer) + 1, "%s: %s", name, buffer);
    write(client_socket, answer, strlen(answer));

    draw_message(answer);
    free(answer);
}

int longest_string_length(char *array_of_strings[], int num_strings) {
    if (num_strings == 0 || array_of_strings == NULL) {
        return 0; // Нет строк или некорректные аргументы
    }

    int max_length = strlen(array_of_strings[0]); // Инициализируем максимальную длину

    for (int i = 1; i < num_strings; ++i) {
        int current_length = strlen(array_of_strings[i]);

        if (current_length > max_length) {
            max_length = current_length; // Обновляем максимальную длину
        }
    }

    return max_length > 13 ? max_length : 13;
}

void draw_close_cards(int start_y, int start_x) {
    // Размеры карты
    int card_width = 9;
    int card_height = 5;

    int center_x = (max_x-3-longest_string_length(names_online, MAX_CLIENTS))/2;

    for(int i=0; i<4;i++) {
        mvwprintw(output_win,i+1, 1, "XXXXX");
    }   

    for(int i=0; i<4;i++) {
        mvwprintw(output_win,i+1, 7,"XXXXX");
    }  

    mvwprintw(output_win, 1, center_x,"X");

    wrefresh(output_win);
}

void draw_struct() {
    wresize(output_win, max_y-2, max_x);
    mvwin(input_win, max_y-2, 0);
    wresize(table_win, MAX_CLIENTS+2, longest_string_length(names_online, MAX_CLIENTS) + 2);
    mvwin(table_win, 1, max_x-longest_string_length(names_online, MAX_CLIENTS) - 3);

    draw_output();

    draw_table();

    draw_input();

    draw_close_cards(1,1);

    move(max_y-1, inputIndex);
    wrefresh(output_win);
}

void remove_last_newline(char* str) {
    int len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') {
        str[len - 1] = '\0'; // Заменить последний символ на нулевой символ
    }
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

int check_correctness_input(char input[], char correct_input[]) {
    if(strcmp(input, correct_input) == 0) {
        return 0;
    } else {
        printf("Invalid input. Repeat again\n");
        return 1;
    }
}

void * receive_thread(void* arg) {
    char* buffer = (char *) malloc(sizeof(char) * (MAX_STRING_LENGTH*MAX_CLIENTS));

    while(!shouldExit) {
        int bytes_received = recv(client_socket, buffer, MAX_STRING_LENGTH*MAX_CLIENTS, 0);
        buffer[bytes_received] = '\0';
        if(buffer[0] == 'm') {
            remove_char_and_shift_left(buffer);
            draw_message(buffer);
        } else if(buffer[0] == 'o') {
            remove_char_and_shift_left(buffer);
            strcpy(names_online[clients_online++], buffer);
            draw_table();
        } else if(buffer[0] == 'O') {
            remove_char_and_shift_left(buffer);
            char one_name[MAX_STRING_LENGTH];
            int count=0;
            for(int i=0; buffer[i]!='\0'; i++) {
                if (buffer[i] == '$') {
                    one_name[count] = '\0';
                    strcpy(names_online[clients_online++], one_name);

                    count = 0;
                } else {
                    one_name[count++] = buffer[i];
                }
            }
            draw_table();
        }
    }

    free(buffer);
    pthread_exit(NULL);
}

void handle_resize(int sig) {
    endwin();
    refresh();
    clear();

    initscr();
    getmaxyx(stdscr, max_y, max_x);
    draw_struct();
    draw_all_buffer_messages();
    flag = 1;
}

int main() {
    char server_ip[] = "127.0.0.1"; // IP-адрес сервера
    char id[MAX_ID_LENGTH+1];
    pthread_t receive_thread_id;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        names_online[i] = (char *)malloc((MAX_STRING_LENGTH + 1) * sizeof(char));
        // Плюс 1 для учета завершающего нулевого символа строки
    }

    for (int i = 0; i < MAX_COUNT_MESSAGE; i++) {
        messages[i] = (char *)malloc((MAX_STRING_LENGTH + 1) * sizeof(char));
        // Плюс 1 для учета завершающего нулевого символа строки
    }
    
    // Создание сокета
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Error when creating a socket");
        exit(1);
    }

    // Настройка клиента
    struct sockaddr_in server_address, client_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT+2);
    if (inet_pton(AF_INET, server_ip, &server_address.sin_addr) <= 0) {
        perror("Ошибка при настройке адреса сервера");
        exit(1);
    }

    printf("Welcome to the online chat client\n");
    printf("Enter \"start\" to start\n");

    while(1) {
        fgets(buffer, sizeof(buffer), stdin);
        if(check_correctness_input(buffer, "start\n") == 0) {
            break;
        }
    }

    //Подключение к серверу
    printf("Connecting to the server...\n");
    if (connect(client_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        perror("Error connecting to the server");
        exit(1);
    }
    printf("The connection to the server was successful!\n\n");

    bzero(buffer, sizeof(buffer));
    read(client_socket, buffer, sizeof(buffer)); 

    if(strcmp(buffer, "-1") == 0) {
        printf("The maximum number of users is already connected on the server. Wait until the space is free and try again later\n");
        return 0;
    }
    else {
        strncpy(id, buffer, sizeof(id) - 1);
        id[sizeof(id) - 1] = '\0';  
    }

    printf("My id is - %s\n", id);

    printf("First you need to pass authorization\nEnter your nickname:\n");

    while (1) {
        bzero(buffer, sizeof(buffer));
        fgets(buffer, sizeof(buffer), stdin);
        remove_last_newline(buffer);

        char possible_name[256];
        strcpy(possible_name, buffer);

        printf("Are you sure you want to use the nickname \"%s\".\nFor the answer, use [Y/N]\n", possible_name);

        bzero(buffer, sizeof(buffer));
        fgets(buffer, sizeof(buffer), stdin);
        if(strcmp(buffer, "Y\n") == 0 || strcmp(buffer, "y\n") == 0) {
            remove_last_newline(buffer);
            write_string(client_socket, possible_name);
            strcpy(name, possible_name);
            break;
        } else if(strcmp(buffer, "N\n") == 0 || strcmp(buffer, "n\n") == 0) {
            printf("Enter the name again\n");
            continue;
        } else {
            printf("Invalid input. Repeat again\n");
            continue;
        }
    }

    bzero(buffer, sizeof(buffer));
    read(client_socket, buffer, sizeof(buffer)); 
    printf("%s\n", buffer);
    close(client_socket);

    sleep(2);

    server_address.sin_port = htons(SERVER_PORT);

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Ошибка при создании сокета");
        exit(1);
    }

    if (connect(client_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        perror("Error connecting to the server");
        exit(1);
    }
    printf("connected!\n");

    bzero(buffer, sizeof(buffer));
    write_string(client_socket, id);

    signal(SIGWINCH, handle_resize); // Обработка изменения размеров окна
    inputIndex = 0;
    outputIndex = 0;

    strcpy(names_online[clients_online++], name);

    initscr(); // Инициализация ncurses
    raw();     // Режим raw (ввод символов без буферизации)
    noecho();  // Отключение отображения введенных символов

    getmaxyx(stdscr, max_y, max_x);

    output_win = newwin(max_y-2, max_x, 0, 0); // Окно для ввода
    input_win = newwin(2, max_x, max_y-2, 0);
    table_win = newwin(MAX_CLIENTS+3, longest_string_length(names_online, MAX_CLIENTS) + 2, 1, max_x-longest_string_length(names_online, MAX_CLIENTS) - 3);
    refresh();

    draw_struct();

    if (pthread_create(&receive_thread_id, NULL, receive_thread, NULL) != 0) {
        perror("Ошибка при создании потока");
        close(client_socket);
    }

    while (1) {
        int ch = getch();

        if (ch == '\n') {
            buffer[inputIndex] = '\0'; // Добавляем завершающий нуль
            if(strcmp(buffer, "") == 0) {
                continue;
            }
            if(strcmp(buffer, "exit") == 0) {
                break;
            }
            write_message(buffer);
            bzero(buffer, sizeof(buffer)); // Очищаем буфер
        } else if (ch == KEY_BACKSPACE || ch == 127) {
            // Обработка клавиши Backspace
            if (inputIndex > 0) {
                inputIndex--;
                mvaddch(max_y - 1, inputIndex, ' '); // Заменяем символ на пробел 
                move(max_y - 1, inputIndex); // Смещаем курсор
                wrefresh(input_win);
            }
        } else if ((ch < 33 ||  ch>126) && ch != ' ') {
            continue;
        } else {
            if(flag) {
                draw_struct();
                draw_all_buffer_messages();
                flag = 0;
            }
            if (inputIndex < max_x - 1) {
                mvaddch(max_y - 1, inputIndex, ch); // Отображаем символ
                buffer[inputIndex] = ch; // Сохраняем символ в буфере
                inputIndex++;
                move(max_y - 1, inputIndex); // Смещаем курсор
                wrefresh(input_win);
                wrefresh(output_win);
            }
        }
    }

    endwin(); // Завершение работы с ncurses

    shouldExit = 1;

    pthread_join(receive_thread_id, NULL);

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        free(names_online[i]);
    }
    for (int i = 0; i < MAX_COUNT_MESSAGE; ++i) {
        free(messages[i]);
    }

    close(client_socket);

    return 0;
}
