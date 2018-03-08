/*
** Created By cyachen
** Enviroment: Intel,linux,x64
**
** This file is used to implement the function needed to be insert
** in the IR
*/

#ifndef __CYC_UAFCHECK_H
#define __CYC_UAFCHECK_H
#endif


#include<stdio.h>
#include<stdlib.h>

typedef unsigned long long u64;

#define CYC_GLOBAL_ID 1
#define CYC_MAIN_ID 2

#define CYC_ID_INVALIDATE 0
#define CYC_ID_VALIDATE 1
#define CYC_ID_CHECK_VALID 2

#define CYC_ID_N_PER 32

#define CYC_ID_ARG_STACK_N 100

#define CYC_HEAP_ID_ATTR (0x80000000000000)

#define __WEAK_INLINE __attribute__((__weak__,__always_inline__)) 


extern size_t **cyc_uafcheck_trie_entry;
extern size_t **cyc_id_trie_entry;
extern u64 cyc_id_cnt;

extern u64 *cyc_id_call_stack;
extern u64 cyc_id_cur_stack;

static const size_t CYC_UAFCHECK_TRIE_ENTRIES_N=((size_t) 32 * (size_t) 1024 * (size_t) 1024);
static const size_t CYC_UAFCHECK_SECOND_ENTRIES_N=((size_t) 64 * (size_t) 1024 * (size_t) 1024); 

static const size_t CYC_ID_TRIE_ENTRIES_N=((size_t) 4 * (size_t) 1024 * (size_t) 1024);
static const size_t CYC_ID_SECOND_ENTRIES_N=((size_t) 64 * 1024 * (size_t) 1024);

static const size_t CYC_ID_CALL_STACK_N=((size_t) 64 * 1024 * (size_t) 1024);

int cyc_validate_id(u64 cyc_id,int cyc_isValidate){
    //global 
    if(cyc_id==CYC_GLOBAL_ID){
        return 1;
    }
    //find id
    size_t cyc_id_fst_entry=cyc_id/CYC_ID_SECOND_ENTRIES_N/CYC_ID_N_PER;
    size_t cyc_id_snd_entry=cyc_id/CYC_ID_N_PER%CYC_ID_SECOND_ENTRIES_N;

    size_t *cyc_id_entry_pos=*(cyc_id_trie_entry+cyc_id_fst_entry);
    
    //if need malloc new second entry
    if(cyc_isValidate==CYC_ID_INVALIDATE||cyc_id_entry_pos==NULL){
        cyc_id_entry_pos=(size_t *)malloc(CYC_UAFCHECK_SECOND_ENTRIES_N);
        *(cyc_id_trie_entry+cyc_id_fst_entry)=cyc_id_entry_pos;
    }

    //validate id
    size_t cyc_id_pos=1>>(cyc_id_snd_entry%CYC_ID_N_PER);
    cyc_id_entry_pos+=cyc_id_snd_entry;
    if(cyc_isValidate==CYC_ID_INVALIDATE){
        *(cyc_id_entry_pos)&=(~cyc_id_pos);
    }
    else if(cyc_isValidate==CYC_ID_VALIDATE){
        *(cyc_id_entry_pos)|=(cyc_id_pos);
    }
    else if(cyc_isValidate==CYC_ID_CHECK_VALID){
        if((*(cyc_id_entry_pos)&(cyc_id_pos))==0){
            return 0;
        }
    }

    return 1;
}

//stack operation
void cyc_id_call_stack_pop(){
    cyc_id_cur_stack--;
}
void cyc_id_call_stack_push(u64 cyc_id){
    cyc_id_cur_stack++;
    *(cyc_id_call_stack+cyc_id_cur_stack)=cyc_id;
}
u64 cyc_id_call_stack_top(){
    return *(cyc_id_call_stack+cyc_id_cur_stack);
}

//uafcheck pos
size_t *cyc_uafcheck_find_pos(size_t cyc_cur_addr){
    size_t cyc_uafcheck_fst_entry=cyc_cur_addr/CYC_UAFCHECK_SECOND_ENTRIES_N;
    size_t cyc_uafcheck_snd_entry=cyc_cur_addr%CYC_UAFCHECK_SECOND_ENTRIES_N;

    if(cyc_uafcheck_fst_entry>CYC_UAFCHECK_TRIE_ENTRIES_N){
        printf("tooo much\n");
    }

    size_t* cyc_uafcheck_pos=*(cyc_uafcheck_trie_entry+cyc_uafcheck_fst_entry);
    if(cyc_uafcheck_pos==NULL){
        cyc_uafcheck_pos=(size_t*)malloc(CYC_UAFCHECK_SECOND_ENTRIES_N*(sizeof(u64)));
        *(cyc_uafcheck_trie_entry+cyc_uafcheck_fst_entry)=cyc_uafcheck_pos;
    }
    
    return (cyc_uafcheck_pos+cyc_uafcheck_snd_entry);
}