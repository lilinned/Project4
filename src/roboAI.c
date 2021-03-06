/**************************************************************************
  CSC C85 - Fall 2013 - UTSC RoboSoccer AI core

  This file is where the actual planning is done and commands are sent
  to the robot.

  Please read all comments in this file, and add code where needed to
  implement your game playing logic.

  Things to consider:

  - Plan - don't just react
  - Use the heading vectors!
  - Mind the noise (it's everywhere)
  - Try to predict what your oponent will do
  - Use feedback from the camera

  What your code should not do:

  - Attack the opponent, or otherwise behave aggressively toward the
    oponent
  - Hog the ball (kick it, push it, or leave it alone)
  - Sit at the goal-line or inside the goal
  - Run completely out of bounds

  AI scaffold: Parker-Lee-Estrada, Summer 2013

  Release version: 0.1
***************************************************************************/

#include "imagecapture/imageCapture.h"
#include "roboAI.h"         // <--- Look at this header file!
#include <nxtlibc/nxtlibc.h>
#include <stdio.h>
#include <stdlib.h>

#define ANGLE_TOL 0.4
#define PI 3.14159265359
#define SLOW_MOVE_SPEED 40
#define FAST_MOVE_SPEED 65
#define MAX_SPEED 90
#define KICK_SPEED 100
#define POSITION_TOL 30
#define KICKER_LENGTH 140// 75  
#define BOT_HIGHT 190
#define BALL_HIGHT 30
#define OPPO_RADIUS 150
#define OPP_HIGHT 90
#define FIELD_LENGTH 1680//1.4
#define FIELD_WIDTH 1620//0.8
#define CAM_DISTANCE 360//0.2
#define CAM_X 740
#define CAM_HIGHT 2110//1.3
#define SCREEN_WIDTH 1024
#define SCREEN_HIGHT 768
/*states*/
#define MODE_SOCCER 0
#define MODE_PENALTY 1
#define MODE_CHASE 2
#define START 1
#define FINISH 2
#define KICK_LEFT 3
#define KICK_RIGHT 4
#define CHASE_TO_LEFT 5
#define CHASE_TO_RIGHT 6
#define PUSH 7

//need to reset
int direction = 1;
bool kicking = false;
double total_time = 0.0;
double dir_time = 2.0;

double left_pos[2];
double right_pos[2];
double push_pos[2];
struct RoboAI *myai;
double cam_pos[] = {CAM_X,-(FIELD_WIDTH+CAM_DISTANCE)};

int pre_l_power=0, pre_r_power=0;
int current_speed = MAX_SPEED;


void clear_motion_flags(struct RoboAI *ai)
{
    // Reset all motion flags. See roboAI.h for what each flag represents
    ai->st.mv_fwd = 0;
    ai->st.mv_back = 0;
    ai->st.mv_bl = 0;
    ai->st.mv_br = 0;
    ai->st.mv_fl = 0;
    ai->st.mv_fr = 0;
}

struct blob *id_coloured_blob(struct RoboAI *ai, struct blob *blobs, int col)
{
    // ** DO NOT CHANGE THIS FUNCTION **
    // Find a blob with the specified colour. If similar colour blobs around,
    // choose the most saturated one.
    // Colour parameter: 0 -> R
    //                   1 -> G
    //                   2 -> B
    struct blob *p, *fnd;
    double BCRT = 1.0;         // Ball colour ratio threshold
    double c1, c2, c3, m;
    int i;

    p = blobs;
    fnd = NULL;
    while (p != NULL)
    {
        if (col == 0)
        {
            c1 = p->R;    // detect red
            c2 = p->G;
            c3 = p->B;
        }
        else if (col == 1)
        {
            c1 = p->G;    // detect green
            c2 = p->R;
            c3 = p->B;
        }
        else if (col == 2)
        {
            c1 = p->B;    // detect blue
            c2 = p->G;
            c3 = p->R;
        }

        if (c1 / c2 > BCRT && c1 / c3 > BCRT && p->tracked_status == 1) // tracked coloured blob!
        {
            //   fprintf(stderr,"col=%d, c1/c2=%f, c1/c3=%f, blobId=%d\n",col,c1/c2,c1/c3,p->blobId);
            if (c1 / c2 < c1 / c3) m = c1 / c2; else m = c1 / c3;

            if (fnd == NULL) fnd = p;        // first one so far
            else if (col == 0)              // Fond a more colorful one!
                if (m > (fnd->R / fnd->G) || m > (fnd->R / fnd->B)) fnd = p;
                else if (col == 1)
                    if (m > (fnd->G / fnd->R) || m > (fnd->G / fnd->B)) fnd = p;
                    else if (m > (fnd->B / fnd->G) || m > (fnd->B / fnd->R)) fnd = p;
        }
        p = p->next;
    }
    // if (fnd!=NULL) fprintf(stderr,"Selected for col=%d, blobId=%d\n",col,fnd->blobId);
    return (fnd);
}

