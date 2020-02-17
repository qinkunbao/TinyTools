/*
 * Copyright 2002-2019 Intel Corporation.
 * 
 * This software is provided to you as Sample Source Code as defined in the accompanying
 * End User License Agreement for the Intel(R) Software Development Products ("Agreement")
 * section 1.L.
 * 
 * This software and the related documents are provided as is, with no express or implied
 * warranties, other than those that are expressly stated in the License.
 */

/*
 *  This file contains an ISA-portable PIN tool for tracing memory accesses.
 */

#include <stdio.h>
#include <iostream>
#include <string>
#include <list>
#include <map>
#include "pin.H"

//#define PRINT_RTN_INSTRUCTION


using namespace std;


FILE * trace;
int numRtnsParsed = 0;

// Print a memory read record
VOID RecordMemRead(VOID * ip, VOID * addr)
{
    fprintf(trace,"%p: R %p\n", ip, addr);
}

// Print a memory write record
VOID RecordMemWrite(VOID * ip, VOID * addr)
{
    fprintf(trace,"%p: W %p\n", ip, addr);
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
        fprintf (trace,"RTN_Name: %s Start.\n", RTN_Name(rtn).c_str());

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
    fprintf(trace, "#eof n Start.\n");
    fclose(trace);
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

    trace = fopen("func_trace.out", "w");

    INS_AddInstrumentFunction(Instruction, 0);
    TRACE_AddInstrumentFunction(Trace, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}