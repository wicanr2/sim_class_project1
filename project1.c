/*
 *
 * Tandem queue simulation 
 * Author: Chun-Yu Wang
 * Email : ｗｉｃａｎｒ２@gmail.com; ｑ３８００１０１８@mail.ncku.edu.tw
 * Description : 
 *      This simulation is a class project on NCKU simulation.
 */

//tandem queue
//encoder and storage
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "lcgrand.h"


//-----------------------------------------------------
/* Exponential variate generation function. */
float expon(float mean) {
    /* Return an exponential random variate with mean "mean". */
    return -mean * log(lcgrand(1));
}
//-----------------------------------------------------
#define TOP_FIELD 1
#define BOTTOM_FIELD 2
#define ENCODED_FIELD 3

// video field structure
typedef struct _video_field_t {
    //field type, 1 = top , 2 = bottom, 0 = undefined
    int type; 
    //the complexity of the field
    float fobs; 
    struct _video_field_t *next;
    struct _video_field_t *prev;
} video_field_t;

//field_buf
typedef struct _field_buf_t {
    int capacity;
    int num;
    video_field_t* head;
    video_field_t* last;
} field_buf_t;

// initialze buffer
void init_buffer( field_buf_t *buf, int capacity ) {
    memset(buf,0, sizeof(field_buf_t));
    buf->capacity = capacity;
}

// get next event
video_field_t* get_next_field( field_buf_t *buf ) {
    video_field_t* tmp = buf->head;
    if ( buf->head == 0 ) {
	    return 0;
    }
    buf->head = buf->head->next;
    if ( buf->head == 0 ) {
        buf->last = 0;
    }
    buf->num--;
    return tmp;
}

int print_field_buf( field_buf_t *buf );
// insert field
int insert_field( field_buf_t *buf, int type, float fobs ) {
    // the capacity is equal to zeor or negative, it is a infinite/unlimited buffer in simulation.
    //printf("insert_field\n");
    if ( buf->capacity > 0 && buf->num > buf->capacity + 1 ) {
        return -1;
    }
    buf->num++;
    video_field_t *tmp = calloc(1, sizeof(video_field_t));
    tmp->type = type;
    tmp->fobs = fobs;
    tmp->next = 0;
    tmp->prev = buf->last;
    if ( buf->last == 0 ) {
        buf->head = tmp;
        buf->last = tmp;
        //print_field_buf(buf);
        return 0;
    }
    buf->last->next = tmp;
    buf->last = tmp;
    //print_field_buf(buf);
    return 0;    
}

int print_field_buf( field_buf_t *buf ) {
    video_field_t *tmp = buf->head;
    printf("----------field buffer-----------------\n");
    while ( tmp != 0 ) {
        printf("field->type = %d, field->fobs = %f\n", tmp->type, tmp->fobs );
        tmp = tmp->next;
    }
    printf("--------------------------------------\n");
    return 0;
}



// drop last frame
// if there is a Top_Field on the buffer, but the Bottom_Field can not fill in, the drop the last Top_Field in the buffer;
void drop_last( field_buf_t *buf ) {
    video_field_t *tmp = buf->last;
    buf->last = tmp->prev;
    buf->last->next = 0;
    buf->num--;
    free(tmp);
}
// clean the buffer
void clean_buffer ( field_buf_t *buf ) {
    video_field_t *tmp = buf->head;
    video_field_t *prev = 0;
    buf->num = 0;
    while ( tmp != 0 ) {
        prev = tmp ;
        tmp = tmp->next;
        free(prev);
    }
}
//-----------------------------------------------------
typedef struct _encoder_t {
    //the processing capacity of the encoder 
    int c_enc; 
    field_buf_t buf;
}encoder_t;

typedef struct _storage_t {
    // the capacity of the storage server
    int c_storage;
    // the storage always has a sufficient buffer space to 
    // store an arrivaing field
    field_buf_t buf;
}storage_t;

void init_encoder(encoder_t *e, int c_enc, int caps) {
    memset(e, 0, sizeof(encoder_t));
    e->c_enc = c_enc;
    init_buffer( &e->buf, caps);
}

