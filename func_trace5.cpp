/*
 *  This file contains an ISA-portable PIN tool for tracing memory accesses while simulating a LRU cache.
 */

#include <stdio.h>
#include <iostream>
#include <string>
#include <list>
#include <map>
#include <cstdlib>
#include <fstream>
#include "pin.H"

//#define PRINT_RTN_INSTRUCTION



//The Cache simulator portion
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
typedef unsigned long addr_t;
typedef enum
{
    Access_I_FETCH, /* An instruction fetch. Read from the I-cache. */
    Access_D_READ,  /* A data read. Read from the D-cache. */
    Access_D_WRITE, /* A data write. Write to the D-cache. */
} AccessType;

typedef enum
{
    Write_WRITE_BACK,
    Write_WRITE_THROUGH,
} WriteScheme;

typedef enum
{
    Allocate_ALLOCATE,
    Allocate_NO_ALLOCATE,
} AllocateType;

typedef enum
{
    Replacement_LRU,
    Replacement_RANDOM,
} ReplacementType;
typedef struct
{
    int num_blocks;
    int words_per_block;
    int associativity;
    ReplacementType replacement;
    WriteScheme write_scheme;     /* D-cache only! */
    AllocateType allocate_scheme; /* D-cache only! */
} CacheInfo;

#include <iostream>



static CacheInfo icache_info;
static CacheInfo dcache_info[3];
struct Cache{
    long *tag;
    int hit;
    int miss;
    int read;
    int compmiss;
    short *valid;
    int *order;
    //variables for only writable D-cache
    short *dirty;
    int writes;
    int words;
    int writemiss;
    int writecompmiss;
};
static struct Cache Icache;
static struct Cache Dcache[3];
int count;

//the function to find the row of address
long findrow(addr_t addr, CacheInfo c){
    long row_shift;
    long row_mask;
    long row_bits;
    long ind;
    row_bits=(int)log2(c.num_blocks/c.associativity);
    row_shift=2+(int)log2(c.words_per_block);
    row_mask=(1<<row_bits)-1;
    ind=(addr>>row_shift)&row_mask;
    return ind;
}

//function to find the tag of the address
long findtag(addr_t addr, CacheInfo c){
    long tag_shift;
    long tag_mask;
    long tag_bits;
    long row_bits;
    long tag;
    row_bits=(int)log2(c.num_blocks/c.associativity);
    tag_bits=46-row_bits-(int)log2(c.words_per_block);
    tag_shift=2+(int)log2(c.words_per_block)+row_bits;
    tag_mask=(1<<tag_bits)-1;
    tag=(addr>>tag_shift)&tag_mask;
    return tag;
}

//function to check the cache with no associativity
void seekcache(addr_t address, struct Cache *cac){
    //find the block index and tag from the address
    long row=findrow(address,icache_info);
    long tag=findtag(address,icache_info);
    if(cac->tag[row]!=tag||cac->valid[row]==0){
        if(cac->valid[row]==0){
            cac->compmiss++;
        }
        cac->miss++;
        cac->tag[row]=tag;
        cac->valid[row]=1;
        cac->read++;
    }
    else{
        cac->read++;
    }
    return;
}

//function to find out if the tag is in the associative set
//return 0 for not found, 1 for found, -1 if the i'th block is empty
long find_in_set(long blk, long tag, struct Cache *cac,CacheInfo c){
    long result=0;
    for(int i=0;i<c.associativity;i++){
        if(tag==cac->tag[blk+i]){
            result=1;
            break;
        }
        if(cac->valid[blk+i]==0){
            result = 0 - i;
        }
    }
    return result;
}

//function to change the order of blocks in a set
//if first==-1, add 1 to the order of all valid members in the whole set(new block created)
//if first!=-1, add 1 to order of all members with a order less than block[first](a old valid block being accessed)
void addorder(long blk, struct Cache *cac, CacheInfo c, long first){
    for(int i=0;i<c.associativity;i++){
        if(cac->valid[blk+i]==0){
            break;
        }
        if(cac->order[blk+i]<cac->order[first]||first==-1){
            cac->order[blk+i]++;
        }
    }
    return;
}

