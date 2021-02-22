#include <time.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <errno.h>
#include <string.h>
#include "syscall.h"
#include <sys/syscall.h>

#include <linux/kernel.h>
#include <linux/mailbox_421.h>

#define __x64_sys_mbox_init_421 436
#define __x64_sys_mbox_shutdown_421 437
#define __x64_sys_mbox_open_421 438
#define __x64_sys_mbox_write_421 439
#define __x64_sys_mbox_read_421 440
#define __x64_sys_mbox_close_421 441
#define __user

#define THREAD_NO_IN_MAIN 1000000
#define NUMBER_THREADS 23
#define RANDOM_MAX 25
#define AVAILABLE_TASK 8
#define OFFSET 3
#define BINARY 2
#define RATIO NUMBER_THREADS/4

/****************************************************************************
wrapper functions
*******************************************************************************/
long mbox_init_421(void){return syscall(__x64_sys_mbox_init_421);}
long mbox_shutdown_421(void){return syscall(__x64_sys_mbox_shutdown_421);}
long mbox_open_421(char __user* name, mailbox_id_t __user *id, uint64_t size){return syscall(__x64_sys_mbox_open_421, name, id, size);}
long mbox_write_421(mailbox_id_t __user *id, uint8_t __user *data, uint64_t size, uint64_t offset){return syscall(__x64_sys_mbox_write_421, id, data, size, offset);}
long mbox_read_421(mailbox_id_t __user *id, uint8_t __user *data, uint64_t size, uint64_t offset){return syscall(__x64_sys_mbox_read_421, id, data, size, offset);}
long mbox_close_421(mailbox_id_t __user *id){return syscall(__x64_sys_mbox_close_421, id);}
/*************************************************************************************
END OF WRAPPER FUNCTION
************************************************************************************/
pthread_mutex_t lock;    //This lock is used to protect the non_empty_read variable
pthread_mutex_t lock1;   //This lock is used to protect the empty_read variable

static int val = 0;
static mailbox_id_t id3 = 0;    //This mailbox id is used only in test 4, this was 
                                //created to allow test 3 and test 4 run concurrently

static mailbox_id_t id = 0;     //This is a mailbox id keeps track of the id for the
                                //mailbox name "ibukun". used for inter thread comm

static mailbox_id_t id2 = 0;    //This mailbox id is used to keep track of the id of 
				//random mailbox names. used for inter thread comm
static mailbox_id_t id4 = 0;   //used to make test 1 and 2 run concurrently without
				//interruption

static int created_id1 = 0;     //checks if id1 has been set
static int created_id2 = 0;     //checks if id2 has been set
static int NON_EMPTY_READ = 0;  //this is used to record the amount of non empty read
				//from successful read operations

static int EMPTY_READ = 0;      //This is used to record the amount of empty read from
				//successful read operations

/***********************************************************************************
THIS STRUCTURE IS USED BY TEST 4. THIS HELPS CREATE A STRUCT FOR EACH READ AND WRITE
OPERATION IN TEST 4, WHICH IS THEN IN TURN PUT INTO A OPER ARRAY. THE OPER ARRAY IS 
USED FOR SIMULATING THE READ AND WRITE OPERATION IN TEST 4.
***********************************************************************************/
typedef struct operations{
	unsigned char * string;    //in a write operation, this takes in the data
				   //being sent to the mailbox, while in the read
				   //operation, this takes in the data received from
				   //the mailbox

	int n;                     //this takes in the size variable passed into the 
				   //read or the write operation
 
	int offset;                //this takes in the offset variable passed into
			           //the read or the write operation

	int r_or_w;                //this variable stores 1 if the operation is a r 
				   //and 0 if the operaion is a write

	int pass_fail;             //this variable stores 1 if the operation was a 
				   //pass, and 0 if the operation was a fail
}oper;  //typedef oper
/************************************************************************************
THIS IS A HELPER FUNCTION ALONG SIDE THE PRINT_FORMAT FUNCTION. IT TAKES IN NAME AND 
THEN WHEN THE STRING "mailbox" IS FOUND, IT PUTS A OPEN PARENTHESIS AFTER IT, AND THEN
ADDS THE MAILBOX_NAME AND THEN CLOSES THE PARENTHESIS. IT RETURNS THE MODIFIED STRING.
*************************************************************************************/
char * parenthesis(char *name, char * mailbox_name){
	//counts the number of variables in the initial string
	int i = 0;
	while (name[i] != '\0'){
		i++;
	}
	//allocates 100 bytes for the new_name. this is because i am 
        //certain all strings in this test use very short length strings, so 100 
	//is always enough for each time this function is called in this test
	char *new_name = (char *)malloc(100); //the modified string variable
	char *cut_out = (char *)malloc(30);   //variable used to words. useful
					     //for holding words and then comparing
					     //to see if the word is "mailbox".
					    //30 is used because i am certain there
					   //no string with a length greater than 30.
	int j = 0;     //for the while loop. the index of the initial string
	int indexer = 0;//this is equal to j until the new string and the parenthesis
		         //has been added. the index of the new string
	int count = 0;    //this is the index for the cut_out char variable
        //loop till the end of the initial string
	while (name[j] != '\0'){
                //if the index is not a space, append to our modified variable and
		//keep appending to cut_out
		if (name[j] != 32){
			cut_out[count] = name[j];
			new_name[indexer] = name[j];
			count++;
		}
		//if the index is a space, we have gotten to the end of
		//the word, do not append to cut_out anymore		
		else{
			cut_out[count] = '\0';
			//if the length of cut_out is not the same as the length of 
			//the word mailbox, which is 7, then the current string is
			//not a mailbox. the cut_out index is back to one, and the 
			//space is added to the modified string
			if (count != 7){
				count = 0;
				free(cut_out);
				cut_out = (char *)malloc(30);
				new_name[indexer] = name[j];
			}
			//if it has the same length as the string "mailbox"
			else{
				//compare string mailbox and cut_out
				int compare = strcmp(cut_out, "mailbox");
				//if they are not of the same length, clear the 
				//cut_out string and add the space to the modified
				//string
				if (compare != 0){
					count = 0;
					free(cut_out);
					cut_out = (char *)malloc(30);		
					new_name[indexer] = name[j];	 
				}
				//if they are the same string
				else{
					int k = 0;  // while loop index
					new_name[indexer] = 40;//set the current index
								//val to '('
					int cont = indexer+1;   //next index = 	
								//previous + 1;
					//loop till the end of the string to be added
					//to the modified string
					while (mailbox_name[k] != '\0'){
						new_name[cont] = mailbox_name[k];
						k++;
						cont++;
					}
					new_name[cont] = 41;//close the parenthesis
					new_name[cont+1] = 32;//add a space after the 
								//parenthesis
					indexer = cont + 1;     //set the indexer val
								//to reflect the 
								//changes
					
				}
			}
		}
	
		j++; 
		indexer++;
	
	}
	free(cut_out);//free cut_out
	//new_name to be freed in the function it is used in.
	return new_name;
}
/************************************************************************************
THIS FUNCTION IS A PRINT FUNCTION THAT HELPS PRESENT THE INFORMATION GOTTEN FROM 
EVERY OPERATION IN A SIMILAR MANNER. 
************************************************************************************/
void print_format(int my_id, char * syscall, int success, char * test, mailbox_id_t m_id){
    //If the my_id value entered is THREAD_NO_IN_MAIN, it means there is no id,
    //print empty spaces and then the operation name 
    if (my_id == THREAD_NO_IN_MAIN){
        printf("%6s", " ");
	printf("%25s", syscall);	
    }
    //else, print the thread number and then the operation name
    else{
	printf("%6d%25s", my_id, syscall);
    }
    //if the operation was a success, print success in green
    if (success == 1) 
    {
       	// Will make the text green
       	printf("\033[1;32m");
	printf("%10s", "success");
    
    }
    //if it failed, print the error message in red
    else
    {
        // Will make the text red
        printf("\033[1;31m");
	fprintf(stdout, "%27s", strerror(errno));
	
    }
  
    
    // Will reset the color
    printf("\033[0m");
	
    //count the description length
    int i = 0;
    while (test[i] != '\0'){
	i++;
    }
    int index = 0;                    //index stores the number of lines to be used
    int j = 0;
    double result = ((double)i)/20;   //this produces a double number
    if ((i % 20) != 0){               //if the length of the description / 20 is not 0
				      //then add 1 to the result. this means if there
				      //still a value after dividing by 20, it goes 
				      //on a new line
	index = result + 1;
    }
    else{
	index = result;
    } 
    int k = 0;
    int val = 0;
    //if the operaion was a success, print the description of the test. not printed
    //when operation fails because the error message takes up a lot of space and 
    //spoils the orderliness in the print
    if (success == 1){
	//while the number of lines is less than the index counter
    	while (j < index){
		//print spaces between the success or fail and the description
		printf("%2s", " ");
		//print on current line until 20 characters have been printed
		while ((k < 20)&&(((j * 20) + k) != i)){
		    printf("%c", test[(j*20)+k]);
		    k++;
		}
		//if the last index on the line is a space, print -
		if (((k - 1) != 32)&&((j * 20) + k) != i){
			printf("-");
		}
        	if ((m_id != THREAD_NO_IN_MAIN)&&(val == 0)){
        		printf("%15ld", m_id);
			val++;
		}
		//when going to the next line, print exactly under the
		//previous line		
        	k = 0;
		printf("\n%41s", " ");
        	j++;
    	}
    }
    else{
	printf("%20ld\n", m_id);
    }	
    printf("\n");
    return;
}
/**********************************************************************************
VARIABLES USED FOR TEST 1
************************************************************************************/
static mailbox_id_t id_number[RATIO];      //This stores the mailbox id numbers 
static int first = -1;			   //This is a variable that checks to see
					   //if 1/4 threads run, and that they run
					   //first