void init_storage(storage_t *s, int c_storage, int caps){
    memset(s, 0, sizeof(storage_t));
    init_buffer( &s->buf, caps);
    s->c_storage = c_storage;
}

// the structure of simulation states
// it defines the simulation state variables
typedef struct _sim_state_t {
    int lost_frames; // the number of frame discarded
    int total_frames; // total frame generation during simulation
    int encoder_capacity_beta; // beta field
    int storage_capacity;
    float storage_busy_time;
    // record the finished time of frame in the encoder 
    float last_encode_finished_time; 

    // record the finished time of encode frame in the storage
    float last_storage_finished_time;

    float current_time; // the current simulation time in second
    float stop_time; // the stop time in second
    // the inter-arrival time mean of a field on exponential distribution
    // = 1/59.94 or 1/50
    float arrival_mean; 

    // the field complexity mean on exponential distribution
    // = 262.5 or 312.5
    float field_complexity_mean;
    
    float alpha; // alpha = h1 + h2

    encoder_t encoder; // encoder buffer
    storage_t storage; // storage buffer
} sim_state_t;

//-----------------------------------------------------
typedef enum event_t {
    NEW_FRAME,
    TOP_ARRIVAL,
    BOTTOM_ARRIVAL,
    ENCODE,
    STORE
}event_t ;
/*
 *    Initial 
 *       |
 *       |
 *       <----------#
 *       |          |
 *    NEW_FRAME-----#
 *       |
 *       |
 *  TOP_ARRIVAL --> BOTTOM_ARRIVAL --> ENCODE--> STORE
 *   ^   |               |                         |
 *   |   |               |                         |
 *   |   |               |                         |
 *   |   -----------------                         |
 *   |           |                                 |
 *   |         DROP                                |
 *   |           |                                 |
 *   <---------------------------------------------#
 *
 *      schedule next event
 *
 *  TOP_ARRIVAL:
 *      If the arriving field is a top field, it is discarded.
 *      The following bottom field will also be discarded
 *
 *  BOTTOM_ARRIVAL:
 *      If the arriving field is a bottom, it is discarded. 
 *      The field that was last placed in the buffer is 
 *      removed and discarded
 *
 * */
//-----------------------------------------------------
// the data structure of event
typedef struct _event_element_t {
    event_t e; // event e
    float ttime; // trigger time
    struct _event_element_t *next;
} event_element_t ;

typedef struct _event_queue_t {
   int total_events;
   int num;
   event_element_t *head;
   event_element_t *last;
} event_queue_t;

// initial event queue
void init_event_queue(event_queue_t *q) {
    memset( q, 0, sizeof(event_queue_t));
}
int print_event_queue( event_queue_t *q );
// insert event
void insert_event(event_queue_t *q, event_t e, float ttime) {
    //printf("insert event %d, ttime %f\n", e, ttime );
    event_element_t *tmp = 0;
    q->total_events++;
    q->num++;
    tmp = calloc(1, sizeof(event_queue_t) );
    tmp->e = e;
    tmp->ttime = ttime;
    tmp->next = 0;

    // event insertion is ordered by trigger time
    event_element_t *p = q->head;
    event_element_t *prev = p;
    if ( p == 0 ) {
        q->head = tmp;
        q->last = tmp;
        //print_event_queue(q);
        return ;
    }
    while ( p != 0 ) { 
        if ( p->ttime > tmp->ttime ) {
            break;
        }
        prev = p ;
        p = p->next;
    }
    //printf("p = %d, prev =%d, head =%d, last =%d\n", p, prev, q->head, q->last);
    if ( p == 0 ) {
        prev->next = tmp;
        q->last = tmp;
        //print_event_queue(q);
        return;
    }
    if ( p == q->head ) {
        tmp->next = p;
        q->head = tmp;
    }  else {
        tmp->next = p;
        prev->next = tmp;
    }
    //print_event_queue(q);
}
// get next event
event_element_t *get_next_event( event_queue_t *q ) {
    event_element_t *tmp = q->head;
    q->head = q->head->next;
    if ( q->head == 0 ) {
        q->last = 0;
    }
    q->num--;
    return tmp;
}
int print_event_queue( event_queue_t *q ) {
    event_element_t *tmp = q->head;
    printf("----------event queue-----------------\n");
    while ( tmp != 0 ) {
        printf("%d event->e = %d, event->ttime = %f\n", tmp, tmp->e, tmp->ttime );
        tmp = tmp->next;
    }
    printf("--------------------------------------\n");
    return 0;
}