//function to find the LRU block(assume the set is full with no empty blocks)
long find_LRU(long blk, struct Cache *cac, CacheInfo c){
    long lru=0;
    for(int i=0;i<c.associativity;i++){
        if(cac->order[blk+i]==c.associativity){
            lru=blk+i;
        }
    }
    return lru;
}

//test method for debugging, should not be called in the final version program
void print_order(int blk, struct Cache *cac, CacheInfo c){
    printf("check---------- Asso-%d\n", c.associativity);
    for(int i=0;i<c.associativity;i++){
        printf("%d tag %lx order %d \n", cac->valid[blk+i],cac->tag[blk+i], cac->order[blk+i]);
    }
}

//test method for debug, should never been called in final version
void print_valid(struct Cache *cac,CacheInfo c){
    for(int i=0;i<c.num_blocks;i++){
        printf("%d %lx %d\n", cac->valid[i], cac->tag[i], cac->order[i]);
    }
}

//function to read from associative cache
void seekcacheR(addr_t address, struct Cache *cac, CacheInfo c, int stage){
    //find the block index and tag from the address
    long block=findrow(address,c)*c.associativity;
    long tag=findtag(address,c);
    long newplace;
    //check if it is the miss case
    long find=find_in_set(block, tag, cac,c);
    if(find<=0){
        if(find<0){
            cac->compmiss++;
            newplace=block-find;
        }
        else{
            if(c.replacement==Replacement_RANDOM){
                srand(1000);
                newplace=rand()%c.associativity+block;
            }
            else{
                newplace=find_LRU(block, cac, c);
            }
        }
        addorder(block, cac,c,-1);
        cac->tag[newplace]=tag;
        cac->order[newplace]=1;
        cac->valid[newplace]=1;
        cac->miss++;
        cac->read++;
        if(stage!=-1&&stage<2){
            seekcacheR(address, &Dcache[stage+1], dcache_info[stage+1], stage+1);
        }
        if(stage == 2){
            cache.visit((long)address, 0);
        }
    }
    else{
        cac->read++;
    }
    
}

//function to find block number of a tag, knowing the tag is in a specific row
long find_place(long blk, long tag, struct Cache *cac, CacheInfo c){
    long place=-1;
    for(int i=0;i<c.associativity;i++){
        if(tag==cac->tag[blk+i]){
            place=i+blk;
            break;
        }
    }
    return place;
}

//write through, no allocate
void write_T_N(addr_t address, struct Cache *cac, CacheInfo c, int stage){
    //find the block index and tag from the address
    long block=findrow(address, c)*c.associativity;
    long tag=findtag(address,c);
    long find=find_in_set(block, tag, cac, c);
    //check if it is a miss
    if(find<=0){
        cac->writemiss++;
        if(find<0){
            cac->writecompmiss++;
        }
        if(stage<2){
            write_T_N(address, &Dcache[stage+1], dcache_info[stage+1], stage+1);
        }
    }
    cac->writes++;
    cac->words++;
}

//write through, allocate
void write_T_A(addr_t address, struct Cache *cac, CacheInfo c, int stage){
    long block=findrow(address,c)*c.associativity;
    long tag=findtag(address,c);
    long newplace;
    //check if it is the miss case
    long find=find_in_set(block, tag, cac,c);
    if(find<=0){
        if(find<0){
            cac->writecompmiss++;
            newplace=block-find;
        }
        else{
            if(c.replacement==Replacement_RANDOM){
                srand(1000);
                newplace=rand()%c.associativity+block;
            }
            else{
                printf("aaa%d", dcache_info[0].associativity);
                newplace=find_LRU(block, cac, c);
            }
        }
        addorder(block, cac, c, -1);
        //printf("old tag %x - %x, new tag %x-%x \n", cac->tag[newplace], block, tag, block);
        cac->tag[newplace]=tag;
        cac->order[newplace]=1;
        cac->valid[newplace]=1;
        cac->writemiss++;
        if(c.words_per_block>1){
            //cac->read++;
        }
        if(stage<2){
            write_T_A(address, &Dcache[stage+1], dcache_info[stage+1], stage+1);
        }
        
    }
    else{
        long place;
        place=find_place(block,tag,cac,c);
        if(place==-1){
            printf("fail to find place!");
        }
        addorder(block, cac, c, place);
        cac->order[place]=1;
        //printf("access tag %x-%x\n", cac->tag[place], block);
    }
    cac->writes++;
    cac->words++;
}


