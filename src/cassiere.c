#include "cassiere.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "utility.h"
#include <signal.h>

//Metodo per pulire la lista di clienti ormai obsoleta.
void clear_client_queue(fila *fila_p){
  pthread_mutex_lock(&fila_p->lock_queue);
  fila_p->queue_length = 0;
  queue tmp = fila_p->line;
  while(fila_p->line != NULL){
    tmp = fila_p->line;
    fila_p->line = fila_p->line->next;
    free(tmp);
  }
  pthread_mutex_unlock(&fila_p->lock_queue);
}

static int start_desk(desk *desk_to_open, int desk_id){
    if(desk_id < 0 || desk_id >= K){
      fprintf(stderr, "[Worker %ld] Invalid desk id: %d\n", pthread_self(), desk_id);
      fflush(NULL);
      return -1;
    }
    pthread_mutex_lock(&desk_to_open->lock_desk);
    if(desk_to_open->is_open == 1){
      fprintf(stderr, "[Worker %ld] Desk %d is already open\n", pthread_self(), desk_id);
      fflush(NULL);
      pthread_mutex_unlock(&desk_to_open->lock_desk);
      return -1;
    }
		//Attesa che il cassiere precendente abbia finito.
    pthread_mutex_unlock(&desk_to_open->lock_desk);
    pthread_mutex_lock(&desk_to_open->lock_previous_worker);
    while(!desk_to_open->previous_worker_has_done){
      pthread_cond_wait(&desk_to_open->worker_has_done, &desk_to_open->lock_previous_worker);
    }
    desk_to_open->previous_worker_has_done = 0;
    pthread_mutex_unlock(&desk_to_open->lock_previous_worker);
    pthread_mutex_lock(&desk_to_open->lock_desk);
    unsigned int seed = time(NULL) ^ pthread_self();
    clear_client_queue(desk_to_open->client_queue);
    desk_to_open->is_open = 1;
    desk_to_open->items_bought = 0;
    desk_to_open->worker_delay = (rand_r(&seed) % (MAX_TIME_TO_SERVE_CUSTOMER + 1 - MIN_TIME_TO_SERVE_CUSTOMER)) + MIN_TIME_TO_SERVE_CUSTOMER;
    pthread_mutex_unlock(&desk_to_open->lock_desk);
    printf("[Worker %ld] Desk %d is now open\n", pthread_self(), desk_id);
    return 0;
}

//Metodo per ottenere l' id del prossimo cliente da servire.
int get_first_customer(queue* line_to_update){
  queue tmp = *line_to_update;
  if(tmp == NULL){
    return 0;
  }
  *line_to_update = (*line_to_update)->next;
  int id = tmp->client_id;
  free(tmp);
  return id;
}

//Metodo per ottenere l' id del prossimo cliente da servire.
//Restiuisce 0 se non ci sono clienti da servire.
int update_queue(fila *queue){
  pthread_mutex_lock(&queue->lock_queue);
  if(queue->queue_length == 0){
    pthread_mutex_unlock(&queue->lock_queue);
    return 0;
  }else{
    int next = get_first_customer(&queue->line);
    if(next != 0)
      queue->queue_length--;
    pthread_mutex_unlock(&queue->lock_queue);
    return next;
  }
}

void serve_customer(desk *desk_p){
  pthread_mutex_lock(&(desk_p->lock_desk));
  if(desk_p->is_open <= 0){
    desk_p->next_client = 0;
    pthread_cond_broadcast(&desk_p->cond_desk);
    pthread_mutex_unlock(&desk_p->lock_desk);
    return;
  }
  pthread_cond_broadcast(&desk_p->cond_desk);
  pthread_mutex_unlock(&desk_p->lock_desk);

	//Aspetto che abbia finito.
  pthread_mutex_lock(&desk_p->lock_desk);
  while(!desk_p->client_served)
    pthread_cond_wait(&desk_p->cond_worker, &desk_p->lock_desk);
  desk_p->client_served = 0;
  pthread_mutex_unlock(&desk_p->lock_desk);
}

