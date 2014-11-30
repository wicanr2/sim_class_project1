//tandem queue
//encoder and storage

//-----------------------------------------------------
#define TOP_FIELD 1
#define BOTTOM_FIELD 2
// video field structure
typedef struct _video_field_t {
    //field type, 1 = top , 2 = bottom, 0 = undefined
    int type; 
    //the complexity of the field
    float fobs; 
} video_field_t;

typedef struct _field_buf_t {
    int capacity;
    video_field_t* fields;
} field_buf_t;
//-----------------------------------------------------
enum evnet {
    TOP_ARRIVAL;
    BOTTOM_ARRIVAL;
    QUEUE_IN_ENCODE;
    ENCODE_FRAME;
    QUEUE_IN_STORAGE;
    STORED;
};
typedef struct _encoder_t {
    //the processing capacity of the encoder 
    int c_enc; 
    field_buf_t limited_buf;
}encoder_t;
typedef struct _storage_t {
    // the capacity of the storage server
    int c_storage;
    // the storage always has a sufficient buffer space to 
    // store an arrivaing field
    field_buf_t infinite_buf;
};
// the structure of simulation states
// it defines the simulation state variables
typedef struct _sim_state_t {
    int frames_not_stored; // the number of frame discarded
    int total_frames; // total frame generation during simulation
    int sim_time_in_second; // the current simulation time in second
    int sim_stop_time_in_second; // the stop time in second
    // the inter-arrival time mean of a field on exponential distribution
    // = 1/59.94 or 1/50
    float arrival_mean; 
    // the field complexity mean on exponential distribution
    // = 262.5 or 312.5
    float field_complexity_mean
} sim_state_t;
//-----------------------------------------------------
//global variables 
sim_state_t sim_state;
field_buf_t field_buf;
//-----------------------------------------------------
//method definitions

//-----------------------------------------------------
// simulation initialization
int sim_initial(){
}

//-----------------------------------------------------
// 
int main(int argc, char* argv) {
    printf("Tandem Queue Simulation");
    return 0;
}