void track_agents(struct RoboAI *ai, struct blob *blobs)
{
    // ** DO NOT CHANGE THIS FUNCTION **
    // Find the current blobs that correspond to the bot, opponent, and ball
    //  by detecting blobs with the specified colours. One bot is green,
    //  one bot is blue. Ball is always red.

    // NOTE: Add a command-line parameter to specify WHICH bot is own

    struct blob *p;
    double mg, vx, vy;
    double NOISE_VAR = 15;

    // Find the ball
    p = id_coloured_blob(ai, blobs, 0);
    if (p)
    {
        if (ai->st.ball != NULL && ai->st.ball != p)
        {
            ai->st.ball->idtype = 0;
            ai->st.ball->p = 0;
        }
        ai->st.ballID = 1;
        ai->st.ball = p;
        ai->st.bvx = p->cx[0] - ai->st.old_bcx;
        ai->st.bvy = p->cy[0] - ai->st.old_bcy;
        ai->st.old_bcx = p->cx[0];
        ai->st.old_bcy = p->cy[0];
        ai->st.ball->p = 0;
        ai->st.ball->idtype = 3;
        vx = ai->st.bvx;
        vy = ai->st.bvy;
        mg = sqrt((vx * vx) + (vy * vy));
        if (mg > NOISE_VAR)       // Enable? disable??
        {
            ai->st.ball->vx[0] = vx;
            ai->st.ball->vy[0] = vy;
            vx /= mg;
            vy /= mg;
            ai->st.ball->mx = vx;
            ai->st.ball->my = vy;
        }
    }
    // Find the mark
    // p = id_coloured_blob(ai, blobs, 0);
    // if (p)
    // {
    //     if (ai->st.ball != NULL && ai->st.ball != p)
    //     {
    //         ai->st.mark->idtype = 4;
    //         ai->st.mark->p = 0;
    //     }
    //     ai->st.markID = 1;
    //     ai->st.mark = p;
    //     ai->st.mvx = p->cx[0] - ai->st.old_mcx;
    //     ai->st.mvy = p->cy[0] - ai->st.old_mcy;
    //     ai->st.old_mcx = p->cx[0];
    //     ai->st.old_mcy = p->cy[0];
    //     ai->st.mark->p = 0;
    //     ai->st.mark->idtype = 3;
    //     vx = ai->st.mvx;
    //     vy = ai->st.mvy;
    //     mg = sqrt((vx * vx) + (vy * vy));
    //     if (mg > NOISE_VAR)       // Enable? disable??
    //     {
    //         ai->st.mark->vx[0] = vx;
    //         ai->st.mark->vy[0] = vy;
    //         vx /= mg;
    //         vy /= mg;
    //         ai->st.mark->mx = vx;
    //         ai->st.mark->my = vy;
    //     }
    // }
    // else
    // {
    //     ai->st.mark = NULL;
    // }

    // ID our bot
    if (ai->st.botCol == 0) p = id_coloured_blob(ai, blobs, 1);
    else p = id_coloured_blob(ai, blobs, 2);
    if (p)
    {
        if (ai->st.self != NULL && ai->st.self != p)
        {
            ai->st.self->idtype = 0;
            ai->st.self->p = 0;
        }
        ai->st.selfID = 1;
        ai->st.self = p;
        ai->st.svx = p->cx[0] - ai->st.old_scx;
        ai->st.svy = p->cy[0] - ai->st.old_scy;
        ai->st.old_scx = p->cx[0];
        ai->st.old_scy = p->cy[0];
        ai->st.self->p = 0;
        ai->st.self->idtype = 1;
    }
    else ai->st.self = NULL;

    // ID our opponent
    if (ai->st.botCol == 0) p = id_coloured_blob(ai, blobs, 2);
    else p = id_coloured_blob(ai, blobs, 1);
    if (p)
    {
        if (ai->st.opp != NULL && ai->st.opp != p)
        {
            ai->st.opp->idtype = 0;
            ai->st.opp->p = 0;
        }
        ai->st.oppID = 1;
        ai->st.opp = p;
        ai->st.ovx = p->cx[0] - ai->st.old_ocx;
        ai->st.ovy = p->cy[0] - ai->st.old_ocy;
        ai->st.old_ocx = p->cx[0];
        ai->st.old_ocy = p->cy[0];
        ai->st.opp->p = 0;
        ai->st.opp->idtype = 2;
    }
    else ai->st.opp = NULL;

}

