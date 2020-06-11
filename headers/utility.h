#ifndef utility_h
#define utlity_h

#include <pthread.h>


struct el{
  int client_id;
  struct el* next;
};
typedef struct el el;
typedef el* queue;

struct strel{
  char* info;
  struct strel* next;
};
typedef struct strel strel;
typedef strel* stringList;

struct fila {
    pthread_mutex_t lock_queue;
    queue line;
    int queue_length;
};
typedef struct fila fila;

struct desk {
  int is_open;

  int items_bought;
  int next_client;
  long int client_queue_time;
  long int client_service_time;
  int clients_sum;
  long int client_total_time;
  int client_attempts;

  int previous_worker_has_done;
  pthread_mutex_t lock_previous_worker;
  pthread_cond_t worker_has_done;
  int   times_desk_got_close;
  int total_desk_time;

  stringList receipt;
  pthread_mutex_t lock_desk;
  pthread_cond_t  cond_desk;
  pthread_cond_t  cond_worker;

  int  client_served;
  fila* client_queue;

  int worker_delay;
};
typedef struct desk desk;

extern int ehc;
extern pthread_mutex_t mutex_ehc;
extern pthread_cond_t  can_go;
extern int allowed;

extern int shop_is_open;
extern pthread_mutex_t supermarket_key;

extern int client_amount;
extern pthread_mutex_t client_number_lock;
extern pthread_cond_t  client_can_enter_cond;
extern int K,
    DESK_AT_START,
    C,
    E,
    T,
    MIN_BUY_TIME,
    P,
    MAX_TIME_TO_SERVE_CUSTOMER,
    MIN_TIME_TO_SERVE_CUSTOMER,
    TIME_PER_ITEM,
    UPPER_THRESHOLD;

extern desk *supermarket;
extern int need_to_exit;

extern int clients_kicked;
extern pthread_mutex_t kick_mutex;


#endif
