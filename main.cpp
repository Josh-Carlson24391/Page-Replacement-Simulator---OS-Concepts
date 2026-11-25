#include <iostream>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <linux/ptrace.h>
#include <sys/user.h>
#include <cstdint>
#include <vector>


struct table_entry {
	uint64_t page_num = 0;
	int num_references = 0;
	bool used = false;

};




//DONE!!
int fifo(std::vector<table_entry> &page_table, unsigned long page_num){
	//remove oldest page from from of page table (first index)
	page_table.erase(page_table.begin());
	//add new page at the last available index
	table_entry new_val;
	new_val.page_num = page_num;
	new_val.used = true;
	new_val.num_references = 1;
	page_table.push_back(new_val);
	return 1;
}

//DONE!!
int lfu(std::vector<table_entry> &page_table, uint64_t page_num){
	int min = 10000;
	int least_frequent_index = 0;
	//find entry with lowest value of num_references
	for(int i = 0; i < page_table.size(); i++){
	   if(page_table[i].num_references <= min){min = page_table[i].num_references;}

	}
	//find the index that holds the page entry with the lowest amount of references, and save its index
	for(int i = 0; i < page_table.size(); i++){
	    if(page_table[i].num_references <= min){
		least_frequent_index = i;
		break;
	    }
	}

	//erase target index
	page_table.erase(page_table.begin() + least_frequent_index);
	
	//make new table entry for new page num
	table_entry new_val;
	new_val.page_num = page_num;
	new_val.num_references = 1;
	new_val.used = true;

	//add new table entry to the page table
	page_table.push_back(new_val);
	return 1;

}

//TODO
int lru(std::vector<table_entry> &page_table, uint64_t page_num){}

//TODO
int clock(std::vector<table_entry> &page_table, uint64_t page_num){}


