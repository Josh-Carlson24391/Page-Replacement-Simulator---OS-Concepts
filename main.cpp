#include <iostream>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <linux/ptrace.h>
#include <sys/user.h>
#include <cstdint>
int main(int argc, char* argv[]) {
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
        char *args[] = {(char *) "./short-loop",  nullptr};
        execvp(args[0], args);
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
	    //TODO - MAYBE DONE
            if( ptrace((__ptrace_request) PTRACE_GETREGS, ch_pid, nullptr, &regs) == -1){
		perror("After GetRegs");
	    	break;
	    }

	   //calculate the page number for the current instrucions address
	   uint64_t mem_addr = regs.rip;
	   uint64_t page_num = mem_addr >> page_shift;


            std::printf("Instruction addr: %llx\tPage Num: %llx\n",(unsigned long long) mem_addr,(unsigned long long) page_num);
	    //step to the next instruction after printing the memory address of the previous instruction
	    errno = 0;
            if (ptrace((__ptrace_request)PTRACE_SINGLESTEP, ch_pid, nullptr, nullptr) == -1) {
                perror("After PTRACE_SINGLESTEP");
                break;
            }
	    //TODO

	    //Modify to continue tracing assembly instructions instead

           
        }
        std::cout << "From Parent" << std::endl;
    }
    return 0;
}