void id_bot(struct RoboAI *ai, struct blob *blobs)
{
    // ** DO NOT CHANGE THIS FUNCTION **
    // This routine calls track_agents() to identify the blobs corresponding to the
    // robots and the ball. It commands the bot to move forward slowly so heading
    // can be established from blob-tracking.
    //
    // NOTE 1: All heading estimates are noisy.
    //
    // NOTE 2: Heading estimates are only valid when the robot moves with a
    //         forward or backward direction. Turning destroys heading data
    //         (why?)
    //
    // You should *NOT* call this function during the game. This is only for the
    // initialization step. Calling this function during the game will result in
    // unpredictable behaviour since it will update the AI state.

    struct blob *p;
    static double stepID = 0;
    double frame_inc = 1.0 / 5.0;

    drive_speed(30);       // Need a few frames to establish heading

    track_agents(ai, blobs);

    if (ai->st.selfID == 1 && ai->st.self != NULL)
        fprintf(stderr, "Successfully identified self blob at (%f,%f)\n", ai->st.self->cx[0], ai->st.self->cy[0]);
    if (ai->st.oppID == 1 && ai->st.opp != NULL)
        fprintf(stderr, "Successfully identified opponent blob at (%f,%f)\n", ai->st.opp->cx[0], ai->st.opp->cy[0]);
    if (ai->st.ballID == 1 && ai->st.ball != NULL)
        fprintf(stderr, "Successfully identified ball blob at (%f,%f)\n", ai->st.ball->cx[0], ai->st.ball->cy[0]);

    stepID += frame_inc;
    if (stepID >= 1 && ai->st.selfID == 1)
    {
        ai->st.state += 1;
        stepID = 0;
        all_stop();
    }
    else if (stepID >= 1) stepID = 0;

    return;
}

int setupAI(int mode, int own_col, struct RoboAI *ai)
{
    // ** DO NOT CHANGE THIS FUNCTION **
    // This sets up the initial AI for the robot. There are three different modes:
    //
    // SOCCER -> Complete AI, tries to win a soccer game against an opponent
    // PENALTY -> Score a goal (no goalie!)
    // CHASE -> Kick the ball and chase it around the field
    //
    // Each mode sets a different initial state (0, 100, 200). Hence,
    // AI states for SOCCER will be 0 through 99
    // AI states for PENALTY will be 100 through 199
    // AI states for CHASE will be 200 through 299
    //
    // You will of course have to add code to the AI_main() routine to handle
    // each mode's states and do the right thing.
    //
    // Your bot should not become confused about what mode it started in!

    switch (mode)
    {
    case AI_SOCCER:
        fprintf(stderr, "Standard Robo-Soccer mode requested\n");
        ai->st.state = 0;   // <-- Set AI initial state to 0
        break;
    case AI_PENALTY:
        fprintf(stderr, "Penalty mode! let's kick it!\n");
        ai->st.state = 100; // <-- Set AI initial state to 100
        break;
    case AI_CHASE:
        fprintf(stderr, "Chasing the ball...\n");
        ai->st.state = 200; // <-- Set AI initial state to 200
        break;
    default:
        fprintf(stderr, "AI mode %d is not implemented, setting mode to SOCCER\n", mode);
        ai->st.state = 0;
    }

    all_stop();            // Stop bot,
    ai->runAI = AI_main;       // and initialize all remaining AI data
    ai->st.ball = NULL;
    ai->st.self = NULL;
    ai->st.opp = NULL;
    ai->st.side = 0;
    ai->st.botCol = own_col;
    ai->st.old_bcx = 0;
    ai->st.old_bcy = 0;
    ai->st.old_scx = 0;
    ai->st.old_scy = 0;
    ai->st.old_ocx = 0;
    ai->st.old_ocy = 0;
    ai->st.bvx = 0;
    ai->st.bvy = 0;
    ai->st.svx = 0;
    ai->st.svy = 0;
    ai->st.ovx = 0;
    ai->st.ovy = 0;
    ai->st.selfID = 0;
    ai->st.oppID = 0;
    ai->st.ballID = 0;
    clear_motion_flags(ai);

    all_stop();
    stop_kicker();
    fprintf(stderr, "Initialized!\n");
    return (1);
}

