#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>

int main(int argc, char *argv[])
{
    if (argc != 3){
        printf("Format: ./a.out <file_name> <timer_input>\n");
        exit(1);
    }

    // get values from arguments
    char *filename = argv[1];
    int timer = atoi(argv[2]);

    FILE *file;
    file = fopen(filename, "r"); // open file

    // if file given doesn't exist, end program.
    if (!file){
        printf("\nERROR: %s not found!\n", filename);
        exit(1);
    }
    // create pipes
    int cpuToMem[2];
    int memToCpu[2];
    
    //if pipe creations fail, end program.
    int pipeResult_cpuToMem = pipe(cpuToMem);
    int pipeResult_memToCpu = pipe(memToCpu);
    if (pipeResult_cpuToMem < 0 || pipeResult_memToCpu < 0)
    {
        exit(1);
    }

    int pid = fork();

    // Invalid process
    if (pid < 0)
    {
        printf("ERROR: Invalid process/fork!\n");
        exit(1);
    }
    else if (pid == 0)
    {
        //memory array
        int mem[2000];

        //variables for reading file in by lines
        char *f_line;
        size_t f_len = 0;
        ssize_t f_read;

        int addr_read = 0; //variable that will hold the memory location that is received when parsed in
        while ((f_read = getline(&f_line, &f_len, file)) > 0) //loop while the file line read in is a valid line
        {
            int curr = 0; //indexer
            char input[10] = {0}; //used to store value of what to store into memory

            // If it's a jump to address, save to input[0] and move past '.'
            if (f_line[0] == '.')
            {
                input[0] = '.';
                curr++;
            }
            //reads numbers in line to current index within input
            //allows ignoring of comments and non digit characters
            for(;isdigit(f_line[curr]); curr++){
                input[curr] = f_line[curr];
            }

            // If the input line needs to be stored in memory
            if (input[0] == '.' || isdigit(input[0])){

                //if it is a jump to memory line, get address to jump to by starting reading in new 
                if (input[0] == '.'){
                    for (int i = 0; i < sizeof(input); i++)
                        input[i] = input[i + 1]; //getting the digits after '.'
                    addr_read = atoi(input); //storing address to jump to into addr_read
                }
                else{
                    mem[addr_read] = atoi(input); //stores input 
                    addr_read++;
                }
            }
        }
        fclose(file);
        free(f_line);

        int cmd, val, addr;
        while (true)
        {

            read(cpuToMem[0], &cmd, sizeof(cmd));

            // read command
            if (cmd != -1){
                int readVal = mem[cmd];
                write(memToCpu[1], &readVal, sizeof(readVal)); // write value from given address
            }

            // write command
            else{
                read(cpuToMem[0], &addr, sizeof(addr)); // get address to write to
                read(cpuToMem[0], &val, sizeof(val));  // get value that we are writing to given address
                mem[addr] = val;                        // write value to given address
            }
        }
    }
    else
    {
        //register & other variables
        int pc, ir, ac, x, y, operand = 0;
        int sp = 1000;
        int spTemp;
        int write_cmd = -1;
        
        srand(time(0)); //making sure that random values are random *seeding*

        //interrupt variables
        bool user_mode = true;
        int intr_flag = 0; //0 for no interrupt, 1 timer interrupt, 2 for system call
        bool timer_flag = false;
        int timer_count = 0;
        
        while (true)
        {   
            //if the flag was for a timer interrupt and we are not currently in an interrupt
            //decrement then write approach for stack
            if (timer_flag && intr_flag == 0)
            {   
                user_mode = false;
                timer_flag = false; //reset timer flag
                intr_flag = 1; //set current flag to a timer interrupt flag (1)

                spTemp = sp;
                sp = 2000;

                sp--;
                pc++;
                write(cpuToMem[1], &write_cmd, sizeof(write_cmd));
                write(cpuToMem[1], &sp, sizeof(sp));
                write(cpuToMem[1], &pc, sizeof(pc));

                sp--;
                write(cpuToMem[1], &write_cmd, sizeof(write_cmd));
                write(cpuToMem[1], &sp, sizeof(sp));
                write(cpuToMem[1], &spTemp, sizeof(spTemp));

                pc = 1000;
            }
            
            //reading current instruction
            write(cpuToMem[1], &pc, sizeof(pc));
            read(memToCpu[0], &ir, sizeof(ir));

            //switch case that runs through all instructions for IR
            switch (ir)
            {
                case 1: //Load value into AC
                    pc++;
                    write(cpuToMem[1], &pc, sizeof(pc));
                    read(memToCpu[0], &operand, sizeof(operand));
                    ac = operand;
                    break;
                case 2: //Load value from address into AC
                    pc++;
                    write(cpuToMem[1], &pc, sizeof(pc));
                    read(memToCpu[0], &operand, sizeof(operand));
                    if (user_mode == true && operand > 999){
                        printf("Memory Violation: Accessing system address at %d\n", operand);
                    }
                    else{
                        write(cpuToMem[1], &operand, sizeof(operand));
                        read(memToCpu[0], &operand, sizeof(operand));
                        ac = operand;
                    }
                    break;
                case 3: //Load value from address of given address into AC
                    pc++;
                    write(cpuToMem[1], &pc, sizeof(pc)); 
                    read(memToCpu[0], &operand, sizeof(operand)); //receiving the given address to get the next address
                    write(cpuToMem[1], &operand, sizeof(operand));
                    read(memToCpu[0], &operand, sizeof(operand));//receiving the value at mem[addr from given addr]

                    if (user_mode == true && operand > 999)
                    {
                        printf("Memory Violation: Accessing system address at %d\n", operand);
                    }
                    else
                    {
                        write(cpuToMem[1], &operand, sizeof(operand));
                        read(memToCpu[0], &operand, sizeof(operand));
                        ac = operand; //AC = mem[addr from given addr]
                    }
                    break;
                case 4: //Load value from address+X into AC
                    pc++;
                    write(cpuToMem[1], &pc, sizeof(pc));
                    read(memToCpu[0], &operand, sizeof(operand)); //receiving given address

                    operand += x; //address+X

                    write(cpuToMem[1], &operand, sizeof(operand));
                    read(memToCpu[0], &operand, sizeof(operand));
                    ac = operand; //AC = mem[address+X]
                    break;
                case 5: //Load value from address+Y into AC
                    pc++;
                    write(cpuToMem[1], &pc, sizeof(pc));
                    read(memToCpu[0], &operand, sizeof(operand));

                    operand += y; //address+Y

                    write(cpuToMem[1], &operand, sizeof(operand));
                    read(memToCpu[0], &operand, sizeof(operand));
                    ac = operand; //AC = mem[address+Y]
                    break;
                case 6: //Load from SP+X into AC
                    operand = sp + x;
                    write(cpuToMem[1], &operand, sizeof(operand));
                    read(memToCpu[0], &operand, sizeof(operand)); //receiving value of stored from SP+X
                    ac = operand; //AC = mem[SP+X]
                    break;
                case 7: //Store AC value into given ADDRESS
                    pc++;
                    write(cpuToMem[1], &pc, sizeof(pc)); 
                    read(memToCpu[0], &operand, sizeof(operand)); //receiving address to store to
                    write(cpuToMem[1], &write_cmd, sizeof(write_cmd));
                    write(cpuToMem[1], &operand, sizeof(operand));
                    write(cpuToMem[1], &ac, sizeof(ac)); //writing value of AC to address
                    break;
                case 8: //Load AC with random number [1,100]
                    ac = rand() % 100 + 1;
                    break;
                case 9: //Display AC value as either int or char with given 1 or 2 respectively
                    pc++;
                    write(cpuToMem[1], &pc, sizeof(pc));
                    read(memToCpu[0], &operand, sizeof(operand)); //receiving type to display as
                    if (operand == 1)
                    {
                        printf("%i", ac);
                    }
                    else if (operand == 2)
                    {
                        printf("%c", ac);
                    }
                    break;
                case 10: //Add X to AC
                    ac += x;
                    break;
                case 11: //Add Y to AC
                    ac += y;
                    break;
                case 12: //Subtract X from AC
                    ac -= x;
                    break;
                case 13: //Subtract Y from AC
                    ac -= y;
                    break;
                case 14: //Load X from AC
                    x = ac;
                    break;
                case 15: //Load AC from X
                    ac = x;
                    break;
                case 16: //Load Y from AC
                    y = ac;
                    break;
                case 17: //Load AC from Y
                    ac = y;
                    break;
                case 18: //Load SP from AC
                    sp = ac;
                    break;
                case 19: //Load AC from SP
                    ac = sp;
                    break;
                case 20: //Jump to given address
                    pc++; //moving up 1 to get address
                    write(cpuToMem[1], &pc, sizeof(pc));
                    read(memToCpu[0], &operand, sizeof(operand)); //receiving address into operand
                    pc = operand - 1; //'jumping' by setting PC to address-1
                    break;
                case 21: //Jump to given address if AC is 0
                    pc++;
                    write(cpuToMem[1], &pc, sizeof(pc));
                    read(memToCpu[0], &operand, sizeof(operand)); //receiving address into operand
                    if (ac == 0){
                        pc = operand - 1; //'jumping' by setting PC to address-1
                    }
                    break;
                case 22: //Jump to given address if AC is not 0
                    pc++;
                    write(cpuToMem[1], &pc, sizeof(pc));
                    read(memToCpu[0], &operand, sizeof(operand)); //receiving address into operand
                    if (ac != 0)
                    {
                        pc = operand - 1; //'jumping' by setting PC to address-1
                    }
                    break;
                case 23: //Jump to given address while pushing current address to stack *allows returning back*
                    pc++;
                    write(cpuToMem[1], &pc, sizeof(pc));
                    read(memToCpu[0], &operand, sizeof(operand));

                    //pushing the address to stack to allow for returning
                    sp--;
                    pc++;
                    write(cpuToMem[1], &write_cmd, sizeof(write_cmd));
                    write(cpuToMem[1], &sp, sizeof(sp));
                    write(cpuToMem[1], &pc, sizeof(pc));
                    
                    //jump to given address
                    pc = operand - 1;
                    break;
                case 24: //Jump back to return address from stack
                    write(cpuToMem[1], &sp, sizeof(sp)); //getting stack address to pop back
                    read(memToCpu[0], &pc, sizeof(pc));
                    pc--; //returning back to return address that was popped from stack
                    sp++; //popping stack value ==> move back stack "back" 1
                    break;
                case 25: //Increment X by 1
                    x++;
                    break;
                case 26: //Decrement X by 1
                    x--;
                    break;
                case 27: //Push AC value onto stack
                    sp--; //moving stack pointer foward 1
                    write(cpuToMem[1], &write_cmd, sizeof(write_cmd));
                    write(cpuToMem[1], &sp, sizeof(sp)); //writing current stack pointer address to sp
                    write(cpuToMem[1], &ac, sizeof(ac)); //value being stored
                    break;
                case 28:  //Pop AC value from stack
                    write(cpuToMem[1], &sp, sizeof(sp)); //get
                    read(memToCpu[0], &ac, sizeof(ac));
                    sp++;
                    break;
                case 29: //Perform system call
                    //if we are not in interrupt, continue. otherwise it 'disables' interrupts.
                    if (intr_flag == 0){
                        intr_flag = 2; //flag number that identifies as 'system call interrupt'
                        user_mode = false; //enter kernel mode
                        spTemp = sp;
                        sp = 2000;

                        write(cpuToMem[1], &write_cmd, sizeof(write_cmd));
                        write(cpuToMem[1], &sp, sizeof(sp));
                        write(cpuToMem[1], &pc, sizeof(pc));
                        sp--;
                        
                        write(cpuToMem[1], &write_cmd, sizeof(write_cmd));
                        write(cpuToMem[1], &sp, sizeof(sp));
                        write(cpuToMem[1], &spTemp, sizeof(spTemp));

                        pc = 1499; //causing int instruction to be done at 1499 (which is 1500)
                        break;
                    }
                    break;
                case 30:
                    write(cpuToMem[1], &sp, sizeof(sp));
                    read(memToCpu[0], &spTemp, sizeof(spTemp));
                    sp++;

                    write(cpuToMem[1], &sp, sizeof(sp));
                    read(memToCpu[0], &pc, sizeof(pc));
                    pc -= 2; //returns PC back to original spot (program returns from sys call)
                    sp++; //move stack pointer back

                    //if we returned from a timer interrupt, set flag back to false
                    if (intr_flag == 1){
                        timer_flag = false;
                    }
                    intr_flag = 0;
                    sp = spTemp;
                    user_mode = true; //return back to user mode
                    break;
                case 50:
                    exit(0);
                    break;
                default:
                    printf("\nError: %d is not a command.\n", ir);
            }

            timer_count++;
            pc++;

            //once we reach value for timer interrupt, flag for timer interrupt.
            if (timer_count % timer == 0){
                timer_flag = true;
            }
        }
    }
    exit(0);
}