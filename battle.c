#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#ifndef PORT
    #define PORT x
#endif

# define SECONDS 10
# define BUSY 1
# define NOTBUSY 0
# define NAME_MAX_LENGTH 20
# define ACTIVE 1
# define NOTACTIVE 0
# define MAX_BUF 1024

struct client {
    int fd;
    struct in_addr ipaddr;
    struct client *next;
    struct client *opponent;
    char name[NAME_MAX_LENGTH];
    int state;
    int active;
    int hitpoints;
    int powermoves;
    int healthpotion;
    struct client *matched_clients;
};

static struct client *addclient(struct client *top, int fd, struct in_addr addr, char *name);
static struct client *removeclient(struct client *top, int fd);
static void broadcast(struct client *top, char *s, int size, int clientfd);
int handleclient(struct client *p, struct client *top);
static void matchandplay(struct client *top);
int checkmatch(struct client* player1, struct client * player2);
void use_potion(struct client *top, struct client *user, struct client *opponent);
void end_game(struct client *top, struct client *winner, struct client *loser);
void perform_power_move(struct client* top, struct client *attacker, struct client *defender);
void perform_attack(struct client *top, struct client *attacker, struct client *defender);
void handle_battle_action(struct client *top, struct client *player, struct client *opponent, char *input);
void initialize_player_stats(struct client *player);
char *read_data(int socket);

int bindandlisten(void);

int main(void) {
    int clientfd, maxfd, nready;
    struct client *p;
    struct client *head = NULL;
    socklen_t len;
    struct sockaddr_in q;
    struct timeval tv;
    fd_set allset;
    fd_set rset;

    int i, j;

    int listenfd = bindandlisten();
    // initialize allset and add listenfd to the
    // set of file descriptors passed into select
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    // maxfd identifies how far into the set to search
    maxfd = listenfd;

    while (1) {
        // make a copy of the set before we pass it into select
        rset = allset;
        /* timeout in seconds (You may not need to use a timeout for
        * your assignment)*/
        tv.tv_sec = SECONDS;
        tv.tv_usec = 0;  /* and microseconds */

        nready = select(maxfd + 1, &rset, NULL, NULL, &tv);
        if (nready == 0) {
            printf("No response from clients in %d seconds\n", SECONDS);
            continue;
        }

        if (nready == -1) {
            perror("select");
            continue;
        }

        if (FD_ISSET(listenfd, &rset)){
            printf("a new client is connecting\n");
            len = sizeof(q);
            if ((clientfd = accept(listenfd, (struct sockaddr *)&q, &len)) < 0) {
                perror("accept");
                exit(1);
            }
            printf("%d \n", clientfd);
            FD_SET(clientfd, &allset);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
            char *message = "What is your name?\n";
            write(clientfd, message, strlen(message));
            char *user_input = read_data(clientfd);
            if (user_input == NULL) {
            // Handle error
            printf("Error reading user input\n");
        }

        // Add the client with the user input as the name
            user_input[strcspn(user_input, "\r\n")] = 0; // Remove newline characters
            head = addclient(head, clientfd, q.sin_addr, user_input);
            char outbuf[512];
            sprintf(outbuf, "Welcome %s \n", user_input);
            j = write(clientfd, outbuf, strlen(outbuf));
            if (j <= 0){
                removeclient(head, clientfd);
            }
            sprintf(outbuf, "** %s enters the arena ** \n", user_input);
            broadcast(head, outbuf, strlen(outbuf), clientfd);
            free(user_input); // Free dynamically allocated memory
            matchandplay(head);
        }
        for (i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, &rset)) {
                for (p = head; p != NULL; p = p->next) {
                    if (p->fd == i) {
                        if (p->active != NOTACTIVE){
                            printf("%s", p->name);
                            printf("%d \n", p->fd);
                            printf("%d \n", p->active);
                            char move;
                            int num_bytes_read = read(p->fd, &move, sizeof(move));
                            if (num_bytes_read < 0){
                                head = removeclient(head, p->fd);
                                break;
                            }
                            if (move == 's'){
                                char *msg = "Enter you message: \r\n";
                                j = write(p->fd, msg, strlen(msg));
                                if (j < 0){
                                    head = removeclient(head, p->fd);
                                }
                                move = 's';
                                char *message = read_data(p->fd);
                                if (message == NULL) {
                                    // Error or connection closed
                                    head = removeclient(head, p->fd);
                                    free(message);
                                    break;
                                }
                                else{
                                    char full_message[strlen(message) + 3]; // +3 for 's', space, and null terminator
                                    snprintf(full_message, sizeof(full_message), "s %s", message);
                                    // Handle the 's' move
                                    handle_battle_action(head, p, p->opponent, full_message);
                                    // Free dynamically allocated memory for the message
                                    free(message);
                                    break;
                                }
                            }
                            else{
                                char move_str[2] = {move, '\0'};
                                handle_battle_action(head, p, p->opponent, move_str);
                                break;
                            }
                        }
                        int result = handleclient(p, head);
                        if (result == -1) {
                            int tmp_fd = p->fd;
                            head = removeclient(head, p->fd);
                            FD_CLR(tmp_fd, &allset);
                            close(tmp_fd);
                        }
                        break;
                    }
                }
            }
        }
    }
    return 0;
}


