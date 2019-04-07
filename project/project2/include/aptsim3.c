#include <sys/mman.h>
#include <linux/unistd.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include "sem.h"


//tools for time interval
time_t *start;  //program start time
time_t *end;    //check out time
int elapsedTime;  //time interval

//shaerd numbers
int *tp;    //number of tenants inside
int *ap;    //number of agents inside

int *ot;    //number of outside tenants
int *oa;    //number of outside agents

int *over;  //counter of viewed tenants

//command line inputs
int m;  //number of tenants
int k;  //number of agents

//tenant input
int pt;     //probability of a tenant immediately following another tenant
int dt;     //delay before any new tenant will come
unsigned int st;     //random seed for the tenant arrival process

//agent input
int pa;     //probability of an agent immediately following another agent
int da;     //delay in seconds when an agent does not immediately follow another agent
unsigned int sa;     //random seed for the agent arrival process

//randon number
int *r;

//our cs1550 function
void down(struct cs1550_sem *sem) {
  syscall(__NR_cs1550_down, sem);
}

void up(struct cs1550_sem *sem) {
  syscall(__NR_cs1550_up, sem);
}





//tenant leave
void tenantLeaves(struct cs1550_sem *mutex_t,
  struct cs1550_sem *t_l, struct cs1550_sem *t_apt, struct cs1550_sem *finish,
  struct cs1550_sem *nobody, int t)
  {

    down(mutex_t); //"tenant leave" semaphore
    down(t_apt);  //"modify tenant insde"

    (*tp)--;  //decrement number of tenants in apartment
    (*over)++;  //update viewed tenants

    time(end);  //check out leaving time
    elapsedTime = difftime (*end,*start);
    printf("Tenant %d leaves the apartment at time %d\n", t, elapsedTime);

    if((*tp) == 0 )
    {
      up(nobody);   //tell agent no tenant is inside now
    }

    if((*over) == m)
    {
      up(finish);   //tell agent she could leave
    }

    up(t_apt);  //recover "modify" available
    up(t_l); //if any tenant is waiting, now one could enter
    up(mutex_t); //free the apartment

    exit(0);
  }



  //agent leave
  void agentLeaves(struct cs1550_sem *mutex_a,
    struct cs1550_sem *a_l, struct cs1550_sem *a_apt, int a)
    {

      down(mutex_a);  //protect apartment area
      down(a_apt);    //"modify agent inside"

      (*ap)--;
      time(end);  //check out agent leaving time
      elapsedTime = difftime (*end,*start);
      printf("Agent %d leaves the apartment at time %d\n", a, elapsedTime);

      up(a_apt);  //recover "modify" available
      up(a_l);  //now an agent could enter
      up(mutex_a);  //free apartment area

      exit(0);
    }
















    /************  agent arrive function  **************/

    void openApt(struct cs1550_sem *a_apt,
      struct cs1550_sem *a_l,
      struct cs1550_sem *mutex_a,
      struct cs1550_sem *finish,
      struct cs1550_sem *nobody,
      int a)
      {
        
        down(a_apt);  //agent inside semaphore

        time(end);  //check out entering time
        elapsedTime = difftime (*end,*start);
        printf("Agent %d opens the apartment for inspection at time %d.\n", a, elapsedTime);

        (*ap)++;  //update number if agents in the apartment

        up(a_apt);  //recover agent inside sempahore

        down(nobody); //wait for tenants inside

        //now she could leave
        agentLeaves(mutex_a, a_l, a_apt, a);


      }




      /*********** Agent Arrive ***************/
      void agentArrives(
        struct cs1550_sem *mutex_a,
        struct cs1550_sem *a_a,
        struct cs1550_sem *a_l,
        struct cs1550_sem *a_apt,
        struct cs1550_sem *a_c,
        struct cs1550_sem *t_a,
        struct cs1550_sem *t_c,
        int a,
        struct cs1550_sem *outside,
        struct cs1550_sem *finish,
        struct cs1550_sem *nobody
      )
      {

        down(outside);    //make sure outside critical region safe
        down(a_a);        //make sure no outside agents conflicts

        (*oa)++;          //update number of outside agents

        //check whether other tenants are waiting
        if((*ot) >= 1 && (*oa) == 1)
        {
          int c;
          for(c = 0; c< (*ot); c++)
          {
            up(a_c);  //tell all waiting tenants that now an agent has come
          }
        }

        time(end);
        elapsedTime = difftime (*end,*start);

        printf("Agent %d arrives at time %d.\n", a, elapsedTime);

        up(a_a);        //free outside agents' sempahore
        up(outside);    //free outside semahpre

        //then check if a tenant has arrived
        while(1)
        {
          //check whether should agent wait for tenants coming
          if((*ot) <=0)
          {
            down(t_c);  //wait tenant come
          }
          else
          {
            break;
          }
        }

        //see if any agent is inside the apartment
        while(1)
        {
          down(a_l);    //wait inside agent leaves

          if(*over == m)  //check whether all tenants have viewd
          {
            up(a_l);     //free other waiting agents
            exit(0);
          }

          break;
        }



        //now this agent is able to open the apt
        openApt(a_apt, a_l, mutex_a, finish, nobody, a);
      }






      /*************  tenant  function  ***************/


      void viewApt(struct cs1550_sem *t_apt,
        struct cs1550_sem *t_l,
        struct cs1550_sem *mutex_t,
        struct cs1550_sem *finish,
        struct cs1550_sem *nobody,
        int t)
        {

          //update tenants inside the apartment
          (*tp)++;

          time(end);
          elapsedTime = difftime (*end,*start);
          printf("Tenant %d inspects the apartment at time %d.\n", t, elapsedTime);

          up(t_apt);  //free inside tenants sempahore


          sleep(2);   //view the apartment 2 seconds

          //now this tenant leave
          tenantLeaves(mutex_t, t_l, t_apt, finish, nobody, t);

        }




        /************ Arrive *************/
        void tenantArrives(
          struct cs1550_sem *mutex_t,
          struct cs1550_sem *t_a,
          struct cs1550_sem *t_l,
          struct cs1550_sem *t_apt,
          struct cs1550_sem *t_c,
          struct cs1550_sem *a_a,
          struct cs1550_sem *a_c,
          int t,
          struct cs1550_sem *outside,
          struct cs1550_sem *finish,
          struct cs1550_sem *nobody
        )
        {

          down(outside);    //make sure outside critical region safe
          down(t_a);        //make sure no outside tenants conflict

          //update the number of outside tenants
          (*ot)++;

          //check whehter other agents are waiting for tenants
          if( (*ot) == 1 && (*oa) >= 1)
          {
            int i;
            for(i=0;i<(*oa);i++)
            {
              up(t_c);  //tell all waiting agents that now a tenant has come
            }
          }

          time(end);
          elapsedTime = difftime (*end,*start);
          printf("Tenant %d arrives at time %d.\n", t, elapsedTime);

          up(t_a);          //free outside tenants' semaphore
          up(outside);      //free outside sempahore

          //check if an agent has arrived
          while(1)
          {
            //check whether should wait agents coming
            if((*oa) <= 0)
            {
              down(a_c); //wait agent to come
            }


            //check if the apartment is full
            down(t_apt);
            //check whether the apartment is full of tenants
            if((*tp) > 10)
            {
              //printf("Tenant %d waiting for a tenant to leave\n\n", t);//wait for a tenant inside to leave
              up(t_apt);
              down(t_l);    //wait for other tenants leave
            }

            //check whether an agent is inside
            if((*ap) == 1)
            {
              break;
            }


          }

          //now this tenant is able to enter the apt
          viewApt(t_apt, t_l, mutex_t, finish, nobody, t);

        }


        /***********************************************************************/
        /*
        main
        */
        int main(int argc, char *argv[]){

          //Initialize timer
          start = mmap(NULL, (sizeof(time_t)), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
          end = mmap(NULL, (sizeof(time_t)), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
          time(start);
          sleep(1);
          printf("The apartment is now empty\n");

          char* s;  //command line input




          //set up arguments
          if(argc >= 2)
          {
            int i;
            for(i=1; i < argc; i++){

              //set up number of tenants
              if(strcmp(argv[i],"-m") == 0){
                s = argv[i+1];
                m = atoi(s);
              }

              //set up number of agents
              else if(strcmp(argv[i],"-k") == 0){
                s = argv[i+1];
                k = atoi(s);
              }


              //set up probability of tenants
              else if(strcmp(argv[i], "-pt") == 0){
                s = argv[i+1];
                pt = atoi(s);
              }

              //set up random seed for tenant
              else if(strcmp(argv[i], "-st") == 0){
                s = argv[i+1];
                st = atoi(s);
              }

              //set up delay of tenant
              else if(strcmp(argv[i], "-dt") == 0){
                s = argv[i+1];
                dt = atoi(s);
              }

              //set up probability of agents
              else if(strcmp(argv[i], "-pa") == 0){
                s = argv[i+1];
                pa = atoi(s);
              }

              //set up delay of agents
              else if(strcmp(argv[i], "-da") == 0){
                s = argv[i+1];
                da = atoi(s);
              }

              //set up delay of agents
              else if(strcmp(argv[i], "-sa") == 0){
                s = argv[i+1];
                sa = atoi(s);
              }

            }
          }




          /*intialize "sems"*/
          struct cs1550_sem *a_a;   //agent arrive
          struct cs1550_sem *t_a;   //tenant arrive

          //apartment door
          struct cs1550_sem *t_apt;     //inside tenants semaphore
          struct cs1550_sem *a_apt;     //inside agents semaphore

          struct cs1550_sem *t_c;   //waiting tenant to come
          struct cs1550_sem *a_c;   //waiting agent to come
          struct cs1550_sem *t_l;   //tenant leave sem
          struct cs1550_sem *a_l;   //agent leave sem

          struct cs1550_sem *mutex_t; //for tenant leave
          struct cs1550_sem *mutex_a; //for agent leave

          struct cs1550_sem *outside;   //outside sempahore
          struct cs1550_sem *finish;  //all done
          struct cs1550_sem *nobody; //no tenant inside the apartment

          //intialize sempahores
          t_a = mmap(NULL,sizeof(struct cs1550_sem),
          PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

          a_a = mmap(NULL,sizeof(struct cs1550_sem),
          PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

          mutex_t = mmap(NULL,sizeof(struct cs1550_sem),
          PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

          mutex_a = mmap(NULL,sizeof(struct cs1550_sem),
          PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

          t_c = mmap(NULL,sizeof(struct cs1550_sem),
          PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

          a_c = mmap(NULL,sizeof(struct cs1550_sem),
          PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

          t_l = mmap(NULL,sizeof(struct cs1550_sem),
          PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

          a_l = mmap(NULL,sizeof(struct cs1550_sem),
          PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

          a_apt = mmap(NULL,sizeof(struct cs1550_sem),
          PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

          t_apt = mmap(NULL,sizeof(struct cs1550_sem),
          PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

          outside = mmap(NULL,sizeof(struct cs1550_sem),
          PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

          finish = mmap(NULL,sizeof(struct cs1550_sem),
          PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

          nobody = mmap(NULL,sizeof(struct cs1550_sem),
          PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);


          //intialize available "tickets"
          up(a_a);
          up(a_l);
          up(mutex_a);
          up(a_apt);

          up(t_a);
          up(t_apt);
          up(mutex_t);

          up(outside);

          //for allocating shared memeory
          void *ptr = mmap(NULL, (6*sizeof(int)), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
          tp = ptr;
          ot = tp + 1;

          ap = ot + 1;
          oa = ap + 1;
          over = oa + 1;
          r = over + 1;

          //intialize variables
          *tp = 0;
          *ap = 0;
          *ot = 0;
          *oa = 0;
          *over = 0;

          *r = 0;

          int i;  //for for-loop

          pid_t pid_first = fork(); //first child


          if(pid_first == 0)
          {
            //the process to create tenants
            //tenant field
            int pid;
            for(i=0;i<m;i++)
            {
              //craeting new tenant
              pid = fork();

              //intialize random number generator
              srand(st);

              //generate probability
              (*r) = (rand() % 100);  //generate random probability
              //make decision
              if((*r) < (100 - pt))
              {
                sleep(dt);
              }


              if(pid == 0)
              {
                //tenant algorithm
                tenantArrives(
                  mutex_t,
                  t_a,
                  t_l,
                  t_apt,
                  t_c,
                  a_a,
                  a_c,
                  i,
                  outside,
                  finish,
                  nobody
                );
                exit(0);
                break;
              }

            }

            //wait for all tenants process
            int j;
            for(j=0;j<m;j++)
            {
              wait(NULL);
            }
            exit(0);
          }


          else{
            //now back to parent
            pid_t pid_second = fork(); //create agents' children



            if(pid_second == 0)
            {
              //the process to create agents
              //agent fields
              int pid;
              for(i=0;i<k;i++)
              {

                //create new agent
                pid = fork();

                //intialize random number generator
                srand(sa);

                //probability field
                (*r) = rand() % 100; //generate random probability
                if((*r) < (100 - pa))
                {
                  sleep(da);
                }


                if(pid == 0)
                {
                  //agent algorithm
                  agentArrives(
                    mutex_a,
                    a_a,
                    a_l,
                    a_apt,
                    a_c,
                    t_a,
                    t_c,
                    i,
                    outside,
                    finish,
                    nobody
                  );
                  exit(0);
                  break;
                }

              }

              //wait all agents process
              int j;
              for(j=0;j<k;j++)
              {
                wait(NULL);
              }

              exit(0);
            }

            else
            {
              //go back to parents again
              down(finish);   //main process wait for all tenants finish

              //Make sure parents wait two children
              int status_first;
              int status_second;
              waitpid(pid_first, &status_first, 0); //wait the first child
              waitpid(pid_second, &status_second, 0); //wait the second child


            }



          }


          return 0;
        }