int main(int argc, char* argv[]) {
    //parse the command and option flags before counting page size


    //set default values of arguments
    std::string policy = "fifo";
    int command_start = -1;
    bool quiet = false;
    bool verbose = false;
    int max_page_allocations = 10;
    int instructions_traced = 0;
    int page_faults = 0;
    bool debug_mode = false;



    //parse through all args after ./vmtrace
    for(int i = 1; i < argc; i++){
	//get current argument
	std::string arg = argv[i];
	//policy option
	if(arg == "-p"){
	   if(i + 1 > argc){ std::cerr << "Error: Missing value after -p"; return 1;}
	   policy = argv[++i];
	}
	
	//num page allocations
	else if(arg == "-n"){
	   if(i + 1 > argc) {std::cerr << "Error: Missing value after -n"; return 1;}
           max_page_allocations = std::atoi(argv[++i]);
	}

	//verbose mode
	else if(arg == "-v"){
	   verbose = true;
	}
	
	//quiet mode
	else if(arg == "-q"){
	   quiet = true;
	}

	else if(arg == "-d"){
	   debug_mode = true;
	} 
	//if arg is not an option, it must be the start of the command that we want to simulate (ex: ls or cal)
	else{
	   command_start = i;
	   break;
	   
	}
	

    }

    if(command_start == -1){std::cerr << "Error: Missing Command after options - command start = %d", command_start; return 1;}
    if(max_page_allocations <= 0) {std::cerr << "Error: max page allocations must be a positive value"; return 1;}
    //declare the page table as a vector -- maybe not the best approach, change to a vector of structs
   

    //was previously a vector of uint64_t's and now it will be a vector of structs
    std::vector<table_entry> page_table;

    //declare a vector to store the list of most recently used addresses
    std::vector<uint64_t> recents;

    //setup for page num calculations
    long page_size = sysconf(_SC_PAGESIZE);
    //assume 4kb if sysconf fails
    if (page_size <= 0) {
        page_size = 4096;
    }

    unsigned int page_shift = 0;
    //calculate the number of bits to shift by doubling unsigned long of 1 until it reaches the page size
    while ((1UL << page_shift) < (unsigned long)(page_size)) {
        ++page_shift;
    }





    pid_t ch_pid = fork();
    if (ch_pid == 0) {
        // Tell parent to trace this process
        ptrace((__ptrace_request)(PTRACE_TRACEME), getpid(), nullptr, nullptr);
        raise(SIGSTOP); // stop myself so the parent can start tracing
        //char *args[] = {(char *) "./short-loop",  nullptr};
        execvp(argv[command_start], &argv[command_start]);
    } else {
	//new struct to hold return value of assembly version
        int status = 0;

        // wait for the child's initial raise(SIGSTOP) after PTRACE_TRACEME
        errno = 0;
        if (wait(&status) == -1) {
            perror("After first wait in parent");
            return 1;
        }
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            return 0;
        }

        // Step once before the loop so the first wait() inside the loop
        // captures a single-step stop at an executed instruction
        errno = 0;
        if (ptrace((__ptrace_request)(PTRACE_SINGLESTEP), ch_pid, nullptr, nullptr) == -1) {
            perror("After initial PTRACE_SINGLESTEP");
            return 1;
        }
        while(true) {
	    errno = 0;
            pid_t who = wait(&status);
            
	    //exit loop when child is done executing instructions
            if (who == -1){
	      if(errno != ECHILD){
		   perror("After parent wait in loop");

		}
      	       break;
	    }


	    if(WIFEXITED(status) || WIFSIGNALED(status)){break;}

            // Get information about the current assembly instruction
	    struct user_regs_struct regs;
    	    errno = 0;
            if( ptrace((__ptrace_request) PTRACE_GETREGS, ch_pid, nullptr, &regs) == -1){
		perror("After GetRegs");
	    	break;
	    }
	    //incriment the number of instructions traced
	    instructions_traced += 1;

	    //calculate the page number for the current instrucions address
	    uint64_t mem_addr = regs.rip;
	    uint64_t page_num = mem_addr >> page_shift;
		

	   //CHECKING FOR PAGE FAULTS

	
	    //default to assuming it is a page fault unless page is found in page table
	    bool page_fault = true;
	    //check if the current instructions page is in the page table already
	    for(int i = 0; i < page_table.size(); i++){
		//should be updated to work with new struct
		if(page_table[i].page_num == page_num){
		   page_fault = false;
		   //incriment number of references
		   page_table[i].num_references += 1;
		   //make sure it is still saying it is being used
		   page_table[i].used = true;
		   if(debug_mode){std::printf("Page is already in the page table!\n");}
		   break;
		}

	    }

	    //HANDLE PAGE FAULTS WHEN THEY HAPPEN:

	    //if it is not, then add it to the page table if there is room: if there is a page fault and the num of pages in pt is less than max
	    if(page_fault && page_table.size() < max_page_allocations){
		//add the page to the page table
		if(debug_mode){std::printf("Page fault and room in the table\n");}
		//make new struct to hold the new page table entry
		table_entry new_val;
		new_val.page_num = page_num;
		new_val.num_references = 1;
		new_val.used == true;
		page_table.push_back(new_val);
		page_faults += 1;

	    }
	    //if there is not room in the page table, call whichever page replacement algorithm was selected to kick a page from the page table, then add new page
	    else if(page_fault && page_table.size() >= max_page_allocations){
		//call page replacement algorithm to handle swapping the pages
		int return_val = 0;
		if(debug_mode){std::printf("Page fault and no room in page table - calling page replacement alg, which is %s\n", policy.c_str());}
		if(policy == "fifo"){
		      //call fifo on page table with page num
		      return_val = fifo(page_table, page_num);
		}
		else if(policy == "lfu"){
		      return_val = lfu(page_table, page_num);
		}

		page_faults += 1;
	    }
	    //prints whole page table for each step
	    if(debug_mode){
		std::printf("START OF PAGE TABLE\n");
		for(int i = 0; i < page_table.size(); i++){
		   std::printf("PT Index %d is page %llx with frequency of %d\n", i, (unsigned long long) page_table[i].page_num, page_table[i].num_references);
		}
		std::printf("END OF PAGE TABLE -- Page Faults = %d, Current page = %llx Current Address = %llx\n", page_faults, (unsigned long long) page_num, (unsigned long long) mem_addr);
	    }
	    //if verbose mode is active, print every memory address accessed along with the page number
	    if(verbose){std::printf("[%d/%d]: At: mem addr: %llx\tPage: %llx\n",instructions_traced, page_faults,(unsigned long long) mem_addr,(unsigned long long) page_num);}
	    //if quiet mode, skip printing all together and just print the total number of memory accesses, along with the number of page faults
	    if(quiet){
	       std::printf("Instructions traced = %d\t page faults = %d\n", instructions_traced, page_faults);
	    }
	    //if not quiet, show every memory access that accesses a new page that is not in the page table (basically show the ones that cause a page fault)
	    if(!quiet && page_fault){std::printf("Page fault! @ memory address: %llx from page %llx\n", (unsigned long long) mem_addr, (unsigned long long) page_num);}

	    //step to the next instruction after printing the memory address of the previous instruction
	    errno = 0;
            if (ptrace((__ptrace_request)PTRACE_SINGLESTEP, ch_pid, nullptr, nullptr) == -1) {
                perror("After PTRACE_SINGLESTEP");
                break;
            }


        }
        std::cout << "From Parent" << std::endl;
    }
    return 0;
}