void AI_main(struct RoboAI *ai, struct blob *blobs, void *state)
{
    /*************************************************************************
     You will be working with a state-based AI. You are free to determine
     how many states there will be, what each state will represent, and
     what actions the robot will perform based on the state as well as the
     state transitions.

     You must *FULLY* document your state representation in the report

     Here two states are defined:
     State 0,100,200 - Before robot ID has taken place (this state is the initial
                       state, or is the result of pressing 'r' to reset the AI)
     State 1,101,201 - State after robot ID has taken place. At this point the AI
                       knows where the robot is, as well as where the opponent and
                       ball are (if visible on the playfield)

     Relevant UI keyboard commands:
     'r' - reset the AI. Will set AI state to zero and re-initialize the AI
    data structure.
     't' - Toggle the AI routine (i.e. start/stop calls to AI_main() ).
     'o' - Robot immediate all-stop! - do not allow your NXT to get damaged!

     ** Do not change the behaviour of the robot ID routine **
    **************************************************************************/

    if (ai->st.state == 0 || ai->st.state == 100 || ai->st.state == 200) // Initial set up - find own, ball, and opponent blobs
    {
        // Carry out self id process.
        fprintf(stderr, "Initial state, self-id in progress...\n");
        id_bot(ai, blobs);
        init_my_ai(ai);
        if ((ai->st.state % 100) != 0) // The id_bot() routine will change the AI state to initial state + 1
        {
            // if robot identification is successful.
            if (ai->st.self->cx[0] >= 512) ai->st.side = 1; else ai->st.side = 0;
            all_stop();
            clear_motion_flags(ai);
            fprintf(stderr, "Self-ID complete. Current position: (%f,%f), current heading: [%f, %f], AI state=%d\n", ai->st.self->cx[0], ai->st.self->cy[0], ai->st.self->mx, ai->st.self->my, ai->st.state);
        }
    }
    else
    {
        /****************************************************************************
         TO DO:
         You will need to replace this 'catch-all' code with actual program logic to
         have the robot do its work depending on its current state.
         After id_bot() has successfully completed its work, the state should be
         1 - if the bot is in SOCCER mode
         101 - if the bot is in PENALTY mode
         201 - if the bot is in CHASE mode

         Your AI code needs to handle these states and their associated state
         transitions which will determine the robot's behaviour for each mode.
        *****************************************************************************/



        track_agents(ai,blobs);        // Currently, does nothing but endlessly track
        // fprintf(stderr,"Just trackin'!\n");    // bot, opponent, and ball.

        int mode = ai->st.state / 100;
        int sub_state = ai->st.state % 100;
        update_my_ai(ai);
        // int next_state = fsm(mode, sub_state, myai, ai) + mode * 100;
        //leave it for now
        int next_state = fsm(mode, sub_state, myai, myai) + mode * 100;
        if (dir_time < 2)
        {
            dir_time += getTimeDiff();
        }
        ai->st.state = next_state;
    }
}

/**********************************************************************************
 TO DO:

 Add the rest of your game playing logic below. Create appropriate functions to
 handle different states (be sure to name the states/functions in a meaningful
 way), and do any processing required in the space below.

 AI_main() should *NOT* do any heavy lifting. It should only call appropriate
 functions based on the current AI state.

 You will lose marks if AI_main() is cluttered with code that doesn't belong
 there.
**********************************************************************************/

int fsm(int mode, int state, struct RoboAI *ai, struct RoboAI *pai)
{
    int fake_state = 0;
    if (find_ball(&fake_state, pai))
    {
        // update pos's
        update_kick_pos(ai);
        update_push_pos(ai);
    }
    // change states
    switch (state)
    {
        case START:
            if (mode == MODE_PENALTY)
            {
                if (find_ball(&state, pai))
                {
                    chase_lr_or_push(&state, ai);
                    at_kick_pos(&state, ai);
                }
            }
            else if (mode == MODE_CHASE)
            {
                if (find_ball(&state, pai))
                {
                    chase_lr_or_push(&state, ai);
                } 
            }
            else if (mode == MODE_SOCCER)
            {
                if (find_ball(&state, pai))
                {
                    chase_lr_or_push(&state, ai);
                    at_kick_pos(&state, ai);
                }
            }
            break;
        case FINISH:
            if (mode == MODE_PENALTY)
            {
                //do nothing 
            }
            else if (mode == MODE_CHASE)
            {
                //shouldn't be here
                state = START;
            }
            else if (mode == MODE_SOCCER)
            {
                //shouldn't be here
                state = START;
            }   
            break;
        case KICK_RIGHT:
        case KICK_LEFT:
            if (mode == MODE_PENALTY)
            {
                int temp = state;
                if (find_ball(&state, pai))
                {
                    chase_lr_or_push(&state, ai);
                    at_kick_pos(&state, ai);
                }
                if (kicking)
                {
                    state = temp;
                    if (!kick_miss())   
                    {
                        if (kick_finished(&state))
                        {
                            state = FINISH;
                        }
                    }
                }
                    
            }
            else if (mode == MODE_CHASE)
            {
                if (find_ball(&state, pai))
                {
                    chase_lr_or_push(&state, ai);
                } 
            }
            else if (mode == MODE_SOCCER)
            {
                int temp = state;
                if (find_ball(&state, pai))
                {
                    chase_lr_or_push(&state, ai);
                    at_kick_pos(&state, ai);
                }
                if (kicking)
                {
                    // printf("11111111111111\n");
                    state = temp;
                    if (!kick_miss())   
                    {
                        // printf("2222222222222\n");
                        if (kick_finished(&state))
                        {
                            // printf("333333333333333\n");
                            //do nothing
                            state = START;
                        }
                    }
                }
            }
            break;
        case CHASE_TO_LEFT:
        case CHASE_TO_RIGHT:
        case PUSH:
            if (mode == MODE_PENALTY)
            {
                if (find_ball(&state, ai))
                {
                    chase_lr_or_push(&state, ai);
                    at_kick_pos(&state, ai);
                }
            }
            else if (mode == MODE_CHASE)
            {
                if (find_ball(&state, ai))
                {
                    chase_lr_or_push(&state, ai);
                } 
            }
            else if (mode == MODE_SOCCER)
            {
                if (find_ball(&state, ai))
                {
                    chase_lr_or_push(&state, ai);
                    at_kick_pos(&state, ai);
                }
            }
            break;
    }

    // action now!
    switch (state)
    {
        case START:
        case FINISH:
            stop_kicker();
            all_stop();
            break;
        case KICK_RIGHT:
            my_kick(KICK_SPEED);
            chase(ai, right_pos);
            break;
        case KICK_LEFT:
            my_kick(-KICK_SPEED);
            chase(ai, left_pos);
            break;
        case CHASE_TO_LEFT:
            chase(ai, left_pos);
            break;
        case CHASE_TO_RIGHT:
            chase(ai, right_pos);
            break;
        case PUSH:
            chase(ai, push_pos);
            break;
    }
    return state;
}

