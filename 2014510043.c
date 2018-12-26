#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

#define STUDENT_CREATION_TIME 3 // This time can be changed (cannot be 0)
#define TRANSPORTING_TIME 8 // This time can be changed

#define STUDENT_NUM 100 // Maximum student thread number that will be created
#define DRIVER_NUM 10 // Driver thread number that will be created
#define TAXI_NUM 10 // Total taxi number
#define TAXI_CAPACITY 4 // Total available seats for each taxi 

// States for the taxi drivers
#define NOTCREATED -1
#define ANNOUNCING 0
#define RESTING 1
#define DRIVING 2

sem_t relax[TAXI_NUM]; // For the students in taxi
sem_t taxiStop; // For the students at the taxi stop
sem_t taxi[TAXI_NUM]; // For the taxis
sem_t mutex; // For the critical sections

int taxiSeats[TAXI_NUM] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // Keep the number of the students in the each taxi
int driverStates[DRIVER_NUM] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1}; // Keep the status of the drivers
int serviceNumber[DRIVER_NUM] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // Keep the service number of taxis to prevent starvation

int studentsCreated = 0, studentArrived = 0;
int taxiNo = -1;

void simulation() // Simulate the program
{
    int i = 0;
    int j = 0;

    printf("DEU DOLMUS TAXI \n");
    printf("------------------\n");

    for(i = 0; i < TAXI_NUM; i++)
    {
        printf("Taxi %d [ ", i + 1);
        
        
        for(j = 0; j < taxiSeats[i]; j++)
        {
            printf("S ");
        }

        for(j = 0; j < TAXI_CAPACITY - taxiSeats[i]; j++)
        {
            printf("* ");
        }

        printf("] - ");

        if(driverStates[i] == RESTING)
            printf("Sleeping ");
        else if(driverStates[i] == DRIVING)
            printf("Transporting ");
        else if(driverStates[i] == ANNOUNCING)
            printf("Announcing: Get up! Last %d seat. ", TAXI_CAPACITY - taxiSeats[i]);
        else if(driverStates[i] == NOTCREATED)
            printf("Not Created ");

        printf("| Total Transport Number: %d", serviceNumber[i]);
        
        printf("\n");
    }

    printf("Total Students Arrived: %d\n \n", studentArrived);
    
}

void *student(void *studentId)
{
    int id = (int)studentId;
    int i = 0;
    int taxiID;

    sem_wait(&mutex); // lock until global variable changes
    studentsCreated++; // the waiting student calculated with this variable
                       // (waiting students = createdStudents - arrivedStudents - all students in the all taxis)
    sem_post(&mutex); // unlock


    sem_wait(&taxiStop); // If there is no taxi at the stop, the student will sleep (waiting at the taxi stop).

    sem_wait(&mutex);

    while (1)
    {
        int minStarvation = serviceNumber[0]; // Starvation control
        int maxTotalSeat = 0;
      
        for(i = 1; i < TAXI_NUM; i++) // Find the minumum transport number in all taxis
        {
            if(serviceNumber[i] < serviceNumber[i - 1])
                minStarvation = serviceNumber[i];
        }
              
        for(i = 0; i < TAXI_NUM; i++) // First priority is to have maximum number of student waiting in taxi.
        {                             // Second is to have minumum transport number. These are going to prevent starvation.
                    
            if (driverStates[i] == DRIVING || driverStates[i] == NOTCREATED || taxiSeats[i] >= TAXI_CAPACITY)
            {
                continue;
            }

            if(taxiSeats[i] > maxTotalSeat && taxiSeats[i] < TAXI_CAPACITY){
                taxiNo = i;  
                maxTotalSeat = taxiSeats[i];             
            }
            else if(taxiSeats[i] == maxTotalSeat && taxiSeats[i] < TAXI_CAPACITY){
                if(serviceNumber[i] <= minStarvation){ 
                    taxiNo = i; 
                }
            }
        }
        
        if (driverStates[taxiNo] != DRIVING && driverStates[taxiNo] != NOTCREATED && taxiSeats[taxiNo] < TAXI_CAPACITY )
        {
            taxiID = taxiNo; // The result of the starvation control will be used for current student.
            taxiSeats[taxiID]++; // Seat number of the taxi is updated.

            if(taxiSeats[taxiID] == TAXI_CAPACITY)
                serviceNumber[taxiID]++;

            if(taxiSeats[taxiID] < TAXI_CAPACITY) 
                sem_post(&taxiStop); // Wake one student from waiting queue for taxi stop, because there is still empty seats in the taxi.

            break;
        }

    }

    if (driverStates[taxiID] == RESTING) // If the taxi driver is sleeping, then the student have to wake the driver up.
    {
        sem_post(taxi + taxiID);
        sem_post(&mutex);
    }
    else
    {
        sem_post(&mutex);
    }

    sem_wait(&mutex);
    
    if (taxiSeats[taxiID] < TAXI_CAPACITY) // Students will sleep in the taxi, until taxi is full.
    {
        sem_post(&mutex);
        sem_wait(relax + taxiID);
        sleep(1);
    }
    else if (taxiSeats[taxiID] == TAXI_CAPACITY) // The last student will wake all sleeping students up in the taxi.
    {
        sem_post(&mutex);
        sem_post(relax + taxiID);
        sem_post(relax + taxiID);
        sem_post(relax + taxiID);
    }

    return NULL;
}

