#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/time.h>

//character sequences to display game objects
#define RED_BLOCK "\x1B[1;31;41m \x1B[0m"
#define GRN_BLOCK "\x1B[1;32;42m \x1B[0m"
#define YLW_BLOCK "\x1B[1;33;43m \x1B[0m"
#define BLU_BLOCK "\x1B[1;34;44m \x1B[0m"
#define MAG_BLOCK "\x1B[1;35;45m \x1B[0m"
#define CYN_BLOCK "\x1B[1;36;46m \x1B[0m"
#define BALL "\x1B[1;31mO\x1B[0m"
#define YOU_LOST "\x1B[1;31mYOU LOST\x1B[0m"
#define YOU_WON "\x1B[1;33mYOU WON\x1B[0m"

const char *block_seqs[] = {RED_BLOCK, YLW_BLOCK, GRN_BLOCK, BLU_BLOCK, MAG_BLOCK}; //character sequences to display Block in the appropriate row (first row = red, etc.)
const int num_rows = 5;                                                             //number of rows of blocks
int num_cols;                                                                       //number of columns of blocks
const int top_row_y = 4;                                                            //y position of top row

typedef enum GameStatus
{
    ACTIVE,
    WON,
    LOST
} GameStatus;

typedef struct Block
{
    char char_seq[50]; //sequence of characters to display block
    int x;             //x position
    int y;             //y position
    int status;        //1=not destroyed, 0=destroyed
} Block;

typedef struct Paddle
{
    char block_seq[50];    //character sequence to display one character of the paddle
    unsigned short length; //number of characters in a row to display the paddle
    int x;                 //position of the leftmost character of the paddle
    int y;
} Paddle;

typedef struct Ball
{
    char char_seq[50]; //character sequence to display the ball
    int x;             //x position
    int y;             //y position
    short xv;          //x velocity
    short yv;          //y velocity
} Ball;

Block *init_blocks(int num_blocks);

Paddle init_paddle(struct winsize size);

Ball init_ball(Paddle *paddle);

void move_paddle(Paddle *paddle);

GameStatus update_game(Block *blocks, Paddle *paddle, Ball *ball, int num_blocks);

void bouncing_message_sequence(char *message, int message_length);

int esc_pressed();

void gotoxy(int x, int y) //stolen from http://ubuntuforums.org/showthread.php?t=549023
{
    printf("%c[%d;%df", 0x1B, y, x);
}

void breakout_run()
{
    //SET UP TERMINAL
    printf("\n");    //flush
    system("clear"); //clear screen

    struct termios t_info; //put terminal in raw mode and disable echo
    tcgetattr(STDIN_FILENO, &t_info);
    t_info.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &t_info);

    int old_flags = fcntl(STDIN_FILENO, F_GETFL, 0); //enable non-blocking IO to get keyboard presses
    fcntl(STDIN_FILENO, F_SETFL, old_flags | O_NONBLOCK);

    printf("\e[?25l"); //hide cursor

    struct winsize size; //get console width and height
    ioctl(STDIN_FILENO, TIOCGWINSZ, &size);

    GameStatus status = ACTIVE;

    //initialize blocks
    int num_blocks = num_rows * size.ws_col;
    Block *blocks = init_blocks(num_blocks);

    //initialize paddle
    Paddle paddle = init_paddle(size);

    //initialize ball
    Ball ball = init_ball(&paddle);

    //MAIN GAME LOOP
    while (esc_pressed() != 1)
    {
        if (status == ACTIVE)
        {
            move_paddle(&paddle);
            status = update_game(blocks, &paddle, &ball, num_blocks);
        }
        else if (status == WON)
        {
            bouncing_message_sequence(YOU_WON, 7);
            break;
        }
        else
        {
            bouncing_message_sequence(YOU_LOST, 8);
            break;
        }
    }

    //CLEANUP
    free(blocks);

    printf("\e[?25h"); //unhide cursor

    t_info.c_lflag |= (ICANON | ECHO); //return terminal to canonical mode with echo
    tcsetattr(STDIN_FILENO, TCSANOW, &t_info);

    fcntl(STDIN_FILENO, F_SETFL, old_flags); //re-enable blocking IO

    printf("\n");    //flush
    system("clear"); //clear screen
}