/*Return true if ball is not kicked*/
bool kick_miss(int *state)
{
    //TODO
    bool miss = false;
    if (miss)
    {
        *state = START;
        return true;
    }
    return false;
}

/*Return true if kicking action is finished*/
bool kick_finished(int *state)
{
    if (total_time > 0.45)
    {
        stop_kicker();
        total_time = 0.0;
        kicking = false;
        return true;
    }
    return false;
}

/*customized kick function
performe kick in a time interval*/
void my_kick(int speed)
{
    if (!kicking)
    {
        kicking = true;
        kick_speed(speed);
    }
    total_time += getTimeDiff();
}

/*Return true if ball is in the field*/
bool find_ball(int *state, struct RoboAI *ai)
{
    //TODO need to check something else
    if (ai->st.ball)
    {
        return true;
    }
    else
    {   
        *state = START;
        return false;
    }
}

/*Return true if the pos is reachable*/
bool reachable(double x, double y)
{
    return (x > KICKER_LENGTH && x < FIELD_LENGTH - KICKER_LENGTH) && (-y > KICKER_LENGTH && -y < FIELD_WIDTH - KICKER_LENGTH);
}

/*Return true if should push the ball*/
bool pushable(double x, double y)
{
    //TODO check in front of gate
    return false;
}

/*Change the state to chase or kick*/
void chase_lr_or_push(int *state, struct RoboAI *ai)
{
    double sx = ai->st.self->cx[0];
    double sy = ai->st.self->cy[0];
    double left_dist = pow(sx - left_pos[0], 2) + pow(sy - left_pos[1], 2);
    double right_dist = pow(sx - right_pos[0], 2) + pow(sy - right_pos[1], 2);
    bool left_reach = reachable(left_pos[0], left_pos[1]);
    bool right_reach = reachable(right_pos[0], right_pos[1]);
    bool push_reach = reachable(push_pos[0], push_pos[1]);

    double gate[] = {(1 - ai->st.side) * FIELD_LENGTH, -FIELD_WIDTH / 2.0};
    bool block = false;

    if (ai->st.opp && ai->st.ball && ai->st.self)
    {
        block = oppo_on_line(ai->st.opp, ai->st.ball, gate, OPPO_RADIUS * 1.5);
    }

    if (block && push_reach)
    {
        *state = PUSH;
    }
    else if (left_reach && right_reach)
    {
        if (left_dist < right_dist)
        {
            *state = CHASE_TO_LEFT;
        }
        else
        {
            *state = CHASE_TO_RIGHT;
        }
    }
    else if (left_reach && !right_reach)
    {
        *state = CHASE_TO_LEFT;
    }
    else if (!left_reach && right_reach)
    {
        *state = CHASE_TO_RIGHT;
    }
    else
    {
        if (pushable(push_pos[0], push_pos[1]))
        {
            *state = PUSH;
        }
    }
}

/*inistialize my ai*/
void init_my_ai(struct RoboAI *ai)
{
    if (myai == NULL)
    {
        myai = malloc(sizeof(struct RoboAI));
        myai->st.ball = malloc(sizeof(struct blob));
        myai->st.self = malloc(sizeof(struct blob));
        myai->st.opp = malloc(sizeof(struct blob));
        myai->st.side = 2;
    }
    myai->st.state = ai->st.state;
    init_blob(myai->st.ball, ai->st.ball, BALL_HIGHT);
    init_blob(myai->st.self, ai->st.self, BOT_HIGHT);
    init_blob(myai->st.opp, ai->st.opp, OPP_HIGHT);
    myai->st.old_bcx = 0;
    myai->st.old_bcy = 0;
    myai->st.old_scx = 0;
    myai->st.old_scy = 0;
    myai->st.old_ocx = 0;
    myai->st.old_ocy = 0;
    myai->st.bvx = 0;
    myai->st.bvy = 0;
    myai->st.svx = 0;
    myai->st.svy = 0;
    myai->st.ovx = 0;
    myai->st.ovy = 0;
    myai->st.selfID = 0;
    myai->st.oppID = 0;
    myai->st.ballID = 0;
    direction = 1;

    dir_time = 2.0;
    kicking = false;
    total_time = 0.0;
}