//-----------------------------------------------------
// event method declaration
int schedule_new_frame(sim_state_t *sim_state, event_queue_t *q);
int schedule_encode(sim_state_t *sim_state, event_queue_t *q);
int schedule_store(sim_state_t *sim_state, event_queue_t *q);
//-----------------------------------------------------

void event_scheduler(sim_state_t *sim_state, event_queue_t *q ) {
    event_element_t *event = 0;
    while(1) {
        //print_event_queue(q);
        event = get_next_event(q);
        if ( event == 0 ) { 
            printf("empty event queue");
            break;
        }
        if ( sim_state->stop_time < 
                sim_state->current_time ) 
        {
           printf("simluation timeout");
           break; 
        }
        //update current time
        sim_state->current_time = event->ttime;
        float fobs = 0.0f;
        int r = 0;
        //printf("event->e = %d\n", event->e );
        //printf("event->ttime = %f\n", event->ttime );
        switch ( event->e ) {
            case NEW_FRAME:
                //printf("NEW_FRAME\n");
                insert_event(q, TOP_ARRIVAL, event->ttime );
                insert_event(q, BOTTOM_ARRIVAL, event->ttime );
                schedule_new_frame(sim_state, q);
                break;
            case TOP_ARRIVAL:
                //printf("TOP_ARRIVAL\n");
                fobs = expon(sim_state->field_complexity_mean);
                r = insert_field ( &sim_state->encoder.buf,
                        TOP_FIELD, fobs);
                sim_state->total_frames+=2;
                if ( r < 0 ) {
                    // discard bottom arrival event
                    event_element_t *next_event = 
                        get_next_event(q);;
                    free(next_event);
                    sim_state->lost_frames+=2;
                }
                break;
            case BOTTOM_ARRIVAL:
                //printf("BOTTOM_ARRIVAL\n");
                fobs = expon(sim_state->field_complexity_mean);
                r = insert_field ( &sim_state->encoder.buf,
                        BOTTOM_FIELD, fobs);
                if ( r < 0 ) {
                    // remove last top field from buffer 
                    drop_last( &sim_state->encoder.buf );
                    sim_state->lost_frames+=2;
                } else {
                    schedule_encode( sim_state, q );
                }
                break;

            case ENCODE:
                schedule_store( sim_state, q );
                break;
            case STORE:
                //TODO drop field
                break;
            default:
                printf("unknow event");
                return;
        }
        free(event);
        //forth time and put next event
    }
    printf("simulation end");
}



//-----------------------------------------------------
//event method 
int schedule_new_frame(sim_state_t *sim_state, event_queue_t *q) {
    float packet_time = expon(sim_state->arrival_mean);
    float ttime = packet_time + sim_state->current_time;
    insert_event( q, NEW_FRAME, ttime );
    return 0;
}

// schedule_encode
int schedule_encode(sim_state_t *sim_state, event_queue_t *q) {
    //printf("schedule_encode\n");
    video_field_t *top = get_next_field(&sim_state->encoder.buf); 
    video_field_t *bottom = get_next_field(&sim_state->encoder.buf); 
    
    if ( top->type != TOP_FIELD ) {
        printf("type error !!!, top->type != TOP_FIELD\n");
        printf("top->type = %d\n", top->type );
        exit(1);
    }
    if ( bottom->type != BOTTOM_FIELD ) {
        printf("type error !!!, bottom->type != BOTTOM_FIELD\n");
        exit(1);
    }
    // frame_size = h1 + h2
    float frame_size = top->fobs + bottom->fobs;
    float encode_size = frame_size * sim_state->alpha;
    
    float process_time = frame_size / sim_state->encoder.c_enc;
    float ttime = 0.0f;
    if ( sim_state->last_encode_finished_time <= 0.0 ) {
        ttime = process_time + sim_state->current_time;
    } else {
        ttime = process_time + sim_state->last_encode_finished_time;
    }
    sim_state->last_encode_finished_time = ttime;
    insert_event( q, ENCODE, ttime );
    insert_field( &sim_state->storage.buf, 
            ENCODED_FIELD , encode_size );
    free(top); free(bottom);
    return 0;
}

