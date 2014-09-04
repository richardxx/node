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

#include "jsweeter.hh"

// Record of a single object, array, and function
class InstanceDescriptor
{
public:
  // Internal id and raw address
  int id, raw_addr;
  // Whether the backing storage for properties and elements are dictionary
  bool prop_dict, elem_dict;
  // Is this instance watched for some purpose?
  bool is_watched;
  // Next operation on this object changes map transition graph and invalidates all operations dependent on these maps
  bool force_deopt; 
  // State machine that contains this instance
  StateMachine* sm;
  // Birth information for this instance
  TransPacket* birth_place;

 public:
  InstanceDescriptor() 
  { 
    id = raw_addr = -1;
    prop_dict = elem_dict = false;
    is_watched = false;
    force_deopt = false;
    sm = NULL;
    birth_place = NULL;
  }
   
public:
  // Return the state for this instance
  State* location();
  // Search if this instance owned his_s in history
  bool has_history_state(State* his_s);
  // Search if this instance owned his_map in history
  bool has_history_map(Map* his_map);
};


// The information asscociated to a transition
class TransPacket
{
 public:
  struct ptr_cmp
  {
    bool operator()(TransPacket* lhs, TransPacket* rhs)
    {
      return *lhs < *rhs;
    }
  };

 public:
  // The transition that holds this packet
  Transition* trans;
  // Why this transition happened?
  string reason;
  // Cost of this transition
  int cost;
  // Contexts are the call chain for locating an event
  vector<StateMachine*> contexts;
  // The number of instances that go through this transition
  int count;

 public:
  TransPacket();
  TransPacket(const char*, vector<StateMachine*>&, int);
  TransPacket(const TransPacket& other ) { *this = other; }

  //
  bool has_reason() { return reason.size() != 0; }
  //
  bool describe(std::stringstream& ss, bool = true) const;
  //
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
  
 public:  
  Transition();
  
  Transition(State* s_, State* t_);
  
  virtual TransType type() { return TNormal; }

  // Insert a new transition reason
  TransPacket* insert_reason(const char* r, vector<StateMachine*> &contexts, int cost = 0);
  TransPacket* insert_reason(TransPacket* tp);

  // Generate a single string for all reasons
  void merge_reasons(string& final, bool = true);

  // Search the reason that begins with r
  TransPacket* reason_begin_with(const char* r);

  // Decide if there is a reson other than the specified one
  bool reason_other_than(const char* r);

  // Tell the visualizer how to draw this transition
  virtual const char* graphviz_style();
};


// Summarize the states transitions on another machine similar to recursive automaton
// An example usage is cloning a boilerplate
class SummaryTransition : public Transition
{
 public:
  // From which state on another machine, it exits and returns back
  StateMachine* boilerplate;

 public:
  SummaryTransition() { boilerplate = NULL; }
  SummaryTransition(State* s_, State* t_)
    : Transition(s_, t_) { boilerplate = NULL; }
  SummaryTransition(State* s_, State* t_, StateMachine* bp_ )
    : Transition(s_, t_) { boilerplate = bp_; }

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
  // The shortest distance from root to this state
  int depth;
  // Parent link to form the shortest path tree
  Transition* parent_link;
  // The state machine that contains this state
  StateMachine* machine;

 public:
  State();

  void set_machine(StateMachine* sm) { machine = sm; }
  StateMachine* get_machine() { return machine; }

  // Return the number of transitions emanating from this state
  int size() { return out_edges.size(); }

  // find or create a transition to state next_s
  // is_missing = true if the evolution is caused by unknown reasons
  Transition* transfer(State* next_s, ObjectMachine* boilerplate, bool is_missing = false);

public:
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
  // Bind the map to the state (i.e. track the state<->map coupling)
  virtual Map* get_map() const = 0;
  virtual void set_map(Map*) = 0;
  // Do not track the usage of the map
  virtual void attach_map(Map* a_map) = 0;

private:
  // Search the transition with specified destinate state
  Transition* find_transition( State* next_s, bool by_boilerplate = false );
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

public:
  void attach_map(Map* a_map) { map_d = a_map; }

public:
  bool less_than(const State* other) const;   
  State* clone() const;
  string toString() const;
  const char* graphviz_style() const;
  Stype type() const { return SObject; }
  Map* get_map() const { return map_d; }
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
  
  void attach_code(Code* a_code) { code_d = a_code; }
  void set_code(Code*);
  Code* get_code() const { return code_d; }

public:
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
  State *start;
  // Name of this machine
  string m_name;
  // Record the type of this machine
  Mtype type;
  
 public:
  
  void set_name(const char* name) { m_name.assign(name); }
  
  bool has_name() { return m_name.size() != 0; }
  
  string toString(bool succinct = false);

  // If this allocation source is from library code
  bool is_in_lib();

  // Return the number of nodes+edges
  int size();

  // Return the next usable ID for state
  int get_next_id() { return states.size(); }

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
  State* find_instance(int d, bool new_instance=false);
  
  // Add an instance to this automaton
  void add_instance(int, State*);
  
  // Replace an instance name to with a given name
  void rename_instance(int,int);
  
  // Move an instance to another state
  void migrate_instance(int, TransPacket*);

  // Search down the tree from cur_s to end_s
  // Return the distance between the two states (-1 means not found)
  int forward_search_path(State* cur_s, State* end_s, deque<Transition*> *path);
  
  // Search up the tree from cur_s to end_s (-1 means not found)
  int backward_search_path(State* cur_s, State* end_s, deque<Transition*> *path);

  // Output graphviz instructions to draw this machine
  // m_id is used as the id of this machine
  // if sig != NULL, draw this machine iff the name contains the string sig
  void draw_graphviz(FILE* file, const char* sig);

 protected:
  // Not available for instantialization
  StateMachine();

public:
  static Map *start_map, *hole_map;
  static Code *start_code;

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
  //TransPacket* set_instance_map(InstanceDescriptor*, int map_d);

  // new_map_id == -1: reuse current map
  TransPacket* evolve(InstanceDescriptor*, vector<StateMachine*>&, int old_map_id, int new_map_id, 
		      ObjectMachine* boilerplate, const char* = "", int = 0, bool = false );

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
  string optMsg;
   
 public:
  FunctionMachine();

  // A specialized version that searches function states only
  State* search_state(Map*, Code*, bool=true);  
  // Turn on/off the opt
  void set_opt_state( bool allow, const char* msg );
  // Record a deoptimization
  void add_deopt(int);
  // 
  void check_bailouts();
  // Evolve to the next state
  TransPacket* evolve(InstanceDescriptor*, int, int, 
		      const char* = "", int = 0, bool = false);
};


// Public functions
void 
print_transition(Transition* trans, bool prt_src = true, bool prt_trans = true, 
		 bool prt_tgt = true, const char* line_header="\t", const char dir='|');

void 
print_path(deque<Transition*>& path, const char* title, int skip_n = 0);

#endif
