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
    Node *prev, *next;
    Node(long p): page(p), prev(NULL), next(NULL) {}
};

class StampQueue {
    public:
    Stamp *front, *rear;
    map<long, Stamp*> smap;;
    StampQueue(): front(NULL), rear(NULL), smap(NULL) {}

    void visit_stamp(long pn){
        Stamp *stmp;
        if(!front && !rear) {
            stmp = new Stamp(pn);
            smap = map<long, Stamp*>();
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

    public:
    LRUCache(int capacity) {
        this->capacity = capacity;
        size = 0;
        pageList = new DoublyLinkedList();
        stampqueue = new StampQueue();
        pageMap = map<long, Node*>();
        visitingRecord = map<long, RecordList*>();
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
        //update stamp queue
        stampqueue->visit_stamp(key);

        if(pageMap.find(key)!=pageMap.end()) {
            // if key already present, update value and move page to head
            pageMap[key]->value = value;
            pageList->move_page_to_head(pageMap[key]);
            visitingRecord[key]->increment_record();
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
        visitingRecord[key]->add_record(counter, d);
        size++;
        pageMap[key] = page;
    }

    void print_record(){
        ofstream trace, distance, pg_freq, reuse_ct;
        trace.open("func_trace.out");
        distance.open("mem_distance.txt");
        pg_freq.open("page_freq.txt");
        reuse_ct.open("reuse_distance");
        map<long, RecordList*>::iterator iter;
        for(iter = visitingRecord.begin(); iter != visitingRecord.end(); iter++){
            trace << iter->first << ": ";
            long dist;
            long freq = 0;
            Record *rc= iter->second->front;
            while(true){
                trace << "[" << rc->start << "," << rc->visit << "," << rc->end <<"] ";
                reuse_ct << rc->reuse_dist << "\n";
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
    cache.visit(x/4096, 0);
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
    cache.visit(x/4096, 1);
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


    INS_AddInstrumentFunction(Instruction, 0);
    TRACE_AddInstrumentFunction(Trace, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}