//Metodo per scrivere nella stringa "scontrino" i valori messi dal client nella cassa.
void update_receipt(desk *desk_p){
  pthread_mutex_lock(&desk_p->lock_desk);
  if(desk_p->next_client == 0){
    pthread_mutex_unlock(&desk_p->lock_desk);
    return;
  }
  pthread_mutex_unlock(&desk_p->lock_desk);
  stringList tmp = (stringList) malloc(sizeof(strel));
  if(desk_p->receipt == NULL){
    tmp->next = NULL;
    desk_p->receipt = tmp;
  }
  else{
    tmp->next = desk_p->receipt;
    desk_p->receipt = tmp;
  }
  tmp->info = (char *) malloc(sizeof(char) * 151);
  sprintf(tmp->info, "id: %d | items: %d | total_time: %ld | queue_time: %ld | queues_visited: %d\n", desk_p->next_client, desk_p->items_bought, desk_p->client_total_time, desk_p->client_queue_time, desk_p->client_attempts);
  #if defined(DEBUG)
    printf("%s\n", tmp->info);
  #endif

}

void* go_work(void* desk_id){
		sigset_t set;
		sigemptyset(&set);
		sigaddset(&set, SIGHUP);
		sigaddset(&set, SIGQUIT);
		if(pthread_sigmask(SIG_BLOCK, &set, NULL) != 0){
			perror("Error pthread sigmask (Worker)");
			return NULL;
		}
    int id = *((int *) desk_id);
    free(desk_id);
    desk* my_desk = &supermarket[id];
    if(start_desk(my_desk, id) != 0){
      return NULL;
    }
    struct timespec start_time = update_timespec();
    struct timespec worker_pause = get_timespec(50);
    fila* my_queue = my_desk->client_queue;
    pthread_mutex_lock(&my_desk->lock_desk);
		//Loop principale del cassiere.
    while(my_desk->is_open == 1){
      pthread_mutex_unlock(&my_desk->lock_desk);
      if((my_desk->next_client = update_queue(my_queue)) != 0){
        serve_customer(my_desk);
        update_receipt(my_desk);
      }
      nanosleep(&worker_pause, NULL);
      pthread_mutex_lock(&my_desk->lock_desk);
    }
		if(my_desk->is_open == -1)
			clear_client_queue(my_queue);

    pthread_cond_broadcast(&my_desk->cond_desk);
    pthread_mutex_unlock(&my_desk->lock_desk);
    nanosleep(&worker_pause, NULL);

    pthread_mutex_lock(&my_desk->lock_previous_worker);
    my_desk->previous_worker_has_done = 1;
    my_desk->times_desk_got_close = my_desk->times_desk_got_close  + 1;
    struct timespec end_time = update_timespec();
    my_desk->total_desk_time = my_desk->total_desk_time + get_elapsed_time(start_time, end_time);
    pthread_cond_signal(&my_desk->worker_has_done);
    pthread_mutex_unlock(&my_desk->lock_previous_worker);

    printf("[Worker %ld] Desk %d is now close\n", pthread_self(), id);
return NULL;
}

//Inizializzazione valori di default del supermarket.
void setup_market(){
  int i;
  for(i = 0; i < K; i++){
    supermarket[i].is_open = 0;
    supermarket[i].client_served = 0;
    supermarket[i].next_client = -1;
    supermarket[i].times_desk_got_close = 0;
    supermarket[i].total_desk_time = 0;
    supermarket[i].previous_worker_has_done = 1;
    supermarket[i].clients_sum = 0;
    supermarket[i].client_service_time = 0;
    pthread_mutex_init(&supermarket[i].lock_previous_worker, NULL);
    pthread_cond_init(&supermarket[i].worker_has_done, NULL);
    pthread_mutex_init(&supermarket[i].lock_desk, NULL);
    pthread_cond_init(&supermarket[i].cond_desk, NULL);
    supermarket[i].receipt = NULL;
    pthread_cond_init(&supermarket[i].cond_worker, NULL);
    supermarket[i].client_queue = malloc(sizeof(fila));
    pthread_mutex_init(&supermarket[i].client_queue->lock_queue, NULL);
    supermarket[i].client_queue->line = NULL;
    supermarket[i].client_queue->queue_length = 0;
  }

}