/*initialize a blob*/
void init_blob(struct blob *myblob, struct blob *p, double height)
{
    int i;
    if (!p)
    {
        return;
    }
    for (i = 0; i < 5; i++)
    {
        double projx = p->cx[i] / SCREEN_WIDTH * FIELD_LENGTH;
        double projy =-p->cy[i] / SCREEN_HIGHT * FIELD_WIDTH;
        double vectorx= cam_pos[0] - projx;
        double vectory= cam_pos[1] - projy;
        double length1 = sqrt(pow(vectorx,2)+pow(vectory,2));
        double length2 = length1 / CAM_HIGHT * height;
        double ratio = length2 / length1;
        myblob->cx[i] = projx + vectorx * ratio;
        myblob->cy[i] = projy + vectory * ratio;
        myblob->vx[i] = p->vx[i] / 10;
        myblob->vy[i] = -(p->vy[i]) / 10;
    }
}

/*update the kicking positions*/
void update_pos(struct blob *myblob, struct blob *p, double height)
{
    double projx = p->cx[0] / SCREEN_WIDTH * FIELD_LENGTH;
    double projy =-(p->cy[0]) / SCREEN_HIGHT * FIELD_WIDTH;
    double vectorx= cam_pos[0] - projx;
    double vectory= cam_pos[1] - projy;
    double length1 = sqrt(pow(vectorx,2)+pow(vectory,2));
    double length2 = length1 / CAM_HIGHT * height;
    double ratio = length2 / length1;
    myblob->cx[0] = projx + vectorx * ratio;
    myblob->cy[0] = projy + vectory * ratio;
}

/*update attributes in blobs*/
void update_blob(struct blob *myblob, struct blob *p, double height)
{
    double len;
    double timediff;
    int i;
    double noiseV =  0.2;// Was 5.0
    for (i = 3; i >= 0; i--)
    {
        myblob->cx[i + 1] = myblob->cx[i];
        myblob->cy[i + 1] = myblob->cy[i];
        myblob->vx[i + 1] = myblob->vx[i];
        myblob->vy[i + 1] = myblob->vy[i];
    }
    update_pos(myblob, p, height);
    timediff = getTimeDiff();
    myblob->vx[0] = (myblob->cx[0] - myblob->cx[1]) / timediff;
    myblob->vy[0] = (myblob->cy[0] - myblob->cy[1]) / timediff;

    // Smoothed velocity vector
    myblob->vx[0] = (.7 * myblob->vx[0]) + (.2 * myblob->vx[1]) + (.1 * myblob->vx[2]);
    myblob->vy[0] = (.7 * myblob->vy[0]) + (.2 * myblob->vy[1]) + (.1 * myblob->vy[2]);
    double pre_heading = atan2(myblob->mx, myblob->my);
    double theta;
    theta = getTimeDiff() * (pre_l_power - pre_r_power) * PI / 100.0 * (current_speed / 100.0);
    double my_mx = sin(pre_heading + theta);
    double my_my = cos(pre_heading + theta);
    double mx = myblob->mx;
    double my = myblob->my;
    if (fabs(myblob->vx[0]) > noiseV && fabs(myblob->vy[0]) > noiseV)
    {
        len = 1.0 / sqrt((myblob->vx[0] * myblob->vx[0]) + (myblob->vy[0] * myblob->vy[0]));
        mx = myblob->vx[0] * len;
        my = myblob->vy[0] * len;
    }
    double diff = fabs(pre_l_power - pre_r_power);
    if (diff > current_speed * 0.5)
    {
        myblob->mx = my_mx;
        myblob->my = my_my;
    }
    else
    {
        myblob->mx = mx * (1 - diff / (double)current_speed * 2.0) + my_mx * diff / (double)current_speed * 2.0;
        myblob->my = my * (1 - diff / (double)current_speed * 2.0) + my_my * diff / (double)current_speed * 2.0;
        double norm = 1 / sqrt(myblob->mx * myblob->mx + myblob->my * myblob->my);
        myblob->mx = myblob->mx * norm;
        myblob->my = myblob->my * norm;
        fprintf(stderr, "myblob->mx: %f,    myblob->my: %f\n", myblob->mx, myblob->my);
    }
}

/*update attributes in my_ai*/
void update_my_ai(struct RoboAI *ai)
{
    if (myai->st.side > 1)
    {
        myai->st.side = ai->st.side;
    }
    myai->st.state = ai->st.state;
    if (ai->st.ball)
    {
        update_blob(myai->st.ball, ai->st.ball, BALL_HIGHT);
    }
    if (ai->st.self)
    {
        update_blob(myai->st.self, ai->st.self, BOT_HIGHT);
    }
    if (ai->st.opp)
    {
        update_blob(myai->st.opp, ai->st.opp, OPP_HIGHT);
    }

}

