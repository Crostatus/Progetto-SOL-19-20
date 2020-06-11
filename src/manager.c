#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "manager.h"
#include "client.h"
#include "cassiere.h"
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include "utility.h"

//Mappa delle casse aperte
static short int *bit_map;
static unsigned int *queues_lenghts;

//Ehc = EmptyHandedClients Vars -
extern int ehc;
extern pthread_mutex_t mutex_ehc;
extern pthread_cond_t  can_go;
extern int allowed;
static int total_ehc = 0;

extern void* goBuy();
//Variabile per l' interrupt handler
static volatile int flag = 0;

extern int client_amount;
extern pthread_mutex_t client_number_lock;
extern pthread_cond_t client_can_enter_cond;


int shop_is_open = 1;
pthread_mutex_t supermarket_key = PTHREAD_MUTEX_INITIALIZER;
int deadline;

void sig_handler(int signo){
    if (signo == SIGQUIT) {
		  flag = 2;
	  }
    else if (signo == SIGHUP) {
		  flag = 1;
	  }
}

//Metodo per mandare segnali ai clienti senza acquisti che attendono il permesso di uscire.
void let_ehc_out(){

pthread_mutex_lock(&mutex_ehc);
	while(ehc > 0){
		ehc--;
		allowed = 1;
		pthread_cond_signal(&can_go);
		pthread_mutex_unlock(&mutex_ehc);
		total_ehc++;
		pthread_mutex_lock(&mutex_ehc);
	}
	pthread_mutex_unlock(&mutex_ehc);
}

//Restituisce l'indice della cassa da aprire, -1 altrimenti.
int check_open(){
  int i = 0;
  int too_crowded_desk = -1;
  while((i < K) && (too_crowded_desk == -1)){
    if(bit_map[i] && (queues_lenghts[i] >= UPPER_THRESHOLD))
      too_crowded_desk = i;
    i++;
  }
  if(too_crowded_desk != -1){
    int new_id = -1;
    i = 0;
    while((i < K) && (new_id == -1)){
      if(!bit_map[i])
        new_id = i;
      i++;
    }

    return new_id;
  }
  return too_crowded_desk;
}

//Restiuisce indice della cassa da chiudere, -1 altrimenti.
int check_close(){
  int i;
  int too_empty_desk = -1;
  int min_amount_to_close = K / 3;
  int avg_queue = C / K;
  int shorter_queue_desk = C + 1;
  int underworking_desks = 0;
  for(i = K; i > -1; i--){
      if(bit_map[i] && queues_lenghts[i] < avg_queue){
        underworking_desks++;
        if(queues_lenghts[i] < shorter_queue_desk){
          shorter_queue_desk = queues_lenghts[i];
          too_empty_desk = i;
        }
      }
  }
  if(underworking_desks <= min_amount_to_close)
    return -1;

  return too_empty_desk;
}

void open_desk(int id){
  pthread_t new_worker;
  int *arg = malloc(sizeof(int *));
  *arg = id;
  if(pthread_create(&new_worker, NULL, &go_work, arg) != 0)
    fprintf(stderr, "pthread_create failed (Worker %d)\n", id);
}

void close_desk(int id, int flag){
  pthread_mutex_lock(&supermarket[id].lock_desk);
  supermarket[id].is_open = flag;
  supermarket[id].client_served = 1;
  pthread_cond_signal(&supermarket[id].cond_worker);
  pthread_cond_broadcast(&supermarket[id].cond_desk);
  pthread_mutex_unlock(&supermarket[id].lock_desk);
}

//Metodo di diagnostica, in uso se viene definito DEBUG.
void print_supermarket(){
  printf("Desks map:       ");
  int i;
  for(i = 0; i < K; i++)
    printf("%d ", bit_map[i]);
  printf("\nDesks queues:    ");
  for(i = 0; i < K; i++)
    printf("%d ", queues_lenghts[i]);
  printf("\n");
  fflush(NULL);
}

