#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "compiler.h"

// cur is at offset 0
// base is at offset 8
// left_limit is at offset 16
// right_limit is at offset 20
// left_init is at offset 24
// right_init is at offset 28

static const char *assembly_start_template = 
"section .text\n"
"    global _start\n"
"    extern ReadTape\n"
"    extern DumpTape\n"
"    extern exit_turing\n"
"_start:\n"
"    ;initial boiler plate\n"
"    ; Ensure there are at least 2 arguments (argc >= 3)\n"
"    mov rax, [rsp]         ; Load argc\n"
"    cmp rax, 3\n"
"    jl _exit_error          ; If less than 3 arguments, exit\n"
"\n"
"    ; Load the address of the first command-line argument (input file)\n"
"    mov rsi, [rsp+16]      ; First argument (input file)\n"
"    sub rsp, 32\n"
"    lea rdi, [rsp]         ; Return struct address\n"
"\n"
"    call ReadTape\n"
"\n"
"    ;!!!ACTUAL CODE: done boiler plate\n";
//"%s\n"  // Placeholder for actual code

static const char *assembly_end_template = 
"    ;DONE:output boilerplate and exit;\n\n"

"    mov rsi, [rsp+32+24]   ; Second argument (output file) now shifted by 32\n"
"    lea rdi, [rsp]         ; Same struct pointer\n"
"\n"
"    call DumpTape\n"
"\n"
"    ; Exit the program\n"
"    mov rdi, 0\n"
"    call exit_turing\n"
"\n"
"exit_out_of_tape:\n"
"    mov rdi, 2\n"
"    call exit_turing\n"
"\n"
"_exit_error:\n"
"    mov rdi, 3\n"
"    call exit_turing\n";

int assemble_and_link(const char* filename,const char* dirname, printer_func_t codefunc,void* data) {
    // Step 1: Generate the assembly code
    char* working_name=null_check(malloc(strlen(filename)+5));
    strcpy(working_name,filename);
    strcat(working_name,".asm");

    FILE *file = fopen(working_name, "w");
    if (file == NULL) {
        free(working_name);
        perror("Failed to open file");
        return 1;
    }

    // Custom code to be inserted into the placeholder
    //const char *custom_code = "    ; Inserted custom code";

    fprintf(file,"%s",assembly_start_template);
    codefunc(file,data);
    fprintf(file,"%s",assembly_end_template);

    if (ferror(file)) {
        perror("Error occurred during Assembly generation\n");
        exit(1);
    }

    fclose(file);
    printf("Assembly code generated successfully.\n");



    const char* cnasm="nasm -g -f elf64 -o %s.o %s";
    char* nasm=null_check(malloc(strlen(cnasm)+strlen(working_name)+strlen(filename)));
    sprintf(nasm,cnasm,filename,working_name);

    // Step 2: Assemble the generated assembly code
    int result = system(nasm);
    if (result != 0) {
        free(working_name);
        free(nasm);
        fprintf(stderr, "Failed to assemble the code.\n");
        return 1;
    }
    printf("Assembly completed successfully.\n");

    // Step 3: Link the object file with io.o to create the final executable
    const char* cld="ld -o %s.out %s.o %s/io.o -lc -dynamic-linker /lib64/ld-linux-x86-64.so.2";
    char* ld=null_check(malloc(strlen(cld)+2*strlen(filename)));
    sprintf(ld,cld,filename,filename,dirname);
    
    result = system(ld);
    free(working_name);
    free(nasm);
    free(ld);

    if (result != 0) {
        fprintf(stderr, "Failed to link the object file.\n");
        return 1;
    }
    printf("Linking completed successfully.\n");

    return 0;
}

const char* spaces="    ";