/*update the postion when we should push the ball to the gate*/
void update_push_pos(struct RoboAI *ai)
{
    double gy = -FIELD_WIDTH / 2.0;
    double gx = (1 - ai->st.side) * FIELD_LENGTH;
    double bx = ai->st.ball->cx[0];
    double by = ai->st.ball->cy[0];
    double ball_to_gate[2] = {gx - bx, gy - by};
    double norm = sqrt(pow(ball_to_gate[0], 2) + pow(ball_to_gate[1], 2));
    ball_to_gate[0] = ball_to_gate[0] / norm * (KICKER_LENGTH - POSITION_TOL);
    ball_to_gate[1] = ball_to_gate[1] / norm * (KICKER_LENGTH - POSITION_TOL);
    push_pos[0] = bx - ball_to_gate[0];
    push_pos[1] = by - ball_to_gate[1];
}

/*update left and right kick positions*/
void update_kick_pos(struct RoboAI *ai)
{
    double gy = -FIELD_WIDTH / 2.0;
    double gx = (1 - ai->st.side) * FIELD_LENGTH;
    double bx = ai->st.ball->cx[0];
    double by = ai->st.ball->cy[0];
    double ball_to_gate[2] = {gx - bx, gy - by};
    double b[2] = { -ball_to_gate[1], ball_to_gate[0]};
    double norm_b = sqrt(pow(b[0], 2) + pow(b[1], 2));
    b[0] = b[0] / norm_b * KICKER_LENGTH;
    b[1] = b[1] / norm_b * KICKER_LENGTH;
    left_pos[0] = bx + b[0];
    left_pos[1] = by + b[1];
    right_pos[0] = bx - b[0];
    right_pos[1] = by - b[1];
}

/*Return true if the opponent is blocking the gate*/
bool oppo_on_line(struct blob *opp, struct blob *from, double *to, double min_dist)
{
    double so[] = {opp->cx[0] - from->cx[0], opp->cy[0] - from->cy[0]};
    double sp[] = {to[0] - from->cx[0], to[1] - from->cy[0]};
    double product = so[0] * sp[0] + so[1] * sp[1];
    if (product <= 0.0)
    {
        return false;
    }
    double norm_sp2 = sp[0] * sp[0] + sp[1] * sp[1];
    double projection = (so[0] * sp[0] + so[1] * sp[1])/norm_sp2;
    double intersect[] = {projection * sp[0] + from->cx[0], projection * sp[1] + from->cy[0]};
    if (pow(opp->cx[0] - intersect[0], 2) + pow(opp->cy[0] - intersect[1], 2) > pow(min_dist, 2))
    {
        return false;
    }
    return true;
}

/*Return true if we are at the kick position and update the state*/
bool at_kick_pos(int *state, struct RoboAI *ai)
{
    double gate[] = {(1 - ai->st.side) * FIELD_LENGTH, -FIELD_WIDTH / 2.0};
    double sx = ai->st.self->cx[0];
    double sy = ai->st.self->cy[0];
    bool block = false;

    if (ai->st.opp && ai->st.ball && ai->st.self)
    {
        block = oppo_on_line(ai->st.opp, ai->st.ball, gate, OPPO_RADIUS * 1.5);
    }

    if (block)
    {
        if (fabs(ai->st.ball->cx[0] - sx) < KICKER_LENGTH + POSITION_TOL && fabs(ai->st.ball->cy[0] - sy) < KICKER_LENGTH + POSITION_TOL)
        {
            if (ai->st.side)
            {
                if (ai->st.ball->cy[0] - sy > 0.0)
                {
                    *state = KICK_LEFT;
                }
                else
                {
                    *state = KICK_RIGHT;
                }
            }
            else
            {
                if (ai->st.ball->cy[0] - sy > 0.0)
                {
                    *state = KICK_RIGHT;
                }
                else
                {
                    *state = KICK_LEFT;
                }
            }
            return true;
        }
    }
    else if (fabs(left_pos[0] - sx) < POSITION_TOL && fabs(left_pos[1] - sy) < POSITION_TOL)
    {
        *state = KICK_LEFT;
        return true;
    }
    else if (fabs(right_pos[0] - sx) < POSITION_TOL && fabs(right_pos[1] - sy) < POSITION_TOL)
    {
        *state = KICK_RIGHT;
        return true;
    }
    else
    {
        return false;
    }
}

/*round a double*/
int my_round(double number)
{
    int ret = round(number);
    if (ret > 100)
    {
        ret = 100;
    }
    else if (ret < -100)
    {
        ret = -100;
    }
    return ret;
}

