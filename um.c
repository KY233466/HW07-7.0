
/**************************************************************
 *                     um.c
 * 
 *     Assignment: Homework 6 - Universal Machine Program
 *     Authors: Katie Yang (zyang11) and Pamela Melgar (pmelga01)
 *     Date: November 24, 2021
 *
 *     Purpose: This C file will hold the main driver for our Universal
 *              MachineProgram (HW6). 
 *     
 *     Success Output:
 *              
 *     Failure output:
 *              1. 
 *              2. 
 *                  
 **************************************************************/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "assert.h"
#include "uarray.h"
#include "seq.h"
#include "bitpack.h"
#include <sys/stat.h>

#define BYTESIZE 8


/* Instruction retrieval */
typedef enum Um_opcode {
        CMOV = 0, SLOAD, SSTORE, ADD, MUL, DIV,
        NAND, HALT, ACTIVATE, INACTIVATE, OUT, IN, LOADP, LV
} Um_opcode;


struct Info
{
    // uint32_t op;
    uint32_t rA;
    uint32_t rB;
    uint32_t rC;
    uint32_t value;
};


typedef struct Info Info;
///////////////////////////////////////////////////////////////////////////////////////////

struct Array_T
{
    uint32_t *array;
    uint32_t size;
};

typedef struct Array_T Array;
// typedef struct Memory Memory;

#define two_pow_32 4294967296
uint32_t MIN = 0;   
uint32_t MAX = 255;
uint32_t registers_mask = 511;
uint32_t op13_mask = 268435455;

uint32_t seg_size = 0;
uint32_t seg_capacity = 10000;

//Memory
Array *all_segments;

//Our Queue!!!
uint32_t *map_queue;
uint32_t front = 0;
uint32_t rear = 0;
uint32_t queue_size = 0;
uint32_t queue_capacity = 5000;
// uint32_t program_counter = 0;


/* Memory Manager */
static inline uint32_t segmentlength(uint32_t segment_index);
static inline uint32_t get_word(uint32_t segment_index, uint32_t word_in_segment);
static inline void set_word(uint32_t segment_index, uint32_t word_index, uint32_t word);
static inline uint32_t map_segment(uint32_t num_words);
static inline void unmap_segment(uint32_t segment_index);
static inline void duplicate_segment(uint32_t segment_to_copy);
static inline void free_segments();
//////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////////

static inline void instruction_executer(uint32_t *all_registers, uint32_t *counter);
//////////////////////////////////////////////////////////////////////////////////////////////////////

/*Making Helper functions for our ARRAYLIST*/

static inline uint32_t array_size(Array array);
static inline uint32_t element_at(Array array, uint32_t index);
static inline void replace_at(Array *array, uint32_t insert_value, uint32_t old_value_index);
static inline void push_at_back(Array *array, uint32_t insert_value);

static inline Array *array_at(Array *segments, uint32_t segment_index);
static inline void seg_push_at_back(Array *insert_seg);
static inline void replace_array(Array *insert_array, uint32_t old_array_index);

static inline void enqueue(uint32_t new_index);
static inline uint32_t dequeue();
static inline void expand_queue();
//  void print_queue();

//////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
    /*Progam can only run with two arguments [execute] and [machinecode_file]*/

    // printf("lollllll\n");
    
    if (argc != 2) {
        fprintf(stderr, "Usage: ./um [filename] < [input] > [output]\n");
        exit(EXIT_FAILURE);
    }

    FILE *fp = fopen(argv[1], "r");
    assert(fp != NULL);

    struct stat sb;

    if (stat(argv[1], &sb) == -1) {
        perror("stat");
        exit(EXIT_FAILURE); // TODO what should the behavior be
    }

////////////////////////////////////////////////////////////////////////////
    /* EXECUTION.C MODULE  */
    Array segment0;
    segment0.array = malloc(sizeof(uint32_t) * sb.st_size / 4);
    segment0.size = 0;