//Metodo che sceglie se aprire/chiudere una cassa.
void supermarket_checkup(){
  int i;
  int desks_open = 0;
  for(i = 0; i < K; i++){
    desk *tmp = &supermarket[i];
    pthread_mutex_lock(&tmp->lock_desk);
    if(tmp->is_open <= 0)
      bit_map[i] = 0;
    else {
      bit_map[i] = 1;
      desks_open++;
    }
    fila* check = tmp->client_queue;
    pthread_mutex_lock(&check->lock_queue);
    if(bit_map[i])
      queues_lenghts[i] = check->queue_length;
    else
      queues_lenghts[i] = 0;
    pthread_mutex_unlock(&check->lock_queue);
    pthread_mutex_unlock(&tmp->lock_desk);
  }
  #if defined(DEBUG)
    print_supermarket();
  #endif
  if(desks_open < 1){
    int id_to_open = check_open();
    if(id_to_open != -1) {
      printf("[MANAGER] Opening desk %d\n", id_to_open);
      open_desk(id_to_open);
    }
    return;
  }
  if(desks_open == K){
    int id_to_close = check_close();
    if(id_to_close != -1){
      close_desk(id_to_close, 0);
      printf("[MANAGER] Closing desk %d\n", id_to_close);
    }
    return;
  }
  int desk_to_open = check_open();
  int desk_to_close = check_close();
  if(((desk_to_open == -1) && (desk_to_close == -1)) || ((desk_to_open != -1) && (desk_to_close != -1))){
    return;
  }
  if(desk_to_open != -1){
    printf("[MANAGER] Opening desk %d\n", desk_to_open);
    open_desk(desk_to_open);
    return;
  }
  if(desk_to_close != -1){
    printf("[MANAGER] Closing desk %d\n", desk_to_close);
    close_desk(desk_to_close, 0);
    return;
  }
}

//Espulsione dei clienti ancora in attesa di uscire quando viene chiuso il supermarket.
void kick_ehc_clients(){
  pthread_mutex_lock(&mutex_ehc);
  need_to_exit = 1;
  pthread_cond_broadcast(&can_go);
  pthread_mutex_unlock(&mutex_ehc);
}

//Metodo per chiudere il supermarket, senza aspettare che escano tutti i clienti al suo interno. (SIGQUIT)
static void hard_close(){
  pthread_mutex_lock(&supermarket_key);
  shop_is_open = 0;
  pthread_mutex_unlock(&supermarket_key);
  printf("[MANAGER] Closed the supermarket!\n");
  int i;
  for(i = 0; i < K; i++)
    close_desk(i, -1);

  kick_ehc_clients();

}

//Metodo che se il numero dei clienti rimasti nel supermarket dopo la chiusura
//non varia per troppo tempo restituisce 1 (sono stati persi), 0 altrimenti.
int close_time_reached(struct timespec *timer_start, int client_amount, int *prev_client_amount, int deadline, int* times){
  if(*prev_client_amount != client_amount){
    *timer_start = update_timespec();
    *times = 0;
    return 0;
  }
  printf("client_amount: %d prev_client_amount: %d\n", client_amount, *prev_client_amount);
  *times = *times + 1;
  struct timespec now = update_timespec();
  int diff = get_elapsed_time(*timer_start, now);
  if(*times >= 4 && diff >= deadline)
    return 1;

  return 0;
}

void close_all_desks(int flag){
  int i;
  for(i = 0; i < K; i++)
    close_desk(i, -1);
}

