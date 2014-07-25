// For building the typestate automata
// By richardxx, 2013.8

#ifndef SM_BASICS_H
#define SM_BASICS_H

#include "automata.hh"


// initialize data structures
extern void 
prepare_machines();


// Destruct data structures
extern void 
clean_machines();


// Construct the state machine from the log file
bool
build_automata(const char*);


// Find (and create) machine from signature
StateMachine* 
find_signature(int m_sig, StateMachine::Mtype type, bool create=false);


// find (and create) instance descriptor
InstanceDescriptor*
find_instance( int ins_addr, StateMachine::Mtype type, bool create_descriptor=false, bool create_sm=false);


// Draw the state machines in graphviz format
extern void 
visualize_machines(const char*);



#endif