int bindandlisten(void) {
    struct sockaddr_in r;
    int listenfd;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    int yes = 1;
    if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) {
        perror("setsockopt");
    }
    memset(&r, '\0', sizeof(r));
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(PORT);

    if (bind(listenfd, (struct sockaddr *)&r, sizeof r)) {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, 5)) {
        perror("listen");
        exit(1);
    }
    return listenfd;
}
int handleclient(struct client *p, struct client *top) {
    char buf[256];
    int len = read(p->fd, buf, sizeof(buf) - 1);
    if (len >= 0){
        return 0;
    }
    else{
        // socket is closed
        return -1;
    }
}

static void broadcast(struct client *top, char *s, int size, int fd) {
    struct client *p;
    for (p = top; p; p = p->next) {
        if (p->fd != fd){
            int i = write(p->fd, s, size);
            if (i <= 0){
                removeclient(top, p->fd);
            }
        }
    }
    /* should probably check write() return value and perhaps remove client */
}

static struct client *addclient(struct client *top, int fd, struct in_addr addr, char* name) {
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
        perror("malloc");
        exit(1);
    }

    printf("Adding client %s\n", inet_ntoa(addr));

    p->fd = fd;
    p->ipaddr = addr;
    p->active = NOTACTIVE;
    p->next = top;
    strncpy(p->name, name, NAME_MAX_LENGTH - 1);
    p->name[NAME_MAX_LENGTH - 1] = '\0';
    p->opponent = NULL;
    p->state = NOTBUSY;
    top = p;
    return top;
}

static struct client *removeclient(struct client *top, int fd) {
    struct client **p;
    printf("Removing\n ");
    for (p = &top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        printf("%s left the arena", (*p)->name);
        struct client *opp = (*p)->opponent;
        opp->opponent = NULL;
        opp->state = NOTBUSY;
        opp->active = NOTACTIVE;
        struct client *t = (*p)->next;
        char outbuf[512];
        sprintf(outbuf, "%s left the arena \n", (*p)->name);
        broadcast(top, outbuf, sizeof(outbuf), (*p)->fd);
        printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));
        free(*p);
        *p = t;
    } else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                 fd);
    }
    return top;
}