Block *init_blocks(int num_blocks)
{
    Block *blocks = (Block *)malloc(num_blocks * sizeof(Block));
    num_cols = num_blocks / num_rows;

    for (int i = 0; i < num_blocks; i++)
    {
        int x = i % num_cols + 1, y = top_row_y + (i / num_cols);
        gotoxy(x, y);
        Block *b = &blocks[i];

        strcpy(b->char_seq, block_seqs[i / num_cols]); //initialize char_seq
        b->x = x;                                      //initialize position
        b->y = y;
        b->status = 1;

        printf("%s", b->char_seq); //draw block
    }
    return blocks;
}

Paddle init_paddle(struct winsize size)
{
    Paddle paddle;
    strcpy(paddle.block_seq, RED_BLOCK); //use red block character
    paddle.length = 7;                   //7 blocks in a row represent the paddle
    paddle.y = size.ws_row - 4;          //initial position
    paddle.x = size.ws_col / 2 - 3;

    gotoxy(paddle.x, paddle.y); //draw the paddle
    for (int i = 0; i < paddle.length; i++)
    {
        printf("%s", paddle.block_seq);
    }

    return paddle;
}

Ball init_ball(Paddle *paddle)
{
    Ball ball;
    ball.x = paddle->x + (paddle->length / 2);
    ball.y = paddle->y - 1;
    strcpy(ball.char_seq, BALL);

    gotoxy(ball.x, ball.y);
    printf("%s", ball.char_seq);

    ball.xv = (rand() % 2 == 1) ? 1 : -1; //x-velocity: either 1 or -1
    ball.yv = -1;                         //y-velocity: ball initially moving upward

    return ball;
}

/***
 * Get user input to move the paddle
 ***/
void move_paddle(Paddle *paddle)
{
    int ch;
    static struct timeval last_inp_time = {0}; //time of last input
    struct timeval cur_inp_time;               //time of current input
    double diff;                               //elapsed time between inputs, in ms

    gettimeofday(&cur_inp_time, NULL); //calculate time between inputs
    diff = (cur_inp_time.tv_sec - last_inp_time.tv_sec) * 1000.0;
    diff += (cur_inp_time.tv_usec - last_inp_time.tv_usec) / 1000.0;

    if (diff < 10) //if elapsed time is greater than cooldown, update last_time and continue
        return;
    last_inp_time = cur_inp_time;

    ch = getchar();

    switch (ch)
    {
    case 'a': //move the paddle left
        if (paddle->x == 0)
            break;
        gotoxy(paddle->x - 1, paddle->y); //update the characters on the left and right to reflect new position
        printf("%s", paddle->block_seq);
        gotoxy(paddle->x + (paddle->length - 1), paddle->y);
        printf(" ");
        paddle->x -= 1; //update x position
        break;

    case 'd': //move the paddle right
        if (paddle->x + paddle->length == num_cols)
            break;
        gotoxy(paddle->x, paddle->y);
        printf(" ");
        gotoxy(paddle->x + paddle->length, paddle->y);
        printf("%s", paddle->block_seq);
        paddle->x += 1;
        break;
    }

    gotoxy(0, 0);
    printf("\n");
}

/***
 * Move the ball. Break any blocks if necessary, and change the direction of the ball
 * if it collides with something. 
 ***/