/////////////////////////////////////// READ FILE ///////////////////////////
    uint32_t byte = 0; 
    uint32_t word = 0;
    int c;
    int counter = 0;

    /*Keep reading the file and parsing through "words" until EOF*/
    /*Populate the sequence every time we create a new word*/
    while ((c = fgetc(fp)) != EOF) {
        word = Bitpack_newu(word, 8, 24, c);
        counter++;
        for (int lsb = 2 * BYTESIZE; lsb >= 0; lsb = lsb - BYTESIZE) {
            byte = fgetc(fp);
            word = Bitpack_newu(word, BYTESIZE, lsb, byte);
        }
        
        /* Populate sequence */
        push_at_back(&segment0, word);
    }
    
    fclose(fp);

    all_segments = malloc((sizeof(Array)) * seg_capacity);

    all_segments = malloc((sizeof(Array)) * seg_capacity);

    // map_queue = malloc(sizeof(uint32_t)* queue_capacity);
    map_queue = malloc(sizeof(uint32_t) * queue_capacity); //if this changes don't forget to change capacity initialization of global variable

    seg_push_at_back(&segment0);

    uint32_t all_registers[8] = { 0 };

    uint32_t program_counter = 0;

    /////////////////////////////////////// execution /////////////////////
    
    /*Run through all instructions in segment0, note that counter may loop*/
    while (program_counter < segmentlength(0)) {
        /*program executer is passed in to update (in loop) if needed*/
        // printf("execute!\n");
        instruction_executer(all_registers, &program_counter);
    }

    free_segments();

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    return EXIT_FAILURE; /*failure mode, out of bounds of $m[0]*/
}

static inline void seg_push_at_back(Array *insert_seg)
{
    if (seg_size != seg_capacity) {
        all_segments[seg_size] = *insert_seg;
        seg_size++;
    }
    else {
        uint32_t new_capacity = seg_capacity * 2;
        seg_capacity = new_capacity;
        
        Array *new_segments = malloc(sizeof(Array) * new_capacity);

        for (uint32_t i = 0; i < seg_size; i++) {
            new_segments[i] = all_segments[i];
        }

        free(all_segments);

        all_segments = new_segments;

        all_segments[seg_size] = *insert_seg;
        seg_size++;
    }
}

static inline uint32_t array_size(Array array1)
{
    return array1.size;
}

static inline uint32_t element_at(Array array1, uint32_t index)
{
    return array1.array[index];
}

static inline void replace_at(Array *array1, uint32_t insert_value,
                              uint32_t old_value_index)
{
    if (old_value_index == array1->size){
        push_at_back(array1, insert_value);
    }
    else{
        array1->array[old_value_index] = insert_value;
    }
}

static inline void push_at_back(Array *array1, uint32_t insert_value)
{
    uint32_t size = array_size(*array1);
    array1->array[size] = insert_value;
    array1->size = size + 1;
}

static inline uint32_t segmentlength(uint32_t segment_index)
{
    return array_size(*(array_at(all_segments, segment_index)));
}

static inline Array *array_at(Array *arrays, uint32_t segment_index)
{
    return &(arrays[segment_index]);
}

static inline uint32_t get_word(uint32_t segment_index,
                                uint32_t word_in_segment)
{
    return element_at(*((array_at(all_segments, segment_index))), word_in_segment);
}

static inline void set_word(uint32_t segment_index,
                            uint32_t word_index, uint32_t word)
{
    replace_at(array_at(all_segments, segment_index), word, word_index);
}

static inline uint32_t map_segment(uint32_t num_words)
{
    Array new_segment;
    new_segment.array = malloc((sizeof(uint32_t)) * num_words);
    new_segment.size = num_words;
    
    /*Initialize to ALL 0s*/
    for (uint32_t i = 0; i < num_words; i++) {
        replace_at(&new_segment, 0, i);
    }

    if (queue_size != 0) {
        uint32_t seq_index = dequeue();
        replace_array(&new_segment, seq_index);

        return seq_index;
    }
    else {
        seg_push_at_back(&new_segment);

        return seg_size - 1;
    }
}