//write back, allocate
void write_B_A(addr_t address, struct Cache *cac, CacheInfo c, int stage){
    //find the block index and tag from the address
    long block=findrow(address,c)*c.associativity;
    long tag=findtag(address,c);
    long newplace;
    //check if it is the miss case
    long find=find_in_set(block, tag, cac,c);
    long place;
    if(find<=0){
        
        
        if(find<0){
            cac->writecompmiss++;
            newplace=block - find;
        }
        else{
            if(c.replacement==Replacement_RANDOM){
                srand(1000);
                newplace=rand()%c.associativity+block;
            }
            else{
                newplace=find_LRU(block, cac,c);
            }
        }
        //printf("old tag %x - %x, new tag %x-%x \n", cac->tag[newplace], block, tag, block);
        addorder(block, cac,c,-1);
        cac->tag[newplace]=tag;
        if(cac->dirty[newplace]==1){
            cac->words++;
            cac->dirty[newplace]=0;
        }
        cac->order[newplace]=1;
        cac->valid[newplace]=1;
        cac->writemiss++;
        if(c.words_per_block>1){
            //cac->read++;
        }
        if(stage<2){
            write_B_A(address, &Dcache[stage+1], dcache_info[stage+1], stage+1);
        }
        if(stage == 2){
            cache.visit((long)address, 1);
        }
    }
    else{
        place=find_place(block, tag, cac,c);
        if(place==-1){
            printf("did not find place!");
        }
        cac->dirty[place]=1;
        addorder(block, cac,c,place);
        cac->order[place]=1;
        //printf("access tag %x-%x\n", cac->tag[place], block);
    }
    cac->writes++;
}

void setup_caches()
{
    dcache_info[0].num_blocks = 2048;
    dcache_info[0].words_per_block = 16;
    dcache_info[0].associativity = 8;
    dcache_info[1].num_blocks = 16384;
    dcache_info[1].words_per_block = 16;
    dcache_info[1].associativity = 4;
    dcache_info[2].num_blocks = 131072;
    dcache_info[2].words_per_block = 16;
    dcache_info[2].associativity = 16;
    
    //initialize all the cache variables
//    Icache.tag=(int *)calloc(icache_info.num_blocks, sizeof(int));
//    Icache.miss=0;
//    Icache.hit=0;
//    Icache.read=0;
//    Icache.compmiss=0;
//    Icache.valid=(short *)calloc(icache_info.num_blocks, sizeof(short));
//    Icache.order=(int *)calloc(icache_info.num_blocks, sizeof(int));
//    count=0;
    
    for(int i=0;i<3;i++){
        dcache_info[i].replacement = Replacement_LRU;
        dcache_info[i].write_scheme = Write_WRITE_BACK;
        dcache_info[i].allocate_scheme = Allocate_ALLOCATE;
        Dcache[i].tag=(long *)calloc(dcache_info[i].num_blocks, sizeof(long));
        Dcache[i].miss=0;
        Dcache[i].hit=0;
        Dcache[i].read=0;
        Dcache[i].compmiss=0;
        Dcache[i].valid=(short *)calloc(dcache_info[i].num_blocks, sizeof(short));
        Dcache[i].order=(int *)calloc(dcache_info[i].num_blocks, sizeof(int));
        Dcache[i].dirty=(short *)calloc(dcache_info[i].num_blocks, sizeof(short));
        Dcache[i].writes=0;
        Dcache[i].words=0;
        Dcache[i].writecompmiss=0;
        Dcache[i].writemiss=0;
    }
    /* This call to dump_cache_info is just to show some debugging information
     and you may remove it. */
    //dump_cache_info();
}