GameStatus update_game(Block blocks[], Paddle *paddle, Ball *ball, int num_blocks)
{
    static struct timeval last_time; //time of last update
    struct timeval current_time;     //time of current update
    double diff;                     //elapsed time between updates, in ms

    gettimeofday(&current_time, NULL); //calculate time between updates
    diff = (current_time.tv_sec - last_time.tv_sec) * 1000.0;
    diff += (current_time.tv_usec - last_time.tv_usec) / 1000.0;

    if (diff < 100) //if elapsed time is greater than cooldown, update last_time and continue
        return ACTIVE;
    else
        last_time = current_time;

    gotoxy(ball->x, ball->y); //erase ball from its old position
    printf(" ");

    ball->y += ball->yv; //update ball's position
    ball->x += ball->xv;

    //check if ball collides with borders
    if (ball->y <= 0)
    {
        ball->yv = -ball->yv;
        ball->y = 0;
    }
    if (ball->x <= 0)
    {
        ball->xv = -ball->xv;
        ball->x = 0;
    }
    if (ball->x >= num_cols)
    {
        ball->xv = -ball->xv;
        ball->x = num_cols;
    }
    if (ball->y >= paddle->y + 3)
        return LOST;

    //check if ball collides with a block, and also check if all blocks are destroyed
    int all_destroyed = 1;
    for (int i = 0; i < num_blocks; i++)
    {
        if (blocks[i].status == 1 && blocks[i].x == ball->x && blocks[i].y == ball->y) //if ball hits a block
        {
            blocks[i].status = 0;             //block is destroyed
            gotoxy(blocks[i].x, blocks[i].y); //erase block
            printf(" ");

            ball->y -= ball->yv; //move ball back to its old position
            ball->x -= ball->xv;
            ball->yv = -ball->yv; //reverse velocity
        }
        if (blocks[i].status == 1)
            all_destroyed = 0;
    }

    if (all_destroyed == 1)
        return WON;

    //check if ball collides with paddle
    for (int i = 0; i < paddle->length; i++)
    {
        if (ball->x == paddle->x + i && ball->y == paddle->y)
        {
            ball->y -= ball->yv; //move ball back to its old position
            ball->x -= ball->xv;

            ball->yv = -ball->yv; //adjust velocity
            int middle = paddle->x + (paddle->length / 2);
            ball->xv = -(middle - ball->x);
        }
    }

    gotoxy(ball->x, ball->y); //draw ball at new position
    printf("%s", ball->char_seq);
    return ACTIVE;
}

/***
 * Display bouncing message for at least 3 seconds and then
 * return to the shell once a key is pressed. This is called
 * when the player wins or loses
 ***/
void bouncing_message_sequence(char *message, int message_length)
{
    struct timeval lost_time; //keep track of time so that message
    struct timeval curr_time; //is displayed for at least 3 seconds
    double diff = 0;
    gettimeofday(&lost_time, NULL);

    struct winsize size; //window dimensions
    ioctl(STDIN_FILENO, TIOCGWINSZ, &size);

    unsigned short message_x = size.ws_col / 2,
                   message_y = size.ws_row / 2;             //position of the bouncing message
    unsigned short message_xv = (rand() % 2 == 1) ? 1 : -1, //velocity of the bouncing message
        message_yv = (rand() % 2 == 1) ? 1 : -1;

    system("clear");

    while (getchar() == EOF || diff < 3000)
    {
        gettimeofday(&curr_time, NULL); //calculate time between updates
        diff = (curr_time.tv_sec - lost_time.tv_sec) * 1000.0;
        diff += (curr_time.tv_usec - lost_time.tv_usec) / 1000.0;

        message_y += message_yv; //update message's position
        message_x += message_xv;

        //check if message collides with borders
        if (message_y == 0)
            message_yv = -message_yv;
        if (message_x == 0)
            message_xv = 1;
        if (message_x + message_length == size.ws_col)
            message_xv = -1;
        if (message_y >= size.ws_row - 1)
            message_yv = -message_yv;

        gotoxy(message_x, message_y); //write the message
        printf("%s\n", message);
        usleep(1000 * 100);
    }
}

/***
 * Return 1 if esc is pressed, else return 0.
 ***/
int esc_pressed()
{
    int ch = getchar();
    if (ch == 27)
        return 1;
    else if (ch != EOF)
    {
        ungetc(ch, stdin);
        return 0;
    }
}