static inline void replace_array(Array *insert_array, uint32_t old_array_index)
{
    all_segments[old_array_index] = *insert_array;
}

static inline void unmap_segment(uint32_t segment_index)
{
    assert(segment_index > 0);

    Array *seg_to_unmap = array_at(all_segments, segment_index);
    /* can't un-map a segment that isn't mapped */

    free(seg_to_unmap->array);
    seg_to_unmap->array = NULL;

    enqueue(segment_index);
}

static inline void duplicate_segment(uint32_t segment_to_copy)
{
    if (segment_to_copy != 0) {

        Array *seg_0 = array_at(all_segments, 0);

        /*free all c array, that is  in segment 0*/
        free(seg_0->array);

        /*hard copy - duplicate array to create new segment 0*/
        Array *target = array_at(all_segments, segment_to_copy);
        
        uint32_t target_seg_length = array_size(*target);

        seg_0->array = malloc(sizeof(uint32_t) * target_seg_length);
        seg_0->size = 0;

        /* Willl copy every single word onto the duplicate segment */
        for (uint32_t i = 0; i < target_seg_length; i++) {
            uint32_t word = element_at(*target, i);
            push_at_back(seg_0, word);
        }                          
    } else {
        /*don't replace segment0 with itself --- do nothing*/
        return;
    }
}

static inline void free_segments()
{
    for (uint32_t i = 0; i < seg_size; i++)
    {
        Array *target = array_at(all_segments, i);

        if (target->array != NULL) {
            free(target->array);
        }
    }

    free(all_segments);
    free(map_queue);
}

//change back for kcachegrind
static inline void instruction_executer(uint32_t *all_registers, uint32_t *counter)
{
    uint32_t instruction = get_word(0, *counter);
    
    uint32_t op = instruction;

    uint32_t code = op >> 28;
    uint32_t info_rA;
    uint32_t info_rB;
    uint32_t info_rC;
    uint32_t info_value;

    if (code != 13){
        uint32_t registers_ins = instruction &= registers_mask;

        info_rA = registers_ins >> 6;
        info_rB = registers_ins << 26 >> 29;
        info_rC = registers_ins << 29 >> 29;
        info_value = 0;
    }
    else {
        uint32_t registers_ins = instruction &= op13_mask;

        info_rA = registers_ins >> 25;
        info_value = registers_ins << 7 >> 7;
    }

    (*counter)++;

    /*Rest of instructions*/

    if (code == LOADP)
    {
        duplicate_segment(all_registers[info_rB]);
        *counter = all_registers[info_rC];
    }
    else if (code == LV)
    {
        uint32_t *word = &(all_registers[info_rA]);
        *word = info_value;
    }
    else if (code == SLOAD)
    {
        uint32_t *word = &(all_registers[info_rA]);
        *word = get_word(all_registers[info_rB], all_registers[info_rC]);
    }
    else if (code == SSTORE)
    {
        set_word(all_registers[info_rA], all_registers[info_rB], all_registers[info_rC]);
    }
    else if (code == ADD || code == MUL || code == DIV || code == NAND)
    {
        uint32_t rB_val = all_registers[info_rB];
        uint32_t rC_val = all_registers[info_rC];

        uint32_t value = 0;

        /*Determine which math operation to perform based on 4bit opcode*/
        if (code == NAND)
        {
            value = ~(rB_val & rC_val);
        }
        else if (code == ADD)
        {
            value = (rB_val + rC_val) % two_pow_32;
        }
        else if (code == DIV)
        {
            value = rB_val / rC_val;
        }
        else if (code == MUL)
        {
            value = (rB_val * rC_val) % two_pow_32;
        }
        else
        {
            exit(EXIT_FAILURE);
        }

        uint32_t *word = &(all_registers[info_rA]);
        *word = value;
    }
    else if (code == CMOV)
    {
        if (all_registers[info_rC] != 0)
        {
            uint32_t *word = &(all_registers[info_rA]);
            *word = all_registers[info_rB];
        }
    }
    else if (code == ACTIVATE)
    {
        uint32_t *word = &(all_registers[info_rB]);
        *word = map_segment(all_registers[info_rC]);
    }
    else if (code == INACTIVATE)
    {
        unmap_segment(all_registers[info_rC]);
    }
    else if (code == OUT)
    {
        uint32_t val = all_registers[info_rC];

        assert(val >= MIN);
        assert(val <= MAX);

        printf("%c", val);
    }
    else if (code == IN)
    {
        uint32_t input_value = (uint32_t)fgetc(stdin);
        uint32_t all_ones = ~0;

        /*Check if input value is EOF...aka: -1*/
        if (input_value == all_ones)
        {
            uint32_t *word = &(all_registers[info_rC]);
            *word = all_ones;
            return;
        }

        /* Check if the input value is in bounds */
        assert(input_value >= MIN);
        assert(input_value <= MAX);

        uint32_t *word = &(all_registers[info_rC]);
        *word = input_value;
    }
    else if (code == HALT)
    {
        free_segments(all_segments);

        exit(EXIT_SUCCESS);
    }
    else {
        exit(EXIT_FAILURE);
    }

    return;
}