pthread_mutex_t first_mutex;               //mutex used to ensure that the first 
				           //variable is set correctly
/************************************************************************************
VARIABLES USED FOR TEST 2
************************************************************************************/
static int second = -1;                    //This is a variable that checks to see 
					   //that 1/4 threads have been run, and 
					   //that they are run second
static int next_value = 0;                 //This is a variable that tells the test
					   //whether to enter the read, write or 
					   //open section to wait for a close, or
					   //to enter the close section and free 
				           //the thread waiting.
					   //0 means wait for close, 1 is free waiter
static int wait_queue = 0;                 //wait_queue shows the number of items 
					   //waiting. almost always 1.
static int created = 0;                    //this value is set to 0 initially, to 
					   //let this test know that it has to create
					   //a mailbox, and when created it is set to
					   //0
static int operation_type[RATIO];          //array that stores the operation_type
					   //1 means open, 2 means close, 3 means 
					   //write, 4 means read
static int operation_success[RATIO];       //array that stores the operation_success
					   //1 means success, 0 means fail

pthread_mutex_t created_lock;              //mutex used to ensure created variable is
					   //set correctly
pthread_mutex_t second_mutexb;             //mutex used to ensure second variable is 
				           //set correctly
sem_t mutex;                               //semaphore used to hold open, read and 
					   //write operations until a close operation
					   //enters into this test

/*************************************************************************************
VARIABLES FOR THE THIRD TEST
*************************************************************************************/
static int close_success[RATIO];           //this is array that stores the success 
				           //of the operation
static int third = -1;                     //a variable that ensures that 1/4 threads
					   //are run, and that they are run third

static mailbox_id_t id_ne[RATIO];          //an array that stores the id of each 
					   //thread
pthread_mutex_t third_mutex;               //a mutex that ensures that third is set 
					   //correctly
/*************************************************************************************
VARIABLES FOR THE FOURTH TEST
*************************************************************************************/
static oper struct_for_rw[RATIO];          //This is a struct array used to store 
                                           //all read and write information
pthread_mutex_t fourth_mutex;              //a mutex used to ensure that the fourth
					   //variable is set correctly
pthread_mutex_t opening_mailbox;           //a mutex that ensures that opening mailbox
					   //is only called once
static int fourth = -1;                    //a variable used to ensure that 1/4
					   //threads are run, and that they are run
					   //last
static int opened_mailbox = 0;             //sets to 1 when the mailbox is open
static int mailbox_size = 0;               //the size assigned to the mailbox when
					   //when the mailbox was open
/*************************************************************************************
 * This is the code each of the threads will execute
 ************************************************************************************/