static void matchandplay(struct client *top){
    struct client *p;
    struct client *player1 = NULL;
    struct client *player2 = NULL;
    for (p = top; p; p = p->next) {
        if (p->state == NOTBUSY){
            player1 = p;
            break;
        }
    }
    for (p = top; p; p = p->next){
        if(p->fd != player1->fd){
            if (p->state == NOTBUSY){
                int r = checkmatch(player1, p);
                if (r == 0){
                    player2 = p;
                    break;
                }
            }
        }
    }
    char outbuf[512];
    if(player1 != NULL && player2 != NULL){
        player1->state = BUSY;
        player2->state = BUSY;
        initialize_player_stats(player1);
        initialize_player_stats(player2);
        int i;

        sprintf(outbuf, "You engage %s \n", player2->name);
        i = write(player1->fd, outbuf, strlen(outbuf));
        if (i <= 0){
            removeclient(top, player1->fd);
            player2->state = NOTBUSY;
            return;
        }
        sprintf(outbuf, "You engage %s \n", player1->name);
        i = write(player2->fd, outbuf, strlen(outbuf));
        if (i <= 0){
            removeclient(top, player2->fd);
            player2->state = NOTBUSY;
            return;
        }
        struct client *matched_1;
        struct client *matched_2;
        player1->opponent = player2;
        player2->opponent = player1;
        matched_1 = addclient(player1->matched_clients, player2->fd, player2->ipaddr, player2->name);
        player1->matched_clients = matched_1;
        player2->opponent = player1;
        matched_1 = addclient(player1->matched_clients, player2->fd, player2->ipaddr, player2->name);
        player1->matched_clients = matched_1;
        matched_2 = addclient(player2->matched_clients, player1->fd, player1->ipaddr, player1->name);
        player2->matched_clients = matched_2;
        srand(time(NULL));
        int random_number = rand() % 2;
        matched_2 = addclient(player2->matched_clients, player1->fd, player1->ipaddr, player1->name);
        player2->matched_clients = matched_2;
        srand(time(NULL));
        int random_number = rand() % 2;
        if (random_number == 0){
            player1->active = ACTIVE;
            player2->active = NOTACTIVE;
            char message[256]; // Adjust the size as needed
            int num_chars = snprintf(message, sizeof(message), "%s's hitpoints: %d\n\n(a) Attack\n(p) Power Move\n(h) Health Potion\n(s) Speak something\n", player2->name, player2->hitpoints);
            i = write(player1->fd, message, num_chars);
            if (i <= 0){
                removeclient(top, player1->fd);
                return;
            }
            char waiting_msg[256];
            num_chars = snprintf(waiting_msg, sizeof(waiting_msg), "Waiting for %s to attack\n", player1->name);
            i = write(player2->fd, waiting_msg, num_chars);
            if (i <=  0){
                removeclient(top, player2->fd);
                return;
            }
        }
        else{
            player2->active = ACTIVE;
            player1->active = NOTACTIVE;
            char message[256]; // Adjust the size as needed
            int num_chars = snprintf(message, sizeof(message), "%s's hitpoints: %d\n\n(a) Attack\n(p) Power Move\n(h) Health Potion\n(s) Speak something\n", player1->name, player1->hitpoints);
            i = write(player2->fd, message, num_chars);
            if (i <=  0){
                removeclient(top, player2->fd);
                return;
            }
            char waiting_msg[256];
            num_chars = snprintf(waiting_msg, sizeof(waiting_msg), "Waiting for %s to attack\n", player2->name);
            i = write(player1->fd, waiting_msg, num_chars);
            if (i <=  0){
                removeclient(top, player1->fd);
                return;
            }
        }
    }
}

int checkmatch(struct client* player1, struct client * player2){
    // return 1 if player 2 is in player 1's linked list of already matched clients else return 0
    struct client *alreadymatched = player1->matched_clients;
    struct client *p;
    for(p = alreadymatched; p; p->next){
        if (p->fd == player2->fd){
            return 1;
        }
    }
    return 0;
}

void initialize_player_stats(struct client *player) {
    player->hitpoints = (rand() % 11) + 20; // 20 to 30 hitpoints
    player->powermoves = (rand() % 3) + 1;  // 1 to 3 powermoves
    player->healthpotion = 1; // MODIFICATION: each player gets 1 health potion per game
    player->state = BUSY;
}


void handle_battle_action(struct client *top, struct client *player, struct client *opponent, char *input) {
    int i;
    if (player->active != ACTIVE) {
        // It's not this player's turn
        return;
    }

    if (strcmp(input, "a") == 0) {
        // Perform a regular attack
        perform_attack(top, player, opponent);
    } else if (strcmp(input, "p") == 0 && player->powermoves > 0) {
        // Perform a powermove
        perform_power_move(top, player, opponent);
    } else if (strcmp(input, "h") == 0) {
        // Use player's health potion
        use_potion(top, player, opponent);
    } else if (strncmp(input, "s ", 2) == 0) {
        // Send a message
        char *msg = input + 2;
        i = write(opponent->fd, msg, strlen(msg));
        if (i <=  0){
            removeclient(top, opponent->fd);
            return;
        }
    }

    // Check if the game has ended
    if (opponent->hitpoints <= 0) {
        end_game(top, player, opponent);
    } else {
        if (strncmp(input, "s ", 2) != 0){
        // Switch turn
        printf("switching");
        player->active = NOTACTIVE;
        opponent->active = ACTIVE;
        char message[256]; // Adjust the size as needed
        int num_chars = snprintf(message, sizeof(message), "%s's hitpoints: %d\n\n(a) Attack\n(p) Power Move\n(h) Health Potion\n(s) Speak something\n", player->name, player->hitpoints);
        i = write(opponent->fd, message, num_chars);
        if (i <=  0){
            removeclient(top, opponent->fd);
            return;
        }
        char waiting_msg[256];
        num_chars = snprintf(waiting_msg, sizeof(waiting_msg), "Waiting for %s to attack\n", opponent->name);
        i = write(player->fd, waiting_msg, num_chars);
        if (i <=  0){
            removeclient(top, player->fd);
            return;
        }
        }
        else{
            char message[256]; // Adjust the size as needed
            int num_chars = snprintf(message, sizeof(message), "%s's hitpoints: %d\n\n(a) Attack\n(p) Power Move\n(h) Health Potion\n(s) Speak something\n", opponent->name, opponent->hitpoints);
            i = write(player->fd, message, num_chars);
            if (i <= 0){
                removeclient(top, player->fd);
                return;
            }
            char waiting_msg[256];
            num_chars = snprintf(waiting_msg, sizeof(waiting_msg), "Waiting for %s to attack\n", player->name);
            i = write(opponent->fd, waiting_msg, num_chars);
            if (i <= 0){
                removeclient(top, player->fd);
                return;
            }
        }
    }
}
void perform_attack(struct client *top, struct client *attacker, struct client *defender) {
    int damage = (rand() % 5) + 2; // 2 to 6 damage
    defender->hitpoints -= damage;
    char msg[256];
    snprintf(msg, sizeof(msg), "%s attacks %s for %d damage.\n", attacker->name, defender->name, damage);
    int i = write(defender->fd, msg, strlen(msg));
    if (i <= 0){
        removeclient(top, defender->fd);
        return;
    }
    // broadcast(msg, strlen(msg), attacker->fd);
}

