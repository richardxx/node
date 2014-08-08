// Description of state machine

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <cstdio>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <set>
#include <deque>

using std::string;
using std::vector;
using std::set;
using std::map;
using std::stringstream;
using std::deque;

class Transition;
class State;
class StateMachine;
class ObjectMachine;
class FunctionMachine;
class InstanceDescriptor;

#include "type-info.hh"

// The information asscociated to a transition
class TransPacket
{
 public:
  struct ptr_cmp
  {
    bool operator()(TransPacket* lhs, TransPacket* rhs)
    {
      return (lhs->reason) < (rhs->reason);
    }
  };

 public:
  // The transition that holds this packet
  Transition* trans;
  // Why this transition happened?
  string reason;
  // Cost of this transition
  int cost;
  // For an object event, context is the function where this operation happens
  StateMachine* context;
  // The number of instances that go through this transition
  int count;

 public:
  TransPacket();
  TransPacket(const char* desc);
  TransPacket(const char* desc, int c_);

  bool has_reason();

  void describe(std::stringstream& ss) const;
  
  bool operator<(const TransPacket& rhs) const;

  TransPacket& operator=(const TransPacket& rhs);
};


// States transition edge
class Transition
{
 public:
  enum TransType
  {
    TNormal,
    TSummary
  };

  typedef std::set<TransPacket*, TransPacket::ptr_cmp> TpSet;
  typedef TpSet::iterator TpSetIterator;

 public:
  // Transition target
  State *source, *target;
  // Transision triggering operations and their cost
  TpSet triggers;
  // Last triggering operation
  TransPacket* last_;
  
 public:  
  Transition();
  
  Transition(State* s_, State* t_);
  
  virtual TransType type() { return TNormal; }

  void insert_reason(const char* r, int cost = 0);

  void insert_reason(TransPacket* tp);

  void merge_reasons(string& final, bool = true);

  // Tell the visualizer how to draw this transition
  virtual const char* graphviz_style();
};


// Summarize the states transitions on another machine similar to recursive automaton
// An example usage is cloning a boilerplate
class SummaryTransition : public Transition
{
 public:
  // From which state on another machine, it exits and returns back
  State *exit;

 public:
  SummaryTransition();

  SummaryTransition(State* s_, State* t_, State* exit_ );

  TransType type() { return TSummary; }

  //const char* graphviz_style();
};


// Base class for all different states
class State 
{
 public:
  struct ptr_cmp
  {
    bool operator()(State* lhs, State* rhs)
    {
      return lhs->less_than( rhs );
    }
  };
  
  typedef std::map<State*, Transition*, ptr_cmp> TransMap;
  typedef TransMap::iterator TransIterator;

  enum Stype
  {
    SObject,
    SFunction
  };

 public:
  // ID for this state
  int id;
  // Outgoing transition edges
  TransMap out_edges;
  // Parent link
  Transition* parent_link;
  // We didn't track the evolution trail to this state
  bool is_missing;
  // The state machine that contains this state
  StateMachine* machine;

 public:
  State();
  // Return the number of transitions emanating from this state
  int size();
  // Search the transition with specified destinate state
  Transition* find_transition( State* next_s, bool by_boilerplate = false );
  //
  Transition* add_transition( State* next_s );
  // 
  Transition* add_summary_transition( State* next_s, State* exit_s );
  // Update the mapping between instances and map/code
  Transition* transfer(State* next_s, ObjectMachine* boilerplate);

public:
  // Interfaces
  void set_machine(StateMachine* sm) { machine = sm; }
  // Used for state search in set
  virtual bool less_than( const State* ) const = 0;
  // Make a clone of this state
  virtual State* clone() const = 0;
  // Generate a text description for this state
  virtual string toString() const = 0;
  // Generate graphviz style descriptor
  virtual const char* graphviz_style() const = 0;
  //
  virtual Stype type() const = 0;
  //
  virtual Map* get_map() const = 0;
  //
  virtual void set_map(Map*) = 0;
};


// Describe an object
class ObjectState : public State
{
 public:
  //
  Map* map_d;
  
 public:
  ObjectState();
  ObjectState( int my_id );

  // Implement virtual functions
  bool less_than(const State* other) const;   
  State* clone() const;
  string toString() const;
  const char* graphviz_style() const;
  Stype type() const { return SObject; }
  Map* get_map() const;
  void set_map(Map*);
};


// Describe a function
// Function is also an object
class FunctionState : public ObjectState
{
 public:
  Code* code_d;
  
 public:
  FunctionState();
  FunctionState( int my_id ); 

  void set_code(Code*);
  Code* get_code();

