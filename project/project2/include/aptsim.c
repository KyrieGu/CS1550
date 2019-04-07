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
int *counter;    //number of tenants of a single angent
int *inside;     //number of tennants inside
int *ap;    //number of agents inside

int *ot;    //number of outside tenants
int *oa;    //number of outside agents

int *over;  //counter of viewed tenants
int *open;  //door variable

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
void tenantLeaves(struct cs1550_sem *outside,
  struct cs1550_sem *t_apt, struct cs1550_sem *finish,
  int t)
  {

    down(outside);
    down(t_apt);

    (*inside) --;   //update tenants inside
    (*over)++;      //update the total tenants have viewd apt

    //measure time
    time(end);  //check out leaving time
    elapsedTime = difftime (*end,*start);
    char buf[BUFSIZ];
    setbuf(stdout, buf);
    printf("Tenant %d leaves the apartment at time %d\n", t, elapsedTime);
    fflush(stdout);

    //check if all tenants of this agent have finished
    if((*over) == m)
    {
      up(finish);   //free this agent
    }
    else if((*inside) == 0 && ((*counter) == 10 ))
    {
      up(finish);   //free this agent
    }

    up(t_apt);
    up(outside);

    exit(0);
  }



  //agent leave
  void agentLeaves(struct cs1550_sem *outside,
    struct cs1550_sem *a_l,
    struct cs1550_sem *a_apt,
    struct cs1550_sem *door,
    int a)
    {

      down(outside);
      char buf[BUFSIZ];

      //clear the agent inside
      down(a_apt);
      (*ap) --;
      up(a_apt);


      //measure time
      time(end);  //check out agent leaving time
      elapsedTime = difftime (*end,*start);
      setbuf(stdout, buf);
      printf("Agent %d leaves the apartment at time %d\n", a, elapsedTime);
      fflush(stdout);

      //close the door
      (*open) = 0;

      //clear the counter
      (*counter) = 0;

      up(outside);



      //free other waiting agents
      up(a_l);


      exit(0);
    }



    /************  agent arrive function  **************/

    void openApt(
      struct cs1550_sem *a_apt,
      struct cs1550_sem *a_l,
      struct cs1550_sem *finish,
      struct cs1550_sem *door,
      struct cs1550_sem *outside,
      int a)
      {

        down(outside);

        (*open) == 1;  //indicate the door is open

        down(a_apt);
        //update number of outside agents
        (*oa)--;

        //update the number of agents inside
        (*ap)++;
        up(a_apt);

        //measure the time
        time(end);  //check out entering time
        elapsedTime = difftime (*end,*start);
        char buf[BUFSIZ];
        setbuf(stdout, buf);
        printf("Agent %d opens the apartment for inspection at time %d.\n", a, elapsedTime);
        fflush(stdout);

        //at least this agent could handle 10 tenants

        up(door);   //open the door
        up(outside);

        //wait her tenants to leave
        //down(finish);

        while(1)
        {
          if((*over) == m)
          {
            break;
          }
          if((*inside) == 0 && ((*counter) == 10 ))
          {
            break;
          }
        }

        agentLeaves(outside, a_l, a_apt, door, a);

      }




      /*********** Agent Arrive ***************/
      void agentArrives(
        struct cs1550_sem *a_l,
        struct cs1550_sem *a_apt,
        struct cs1550_sem *t_c,
        int a,
        struct cs1550_sem *outside,
        struct cs1550_sem *finish,
        struct cs1550_sem *door
      )
      {
        //update an angent has arrived
        down(outside);

        (*oa)++;    //update the number of outside agents

        //show the arriving time
        time(end);
        elapsedTime = difftime (*end,*start);

        char buf[BUFSIZ];
        setbuf(stdout, buf);
        printf("Agent %d arrives at time %d.\n", a, elapsedTime);
        fflush(stdout);

        up(outside);

        while(1)
        {
          //check if there is an agent inside
          down(a_l);    //waiting for other agent leave

          //check if there is any tenant waiting
          down(outside);
          if((*ot) == 0)
          {
            up(outside);
            down(t_c);    //waiting for tenant come
            break;
          }
          else
          {
            up(outside);
            break;        //now this agent could enter the apartment and open the door
          }
        }

        openApt(a_apt, a_l, finish, door, outside, a);

      }






      /*************  tenant  function  ***************/


      void viewApt(struct cs1550_sem *t_apt,
        struct cs1550_sem *outside,
        struct cs1550_sem *finish,
        int t)
        {
          down(outside);
          down(t_apt);

          (*inside)++;    //update the tenant inside

          (*ot)--;        //update number of tenants outside

          time(end);
          elapsedTime = difftime (*end,*start);

          char buf[BUFSIZ];
          setbuf(stdout, buf);
          printf("Tenant %d inspects the apartment at time %d.\n", t, elapsedTime);
          fflush(stdout);

          up(t_apt);
          up(outside);

          sleep(2);     //view apt for 2 seconds

          //now this tenant leave
          tenantLeaves(outside, t_apt, finish, t);
        }




        /************ Arrive *************/
        void tenantArrives(
          struct cs1550_sem *t_apt,
          struct cs1550_sem *t_c,
          int t,
          struct cs1550_sem *outside,
          struct cs1550_sem *finish,
          struct cs1550_sem *door)
          {
            down(outside);

            //update the number of outside tenants
            (*ot)++;

            //check whehter other agents are waiting for tenants
            if( (*ot) == 1 && (*oa) > 0)
            {
              int i;
              for(i=0;i<(*oa);i++)
              {
                up(t_c);  //tell all waiting agents that now a tenant has come
              }
            }

            //show the arriving time
            time(end);
            elapsedTime = difftime (*end,*start);

            char buf[BUFSIZ];
            setbuf(stdout, buf);
            printf("Tenant %d arrives at time %d.\n", t, elapsedTime);
            fflush(stdout);

            up(outside);

            while(1)
            {
              //check whether should wait agent to come
              //printf("tenant waiting\n");
              down(door);
              down(t_apt);

              (*counter)++;   //update the number of tenants this agent has
              if((*counter) == 10)
              {
                up(t_apt); //do noting and keep going
                break;
              }
              else
              {
                //raedy to enter the apartment
                up(t_apt);
                up(door);   //open the door for the next one

                break;
              }

            }

            viewApt(t_apt, outside, finish, t);

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

            char buf[BUFSIZ];
            setbuf(stdout, buf);
            printf("The apartment is now empty\n");
            fflush(stdout);

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
            struct cs1550_sem *t_apt;     //inside tenants semaphore
            struct cs1550_sem *a_apt;     //inside agents semaphore

            struct cs1550_sem *t_c;   //waiting tenant to come
            struct cs1550_sem *a_l;   //agent leave sem

            struct cs1550_sem *outside;   //outside sempahore
            struct cs1550_sem *finish;   //all done
            struct cs1550_sem *door;    //apartment door

            //intialize sempahores
            t_c = mmap(NULL,sizeof(struct cs1550_sem),
            PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
            t_c->value = 0;

            a_l = mmap(NULL,sizeof(struct cs1550_sem),
            PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
            a_l->value = 0;

            a_apt = mmap(NULL,sizeof(struct cs1550_sem),
            PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
            a_apt->value = 0;

            t_apt = mmap(NULL,sizeof(struct cs1550_sem),
            PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
            t_apt->value = 0;

            outside = mmap(NULL,sizeof(struct cs1550_sem),
            PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
            outside->value = 0;

            door = mmap(NULL,sizeof(struct cs1550_sem),
            PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
            door ->value = 0;

            finish = mmap(NULL,sizeof(struct cs1550_sem),
            PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
            finish->value = 0;

            //intialize available "tickets"
            /*
            up(a_l);
            up(a_apt);
            up(t_apt);
            up(outside);
            */

            //for allocating shared memeory
            void *ptr = mmap(NULL, (10*sizeof(int)), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
            inside = ptr;
            ot = inside + 1;

            ap = ot + 1;
            oa = ap + 1;
            counter = oa + 1;
            open = counter + 1;
            over = open + 1;
            r = over + 1;

            //intialize variables
            *inside = 0;
            *ap = 0;
            *ot = 0;
            *oa = 0;
            *counter = 0;
            *open = 0;
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

                if(pid == 0)
                {
                  //intialize random number generator
                  srand(st);

                  //generate probability
                  (*r) = (rand() % 100);  //generate random probability
                  //make decision
                  if((*r) < (100 - pt))
                  {
                    sleep(dt);
                  }

                  //tenant algorithm
                  tenantArrives(
                    t_apt,
                    t_c,
                    i,
                    outside,
                    finish,
                    door
                  );
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

                  if(pid == 0)
                  {

                    //intialize random number generator
                    srand(sa);

                    //probability field
                    (*r) = rand() % 100; //generate random probability
                    if((*r) < (100 - pa))
                    {
                      sleep(da);
                    }


                    //agent algorithm
                    agentArrives(
                      a_l,
                      a_apt,
                      t_c,
                      i,
                      outside,
                      finish,
                      door
                    );

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

                //Make sure parents wait two children
                int status_first;
                int status_second;
                waitpid(pid_first, &status_first, 0); //wait the first child
                waitpid(pid_second, &status_second, 0); //wait the second child


              }



            }


            return 0;
          }