void* thread_function(void* arg)
{
    int own_moving = 0;                  //This represents the opening thread in test
					 //2
    int own_first = -1;                  //This is a variable used as personal indexes
					 //for the first test
    int own_third = -1;                  //This is a variable used as personal indexes
					 //for the third test
    int own_fourth = -1;                 //This is a variable used as personal indexes
					 //for test four
    int own_second = -1 ;                //This is a variable used as personal indexes
					 //for the second test
    char * test;                         //used to store the output 
					 //from the parenthesis fxn                 
    uint32_t number_offset = (rand() % (OFFSET + 1)); //creates a random offset
    uint32_t number_size = (rand() % (15 + 1));       //creates a random size
    mailbox_id_t inside = 0;                          //used to hold the current id
						      //to make sure it does not 
						      //change unexpectedly during
						      //a test
    long open;                             //open used to receive outputs from 
					   //mailbox open systemcall
    long close;                            //close used to receive outputs from
					   //mailbox close systemcall
    long write;                            //write used to receive outputs from
					   //mailbox write systemcall
    long read;                             //used to receive outputs from mailbox
					   //read systemcall

    uint64_t size = 20;                   
    uint64_t size2 = 30;
    char *name = (char *)malloc(15);       //used to store randomly created mbox names
    unsigned char *name2 = (unsigned char *)malloc(100);//used as data for read, write
    unsigned char *name3 = (unsigned char *)malloc(100);//used as data for read, write
    int my_id = *(int*)arg;                //thread id
    //Random number generators
    uint32_t task2 = (rand() % (BINARY));  
    uint32_t task3 = (rand() % (OFFSET));
   
    //fills name, name2, and name3 up with random letters
   for (size_t i = 0; i < 15; i++){
	uint32_t number = (rand() % (RANDOM_MAX + 1)) + 65;
	name[i] = number;	
	uint32_t number2 = (rand() % (RANDOM_MAX + 1)) + 65;
	name2[i] = number2;	
	uint32_t number3 = (rand() % (RANDOM_MAX + 1)) + 65;
	name3[i] = number3;	
    } 
    //NULL terminate all three strings
    name[15] = '\0';
    name3[15] = '\0';
    name2[15] = '\0';
/*************************************************************************************
TEST 1: TEST TO SEE IF THAT WHEN MULTIPLE THREADS ARE SENT TO A SYSTEMCALL, THEY DO
NOT CREATE A MAILBOX TWICE.
*************************************************************************************/
    pthread_mutex_lock(&first_mutex);//gain the first lock
    if (first < RATIO){
	//increment and then save the index gotten to its own variable
	first++;
        own_first = first;
	//after incrementing first, release the lock and let another increment
	pthread_mutex_unlock(&first_mutex);
	//move to the section that allows this function open mailboxes using the same
	//name
 	goto task_2;
    }
    //if here, then first == ratio, release this lock and let the next thread pass
    pthread_mutex_unlock(&first_mutex);
/*************************************************************************************
TEST 2: TEST TO SEE THAT WHEN A READ AND CLOSE, OPEN AND CLOSE, AND WRITE AND CLOSE
ARE RUN TOGETHER, THEY DO NOT CAUSE PROBLEMS FOR EACH OTHER
*************************************************************************************/
    pthread_mutex_lock(&second_mutexb);//gain the second lock
    if (second + 1 <RATIO){
        //increment and then save the index to its own variable
        second++;
	own_second = second;
	//let another come in, release lock
       	pthread_mutex_unlock(&second_mutexb);
        //gain the created lock, used to set the created variable
	pthread_mutex_lock(&created_lock);


	//if a mailbox has not been opened	
	if (created == 0){
            //set own_moving to one. this will be used as a key in task_2 to properly
	    //set the created value
            own_moving = 1;
            //move to section to open mailbox
	    goto task_2;
	    
	}
	//if the next_value is zero, we wait for another thread to come along,
	//the other thread releases this one.
        if (next_value == 0){
            wait_queue++;
            next_value = 1;
	    //unlock the created lock key
	    pthread_mutex_unlock(&created_lock);
	    //check to see that there is still another potential thread coming into
	    //this condition, or else, do not wait
	    if (second + 1 < RATIO){
                printf("waiting on a close thread, index: %d\n", own_second);
        	
	        sem_wait(&mutex);
	    }
	    if (task3 == 0){
		//move to open
                goto task_2;
            }
	    else if (task3 == 1){
		//move to write
	        goto task_6;
	    }
	    else if (task3 == 2){
		//move to read
	         goto task_7;
	    }
         

        }
	//if next_value is set to zero, move into this condition to release waiting
	//threads
        if (next_value == 1){
            //while still with the created lock, it is released after the current 
            //sequence is done
            created = 0;            
	    next_value = 0;
   	    //release the item on the queue, should be exactly one item, wait_queue is
	    //just a safety measure
	    while (wait_queue > 0){
                sem_post(&mutex);
	        wait_queue--;
	    }

		
	    printf("unlocking the read, write or open, index: %d\n", own_second);

	    goto task_3;
       }
    }
    //test 2 done, moving to test three, unlock test 2 mutex
    pthread_mutex_unlock(&second_mutexb);
/*************************************************************************************
TEST 3: TEST WHAT HAPPENS WHEN MULTIPLE DELETE RUN TOGETHER
*************************************************************************************/
    pthread_mutex_lock(&third_mutex);
    
    if (third + 1 < RATIO){
       //increment third 
       third++;
       //set own_third to be third, so that third does not change during usage by 
       //this thread
       own_third = third;
       //unlock
       pthread_mutex_unlock(&third_mutex);
       //even though this seems regulated, it is not. done this way to allow
       //random entry into the open function and random entry into delete. if 
       //delete deletes the same id twice, error
       if (created_id1 == 0)
           goto task_1;
       goto task_4;
    }
    //done with test three, unlock.
    pthread_mutex_unlock(&third_mutex);
/*****************************************************************************
TEST 4: TEST TO SEE IF PARTIAL READS OR PARTIAL WRITE HAPPENS
*****************************************************************************/	
    pthread_mutex_lock(&fourth_mutex);
    if (fourth + 1< RATIO){
        //increment fourth
	fourth++;
        //save fourth for every thread
	own_fourth = fourth;
        //unlock
	pthread_mutex_unlock(&fourth_mutex);         
        //lock used when trying to create a mailbox.
        pthread_mutex_lock(&opening_mailbox);
	//if mailbox is not open, move to section to open a mailbox for read and write
	//opening_mailbox lock is unlocked in said section
        if (opened_mailbox == 0){
               
	    goto task_1;
	}
        //if mailbox is open, unlock immediately
	pthread_mutex_unlock(&opening_mailbox);

	if (opened_mailbox == 1){
            //go to write
	    if (task2 == 0){
		goto task_5;      
            }
	    //go to read
	    else{
		goto task_8;
	    }
	}
        //if here for some unimaginable reason, just go to cleanup
        goto cleanup;
    }
    //if here, also go to cleanup
    pthread_mutex_unlock(&fourth_mutex);
    goto cleanup;

task_1:    
    test =  parenthesis("checks to see if mailbox was open successfully", name);
    //if test 4 thread
    if ((third + 1 == RATIO)&&(own_third == -1)){
        open = mbox_open_421(name, &id3, size);
	inside = id3;
    }
    //any other type of thread
    else{
        open = mbox_open_421(name, &id, size);
	inside = id;
    }

    
    if (open == 0){
	print_format(my_id, "mailbox open",1,test, inside);
	created_id1 = 1;
        //if test 3 thread
        if ((first >= RATIO)&&(own_third < RATIO) && (own_third > -1)){
            //stores the success as 5 to allow us know that it is an open call
            close_success[own_third] = 5;
	    id_ne[own_third] = inside;
	    
	   
        
        } 
        //if test 4 thread
	if ((third + 1 == RATIO)&&(own_third == -1)){
	    
	    //save the mailbox number
	    mailbox_size = size;
            //data stored in mailbox 1, but actually not useful because this is a 
	    //test for write and read. This data is never used
            struct_for_rw[own_fourth].r_or_w = 3;
            struct_for_rw[own_fourth].pass_fail = 1;
	    struct_for_rw[own_fourth].n = 0;
            struct_for_rw[own_fourth].offset = 0;
	    
	    struct_for_rw[own_fourth].string = (unsigned char *)malloc(mailbox_size * sizeof(unsigned char *));
	    //sets all values from index 0 - size in the mailbox to null, useful
            //when using the data gotten from running in testing
            for (uint64_t i = 0; i < size; i++){
	        name2[i] = '\0';
            }
            //write null to all index in the mailbox
	    write = mbox_write_421(&id3, name2 ,size, 0);
	    opened_mailbox = 1;
            //unlock opening_mailbox and let writers and readers run interruptibly
	    pthread_mutex_unlock(&opening_mailbox);
	    
 	}
    }
    else{
	print_format(my_id, "mailbox open",0, test, inside);
        //if under very rare cases, the mailbox was not open, still unlock the lock
	if ((third + 1 == RATIO)&&(own_third == -1)){
	    
	    pthread_mutex_unlock(&opening_mailbox);
	}
    }

    free(test);
    goto cleanup;
task_2: 
    
    test =  parenthesis("checks to see if mailbox was open successfully", "ibukun");
     
    if ((own_first < RATIO)&&(own_first != -1)){
        open = mbox_open_421("ADEKOYA", &id4, size2);
        inside = id4;
    }
    else{
        open = mbox_open_421("IBUKUN", &id2, size2);
        inside = id2;
    }
    if (open == 0){


	created_id2 = 1;
	print_format(my_id, "mailbox open",1,test, inside);
        //if test 1 thread
	//when inside is -1, it means 
        if ((own_first < RATIO)&&(own_first != -1)){
            id_number[own_first] = id4;
        } 
        //if test 2 thread
	if ((first >= RATIO)&&(own_second < RATIO)&&(own_second != -1)){
            operation_type[own_second] = 1;
	    operation_success[own_second] = 1;
            //if test 2 creating mailbox thread
	    if (own_moving == 1){
	        created = 1;
                own_moving = 0;
                printf("opening, index: %d\n", own_second);
	        //now that created is set, another thread can continue
	        pthread_mutex_unlock(&created_lock);
            }		
	}	    
    }
    else{
	print_format(my_id, "mailbox open",0,test, inside);
        //if test 1 thread
        if ((own_first < RATIO)&&(own_first != -1)){
            id_number[own_first] = id4;
        }  
        //if test 2 thread
	if ((first >= RATIO)&&(own_second < RATIO)&&(own_second != -1)){
            operation_type[own_second] = 1;
	    operation_success[own_second] = 0;
            //if creating mailbox thread
	    if (own_moving == 1){
	        created = 0;
                own_moving = 0;
                printf("opening failed, index: %d\n", own_second);
	        //now that created is set, another thread can continue
	        pthread_mutex_unlock(&created_lock);
            }

	}
    }
 

    
    free(test);
    goto cleanup;

task_3:
    inside = id2;
    close = mbox_close_421(&id2);
    
    if (close == 0){
	print_format(my_id, "mailbox closed",1, "checks to see if mailbox was closed successfully", inside); 
	created_id2 = 0;
        //if test 2 thread
	if ((first >= RATIO)&&(own_second < RATIO)&&(own_second != -1)){
            pthread_mutex_unlock(&created_lock);
            operation_type[own_second] = 2;
	    operation_success[own_second] = 1;

	}
    }
    
    else{
	print_format(my_id, "mailbox closed",0, "checks to see if mailbox was closed successfully", inside); 
        //if test 2 thread
	if ((first >= RATIO)&&(own_second < RATIO)&&(own_second != -1)){
            pthread_mutex_unlock(&created_lock);
            operation_type[own_second] = 2;
	    operation_success[own_second] = 0;


	}
    }

    
    goto cleanup;
task_4:
    inside = id;  //stores previous id to ensure that the id can be referenced 
		  //again when id is set to -1 by close
    close = mbox_close_421(&id);
    
    if (close == 0){
	print_format(my_id, "mailbox closed",1, "checks to see if mailbox was closed successfully", inside); 
	created_id1 = 0;
        //if test 3 test
        if ((first >= RATIO)&&(own_third < RATIO)){
            
            close_success[own_third] = 1;
	    id_ne[own_third] = inside;
        } 
    }
    
    else{
	print_format(my_id, "mailbox closed",0, "checks to see if mailbox was closed successfully", inside); 
        //if test 3 test
        if ((first >= RATIO)&&(own_third < RATIO)){
            
            close_success[own_third] = 0;
            id_ne[own_third] = inside;
        } 
    }

    
    goto cleanup;
task_5:
        //if test 4 test
        if (third + 1 == RATIO){
	    inside = id3;
	    write = mbox_write_421(&id3, name2 ,number_size, number_offset);
        }
        //any other test
        else{
	    inside = id;
	    write = mbox_write_421(&id, name2 ,number_size, number_offset);
        }
	if (write > 0){
		print_format(my_id, "mailbox Writing",1, "checks to see if mailbox was written to successfully", inside);
                //if test 4 test
 	        if (third + 1 == RATIO){
		    //store all useful info from the operation
		    struct_for_rw[own_fourth].pass_fail = 1;
	            struct_for_rw[own_fourth].n = number_size;
		    struct_for_rw[own_fourth].offset = number_offset;
		    struct_for_rw[own_fourth].r_or_w = 1;
	 	    struct_for_rw[own_fourth].string = (unsigned char *)malloc(mailbox_size * sizeof(unsigned char *));
		    memcpy(struct_for_rw[own_fourth].string, name2, number_size);
		    
		}
	
        }
        else{
		print_format(my_id, "mailbox Writing",0,"checks to see if mailbox was written to successfully", inside);
                //if test 4test
	        if (third + 1 == RATIO){
		   // also store useful info from the test
		    struct_for_rw[own_fourth].pass_fail = 0;
	            struct_for_rw[own_fourth].n = number_size;
		    struct_for_rw[own_fourth].offset = number_offset;
		    struct_for_rw[own_fourth].r_or_w = 1;
	 	    struct_for_rw[own_fourth].string = (unsigned char *)malloc(mailbox_size * sizeof(unsigned char *));
		    memcpy(struct_for_rw[own_fourth].string, name2, number_size);
		}		
        }
	
	printf("size: %d, offset: %d\n", number_size, number_offset); 
	printf("\n");
	printf("To be written to mailbox: %s\n\n", name2);
	goto cleanup;  
task_6:
	inside = id2;
	write = mbox_write_421(&id2, name3 ,number_size, number_offset);
	if (write > 0){
		print_format(my_id, "mailbox Writing",1, "checks to see if mailbox was written to successfully", inside);
                //if test 2 thread 
		if ((first >= RATIO)&&(own_second < RATIO)&&(own_second != -1)){

           	    operation_type[own_second] = 3;
	   	    operation_success[own_second] = 1;

		}	
	
        }
        else{
		print_format(my_id, "mailbox Writing",0, "checks to see if mailbox was written to successfully", inside);
                //if test 2 thread
		if ((first >= RATIO)&&(own_second < RATIO)&&(own_second != -1)){

           	 	operation_type[own_second] = 3;
	  	  	operation_success[own_second] = 0;

		}		
        }
	 
	printf("size: %d, offset: %d\n", number_size, number_offset); 
	printf("\n"); 
	printf("To be written to mailbox: %s\n\n", name3);

	goto cleanup;
task_7:
	inside = id2;
	free(name2);
	name2 = (unsigned char *)malloc((100)*sizeof(unsigned char *));
	read = mbox_read_421(&id2, name2 ,number_size, number_offset);
	
	if (read > 0){
		print_format(my_id, "mailbox Reading",1, "checks to see if mailbox was written to successfully", inside); 
		printf("Read from mailbox: ");
		int k = 0;
		int s = 0;
                //attempting to print the data gotten from the mailbox
		while ((name2[k] != '\0')&&(name2[k] < 90)&&(name2[k] > 65)){
			if (name2[k] != '\0'){
				//print if val is not null
				s++;
				printf("%c", name2[k]);
			}
			k++;
		}	
                //if something was actually printed, then it is not empty
		if (s != 0){
			pthread_mutex_lock(&lock);
			NON_EMPTY_READ++;
			pthread_mutex_unlock(&lock);
		}
                //if nothing was printed, it is empty
		else{
			pthread_mutex_lock(&lock1);
			EMPTY_READ++;
			pthread_mutex_unlock(&lock1);
		}
		printf("\n\n");
                //if test 2 thread
		if ((first >= RATIO)&&(own_second < RATIO)&&(own_second != -1)){

           	    operation_type[own_second] = 4;
	   	    operation_success[own_second] = 1;

		}
        }
        else{
		print_format(my_id, "mailbox Reading",0, "checks to see if mailbox was written to successfully", inside);	
		printf("Read from mailbox: ");	
		printf("\n\n");
                //if test 2 thread
		if ((first >= RATIO)&&(own_second < RATIO)&&(own_second != -1)){

           	    operation_type[own_second] = 4;
	   	    operation_success[own_second] = 0;

		}
        } 
	printf("size: %d, offset: %d\n", number_size, number_offset); 
	
	
	printf("\n\n");

	goto cleanup;
task_8:
        //if test 4 test
        if (third + 1 == RATIO){
	    inside = id3;
	    free(name3);
	    name3 = (unsigned char *)malloc((100)*sizeof(unsigned char *));
	    read = mbox_read_421(&id3, name3 ,number_size, number_offset);
        }
        //any other thread
        else{
	    inside = id;
	    free(name3);
	    name3 = (unsigned char *)malloc((100)*sizeof(unsigned char *));
	    read = mbox_read_421(&id, name3 ,number_size, number_offset);
        }
	if (read > 0){
		print_format(my_id, "mailbox Reading",1, "checks to see if mailbox was written to successfully", inside); 
		printf("Read from mailbox: ");
		int l = 0;
		int j = 0;
		while ((name3[l] <= 90)&&(name3[l] >= 65)){
			if (name3[l] != '\0'){
				j++;
				printf("%c", name3[l]);
			}
			l++;
		}
		if (j != 0){
			pthread_mutex_lock(&lock);
			NON_EMPTY_READ++;
			pthread_mutex_unlock(&lock);
		}
		else{
			pthread_mutex_lock(&lock1);
			EMPTY_READ++;
			pthread_mutex_unlock(&lock1);
		}		
		printf("\n\n");
                //if test 4 thread
	        if (third + 1 == RATIO){
		    struct_for_rw[own_fourth].pass_fail = 1;
	            struct_for_rw[own_fourth].n = number_size;
		    struct_for_rw[own_fourth].offset = number_offset;
		    struct_for_rw[own_fourth].r_or_w = 0;
	 	    struct_for_rw[own_fourth].string = (unsigned char *)malloc(mailbox_size * sizeof(unsigned char *));
		    memcpy(struct_for_rw[own_fourth].string, name3, number_size);
		}
		
        }
        else{
		print_format(my_id, "mailbox Reading",0, "checks to see if mailbox was written to successfully", inside);
		printf("Read from mailbox: ");
		printf("\n\n");	
	        //if test 4 thread
	        if (third + 1 == RATIO){
		    struct_for_rw[own_fourth].pass_fail = 0;
	            struct_for_rw[own_fourth].n = number_size;
		    struct_for_rw[own_fourth].offset = number_offset;
		    struct_for_rw[own_fourth].r_or_w = 0;
	 	    struct_for_rw[own_fourth].string = (unsigned char *)malloc(mailbox_size * sizeof(unsigned char *));
		    memcpy(struct_for_rw[own_fourth].string, name3, number_size);
		}	
        } 
	printf("size: %d, offset: %d\n", number_size, number_offset); 
	

	
	printf("\n\n");
cleanup:
    free(name);
    free(name2);
    free(name3);
    return NULL;
}