//Metodo per interrompere l' arrivo di nuovi clienti, e chiudere il supermarket non appena
//i clienti al suo interno sono stati serviti. (SIGHUP)
static void soft_close(int deadline){
  pthread_mutex_lock(&supermarket_key);
  shop_is_open = 0;
  pthread_mutex_unlock(&supermarket_key);
  struct timespec closing_start = update_timespec();

  int times = 0;

  printf("[MANAGER] Closed the supermarket!\n");
  struct timespec delay = get_timespec(1000);
  pthread_mutex_lock(&client_number_lock);
  int prev_client_amount = 0;
  int force_close = 0;
  while(client_amount > 0 && !force_close){
    pthread_mutex_unlock(&client_number_lock);
    let_ehc_out();
    supermarket_checkup();
    nanosleep(&delay, NULL);
    pthread_mutex_lock(&client_number_lock);
    force_close = close_time_reached(&closing_start, client_amount, &prev_client_amount, deadline, &times);
    prev_client_amount = client_amount;
  }
  pthread_mutex_unlock(&client_number_lock);
  printf("[MANAGER] The supermarket is now empty.\n");
  fflush(NULL);


  close_all_desks(-1);
  nanosleep(&delay, NULL);
}

//Metodo per scrivere su file la lista di report ottenuti da ogni cassa per ogni cliente servito.
void write_desk_receipt(FILE *fp, stringList receipt){
  if(receipt == NULL)
    return;

  if(receipt->next != NULL){
    write_desk_receipt(fp, receipt->next);
    fprintf(fp, "%s", receipt->info);
    free(receipt);
    return;
  }
  fprintf(fp, "%s", receipt->info);
  free(receipt);
}

//Metodo per scrivere su file tutte le informazioni raccolte durante l' attività del supermarket.
int write_log(char *file_name, int deadline){
  FILE *fp;
  if((fp = fopen(file_name, "a")) == NULL){
    perror(file_name);
    return -1;
  }
  char start_msg[150];
  time_t timer;
  struct tm *tm_info;
  timer = time(NULL);
  tm_info = localtime(&timer);
  strftime(start_msg, 100, "---------------------- SUPERMARKET LOG ---------------------\n[%H:%M:%S %d-%m-%Y]", tm_info);
  struct timespec security_sleep = get_timespec(deadline);
  nanosleep(&security_sleep, NULL);

  fprintf(fp, "%s\n", start_msg);
  int i;
  float avg_service_time;
  for(i = 0; i < K; i++){
    if(supermarket[i].clients_sum > 0)
      avg_service_time = (float) supermarket[i].client_service_time / supermarket[i].clients_sum;
    else
      avg_service_time = 0;
    fprintf(fp, "       [DESK %d LIST] clients_served: %d | total_open_time: %0.3f | avg_service_time: %0.3f | times_desk_got_close: %d\n", i, supermarket[i].clients_sum, (float)supermarket[i].total_desk_time/1000, (float)avg_service_time/1000, supermarket[i].times_desk_got_close);
    write_desk_receipt(fp, supermarket[i].receipt);
    fprintf(fp, "               [END DESK %d LIST]\n\n", i);
  }

  fprintf(fp, "Empty handed clients served: %d | Clients kicked: %d | Clients lost: %d\n", total_ehc, clients_kicked, client_amount);
  fprintf(fp, "---------------------- END LOG ----------------------\n");

	fclose(fp);
	return 0;
}

void* manage(){
	if(signal(SIGQUIT, sig_handler) == SIG_ERR)
		printf("\n[MANAGER] Can't catch SIGQUIT\n");
	if(signal(SIGHUP, sig_handler) == SIG_ERR)
		printf("\n[MANAGER] Can't catch SIGHUP\n");

  bit_map = (short int *) malloc(sizeof(short int)* K);
  queues_lenghts = (unsigned int *) malloc(sizeof(unsigned int) * K);
  deadline = (TIME_PER_ITEM * P) + MAX_TIME_TO_SERVE_CUSTOMER;

  struct timespec delay = get_timespec(500);
	//Loop principale del manager
	while(!flag){
		let_ehc_out();
    supermarket_checkup();
    nanosleep(&delay, NULL);
	}
  if(flag == 1)
    soft_close(deadline);
  else if(flag == 2)
    hard_close();
  else
    fprintf(stderr,"[MANAGER] Unexpected flag value %d\n", flag);

  free(bit_map);
  free(queues_lenghts);

  write_log("log.txt", deadline);


return NULL;
}