/*apply the calculated left and right power*/
void apply_power(int left_power, int right_power)
{
    pre_r_power = right_power;
    pre_l_power = left_power;
    if (direction > 0)
    {
        drive_custom(left_power, right_power);
    }
    else
    {
        drive_custom(-right_power, -left_power);
    }
}

/*move the robot to the direction specified by theta*/
void move(double theta, struct RoboAI *ai)
{
    double left_power = 0.0;
    double right_power = 0.0;
    current_speed = SLOW_MOVE_SPEED;

    if (fabs(ai->st.self->cx[0] - ai->st.ball->cx[0]) > 300 || fabs(ai->st.self->cy[0] - ai->st.ball->cy[0]) > 300)
    {
        current_speed = MAX_SPEED;
    }
    else if (fabs(ai->st.self->cx[0] - ai->st.ball->cx[0]) > 200 || fabs(ai->st.self->cy[0] - ai->st.ball->cy[0]) > 200)
    {
        current_speed = FAST_MOVE_SPEED;
    }

    if (theta >= ANGLE_TOL)
    {
        left_power = current_speed;
        right_power = cos(theta * 2.0) * current_speed;
    }
    else if (theta <= -ANGLE_TOL)
    {
        right_power = current_speed;
        left_power = cos(theta * 2.0) * current_speed;
    }
    else
    {
        left_power = current_speed;
        right_power = current_speed;
    }

    apply_power(my_round(left_power), my_round(right_power));
}

/*reverse the robot's moving direction*/
void reverse_dir(struct blob *b)
{
    int i;
    if (dir_time > 1.0)
    {
        direction = -direction;
        b->mx *= -1.0;
        b->my *= -1.0;
        for (i = 0; i < 5; i++)
        {
            b->vx[i] = 0.0;
            b->vy[i] = 0.0;
        }
        dir_time = 0.0;   
    }
}

/*chase the ball*/
void chase(struct RoboAI *ai, double *pos)
{
    // ball position
    double xb = pos[0];
    double yb = pos[1];
    // robot position
    double yr = *(ai->st.self->cy);
    double xr = *(ai->st.self->cx);
    // vactor to the ball
    double d[2] = {xb - xr, yb - yr};
    // heading
    double h[2] = {(ai->st.self->mx), (ai->st.self->my)};
    // theta
    double td = atan2(d[0], d[1]);
    double th = atan2(h[0], h[1]);
    // theta between two vectors
    double theta = atan2(sin(td) * cos(th) - sin(th) * cos(td), cos(td) * cos(th) + sin(td) * sin(th));
    if (ai->st.opp)
    {
        if (fabs(ai->st.opp->cx[0] - ai->st.self->cx[0]) < 1.3 * (KICKER_LENGTH + OPPO_RADIUS) && fabs(ai->st.opp->cy[0] - ai->st.self->cy[0]) < 1.3 * (KICKER_LENGTH + OPPO_RADIUS))
        {
            bool block = blocking(pos, ai);
            if (block)
            {
                double leave_angle;
                leave_angle = atan2(ai->st.self->cx[0] - ai->st.opp->cx[0], ai->st.self->cy[0] - ai->st.opp->cy[0]);
                td = atan2(sin(td)+sin(leave_angle)*0.75,cos(td)+cos(leave_angle)*0.75);
                theta = atan2(sin(td) * cos(th) - sin(th) * cos(td), cos(td) * cos(th) + sin(td) * sin(th));
            }
        }
    }
    if (theta > PI / 2.0)
    {
        theta = theta - PI;
        reverse_dir(ai->st.self);
    }
    else if (theta < -(PI / 2.0))
    {
        theta = theta + PI;
        reverse_dir(ai->st.self);
    }
    move(theta, ai);
}

/*Return true if opponent is blocking the ball*/
bool blocking(double *pos, struct RoboAI *ai)
{
    if (!ai->st.opp || !ai->st.ball || !ai->st.self)
    {
        return false;
    }
    double so[] = {ai->st.opp->cx[0] - ai->st.self->cx[0], ai->st.opp->cy[0] - ai->st.self->cy[0]};
    double sp[] = {pos[0] - ai->st.self->cx[0], pos[1] - ai->st.self->cy[0]};
    double product = so[0] * sp[0] + so[1] * sp[1];
    if (product <= 0.0)
    {
        return false;
    }
    double norm_sp2 = sp[0] * sp[0] + sp[1] * sp[1];
    double projection = (so[0] * sp[0] + so[1] * sp[1])/norm_sp2;
    double intersect[] = {projection * sp[0] + ai->st.self->cx[0], projection * sp[1] + ai->st.self->cy[0]};
    if (pow(ai->st.opp->cx[0] - intersect[0], 2) + pow(ai->st.opp->cy[0] - intersect[1], 2) > pow((KICKER_LENGTH + OPPO_RADIUS) * 1.2, 2))
    {
        return false;
    }
    return true;
}