void handle_access(int type, addr_t address)
{
    /* This is where all the fun stuff happens! This function is called to
     simulate a memory access. You figure out what type it is, and do all your
     fun simulation stuff from here. */
    
    switch(type)
    {
        case 0:
            
            seekcacheR(address, &Dcache[0], dcache_info[0],0);
            
            //printf("D_READ at %08lx\n", address);
            break;
        case 1:
            if(dcache_info[0].write_scheme==Write_WRITE_THROUGH){
                if(dcache_info[0].allocate_scheme==Allocate_NO_ALLOCATE){
                    write_T_N(address, &Dcache[0], dcache_info[0],0);
                }
                else{
                    write_T_A(address, &Dcache[0], dcache_info[0],0);
                }
            }
            else{
                write_B_A(address, &Dcache[0], dcache_info[0],0);
            }
            
            //printf("D_WRITE at %08lx\n", address);
            break;
    }
}



//typedef long uintptr_t;
using namespace std;

bool silent;
int numRtnsParsed = 0;

// All LRU Simulation Components
long counter;
map<string, int> function_count;


class Stamp {
    public:
    long page;
    Stamp *prev, *next;
    Stamp(long p): page(p), prev(NULL), next(NULL) {}
};

class StampQueue {
    public:
    Stamp *front, *rear;
    map<long, Stamp*> smap;;
    StampQueue(): front(NULL), rear(NULL), smap(map<long, Stamp*>()) {}

    void visit_stamp(long pn){
        Stamp *stmp;
        if(!front && !rear) {
            stmp = new Stamp(pn);
            front = rear = stmp;
            smap[pn] = stmp;
        }
        else {
            if(smap.find(pn)!=smap.end()) {
                update_stamp(smap[pn]);
            }
            else{
                stmp = new Stamp(pn);
                rear->next = stmp;
                stmp->prev = rear;
                rear = stmp;
                smap[pn] = stmp;
            }
        }
    }

    void update_stamp(Stamp *sp) {
        if(sp == rear) {
            return;
        }


        if(sp == front) {
            front = front->next;
            front->prev = NULL;
        }
        else {
            sp->prev->next = sp->next;
            sp->next->prev = sp->prev;
        }

        rear->next = sp;
        sp->prev = rear;
        sp->next = NULL;
        rear = sp;
    }

    int count_reuse_distance(long pn){
        
        if(smap.find(pn)==smap.end()) {
            return -1;
        }
        int dist = 0;
        Stamp *rc = smap[pn];
        while(true){
            if(!rc->next){
                break;
            }
            rc = rc->next;
            dist++;
        }
        return dist;
    }
};



class Node {
    public:
    long key;
    int value;
    Node *prev, *next;
    Node(long k, int v): key(k), value(v), prev(NULL), next(NULL) {}
};

class Record {
    public:
    long start, end;
    int visit, reuse_dist;
    Record *prev, *next;
    Record(long s, int d): start(s), end(-1), visit(1), reuse_dist(d), prev(NULL), next(NULL) {}
};

class RecordList {
    public:
    Record *front, *rear;
    RecordList(): front(NULL), rear(NULL) {}

    void add_record(long st, int d){
        Record *rec = new Record(st, d);
        if(!front && !rear) {
            front = rear = rec;
        }
        else {
            rear->next = rec;
            rec->prev = rear;
            rear = rec;
        }
    }

    void increment_record(){
        rear->visit++;
    }

    void end_record(long ed){
        rear->end = ed;
    }
};

class DoublyLinkedList {
    Node *front, *rear;
    
    bool isEmpty() {
        return rear == NULL;
    }

    public:
    DoublyLinkedList(): front(NULL), rear(NULL) {}
    
    Node* add_page_to_head(long key, int value) {
        Node *page = new Node(key, value);
        if(!front && !rear) {
            front = rear = page;
        }
        else {
            page->next = front;
            front->prev = page;
            front = page;
        }
        return page;
    }

    void move_page_to_head(Node *page) {
        if(page==front) {
            return;
        }
        if(page == rear) {
            rear = rear->prev;
            rear->next = NULL;
        }
        else {
            page->prev->next = page->next;
            page->next->prev = page->prev;
        }

        page->next = front;
        page->prev = NULL;
        front->prev = page;
        front = page;
    }

    void remove_rear_page() {
        if(isEmpty()) {
            return;
        }
        if(front == rear) {
            delete rear;
            front = rear = NULL;
        }
        else {
            Node *temp = rear;
            rear = rear->prev;
            rear->next = NULL;
            delete temp;
        }
    }
    Node* get_rear_page() {
        return rear;
    }
    
};

