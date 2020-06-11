#include "client.h"
#include "utility.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>


//Client ID------------------------------------------------
static int i = 1;
static pthread_mutex_t mutex_ID = PTHREAD_MUTEX_INITIALIZER;


//Ehc = EmptyHandedClients Vars ----------------------
int ehc = 0;
int allowed = 0;
int need_to_exit = 0;
pthread_mutex_t mutex_ehc  = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  can_go = PTHREAD_COND_INITIALIZER;
int clients_kicked = 0;
pthread_mutex_t kick_mutex = PTHREAD_MUTEX_INITIALIZER;

//Metodo per verificare se il supermarket è stato chiuso e il cliente deve interrompere la sua esecuzione.
void got_kicked(){
	pthread_mutex_lock(&kick_mutex);
	clients_kicked = clients_kicked + 1;
	pthread_mutex_unlock(&kick_mutex);
}

static void exit_store(){
	pthread_mutex_lock(&client_number_lock);
	client_amount = client_amount - 1;
	if(C - client_amount >= E)
		pthread_cond_signal(&client_can_enter_cond);

	pthread_mutex_unlock(&client_number_lock);
}

//Metodo usato dai clienti che non hanno acquistato nulla per mettersi in attesa che il manager li faccia uscire.
static void ask_to_leave(){
	pthread_mutex_lock(&mutex_ehc);
	ehc++;
	while(!allowed && !need_to_exit)
		pthread_cond_wait(&can_go, &mutex_ehc);
	allowed = 0;
	pthread_mutex_unlock(&mutex_ehc);
	if(need_to_exit == 0)
		got_kicked();
}

static int get_id(){
	int id;
	pthread_mutex_lock(&mutex_ID);
	id = i;
	i++;
	pthread_mutex_unlock(&mutex_ID);
	return id;
}

//Fase di acquisto dei clienti.
static int go_buy(int client_id, unsigned int *seed){
	int time_millis = (rand_r(seed) % (T - MIN_BUY_TIME + 1)) + MIN_BUY_TIME;
  struct timespec time_to_wait = get_timespec(time_millis);
	nanosleep(&time_to_wait, NULL);

	return time_millis;
}


//Metodo per aspettare il proprio turno in fila alla cassa.
//Se arriva il turno di un cliente, esso scrive le proprie informazioni nella cassa e viene servito.
//Se la cassa viene chiusa mentre è in attesa o il supermarket è stato chiuso, il cliente abbandona la fila.
int wait_my_turn(desk *desk_p, int client_id, int n_items, int time_buy_phase, struct timespec queue_begin, int queues_bounced){
  pthread_mutex_lock(&desk_p->lock_desk);
  while(desk_p->next_client != client_id && desk_p->is_open == 1){
    pthread_cond_wait(&desk_p->cond_desk, &desk_p->lock_desk);
  }
  if(desk_p->is_open == 0){
		//La cassa è stata chiusa
		desk_p->client_served = 1;
    pthread_cond_signal(&desk_p->cond_worker);
    pthread_mutex_unlock(&desk_p->lock_desk);
    return 2;
  }
	if(desk_p->is_open == -1){
		//Il supermarket è stato chiuso
		desk_p->client_served = 1;
		pthread_cond_signal(&desk_p->cond_worker);
		pthread_mutex_unlock(&desk_p->lock_desk);
		got_kicked();
		return 3;
	}
	//Il cliente viene servito.
	struct timespec queue_end = update_timespec();
	desk_p->items_bought = n_items;
  desk_p->client_attempts = queues_bounced;

  long int elapsed_time = get_elapsed_time(queue_begin, queue_end);
  desk_p->client_queue_time = elapsed_time;
  int millis_to_get_served = (n_items * TIME_PER_ITEM) + desk_p->worker_delay;
  struct timespec service_time = get_timespec(millis_to_get_served);
  nanosleep(&service_time, NULL);
	desk_p->client_service_time = desk_p->client_service_time + millis_to_get_served;
  long int total_time = time_buy_phase + elapsed_time + millis_to_get_served;
  desk_p->client_total_time = total_time;
	desk_p->clients_sum = desk_p->clients_sum + 1;
  desk_p->client_served = 1;
  pthread_cond_signal(&desk_p->cond_worker);
  pthread_mutex_unlock(&desk_p->lock_desk);
  return 0;
}

//Metodo per inserirsi in coda ad una cassa, se la cassa e il supermarket sono aperti.
int start_queue(int index, int client_id, int n_items, int time_buy_phase, struct timespec queue_begin, int queues_bounced){
  desk* desk_p = &supermarket[index];
  pthread_mutex_lock(&desk_p->lock_desk);
  if(desk_p->is_open == 0){
		//Cassa chiusa.
    pthread_mutex_unlock(&desk_p->lock_desk);
    return 1;
  }
	if(desk_p->is_open == -1){
		//supermarket chiuso.
    pthread_mutex_unlock(&desk_p->lock_desk);
		got_kicked();
    return 3;
  }
	//Il client si inserisce in coda.
  pthread_mutex_unlock(&desk_p->lock_desk);
  fila* cFila = desk_p->client_queue;
  pthread_mutex_lock(&cFila->lock_queue);
  queue new_el = malloc(sizeof(el));
  new_el->client_id = client_id;
  new_el->next = NULL;
  queue q = cFila->line;
  if(q == NULL){
    cFila->line = new_el;
  }
  else{
    while(q->next != NULL)
      q = q->next;

    q->next = new_el;
  }
  cFila->queue_length = cFila->queue_length + 1;
  pthread_mutex_unlock(&cFila->lock_queue);

  return wait_my_turn(&supermarket[index], client_id, n_items, time_buy_phase, queue_begin, queues_bounced);
}

//Metodo per cercare una cassa aperta finchè il supermarket è aperto
static void go_checkout(int items_needed, int client_id, int time_buy_phase, unsigned int *seed){
	int queues_bounced = 1;
	int desk_ind = rand_r(seed) % K;
	struct timespec queue_begin = update_timespec();
	int result = start_queue(desk_ind, client_id, items_needed, time_buy_phase, queue_begin, queues_bounced);
	while(result == 1 || result == 2){
		if(result == 2)
    	queues_bounced++;
		desk_ind = rand_r(seed) % K;
    result = start_queue(desk_ind, client_id, items_needed, time_buy_phase, queue_begin, queues_bounced);
	}
}

int supermarket_is_close(unsigned int *seed){
	int r_desk_ind = rand_r(seed) % K;
	desk *desk_p = &supermarket[r_desk_ind];
	int status;
	pthread_mutex_lock(&desk_p->lock_desk);
	status = desk_p->is_open;
	pthread_mutex_unlock(&desk_p->lock_desk);
	return status;
}

extern void* go_shopping(){
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGHUP);
	sigaddset(&set, SIGQUIT);
	if(pthread_sigmask(SIG_BLOCK, &set, NULL) != 0){
		perror("Error pthread sigmask (Client)");
		return NULL;
	}
	pthread_mutex_lock(&client_number_lock);
	client_amount = client_amount + 1;
	pthread_mutex_unlock(&client_number_lock);
  unsigned int seed = time(NULL) ^ pthread_self();
	int client_id = get_id();
	int items_needed = rand_r(&seed) % (P + 1);
	if(supermarket_is_close(&seed) < 0){
		got_kicked();
		exit_store();
		return NULL;
	}
	int time_buy_phase = go_buy(client_id, &seed);
	if(items_needed == 0)
		ask_to_leave();
	else
		go_checkout(items_needed, client_id, time_buy_phase, &seed);

  exit_store();
	return NULL;
}