void *driver(void *driverId)
{
    int id = (int)driverId; // Get unique driver id
    int i = 0;

    while (1)
    {
        sem_post(&taxiStop); // A student sleeping in the taxi stop will be waked up by taxi driver
        sem_wait(&mutex);
        driverStates[id] = ANNOUNCING;
        sem_post(&mutex);
        
        sem_wait(&mutex);
        int totalInTaxis = 0;

        for (i = 0; i < TAXI_NUM; i++)
        {
            totalInTaxis += taxiSeats[i];
        }

        if (studentsCreated == 0 || (studentsCreated - studentArrived - totalInTaxis == 0 && taxiSeats[id] == 0)) 
        { // if there is no student in the taxi stop, the driver is going to sleep until the students come
            driverStates[id] = RESTING; // State of the taxi changes to RESTING
            simulation();
            sem_post(&mutex);
            sem_wait(taxi + id); // Taxi driver is going to waiting queue
            
        }
        else
        {
            sem_post(&mutex);
        }

        sem_wait(&mutex);
        driverStates[id] = ANNOUNCING;
        sem_post(&mutex);

        while (1) // The driver is announcing the remain seats in the taxi, until the taxi is full
        {
            sem_wait(&mutex);
            if (taxiSeats[id] == TAXI_CAPACITY) // if the taxi is full, taxi is starting its destination.
            {
                driverStates[id] = DRIVING; // State of the taxi is changed to DRIVING mode
                sem_post(&mutex);
                break;
            }

            simulation(); 
            sem_post(&mutex);
            sleep(1); //Announcing time for observe simulation easily
            // Sometimes you can observe that the taxi driver announcing, while the taxi has 4 students. That caused by this sleep(1) after simulation.
            // In fact the taxi is on its way, but in the simulation, you can observe it is in announcing for a small time. Then, it will be fixed in a new simulation function call.
        }

        sleep(TRANSPORTING_TIME); // Transportation time

        sem_wait(&mutex);
        simulation(); 
        studentArrived += taxiSeats[id]; // Taxi arrived and arrived students count is added by 4.
        taxiSeats[id] = 0;
        
        sem_post(&mutex);
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    int i;
    pthread_t studentThreads[STUDENT_NUM];
    pthread_t driverThreads[DRIVER_NUM];

    srand(time(NULL));

    sem_init(&taxiStop, 0, 0);
    sem_init(&mutex, 0, 1);

    for (i = 0; i < TAXI_NUM; i++)
    {
        sem_init(taxi + i, 0, 0);
    }

    for (i = 0; i < TAXI_NUM; i++)
    {
        sem_init(relax + i, 0, 0);
    }

    for (i = 0; i < DRIVER_NUM; i++)
    { // thread creation for each drivers
        pthread_create(driverThreads + i, NULL, &driver, (void *)i);
    }

    for (i = 0; i < STUDENT_NUM; i++)
    { // thread creation for each students
        pthread_create(studentThreads + i, NULL, &student, (void *)i);
        sleep(rand() % STUDENT_CREATION_TIME); // random student creation time
    }

    for(i = 0; i < DRIVER_NUM; i++) // join all drivers's thread
    {
        pthread_join(driverThreads[i], NULL);
    }

    for (i = 0; i < STUDENT_NUM; i++) //join all students' thread
    {
        pthread_join(studentThreads[i], NULL);
    }

    return 0;
}