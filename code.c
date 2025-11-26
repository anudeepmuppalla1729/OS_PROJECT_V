/* sleeping_ta.c
   Sleeping TA problem (POSIX threads + semaphores + mutex)
   Usage: ./sleeping_ta <num_students> <num_chairs>
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define DEFAULT_STUDENTS 5
#define DEFAULT_CHAIRS 3

// Semaphores and mutex
sem_t students_sem;
sem_t ta_sem;
sem_t help_finished_sem;
pthread_mutex_t chair_mutex;

int waiting_students = 0;
int NUM_CHAIRS;
int NUM_STUDENTS;
int helped_count = 0;

int *student_helped;

// ---------- TIME PRINT FUNCTION (Correct IST Time) ----------
void print_time() {
    time_t now = time(NULL);

    // Convert UTC to IST (UTC + 5:30)
    now += 5 * 3600 + 30 * 60;

    struct tm *t = gmtime(&now);  // use gmtime because we manually applied offset

    printf("[%02d:%02d:%02d] ", t->tm_hour, t->tm_min, t->tm_sec);
}

// Random generator helper
int rand_between(int a, int b) {
    return a + rand() % (b - a + 1);
}

// --------------------- TA THREAD ---------------------
void *ta_thread(void *arg) {
    while (1) {
        sem_wait(&students_sem);

        pthread_mutex_lock(&chair_mutex);
        waiting_students--;
        print_time();
        printf("TA: Student called in. Waiting students now = %d\n",
               waiting_students);
        pthread_mutex_unlock(&chair_mutex);

        sem_post(&ta_sem);

        int help_time_ms = rand_between(500, 2000);
        print_time();
        printf("TA: Helping a student for %d ms...\n", help_time_ms);
        usleep(help_time_ms * 1000);

        sem_post(&help_finished_sem);

        pthread_mutex_lock(&chair_mutex);
        helped_count++;
        int done = (helped_count >= NUM_STUDENTS);
        pthread_mutex_unlock(&chair_mutex);

        if (done) {
            print_time();
            printf("TA: All students have been helped. TA is done.\n");
            break;
        }
    }
    return NULL;
}

// --------------------- STUDENT THREAD ---------------------
void *student_thread(void *arg) {
    int id = *((int *)arg);
    free(arg);

    while (!student_helped[id]) {

        int arrival_delay_ms = rand_between(100, 1500);
        usleep(arrival_delay_ms * 1000);

        print_time();
        printf("Student %d: Arrived.\n", id);

        pthread_mutex_lock(&chair_mutex);

        if (waiting_students < NUM_CHAIRS) {
            waiting_students++;

            print_time();
            printf("Student %d: Sitting in waiting chair. Chairs used = %d/%d\n",
                   id, waiting_students, NUM_CHAIRS);

            pthread_mutex_unlock(&chair_mutex);

            sem_post(&students_sem);
            sem_wait(&ta_sem);

            print_time();
            printf("Student %d: Getting help from TA.\n", id);

            sem_wait(&help_finished_sem);

            print_time();
            printf("Student %d: Done and leaving.\n", id);

            student_helped[id - 1] = 1;
            break;

        } else {
            print_time();
            printf("Student %d: No free chair, will come back later.\n", id);

            pthread_mutex_unlock(&chair_mutex);

            int retry_delay_ms = rand_between(500, 2000);
            usleep(retry_delay_ms * 1000);
        }
    }
    return NULL;
}

// ---------------------------- MAIN ----------------------------
int main(int argc, char *argv[]) {
    srand(time(NULL));

    NUM_STUDENTS = (argc >= 2) ? atoi(argv[1]) : DEFAULT_STUDENTS;
    NUM_CHAIRS = (argc >= 3) ? atoi(argv[2]) : DEFAULT_CHAIRS;

    print_time();
    printf("Sleeping TA Simulation: %d students, %d chairs\n",
           NUM_STUDENTS, NUM_CHAIRS);

   
 student_helped = calloc(NUM_STUDENTS, sizeof(int));
    if (!student_helped) {
        perror("calloc student_helped");
        exit(1);
    }
    
    sem_init(&students_sem, 0, 0);
    sem_init(&ta_sem, 0, 0);
    sem_init(&help_finished_sem, 0, 0);
    pthread_mutex_init(&chair_mutex, NULL);

    pthread_t ta;
    pthread_t *students = malloc(sizeof(pthread_t) * NUM_STUDENTS);
    if (!students) {
        perror("malloc students array");
        exit(1);
    }

    // TA thread
    if (pthread_create(&ta, NULL, ta_thread, NULL) != 0) {
        perror("pthread_create ta");
        exit(1);
    }
    for (int i = 0; i < NUM_STUDENTS; i++) {
        int *id = malloc(sizeof(int));
        if (!id) {
            perror("malloc student id");
            exit(1);
        }
        *id = i + 1;
    if (pthread_create(&students[i], NULL, student_thread, id) != 0) {
            perror("pthread_create student");
            exit(1);
        }
    }

    for (int i = 0; i < NUM_STUDENTS; i++)
        pthread_join(students[i], NULL);

    pthread_join(ta, NULL);

    sem_destroy(&students_sem);
    sem_destroy(&ta_sem);
    sem_destroy(&help_finished_sem);
    pthread_mutex_destroy(&chair_mutex);

    free(student_helped);
    free(students);

    print_time();
    printf("Simulation complete. Exiting.\n");

    return 0;
}