  // Implement virtual functions
  bool less_than(const State* other) const;
  State* clone() const;
  string toString() const;
  Stype type() const { return SFunction; }
};


// A state machine maintains a collection of states
class StateMachine
{
 public:
  typedef std::set<State*, State::ptr_cmp> StatesPool;

  enum Mtype {
    MBoilerplate,
    MObject,
    MFunction,
    MCount       // Record how many different machines
  };

  // The only way to create an instance of state machine is calling this static function
  static StateMachine* NewMachine(Mtype);

 public:
  //
  int id;
  // Record all the states belonging to this machine
  StatesPool states;
  // Map object/function instances to states
  map<int, State*> inst_at;
  // start: Start state of this machine
  // hole: represents all the missing states
  State *start, *hole;
  // Name of this machine
  string m_name;
  // Record the type of this machine
  Mtype type;
  
 public:
  
  void set_name(const char* name);
  
  bool has_name();
  
  string toString(bool succinct = false);
  
  // Return the number of states
  int size();

  // Return the number of instances for this machine
  int count_instances();

  // Lookup if the given state has been created for this machine
  // If not, we copy the content of input state to create a new state
  State* search_state(State*, bool create=true);
  
  // Directly add the input state to states pool
  void add_state(State*);
  
  // Clone the given state and insert into the states pool
  //State* add_clone_state(State*);
  // Directly delete the input state from states pool
  void delete_state(State*);
  
  // Lookup the state for a particular instance
  State* get_instance_pos(int d, bool new_instance=false);
  
  // Add an instance to this automaton
  void add_instance(int, State*);
  
  // Replace an instance name to with a given name
  void rename_instance(int,int);
  
  // Move an instance to another state
  void migrate_instance(int, Transition*);

  // Search down the tree from cur_s to end_s
  // Return the distance between the two states (-1 means not found)
  int forward_search_path(State* cur_s, State* end_s, deque<Transition*>& path);
  
  // Search up the tree from  cur_s to end_s (-1 means not found)
  int backward_search_path(State* cur_s, State* end_s, deque<Transition*>& path);

  // Output graphviz instructions to draw this machine
  // m_id is used as the id of this machine
  // if sig != NULL, draw this machine iff the name contains the string sig
  void draw_graphviz(FILE* file, const char* sig);

 protected:
  // Not available for instantialization
  StateMachine();

 private:
  static int id_counter; 
};


// Specialized machine for object states
class ObjectMachine : public StateMachine
{
 public:
  // Is this objct used as a boilerplate
  bool is_boilerplate;
  // Has this automaton caused deoptimization?
  bool cause_deopt;

 public:
  ObjectMachine();

  // We have one more constructor because object machine can support other types
  ObjectMachine(StateMachine::Mtype);

  // A specialized version of searching only object states
  State* search_state(Map*, bool=true);

  // Get exit state
  // If all instances are in the same state, we return this state as exit state
  // Otherwise it returns null
  State* exit_state();

  //
  ObjectState* jump_to_state_with_map(InstanceDescriptor*, int, bool);

  // Set transition that just sets map of an instance
  Transition* set_instance_map(InstanceDescriptor*, int map_d);

  // new_map_id == -1: reuse current map
  Transition* evolve(InstanceDescriptor*, int, int new_map_id, ObjectMachine*, const char*, int = 0, bool = false );

private:
  void _init(StateMachine::Mtype);
};


// Specialized state machine storing function states only
class FunctionMachine : public ObjectMachine
{
 public:
  //
  bool been_optimized;
  // Is this function approved for optimization?
  bool allow_opt;
  // Counting the number of deopts for each IC site inside this function
  map<int, int> deopt_counts;
  //
  int total_deopts;
  // Opt/deopt message
  std::string optMsg;
   
 public:
  FunctionMachine();

  // A specialized version of searching only function states
  State* search_state(Map*, Code*, bool=true);  
  // Turn on/off the opt
  void set_opt_state( bool allow, const char* msg );
  //
  void add_deopt(int);
  //
  void check_bailouts();
  // Evolve to the next state
  Transition* evolve(InstanceDescriptor*, int, int, const char*, int = 0, bool = false);
};

// Track the states transitions of a single object
class InstanceDescriptor
{
 public:
  // Internal id
  int id;
  // State machine that contains this instance
  StateMachine* sm;
  // Last transition for this intance
  Transition* last_raw_transition;

 public:
  InstanceDescriptor() { 
    id = -1; 
    sm = NULL;
    last_raw_transition = NULL;
  }
   
public:

  // Search if this instance owned his_s in history
  bool has_history_state(State* his_s);
  // Search if this instance owned his_map in history
  bool has_history_map(Map* his_map);
};

#endif