// schedule_store
int schedule_store(sim_state_t *sim_state, event_queue_t *q) {
    video_field_t *encode_field = 
        get_next_field(&sim_state->storage.buf); 
    if ( encode_field->type != ENCODED_FIELD ) {
        printf("type error !!!, encode_field->type != ENCODED_FIELD");
        exit(1);
    }

    // frame_size = a(h1 + h2)
    float frame_size = encode_field->fobs;
    
    float process_time = frame_size / sim_state->storage.c_storage;
    float ttime = 0.0f;
    if ( sim_state->last_storage_finished_time <= 0.0 ) {
        ttime = process_time + sim_state->current_time;
    } else {
        ttime = process_time + sim_state->last_storage_finished_time ;
    }
    sim_state->last_storage_finished_time = ttime;
    sim_state->storage_busy_time += process_time;
    insert_event( q, STORE, ttime );
    free(encode_field); 
    return 0;
}
//-----------------------------------------------------
// simulation initialization
// sim_initial() --> sim_initial2() 

int sim_initial(
        sim_state_t *sim_state, event_queue_t *q, 
        int enc_caps, int storage_caps, int c_enc, int c_storage,
        float arrival_mean, float field_comp_mean, float stop_time, float alpha )
{
    memset( sim_state, 0, sizeof( sim_state ));
    init_event_queue( q ); 
    sim_state->encoder_capacity_beta = enc_caps;
    sim_state->storage_capacity = storage_caps;
    sim_state->arrival_mean = arrival_mean ;
    sim_state->field_complexity_mean = field_comp_mean ;
    sim_state->stop_time = stop_time;
    sim_state->alpha = alpha;
    init_encoder( &sim_state->encoder, c_enc, sim_state->encoder_capacity_beta );
    init_storage( &sim_state->storage, c_storage , sim_state->storage_capacity );
    // the first field will be top field, arrives at time 0
    insert_event( q, NEW_FRAME , 0.0f);
    return 0;
}
void do_simulation( sim_state_t *sim_state, event_queue_t *q ) {
    event_scheduler( sim_state, q ) ; 
}
void param_report ( sim_state_t *sim_state ) {
    printf("simulation parameter\n");
    printf("simulation:\n");
    printf("\tstop_time : %f\n",
            sim_state->stop_time);

    printf("encoder:\n");
    printf("\tc_enc(encoder process rate) = %d\n", 
            sim_state->encoder.c_enc );
    printf("\tbeta (encoder buffer length)  = %d\n", 
            sim_state->encoder_capacity_beta );

    printf("storage:\n");
    printf("\tc_storage(storage processing rate) = %d\n",
            sim_state->storage.c_storage);
    printf("\tstorage buffer length  = %d\n", 
            sim_state->storage_capacity );

    printf("field:\n");
    printf("\tfield inter-arrival time = %f\n",
            sim_state->arrival_mean);
    printf("\tfield complexity = %f\n",
            sim_state->field_complexity_mean );
    printf("\talpha = %f\n", sim_state->alpha );
}
void report( sim_state_t *sim_state, event_queue_t *q ) {
    printf("total frames : %d\n", sim_state->total_frames );    
    printf("lost frames : %d\n", sim_state->lost_frames );    
    printf("current_time : %f\n", sim_state->current_time );    
    printf("storage_busy_time : %f\n", sim_state->storage_busy_time );    
}

//-----------------------------------------------------
//global variables 
sim_state_t     g_sim_state;
event_queue_t   g_event_queue;

//-----------------------------------------------------
// 
int main(int argc, char* argv) {
    printf("Tandem Queue Simulation\n");
    sim_initial(&g_sim_state, &g_event_queue, 
            20, -1, 15800, 1600, 1/59.94, 262.5,
            8*3600, 0.1
            );
    param_report( &g_sim_state );
    printf("do simulation.......\n");
    do_simulation( &g_sim_state, &g_event_queue );
    printf("simulation finished\n");
    report( &g_sim_state, &g_event_queue );
    return 0;
}
//-----------------------------------------------------