void perform_power_move(struct client *top, struct client *attacker, struct client *defender) {
    if (rand() % 2) { // 50% chance to hit
        int damage = ((rand() % 5) + 2) * 3; // 3x damage of a regular attack
        defender->hitpoints -= damage;
        attacker->powermoves--;
        char msg[256];
        snprintf(msg, sizeof(msg), "%s uses a powermove on %s for %d damage.\n", attacker->name, defender->name, damage);
        int i = write(defender->fd, msg, strlen(msg));
        if (i <= 0){
            removeclient(top, defender->fd);
            return;
        }
    } else {
        char msg[256];
        snprintf(msg, sizeof(msg), "%s's powermove missed!\n", attacker->name);
        int i = write(defender->fd, msg, strlen(msg));
        if (i <= 0){
            removeclient(top, defender->fd);
        }
    }
}

void use_potion(struct client *top, struct client *user, struct client *opponent) {
    if (user->healthpotion > 0) {
        int regained = ((rand() % 5) + 2) * 3; // Regain same amount of hp as a power move
        user->hitpoints += regained;
        user->healthpotion--;
        char msg[256];
        snprintf(msg, sizeof(msg), "%s uses their health potion and regains %d health.\n", user->name, regained);
        int i = write(opponent->fd, msg, strlen(msg));
        if (i<=0) {
            removeclient(top, opponent->fd);
            return;
        }
    } else {
        char msg[256];
        snprintf(msg, sizeof(msg), "%s attempted to use a health potion, but has none left!\n", user->name);
        int i = write(opponent->fd, msg, strlen(msg));
        if (i <= 0) {
            removeclient(top, opponent->fd);
        }
    }
}


void end_game(struct client *top, struct client *winner, struct client *loser) {
    char win_msg[256];
    snprintf(win_msg, sizeof(win_msg), "Game over! %s wins!\n", winner->name);
    int i;

    // Write the message to the winner's file descriptor
    i = write(winner->fd, win_msg, strlen(win_msg));
    if (i <= 0){
        removeclient(top, winner->fd);
    }

    // Write the message to the loser's file descriptor
    i = write(loser->fd, win_msg, strlen(win_msg));
    if (i <= 0){
        removeclient(top, loser->fd);
    }

    winner->state = NOTBUSY;
    winner->opponent = NULL;
    loser->opponent = NULL;
    loser->state = NOTBUSY;
    winner->active = NOTACTIVE;
    loser->active = NOTACTIVE;
}



char *read_data(int socket) {
    char *buffer = (char *)malloc(MAX_BUF);
    if (buffer == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    ssize_t total_bytes_read = 0;
    ssize_t num_bytes_read;

    do {
        num_bytes_read = read(socket, buffer + total_bytes_read, MAX_BUF - total_bytes_read);
        if (num_bytes_read < 0) {
            perror("Error reading from socket");
            free(buffer);
            exit(EXIT_FAILURE);
        }
        total_bytes_read += num_bytes_read;
    } while (num_bytes_read > 0 && total_bytes_read < MAX_BUF - 1 && buffer[total_bytes_read - 1] != '\n');

    buffer[total_bytes_read] = '\0'; // Null-terminate the string
    printf("%s", buffer);
    return buffer;
}