class LRUCache{
    int capacity, size;
    DoublyLinkedList *pageList;
    StampQueue *stampqueue;
    map<long, Node*> pageMap;
    map<long, RecordList*> visitingRecord;
    map<int, long> distanceFreq;

    public:
    LRUCache(int capacity) {
        this->capacity = capacity;
        size = 0;
        pageList = new DoublyLinkedList();
        stampqueue = new StampQueue();
        pageMap = map<long, Node*>();
        visitingRecord = map<long, RecordList*>();
        distanceFreq = map<int, long>();
    }

    int check(long key) {
        if(pageMap.find(key)==pageMap.end()) {
            return -1;
        }
        int val = pageMap[key]->value;

        // move the page to front
        // pageList->move_page_to_head(pageMap[key]);
        return val;
    }

    void visit(long key, int value) {
        

        if(pageMap.find(key)!=pageMap.end()) {
            // if key already present, update value and move page to head
            pageMap[key]->value = value;
            pageList->move_page_to_head(pageMap[key]);
            visitingRecord[key]->increment_record();
            //update stamp queue
            int d = stampqueue->count_reuse_distance(key);
            if(distanceFreq.find(d)==distanceFreq.end()){
                distanceFreq[d] = 0;
            }
            distanceFreq[d]++;
            stampqueue->visit_stamp(key);
            return;
        }

        if(size == capacity) {
            // remove rear page
            long k = pageList->get_rear_page()->key;
            pageMap.erase(k);
            pageList->remove_rear_page();

            visitingRecord[k]->end_record(counter);
            size--;
        }

        // add new page to head to Queue
        Node *page = pageList->add_page_to_head(key, value);
        if(visitingRecord.find(key) == visitingRecord.end()){
            visitingRecord[key] = new RecordList();
        }
        int d = stampqueue->count_reuse_distance(key);
        if(distanceFreq.find(d)==distanceFreq.end()){
            distanceFreq[d] = 0;
        }
        distanceFreq[d]++;
        
        visitingRecord[key]->add_record(counter, d);
        size++;
        pageMap[key] = page;
        //update stamp queue
        stampqueue->visit_stamp(key);
    }

    void print_record(){
        ofstream trace, distance, pg_freq, reuse_ct;
        trace.open("func_trace.out");
        distance.open("mem_distance.txt");
        pg_freq.open("page_freq.txt");
        reuse_ct.open("reuse_distance.txt");
        map<long, RecordList*>::iterator iter;
        for(iter = visitingRecord.begin(); iter != visitingRecord.end(); iter++){
            trace << iter->first << ": ";
            long dist;
            long freq = 0;
            Record *rc= iter->second->front;
            while(true){
                trace << "[" << rc->start << "," << rc->visit << "," << rc->end <<"] ";
                if(rc->reuse_dist != -1){
                    reuse_ct << rc->reuse_dist << "\n";
                }
                freq += rc->visit;
                if(!rc->next){
                    pg_freq << freq << "\n";
                    break;
                }
                dist = rc->next->start - rc->end;
                distance << dist << "\n";
                rc = rc->next;
            }
            trace << "\n";
        }
        trace.close();
        distance.close();
        pg_freq.close();
        reuse_ct.close();
        trace.open("func_count.out");
        map<string, int>::iterator i2;
        for(i2=function_count.begin(); i2!= function_count.end();i2++){
            trace << i2->first << ": " << i2->second << "\n";
        }
        trace.close();

        trace.open("distanceFreq.txt");
        map<int, long>::iterator i3;
        for(i3=distanceFreq.begin(); i3!= distanceFreq.end();i3++){
            trace << i3->first << ":" << i3->second << "\n";
        }
        trace.close();
    }

    ~LRUCache() {
        map<long, Node*>::iterator i1;
        for(i1=pageMap.begin();i1!=pageMap.end();i1++) {
            delete i1->second;
        }
        delete pageList;
    }
};