static inline void enqueue(uint32_t new_index)
{

    if (queue_size < queue_capacity) {

        if (queue_size == 0) {
            map_queue[0] = new_index;       //first item
            front = 0;
            rear = 0;
            queue_size++;
        }
        /* add at front */
        else if (rear == (queue_capacity - 1)) {
            map_queue[0] = new_index;
            rear = 0;
            queue_size++;
        }
        else {
            rear++;
            map_queue[rear] = new_index;
            
            queue_size++;
        }
    } else {
        expand_queue();
        //enqueue(new_index);
        rear++;
        map_queue[rear] = new_index;
            
        queue_size++;
    }
}

static inline uint32_t dequeue()
{
    queue_size--;

    uint32_t front_orig = front;

    if (front != (queue_capacity - 1)){ //
        front+=1; 
    }
    else {
        front = 0;
    }

    return map_queue[front_orig];   
}

static inline void expand_queue()
{
    uint32_t *new_queue = malloc(sizeof(uint32_t) * queue_capacity * 2 );
    
    if (rear == (queue_capacity - 1) ) {
        for (uint32_t i = 0; i < queue_capacity; i++) {
            new_queue[i] = map_queue[i];
            rear = (queue_size - 1);
            front = 0;
        }

        queue_capacity *= 2;

        free(map_queue);
        map_queue = new_queue;
    } else {
        uint32_t new_index = 0;

        for (uint32_t i = front; i < queue_capacity; i++) {
            new_queue[new_index] = map_queue[i];
            new_index++;
        }

        for (uint32_t j = 0; j <= rear; j++) {
            new_queue[new_index] = map_queue[j];
            new_index++;
        }

        free(map_queue);
        map_queue = new_queue;
        rear = (queue_size - 1);
        front = 0;
        queue_capacity *= 2;
    }
}

//   void print_queue()
// {
//     printf("\n Queue { ");
//     for (uint32_t i = 0; i < queue_capacity ; i++) {
//         printf(" %u, ", map_queue[i]);
//     }
//     printf(" } \n\n");

//     if (map_queue[front] != 0){
//         printf("Front is: map_queue[%u] = %u\n", front, map_queue[front]);
//     }

//     if (map_queue[rear] != 0)
//     {
//         printf("Rear is: map_queue[%u] = %u\n", rear, map_queue[rear]);
//     }

//     printf("Queue capacity is: %u\n", queue_capacity);
//     printf("Queue size is: %u\n", queue_size);
// }