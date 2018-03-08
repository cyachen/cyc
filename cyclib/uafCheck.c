/*
** Created By cyachen
** Enviroment: Intel,linux,x64
**
** This file is used to implement the function needed to be insert
** in the IR
*/

#include "uafCheck.h"
#include<stdlib.h>

size_t **cyc_uafcheck_trie_entry=NULL;
size_t **cyc_id_trie_entry=NULL;
u64 cyc_id_cnt=3;

u64 *cyc_id_call_stack=NULL;
u64 cyc_id_cur_stack=1;

u64 cyc_id_arg_stack[CYC_ID_ARG_STACK_N];
int cyc_id_arg_stack_cnt=0;
u64 cyc_id_return_stack[CYC_ID_ARG_STACK_N];
int cyc_id_return_stack_cnt=0;

void cyc_uafcheck_init(){
    cyc_uafcheck_trie_entry=(size_t **)malloc(CYC_UAFCHECK_TRIE_ENTRIES_N*(sizeof(u64))); 
    cyc_id_trie_entry=(size_t **)malloc(CYC_ID_TRIE_ENTRIES_N*(sizeof(u64)));
    size_t *cyc_id_first_entry=(size_t *)malloc(CYC_UAFCHECK_SECOND_ENTRIES_N*(sizeof(u64)));
    (*cyc_id_first_entry)|=0x3;//for gloabal and main
    (*cyc_id_trie_entry)=cyc_id_first_entry;

    cyc_id_call_stack=(u64 *)malloc(CYC_ID_CALL_STACK_N*(sizeof(u64)));
    *cyc_id_call_stack=CYC_MAIN_ID;
}

//id propagate 1--->2
void cyc_handle_id_propagate(size_t cyc_p1_pos,size_t cyc_p2_pos){
    size_t *cyc_p1_uafcheck_pos=cyc_uafcheck_find_pos(cyc_p1_pos);
    size_t *cyc_p2_uafcheck_pos=cyc_uafcheck_find_pos(cyc_p2_pos);

    *cyc_p2_uafcheck_pos=*cyc_p1_uafcheck_pos;
}

u64 cyc_handle_dereference(size_t cyc_p_pos){
    //size_t cyc_p_pos=(size_t)cyc_p;
    size_t *cyc_p_uafcheck_pos=cyc_uafcheck_find_pos(cyc_p_pos>>3);
    u64 cyc_p_id=*cyc_p_uafcheck_pos;
    // if(!cyc_validate_id(cyc_p_id,CYC_ID_CHECK_VALID)){
    //     printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!uaf error!!!!!!!!!!!!!!!!!!");
    // }
    //assert(0&&"use-after-free");
    return cyc_p_id;
}

void cyc_handle_store_heap_addr(size_t cyc_p_pos,u64 cyc_p_id){
    printf("start handle store heap addr %zx %lld\n",cyc_p_pos,cyc_p_id);
    size_t *cyc_p_uafcheck_pos=cyc_uafcheck_find_pos(cyc_p_pos>>3);
    *cyc_p_uafcheck_pos=cyc_p_id;
    printf("end hanlde store heap addr\n");
}

void cyc_handle_store_stack_addr(size_t cyc_p_pos){
    size_t *cyc_p_uafcheck_pos=cyc_uafcheck_find_pos(cyc_p_pos>>3);
    *cyc_p_uafcheck_pos=cyc_id_cur_stack;
}
///heap
u64 cyc_handle_malloc(){
    printf("start handle malloc\n");
    // size_t *cyc_p_uafcheck_pos=cyc_uafcheck_find_pos(cyc_p_pos);
    // *(cyc_p_uafcheck_pos)=cyc_id_cnt|CYC_HEAP_ID_ATTR;
    cyc_validate_id(cyc_id_cnt,CYC_ID_VALIDATE);

    printf("end handle malloc %lld\n",cyc_id_cnt);
    return cyc_id_cnt++;
    // cyc_id_cnt++;
}
void cyc_handle_free(size_t cyc_p_id){
    //size_t *cyc_p_uafcheck_pos=cyc_uafcheck_find_pos(cyc_p_pos);
    //u64 cyc_p_id=*cyc_p_uafcheck_pos;
    cyc_validate_id(cyc_p_id,CYC_ID_INVALIDATE);
}

///stack
void cyc_handle_stack_alloc(size_t cyc_p_pos){
    printf("addr of p in handle stack alloc %zx\n",cyc_p_pos);
    // size_t *cyc_p_uafcheck_pos=cyc_uafcheck_find_pos(cyc_p_pos);
    // *cyc_p_uafcheck_pos=cyc_id_cur_stack;
}

void cyc_handle_call_arg(size_t cyc_cur_pos){
    
}

//handle call , inc id_cnt
//主调函数的调用函数之前和之后的处理
void cyc_before_called(){
    cyc_id_call_stack_push(cyc_id_cnt);
    //validate current id
    cyc_validate_id(cyc_id_cnt,CYC_ID_VALIDATE);
    cyc_id_cur_stack=cyc_id_cnt;
    cyc_id_cnt++;
}

void cyc_after_called(){
    u64 cyc_id_call_func=cyc_id_call_stack_top();
    cyc_validate_id(cyc_id_call_func,CYC_ID_INVALIDATE);
    cyc_id_call_stack_pop();
    cyc_id_cur_stack=cyc_id_call_stack_top();

}

void cyc_is_id_valid(u64 cyc_id){
    if(cyc_validate_id(cyc_id,CYC_ID_CHECK_VALID)==0){
        printf("!!!!!!!!!!!!!!!!!!!!uaf error!!!!!!!!!!!!!!!!!!\n");
    }
}

void cyc_id_arg_stack_push(u64 cyc_id){
    cyc_id_arg_stack[cyc_id_arg_stack_cnt++]=cyc_id;
}

u64 cyc_id_arg_stack_pop(){
    return cyc_id_arg_stack[--cyc_id_arg_stack_cnt];
}

void cyc_id_arg_stack_clear(){
    cyc_id_arg_stack_cnt=0;
}

void cyc_id_return_stack_push(u64 cyc_id){
    cyc_id_return_stack[cyc_id_arg_stack_cnt++]=cyc_id;
}

u64 cyc_id_return_stack_pop(){
    cyc_id_arg_stack_clear();
    if(cyc_id_return_stack_cnt>=1){
        return cyc_id_return_stack[--cyc_id_return_stack_cnt];
    }
    else{

    }
}
// int main(){
//     cyc_uafcheck_init();
//     int p;
//     printf("p addr %p\n",&p);
//     size_t * addrp=cyc_uafcheck_find_pos((size_t)(&p));
// }