/* ===================================================================== */
/*                                                                       */
/*                                                                       */
/*                                                                       */
/* Real Pin Section Start Here!                                           */
/*                                                                       */
/*                                                                       */
/*                                                                       */
/* ===================================================================== */
LRUCache cache(24000);

// Print a memory read record
VOID RecordMemRead(VOID * ip, VOID * addr)
{
    //fprintf(trace,"%p: R %p\n", ip, addr);
    if(silent){
        return;
    }
    //int x = static_cast<int>(reinterpret_cast<uintptr_t>(addr));
    long x = (long) addr;
    counter++;
    //cache.visit(x/4096, 0);
    handle_access(0, x);
}

// Print a memory write record
VOID RecordMemWrite(VOID * ip, VOID * addr)
{
    //fprintf(trace,"%p: W %p\n", ip, addr);
    if(silent){
        return;
    }
    //int x = static_cast<int>(reinterpret_cast<uintptr_t>(addr));
    long x = (long) addr;
    counter++;
    //cache.visit(x/4096, 1);
    handle_access(0, x);
}

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID *v)
{
    // Instruments memory accesses using a predicated call, i.e.
    // the instrumentation is called iff the instruction will actually be executed.
    //
    // On the IA-32 and Intel(R) 64 architectures conditional moves and REP 
    // prefixed instructions appear as predicated instructions in Pin.
    UINT32 memOperands = INS_MemoryOperandCount(ins);

    // Iterate over each memory operand of the instruction.
    for (UINT32 memOp = 0; memOp < memOperands; memOp++)
    {
        if (INS_MemoryOperandIsRead(ins, memOp))
        {
            INS_InsertPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)RecordMemRead,
                IARG_INST_PTR,
                IARG_MEMORYOP_EA, memOp,
                IARG_END);
        }
        // Note that in some architectures a single memory operand can be 
        // both read and written (for instance incl (%eax) on IA-32)
        // In that case we instrument it once for read and once for write.
        if (INS_MemoryOperandIsWritten(ins, memOp))
        {
            INS_InsertPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)RecordMemWrite,
                IARG_INST_PTR,
                IARG_MEMORYOP_EA, memOp,
                IARG_END);
        }
    }
}


static VOID Trace(TRACE t_trace, VOID *v)
{
    INS ins = BBL_InsHead(TRACE_BblHead(t_trace));
    RTN rtn = INS_Rtn(ins);
    
    if (!RTN_Valid(rtn))
    {
        return;
    }

    if (INS_Address(ins) == RTN_Address(rtn)) 
    {
        /* The first ins of an RTN that will be executed - it is possible at this point to examine all the INSs 
           of the RTN that Pin can statically identify (using whatever standard symbol information is available).
           A tool may wish to parse each such RTN only once, if so it will need to record and identify which RTNs 
           have already been parsed
        */
        numRtnsParsed++;
        //fprintf (trace,"RTN_Name: %s Start.\n", RTN_Name(rtn).c_str());
        string fn = RTN_Name(rtn).c_str();
        /*
        if(counter>=2000000000){
            silent = true;
            return;
        }
        */
        if(fn=="_Z7do_testv"){
            silent=false;
        }
        if(silent){
            return;
        }
        if(function_count.find(fn)==function_count.end()){
            function_count[fn]=0;
        }
        function_count[fn]++;

        #ifdef PRINT_RTN_INSTRUCTION
        RTN_Open(rtn);
        for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
        {
            fprintf (trace,"%p %s\n", reinterpret_cast<void *>(INS_Address(ins)), INS_Disassemble(ins).c_str());
        }
        RTN_Close(rtn);
        #endif
    }

}

VOID Fini(INT32 code, VOID *v)
{
    //fprintf(trace, "#eof n Start.\n");
    cache.print_record();
    ASSERTX(numRtnsParsed != 0);
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
   
INT32 Usage()
{
    PIN_ERROR( "This Pintool prints a trace of memory addresses\n" 
              + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    PIN_InitSymbols();
    if (PIN_Init(argc, argv)) return Usage();

    
    //initialize the variables 
    counter = 0;
    function_count = map<string, int>();
    silent = true;
    setup_caches();


    INS_AddInstrumentFunction(Instruction, 0);
    TRACE_AddInstrumentFunction(Trace, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}