//does not handle hault properly yet. other issues with register size specifications on the ops
void O0_IR_to_ASM(FILE *file,TuringIR ir){
    
    //we are using rax rcx rdi 
    //inside of some of the loops. this means that they are NOT ALOWED to be chosen

    const char* address_register="r14";//this is also used in the assembly_end_template so dont mess with it.
    const char* bit_register="r15d";
    const char* right_limit_register="r8";
    const char* left_limit_register="r9";
    const char* right_init_register="r10";
    const char* left_init_register="r11";

    const char* small_right_init_register="r10d";
    const char* small_left_init_register="r11d";

    const int move_size=4;
    const int extend_size=256*4;//same as the interpeter //HAS to be a multiple of 4


    fprintf(file,"%smov %s,qword [rsp]\n",spaces,address_register);//load cur

    char* tmp = "rcx";
    char* tmp2_short = "eax";
    char* tmp2 = "rax";

    //the left side of these isnt inilized properly...

    fprintf(file, "%sxor %s, %s\n", spaces, tmp2,tmp2);
    // Load base address into tmp
    fprintf(file, "%smov %s, qword [rsp+8]\n", spaces, tmp);

    // right_limit
    fprintf(file, "%smovsxd rax, dword [rsp+20]\n", spaces); // Load and sign-extend the value into rax (a 64-bit register)
    fprintf(file, "%slea %s, [%s + 4*rax]\n", spaces, right_limit_register, tmp);

    // left_limit
    fprintf(file, "%smovsxd rax, dword [rsp+16]\n", spaces); // Load and sign-extend the value into rax
    fprintf(file, "%slea %s, [%s + 4*rax]\n", spaces, left_limit_register, tmp);

    // right_init
    fprintf(file, "%smovsxd rax, dword [rsp+24]\n", spaces); // Load and sign-extend the value into rax
    fprintf(file, "%slea %s, [%s + 4*rax]\n", spaces, left_init_register, tmp);

    // left_init
    fprintf(file, "%smovsxd rax, dword [rsp+28]\n", spaces); // Load and sign-extend the value into rax
    fprintf(file, "%slea %s, [%s + 4*rax]\n", spaces, right_init_register, tmp);




    fprintf(file,"%scld\n",spaces);

    for(int i=0;i<ir.len;i++){
        fprintf(file,"L%d:;%s\n",i,ir.names[i]);
        
        //brench based on bit
        fprintf(file,"%smov %s,dword [%s]\n",spaces,bit_register,address_register);
        fprintf(file,"%stest %s, %s\n",spaces,bit_register,bit_register);
        fprintf(file,"%sjnz L%d_1\n",spaces,i);    

        for(int k=0;k<2;k++){
            fprintf(file,"L%d_%d:;%s[%d]\n",i,k,ir.names[i],k);
            fprintf(file,"%smov [%s],dword %d \n",spaces,address_register,ir.states[i].trans[k].write);

            //move
            switch(ir.states[i].trans[k].move){
                case Stay:
                    break;
                case Right:
                    fprintf(file, "%slea %s, [%s+%d] \n", spaces, address_register, address_register,move_size);
                    
                    fprintf(file, "%scmp %s, %s;bounds check \n", spaces, address_register,right_init_register);
                    fprintf(file, "%sjbe Done_L%d_%d\n", spaces,i,k);

                    //!!! these 2 lines are single handedly responsible for over a 100x preformance drop
                    fprintf(file, "%scmp %s, %s;check out of tape\n", spaces, address_register,right_limit_register);
                    fprintf(file, "%sja exit_out_of_tape\n", spaces);
                    //!!!!

                    tmp = "rcx";//using this to avoid a move

                    fprintf(file, "%slea %s,[%s+%d]\n",spaces,tmp,right_init_register,extend_size);
                    
                    //tmp = min(tmp right_limit)
                    fprintf(file, "%scmp %s,%s\n",spaces,tmp,right_limit_register);
                    fprintf(file, "%sjbe Extend_L%d_%d\n", spaces,i,k);

                    fprintf(file, "%smov %s,%s\n",spaces,tmp,right_limit_register);

                    fprintf(file,"Extend_L%d_%d:\n",i,k);

                    //memset 0 
                    // Set rdi to the starting address
                    fprintf(file, "%smov rdi, %s;setting up for stosq \n", spaces, address_register);
                    fprintf(file, "%smov %s, %s\n", spaces, right_init_register, tmp); // Update the right_init_register to the new end

                    // Calculate the number of 32-bit elements to zero out
                    fprintf(file, "%ssub %s, rdi\n", spaces,tmp);
                    fprintf(file, "%sshr %s, 2;bad more effishent to do quads\n", spaces,tmp); // Divide by 8
                    fprintf(file, "%ssub %s, 1\n", spaces,tmp);//????? needed but idk why


                    // Zero out the memory
                    //MAJO BUG IN THE ORDER
                    //no need tmp is rcx fprintf(file, "%smov rcx, rax\n", spaces); // Number of 32-bit elements to store
                    fprintf(file, "%sxor rax, rax\n", spaces); // Zero value to store
                    fprintf(file, "%srep stosd\n", spaces);
                    

                   
                    
                    //when we improve the speed fprintf(file, "%smov [%s],dword 0;maybe there is a 4byte remainder\n", spaces,right_init_register);


                    fprintf(file,"Done_L%d_%d:\n",i,k);
                    break;
                case Left:
                    

                    fprintf(file, "%slea %s, [%s-%d] \n", spaces, address_register, address_register,move_size);
                    
                    fprintf(file, "%scmp %s, %s;bounds check \n", spaces, address_register,left_init_register);
                    fprintf(file, "%sjae Done_L%d_%d\n", spaces,i,k);

                    //!!! these 2 lines are single handedly responsible for over a 100x preformance drop
                    fprintf(file, "%scmp %s, %s;check out of tape\n", spaces, address_register,left_limit_register);
                    fprintf(file, "%sjb exit_out_of_tape\n", spaces);
                    //!!!!

                    tmp = "rax";//rcx is used down

                    fprintf(file, "%smov rcx, %s\n", spaces, left_init_register);
                    fprintf(file, "%slea %s,[%s-%d]\n",spaces,tmp,left_init_register,extend_size);
                    
                    //tmp = max(tmp right_limit)
                    fprintf(file, "%scmp %s,%s\n",spaces,tmp,left_limit_register);
                    fprintf(file, "%sjae Extend_L%d_%d\n", spaces,i,k);

                    fprintf(file, "%smov %s,%s\n",spaces,tmp,left_limit_register);

                    fprintf(file,"Extend_L%d_%d:\n",i,k);

                    //memset 0 
                    // Set rdi to the starting address 
                    // fprintf(file, "%smov rdi, %s ;setting up for stosq\n", spaces, address_register);
                    
                    
                    fprintf(file, "%smov %s, %s\n", spaces, left_init_register, tmp); // Update the left_init_register to the new end
                    fprintf(file, "%smov rdi, %s ;setting up for stosq\n", spaces, left_init_register);
                    
                    
                    // Calculate the number of 32-bit elements to zero out
                    fprintf(file, "%ssub rcx, %s\n",spaces,tmp);
                    
                    fprintf(file, "%sshr rcx, 2;bad more effishent to do quads\n", spaces); // Divide by 8
                    //fprintf(file, "%ssub rcx, 1\n", spaces);//????? needed but idk why

                    // Zero out the memory
                    //no need we did earlier with rcx fprintf(file, "%smov rcx, rax\n", spaces); // Number of 64-bit elements to store
                    fprintf(file, "%sxor rax, rax\n", spaces); // Zero value to store
                    
                    //fprintf(file, "%sstd\n", spaces);
                    fprintf(file, "%srep stosd\n", spaces);
                    //fprintf(file,"%scld\n",spaces);

                   
                    
                    // //when we improve the speed fprintf(file, "%smov [%s],dword 0;maybe there is a 4byte remainder\n", spaces,left_init_register);


                    fprintf(file,"Done_L%d_%d:\n",i,k);
                    break;
            }

            //next
            if(ir.states[i].trans[k].nextState!=-1){
                fprintf(file,"%sjmp L%d\n",spaces,ir.states[i].trans[k].nextState);
            }
            else{
                fprintf(file,"%sjmp exit_good\n",spaces);
            }
        }
    }

    //write to the struct
    fprintf(file,"exit_good:\n");
    fprintf(file,"%smov [rsp],qword %s\n",spaces,address_register);

    tmp = "rcx";
    tmp2_short="eax";
    tmp2="rax";
    // Load base address into tmp
    fprintf(file, "%smov %s, qword [rsp+8]\n", spaces, tmp);

    //not handeling the sign right

    //right init
    fprintf(file,"%ssub %s,%s\n",spaces,right_init_register,tmp);
    fprintf(file, "%ssar %s, 2;move to int indexing like c\n", spaces,right_init_register);
    fprintf(file, "%smov %s, dword %s;sign handeling \n", spaces, tmp2_short,small_right_init_register);
    fprintf(file, "%smov [rsp+28], dword %s \n", spaces, tmp2_short);

    fprintf(file,"%ssub %s,%s\n",spaces,left_init_register,tmp);
    fprintf(file, "%ssar %s, 2;move to int indexing like c\n", spaces,left_init_register);
    //sign non sense bug
    fprintf(file, "%smov %s, dword %s;sign handeling \n", spaces, tmp2_short,small_left_init_register);
    fprintf(file, "%smov [rsp+24], dword %s \n", spaces, tmp2_short);
    //this confirms I am writing to the correct spot fprintf(file, "%smov [rsp+28], dword 54 \n", spaces );
}