int main(int argc, char** argv)
{
    int test_number = 1;
    //initialize the semaphore used in test 2
    sem_init(&mutex, 0, 0);
    time_t t;
    srand(time(&t));

    pthread_t thread_ids[NUMBER_THREADS];
    int thread_args[NUMBER_THREADS];
    //print headers
    printf("%5s%10s%22s%15s%20s\n", "Thread id", "Syscall", "Success", "Description", "Mailbox_id");
    //initialize mailbox
   /* val = mbox_init_421();
    if (val == 0){
	print_format(THREAD_NO_IN_MAIN, "mailbox initialize",1, "checks to see if system was initialized", THREAD_NO_IN_MAIN); 
	
    }
    else{
	print_format(THREAD_NO_IN_MAIN, "mailbox initialize",0, "checks to see if system was initialized", THREAD_NO_IN_MAIN); 
    }*/
    // Create all the threads
    for (size_t i = 0; i < NUMBER_THREADS; i++)
    {
        thread_args[i] = i;
        // The thread starts working after this function call finishes
        pthread_create(&thread_ids[i], NULL, thread_function, &thread_args[i]);
    }
    
    // We wait for each of the threads to finish here
    for (size_t i = 0; i < NUMBER_THREADS; i++)
    {
        pthread_join(thread_ids[i], NULL);


    }



    // Will reset the color
   printf("\033[0m");
    //shutdown mailbox
    /*int shutdown = mbox_shutdown_421();
    if (shutdown == 0){
	print_format(THREAD_NO_IN_MAIN, "mailbox shutdown",1, "checks to see if system was shutdown successfully", THREAD_NO_IN_MAIN); 
    }
    else{
	print_format(THREAD_NO_IN_MAIN, "mailbox shutdown",0, "checks to see if system was shutdown successfully", THREAD_NO_IN_MAIN);
    }*/
    int exit_code = 0;  //returns 0 if passed and 1 when failed
    printf("\\\\RUNTIME STATS\\\\\n");
    printf("Number of successful empty strings read from the mailbox: %d\n", EMPTY_READ);
    printf("Number of successful non empty strings read from the mailbox: %d\n", NON_EMPTY_READ);
    printf("Number of threads running: %d\n\n", NUMBER_THREADS);

    printf("\\\\TEST MULTITHREADING\\\\\n");
    printf("%d. check to see that the same id is always returned for a mailbox that \n   already exist. Tested by sending the first 1/4 of number_threads to create\n   the same mailbox: ", test_number);
    test_number++;
/************************************************************************************
WORKING WITH THE INFORMATION GOTTEN FROM TEST 1 TO PRODUCE AN OUTPUT: TESTING IN
THIS IS DONE BY COMPARING ALL THE ID's GOTTEN FROM THIS TEST, IF THEY ARE NOT THE
SAME, THEN THE KERNEL DOES NOT RESTRICT AGAINST DOUBLE CREATION
***********************************************************************************/
    int m = 0;                        //indexer to loop through first test data
    mailbox_id_t first_id_created = 0;//once an id is created, store the value
				      //in here.
    int set = 0;                      //once the first mailbox has been created
				      //set is equal to 1, to ensure only one
				      //mailbox id is set as the first id
    int fail = 0;                     //if during the loop, the test fails, record
				      //failure by setting fail = 1
    while (m < RATIO){
        
        //if created
        if (id_number[m] != 0){
            //if first id has not been found
            if (set == 0){
                set = 1;
                first_id_created = id_number[m];
             }
             //if first id has been found, compare current index with previous index 
	     //if they are not the same, fail
             else{
		if (id_number[m] != first_id_created){
		    
		    fail = 1;
                }
             }
         }
	 m++;
    }
    if (fail == 1){
        printf("\033[1;31m");
        printf("FAIL\n\n");
        exit_code = 1;   //error code set to one to reflect failure
    
    }
    else{
       	printf("\033[1;32m");
	printf("SUCCESS\n\n");
        //error code remains 0 when successful
    }
    printf("\033[0m");
    //TEST IS HIDDEN UNTIL ARGC IS 2, THIS TEST FAILS FOR MULTIPLE PROCESSES, BUT 
    //COULD BE USEFUL FOR SINGLE PROCESSES
    if (argc == 2){
    	printf("%d. check to see that open, write, or read can work side by side with the  delete function\n   without causing an error as a result of the delete removing the the mailbox currently \n   being opened, writen to or read. Tested by first creating a new mailbox, when open, a \n   write, read or open function would be blocked next using a semaphore and when  a delete\n   function comes along, it unlocks the operation waiting to test concurrency of close \n   and any of the previous system calls. also tested with 1/4 of the available threads:\n   ", test_number);
        test_number++;    

/***********************************************************************************
WORKING WITH THE DATA MINED DURING TEST 2. THIS TEST WORKS BY CHECKING THE SEQUENCES 
OF THE RESULT. IF THE FIRST WAS AN OPEN, AND THE SECOND WAS A READ, AND THE LAST
WAS A CLOSE, IT MEANS THAT IF THE FIRST PASSED, WRITE MIGHT BE FAIL OR PASS, WHILE
CLOSE MUST BE SUCCESSFUL. IF THE OPEN FAILED, THEN THE REST MUST BE UNSUCCESSFUL. 
IF THE FIRST IS A WRITE AND THE NEXT IS CLOSE, THEN WE KNOW IF CLOSE FAILED, THE 
THE WRITE MUST ALSO FAIL. LASTLY, IF CLOSE WAS A SUCCESS, THEN WRITE MIGHT PASS OR
FAIL
NOTE: WHEN I SAY READ - I MEAN READ, WRITE, OR OPEN = R
      WHEN I SAY WRITE- I MEAN READ, WRITE, OR OPEN = W
SEQUENCE TYPE 1: READ -> CLOSE
SEQUENCE TYPE 2: OPEN -> WRITE -> CLOSE 
PASS = 1
FAIL = 0
OPEN = O
CLOSE = C
                           	LOOK-UP TABLE
                                =============
VALID SEQUENCE: 
IF SEQUENCE 2:                                      IF SEQUENCE 1:
O - 1   O - 0                                       R - 1     R - 0
R - 1,0 R - 0                                       C - 1     C - 0, 1
C - 1,  R - 0
THIS TEST MUST ALWAYS BE TRUE BECAUSE THE TESTING DATA ENSURES THAT WHILE A SEQUENCE
EXIST, NO OTHER THREAD IS ALLOWED TO INTERACT WITH THE SEQUENCE, HENCE MAKING SURE
THAT ALL THE CONDITIONS IN THIS TEST IS TRUE.
***********************************************************************************/
        int n = 0;                    //the indexer
        fail = 0;                     //if fail, sets to 1
        while (n < RATIO){

            int in = 0;
        
            //if the current and the next indexes are attainable
	    if (n + 1 < RATIO){
                    //if sequence type one
		    if (((operation_type[n] == 1)||(operation_type[n] == 3)||(operation_type[n] == 4))&&(operation_type[n+1] == 2)){

                        //variable that shows that the current index sequence is sequence
		        //1
		        in = 1;
		        //implementing a test that says if the mailbox was opened
		        //it has to be closed. 
		        if ((operation_success[n] == 1)&&(operation_success[n+1] == 0)){
			    fail = 1;
			    printf("1");
			    goto out_of_loop_2;
		         }
		         n++;
		    }
	    }
            //if the next 2 indexes are valid indexes, and the sequence is not the prev
            //sequence
	    if ((n + 2 < RATIO)&&(in == 0)){
	        //if sequence 2, use the lookup table to understand what seq 2 is
	        if ((operation_type[n] == 1)&&((operation_type[n+1] == 1)||(operation_type[n+1] == 3)||(operation_type[n+1] == 4))&&(operation_type[n+2] == 2)){
	    	    if (operation_success[n] = 0){
			     //also use seq table for explanation of the following branch
		 	    if (operation_success[n+1] == 1){
			        fail = 1;
			        printf("2");
			        goto out_of_loop_2;
			    }
			    if (operation_success[n+2] == 1){
			        fail = 1;
			        printf("3\n");
			        goto out_of_loop_2;
			    }
	    	     }

	    	     else{
			   if (operation_success[n+2] == 0){
			       fail = 1;
			       printf("4\n");
			       goto out_of_loop_2;
			    }
	    	    }

	   	    n+=2;
	        }
            }
	
	    n++;
        }
out_of_loop_2:
        if (fail == 1){
            printf("\033[1;31m");
            printf("FAIL\n\n");
            exit_code = 1;   //set error code to 1 if it fails
    
        }
        else{
       	    printf("\033[1;32m");
	    printf("SUCCESS\n\n");
       }
       printf("\033[0m");
    }
    printf("%d. Checks to see that an id is not deleted twice. This is done by sending 1/4 of the threads into \n   create and delete continously: ", test_number);
    test_number++;   
/**********************************************************************************
WORKING WITH THE OUTPUTS GOTTEN FROM TEST 3. THIS TEST CHECKS TO SEE THAT THE SAME
ID IS NOT DELETED TWICE
**********************************************************************************/   
    m = 0;                //indexer
    fail = 0;             //set to 1 when test is failed
    int j = 0;            
     
    while (m < RATIO){
	 //if the operation is a close, check to see that it does not delete
	 //the same id twice
	 if (close_success[m] == 1){
	     while (j < RATIO){
		if ((id_ne[j] == id_ne[m])&&(close_success[j] == 1)&&(j !=m)){
		    fail = 1;
		    goto out_of_test_1;
		}
		j++;
	     }
         }
	 m++;
    }

out_of_test_1:
    if (fail == 1){
        printf("\033[1;31m");
        printf("FAIL\n\n");
        exit_code = 1;    //sets exit code to 1 when failed
    
    }
    else{
       	printf("\033[1;32m");
	printf("SUCCESS\n\n");
    }
    printf("\033[0m");
    printf("%d. Checks to see that the read and write are not partial read and write. This is achieved by\n   simulating the actions of the mailbox rw_sem. since mailboxes are expected to have race\n   conditions, it is possible that even though a write went in first, it might have to wait\n   for current readers to be done, and that other readers after this reader might still be \n   allowed to go in while the writer waits. all this means is that seeing a writer before a \n   reader in the list is not enough to pass the test. the test can only be passed when all \n   readers read correct values that they are expected to: ", test_number);
/***********************************************************************************
WORKING WITH THE INFORMATION GOTTEN FROM TEST 4. THIS TEST TRIES TO SIMULATE THE 
WRITE AND READ OPERATION. IF A READ OPERATION IS FOUND IN THE STRUCT ARRAY, IT CHECKS
TO SEE THAT THE RESULT GOTTEN FROM THE MAILBOX IS ACTUALLY CORRECT. IT DOES THIS BY
FIRST CHECKING SEARCHING FOR WRITES, WHEN A WRITE IS FOUND, IT WRITES TO OUR 
REPLICA MAILBOX, AND AFTER WRITING, A REPLICA READER READS FROM THE REPLICA MAILBOX,
IF THE REPLICA READER IS EQUAL TO THE MAILBOX READER, THEN IT PASSES THE CURRENT TEST
, MOVES ON TO THE NEXT READER AND REPEATS THE SAME TEST UNTIL THE LAST READER.
NOTE: THIS TEST STARTS AT INDEX 1 BECAUSE INDEX 0 CONTAINS THE INFORMATION OF THE
      OPEN INSTRUCTION WHICH IS NOT RELEVEANT TO THIS TEST
************************************************************************************/
    int temp_cond=0;         //this is a variable which is 0 when the replica mailbox
			     //was created successfully, and 1 when the replica 
			     //mailbox failed to create using the same conditions as
			     //the original mailbox. if temp_cond signifies fail 
			     //and the origina mailbox passes, then the mailbox 
			     //failed, and vice versa.
    fail = 0;                //set to 1 if test is failed
    unsigned char * replica_mailbox = (unsigned char *)malloc(sizeof(unsigned char) * mailbox_size);               //this is the replica mailbox used for testing
    unsigned char * replica_reader = (unsigned char *)malloc(sizeof(unsigned char) * mailbox_size);              //replica reader used for testing
    //set all index of replica mailbox to the NULL terminator, to prevent comparing
    //using garbage values
    for (int i = 0; i < mailbox_size; i++){
	replica_mailbox[i] = '\0';
    }
    int error = 0;             //error detected, used when comparing the original 
			       //reader and the replica. important, because the 
			       //replica reader might not be the answer in the first
			       //iteration, so using error, allows us to know that 
			       //the current index write does not result in the same
			       //same string as the original mailbox reader
    m = 1;                     //indexer
    //if the mailbox meant to be used for testing does not print, skip the whole test,
    //because, test does not test to see if mailbox was open. goto print retry test
    //message
    if (id3 == 0){
	goto not_open;
    }
  

    //NOTE: THIS TEST USES USERSPACE IMPLEMENTATION OF MAILBOX READ AND WRITE, HENCE, TESTING CAN ONLY BE DONE WITH CORRECT STRING INPUTS OR NULL
    while (m < fourth){
         //if the current index is a read operation
	 if (struct_for_rw[m].r_or_w == 0){
	     //check to see that size is not 0, if it is zero, original mailbox
	     //should see that as an error, set temp_cond to 1 which means, operation
	     //failed in replica mailbox
	     if (struct_for_rw[m].n == 0){
		temp_cond = 1;
	     }
             //if the size + offset is greater that the size of the mailbox, temp_cond
	     //is set to one for the same reason as the one above.
	     if (struct_for_rw[m].n + struct_for_rw[m].offset > mailbox_size){
		temp_cond = 1;
	     }
             //if replica conditions say pass and the original mailbox say fail,
	     //the original mailbox is incorrect.
	     if ((temp_cond == 0)&&(struct_for_rw[m].pass_fail == 0)){
		 fail = 1;
		 goto out_of_loop_3;
	
	    }
             //if replica conditions say fail and the original mailbox say pass,
	     //the original mailbox is incorrect. failed test
	    if ((temp_cond == 1)&&(struct_for_rw[m].pass_fail == 1)){
	         fail = 1;
	         goto out_of_loop_3;
	
	    }
            
             //if the temp_cond is still 0, we want to loop through the array.
      	     if (temp_cond == 0){		
		for (int i = 1; i < fourth; i++){
		     //if a writer is found
	             if (struct_for_rw[i].r_or_w == 1){
            		 //just like we did with the readers, check to see that the 
		         //input passed into the mailbox is valid.
	     	     	 if (struct_for_rw[i].string == NULL){
			     temp_cond = 1;
	     	     	 }
	     	     	 if (struct_for_rw[i].n == 0){
			     temp_cond = 1;
	   	     	 }
	     	     	 if (struct_for_rw[i].n + struct_for_rw[i].offset > mailbox_size){
			     temp_cond = 1;
	     	     	 }
                         //if temp_cond is still 0, memcpy the string into our replica
		         //mailbox. Note, we use the exact same raw data character 
                         //array used in the original mailbox, since it is the same 
			 //data, we should expect that performing the same operation 
			 //has the original mailbox should create a mailbox with 
			 //the exact same data. Also note, the original milbox was 
			 //initialized with all its index having the null term, the
			 //same thing was done for the replica. 
                         
                         //copy n bytes form string to offset bytes of replica mailbox
	     	     	 if (temp_cond == 0){
		
			     memcpy(replica_mailbox+struct_for_rw[i].offset, struct_for_rw[i].string, struct_for_rw[i].n);
                             //if for some reason,memcpy is NULL, we send a FAIL
			     if (replica_mailbox == NULL){
                    		 fail = 1;
		    		 goto out_of_loop_3;
			     }
			}
                        //if false false was reported by the original mailbox for 
		        //the write operation, fail test
	 	        if ((temp_cond == 0)&&(struct_for_rw[i].pass_fail == 0)){
			    //test failed
			    fail = 1;
		       	    goto out_of_loop_3;
	
	    	 	}
                        //if false true was reported by the mailbox for the write 
			//operation, fail test
	     		if ((temp_cond == 1)&&(struct_for_rw[i].pass_fail == 1)){
			    //test failed
			    fail = 1;
			    goto out_of_loop_3;
	
	     		}
                        //if the replica mailbox says failed and the original
		        //also says failed, carry on, skip the else if
	     		if ((temp_cond == 1)&&(struct_for_rw[i].pass_fail == 0)){
			    //has no body, used only because of the else if
	
	     		}
                        //if we replica mailbox is not saying failed, then we can 
			//copy from replica writer into replica reader. This copy
			//is done in the same exact style as the mailbox read. it 
			//it copies n bytes from replica mailbox+offset. The n and 
			//offset is the same as the one used for the original read
			//operation
	     		else if (temp_cond == 0){
			    memcpy(replica_reader, replica_mailbox+struct_for_rw[m].offset, struct_for_rw[m].n);
			    if (replica_reader == NULL){
                    		fail = 1;
		   		goto out_of_loop_3;
			    }
                            int p = 0;       //index
                            //loop through the array gotten by the original reader
                            while (p < struct_for_rw[m].n){
		               //if index of the original reader is not NULL
				if (struct_for_rw[m].string[p] != '\0'){
		    		    if (struct_for_rw[m].string[p] != replica_reader[p]){
                                          //if the strings are not the same, report
					  //error, continue for loop till another 
					  //writer is found.
		      			  error = 1;
		    		     }
				}
				p++;
	     		   }
			   //if this is the last index for the for loop, and error is
			   //still 1, fail
	     	           if ((error == 1)&&(i + 1 == RATIO)){
				fail = 1;
				
				goto out_of_loop_3;
			   }
			   if (error == 0){
				break;
			   }
                           //set error back to zero
			   error = 0;
		
			}
		    }
                    //set temp_cond back to zero
		    temp_cond = 0;
		}
	     }
         }
         //when done with current reader, set temp to zero
         temp_cond = 0;
         //empty up replica mailbox by setting all its index to '\0'
         for (int i = 0; i < mailbox_size; i++){
	     replica_mailbox[i] = '\0';
         }
	
	 	
	 m++;
    }
out_of_loop_3:
    if (fail == 1){
        printf("\033[1;31m");
        printf("FAIL\n\n");
        exit_code = 1;       //set error code to one when error found
    
    }
    else{
       	printf("\033[1;32m");
	printf("SUCCESS\n\n");
    }
    //free all opened
not_open:    free(replica_reader);
    free(replica_mailbox);
    m = 0;
    while (m < RATIO){
        free(struct_for_rw[m].string);
	m++;
    }

    printf("\033[0m");
    

    if (id3 == 0){
        printf("MAILBOX ID3 WAS NOT OPENED: rerun the test, id3: %ld\n", id3);
	printf("Scroll up, you would see zeros and errors in front of each operation.\nThis happens because this test uses only one mailbox, if the mailbox is not open, it fails.\n\n");
    }
    printf("Note: -1 in the mailbox_id means the mailbox was deleted, while 0 means it has\n  not been created yet, although 0 is very unlikely because i have updated the \n  code to make opening mandatory to avoid excessive amounts of unopen mailbox \n  errors\n\n");
    //delete all locks used in this test
    pthread_mutex_destroy(&lock1);           
    pthread_mutex_destroy(&lock);

    pthread_mutex_destroy(&first_mutex);
    pthread_mutex_destroy(&opening_mailbox);
    pthread_mutex_destroy(&created_lock);


    pthread_mutex_destroy(&second_mutexb);
    sem_destroy(&mutex);

    pthread_mutex_destroy(&third_mutex);
    pthread_mutex_destroy(&fourth_mutex);
    return exit_code;
}