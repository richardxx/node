// Implementation of the inline functions of state machine
// by richardxx, 2013

#include <queue>
#include <climits>
#include "options.h"
#include "automata.hh"

using namespace std;

// Global data
static ObjectState temp_o;
static FunctionState temp_f;

static Map* start_map = new Map(INT_MAX);
static Map* hole_map = new Map(INT_MAX-1);
static Code* start_code = new Code(INT_MAX);


TransPacket::TransPacket()
{
  trans = NULL;
  reason.clear();
  cost = 0;
  context = NULL;
  count = 0;
}


TransPacket::TransPacket(const char* desc)
{
  trans = NULL;
  reason.assign(desc);
  cost = 0;
  context = NULL;
  count = 0;
}


TransPacket::TransPacket(const char* desc, int c_)
{
  trans = NULL;
  reason.assign(desc);
  cost = c_;
  context = NULL;
  count = 0;
}


bool TransPacket::has_reason()
{
  return reason.size() != 0;
}

  
void TransPacket::describe(stringstream& ss) const
{
  //if ( count > 1 ) ss << count;

  ss << "(";
  if ( context != NULL ) {
    ss << context->toString(true);
    ss << ", ";
  }
  ss << reason << ")";
}


bool TransPacket::operator<(const TransPacket& rhs) const
{
  return reason < rhs.reason;
}


TransPacket& TransPacket::operator=(const TransPacket& rhs)
{
  reason = rhs.reason;
  cost = rhs.cost;
  context = rhs.context;
  return *this;
}


Transition::Transition()
{
  source = NULL;
  target = NULL;
  last_ = NULL;
}


Transition::Transition(State* s_, State* t_)
{
  source = s_;
  target = t_;
  last_ = NULL;
}


void 
Transition::insert_reason(const char* r, int cost)
{
  TransPacket *tp = new TransPacket(r, cost);
  
  TpSet::iterator it = triggers.find(tp);
  if ( it != triggers.end() ) {
    delete tp;
    TransPacket* old_tp = *it;
    // It does not invalidate the order in triggers
    old_tp->cost += cost;
    tp = old_tp;
  }
  else {
    triggers.insert(tp);
  }

  tp->trans = this;
  tp->count++;
  last_ = tp;
}


void Transition::insert_reason(TransPacket* tp)
{
  TpSet::iterator it = triggers.find(tp);

  if ( it != triggers.end() ) {
    TransPacket* old_tp = *it;
    old_tp->cost += tp->cost;
    tp = old_tp;
  }
  else {
    TransPacket *new_tp = new TransPacket;
    *new_tp = *tp;
    tp = new_tp;
    triggers.insert(tp);
  }
  
  tp->trans = this;
  tp->count++;
  last_ = tp;
}


void 
Transition::merge_reasons(string& final, bool extra_newline)
{
  int i = 0;
  int cost = 0;
  stringstream ss;
  
  TpSet::iterator it = triggers.begin();
  TpSet::iterator end = triggers.end();

  for ( ; it != end; ++it ) {
    if ( i >= 30 ) break;
    if ( i > 0 ) {
      ss << "+";
      if ( extra_newline ) ss << "\\n";
    }
    TransPacket* tp = *it;
    tp->describe(ss);
    cost += tp->cost;
    ++i;
  }
  
  if ( it != end ) {
    if ( extra_newline ) ss << "+\\n";
    ss << "(More...)";
  }
  
  if ( cost != 0 )
    ss << "$$" << cost;
  
  final = ss.str();
  if ( final.size() == 0 )
    final = "?";
}


const char* 
Transition::graphviz_style()
{
  const char* trans_style = "style=dotted";

  if (target -> parent_link == this)
    trans_style = "style=solid";
  
  return trans_style;
}


SummaryTransition::SummaryTransition()
  : Transition()
{
  exit = NULL;
}


SummaryTransition::SummaryTransition( State* s_, State* t_, State* exit_ )
  : Transition(s_,t_)
{
  exit = exit_;
}


// const char* 
// SummaryTransition::graphviz_style()
// {
//   return "style=dotted";
// }


State::State() 
{
  id = -1;
  parent_link = NULL;
  is_missing = false;
  machine = NULL;
  out_edges.clear();
}


int 
State::size() 
{ 
  return out_edges.size(); 
}


Transition* 
State::find_transition( State* next_s, bool by_boilerplate )
{
  TransMap::iterator it = out_edges.find(next_s);
  if ( it == out_edges.end() ) return NULL;
  
  Transition* ans = it->second;
  if ( by_boilerplate && ans->type() != Transition::TSummary ) return NULL;
  return ans;
}


Transition* 
State::add_transition( State* next_s )
{
  Transition* trans = new Transition(this, next_s);
  out_edges[next_s] = trans;

  if ( next_s->parent_link == NULL ) {
    // We can make a loop if this object is in slow mode
    // We only keep the link to the immediate dominator
    next_s->parent_link = trans;
  }

  return trans;
}


Transition* 
State::add_summary_transition( State* next_s, State* exit_s )
{
  Transition* trans = new SummaryTransition(this, next_s, exit_s);
  out_edges[next_s] = trans;

  if ( next_s->parent_link == NULL )
    next_s->parent_link = trans;

  return trans;
}


Transition* 
State::transfer(State* maybe_next_s, ObjectMachine* boilerplate)
{
  Transition* trans;
  State* next_s;

  // Search for the same state
  trans = find_transition(maybe_next_s, boilerplate != NULL);
  
  if ( trans == NULL ) {
    // Not exist
    // We try to search and add to the state machine pool
    next_s = machine->search_state(maybe_next_s);

    if ( boilerplate != NULL ) {
      State* exit_s = boilerplate->exit_state();
      trans = add_summary_transition(next_s, exit_s);
    }
    else {
      trans = add_transition(next_s);
    }
  }
  
  // Update transition
  return trans;
}


ObjectState::ObjectState() 
{ 
  id = 0;
  machine = NULL;
  map_d = null_map; 
}


ObjectState::ObjectState( int my_id )
{
  id = my_id;
  machine = NULL;
  map_d = null_map;
}


void ObjectState::set_map(Map* new_map)
{
  /*
  if ( map_d != null_map )
    map_d->remove_usage(this);
  */

  map_d = new_map;
  map_d->add_usage(this);
}

Map* ObjectState::get_map() const
{
  return map_d;
}


bool ObjectState::less_than(const State* other) const
{
  return map_d->id() < other->get_map()->id();
}


State* ObjectState::clone() const
{
  ObjectState* new_s = new ObjectState;
  new_s->set_map(map_d);
  new_s->machine = machine;
  return new_s;
}


string ObjectState::toString() const 
{
  stringstream ss;

  if ( id == 0 )
    ss << machine->m_name;
  else
    ss << hex << map_d->map_id;
  
  return ss.str();
}

const char* ObjectState::graphviz_style() const
{
  const char* style = NULL;

  if ( id == 0 )
    // id = 0 indicates this is the starting state
    style = "shape=doublecircle";
  else
    style = "shape=egg"; 
  
  return style;
}


FunctionState::FunctionState() 
{ 
  id = 0;
  machine = NULL;
  map_d = null_map;
  code_d = null_code;
}


FunctionState::FunctionState( int my_id )
{
  id = my_id;
  machine = NULL;
  map_d = null_map;
  code_d = null_code;
}
  

void FunctionState::set_code(Code* new_code)
{
  code_d = new_code;
  code_d->add_usage(this);
}


Code* FunctionState::get_code()
{
  return code_d;
}


// Implement virtual functions
bool FunctionState::less_than(const State* other) const
{
  switch (other->type()) {
  case State::SObject:
    return !other->less_than(this);
    
  case State::SFunction:
    {
      const FunctionState* o_ = (const FunctionState*)other;
      int diff = code_d->id() - o_->code_d->id();
      if ( diff == 0 )
	return map_d->id() < o_->map_d->id();
      return diff < 0;
    }
  }
  
  return this < other;
}


State* FunctionState::clone() const
{
  FunctionState* new_s = new FunctionState;
  new_s->set_map( map_d );
  new_s->set_code( code_d );
  new_s->machine = machine;
  return new_s;
}


string FunctionState::toString() const 
{
  stringstream ss;

  if ( id == 0 )
    ss << machine->m_name;
  else
    ss << hex << code_d->code_id;
  
  return ss.str();
}

int StateMachine::id_counter = 0;

StateMachine* 
StateMachine::NewMachine( StateMachine::Mtype type )
{
  StateMachine* sm = NULL;

  switch( type ) {
  case MBoilerplate:
  case MObject:
    sm = new ObjectMachine(type);
    break;

  case MFunction:
    sm = new FunctionMachine();
    break;

  default:
    break;
  }

  sm->id = id_counter++;
  //if ( sm->id == 10256 )
  //printf( "come on\n" );
  return sm;
}


StateMachine::StateMachine()
{
  states.clear();
  inst_at.clear();
  m_name.clear();
}


void 
StateMachine::set_name(const char* name)
{
  m_name.assign(name);
}


bool 
StateMachine::has_name()
{
  return m_name.size() != 0;
}


string 
StateMachine::toString(bool succinct)
{
  stringstream ss;

  if ( !succinct ) ss << m_name << "(";
  ss << (type == MFunction ? "F" : "O") << id;
  if ( !succinct ) ss << ")";
  return ss.str();
}


int 
StateMachine::size() 
{
  int ans = 0;

  for ( StatesPool::iterator it = states.begin(),
	  end = states.end(); it != end; ++it ) {
    State* s = *it;
    ans += s->size();
  }

  return ans + states.size(); 
}


int
StateMachine::count_instances()
{
  int ans = 0;

  State::TransIterator it, end;
  State::TransMap &t_edges = start->out_edges;
  
  for ( it = t_edges.begin(), end = t_edges.end();
	it != end; ++it ) {
    Transition* trans = it->second;
    
    Transition::TpSetIterator tp_it, tp_end;
    Transition::TpSet &packs = trans->triggers;
    
    for ( tp_it = packs.begin(), tp_end = packs.end();
	  tp_it != tp_end; ++tp_it )
      ans += (*tp_it)->count;
  }
  
  return ans;
}


State* 
StateMachine::search_state(State* s, bool create)
{
  StatesPool::iterator it = states.find(s);
  
  if ( it == states.end() ) {
    if ( create ) {
      s = s->clone();
      s->id = states.size();
      add_state(s);
    }
    else {
      s = NULL;
    }
  }
  else
    s = *it;
  
  return s;
}


void 
StateMachine::add_state(State* s)
{
  states.insert(s);
}

void 
StateMachine::delete_state(State* s)
{
  states.erase(s);
}


State* 
StateMachine::get_instance_pos(int d, bool new_instance)
{
  State* s = NULL;
  
  map<int, State*>::iterator it = inst_at.find(d);
  if ( it == inst_at.end() ) {
    // Add this instance
    s = start;
    inst_at[d] = s;
  }
  else {
    // We obtain the old position first
    s = it->second;

    if ( new_instance == true ) {
      // This instance has been reclaimed by GC and it is allocated to the same object again
      s = start;
      inst_at[d] = s;
    }
  }    
  
  return s;
}


void 
StateMachine::add_instance(int ins_name, State* s)
{
  inst_at[ins_name] = s;
}


void 
StateMachine::rename_instance(int old_name, int new_name)
{
  map<int, State*>::iterator it = inst_at.find(old_name);
  if ( it == inst_at.end() ) return;
  State* s = it->second;
  inst_at.erase(it);
  inst_at[new_name] = s;
}


void 
StateMachine::migrate_instance(int ins_d, Transition* trans)
{
  State* src = trans->source;
  State* tgt = trans->target;

  // Update instance <-> state maps
  inst_at[ins_d] = tgt;
  
  // Call monitoring action
  if ( do_analyze ) {
    Map* src_map = ((ObjectState*)src)->get_map();
    src_map->deopt_deps(trans);
  }
}


// We search backwardly and push the deque in front
int
StateMachine::forward_search_path(State* cur_s, State* end_s, deque<Transition*>& path)
{
  int dist = 0;
  
  State* temp = cur_s;
  cur_s = end_s;
  end_s = temp;
  
  while ( cur_s != end_s && cur_s != start ) {
    Transition* trans = cur_s->parent_link;
    path.push_front(trans);
    dist++;
    cur_s = trans->source;
  }

  if ( cur_s == start && end_s != start ) {
    // the path does not exist
    path.clear();
    return -1;
  }

  return dist;
}


int 
StateMachine::backward_search_path(State* cur_s, State* end_s, deque<Transition*>& path)
{
  int dist = 0;
  
  while ( cur_s != end_s && cur_s != start ) {
    Transition* trans = cur_s -> parent_link;
    path.push_back(trans);
    dist++;
    cur_s = trans->source;
  }

  if ( cur_s == start && end_s != start ) {
    // the path does not exist
    path.clear();
    return -1;
  }
  
  return dist;
}  


void
StateMachine::draw_graphviz(FILE* file, const char* sig)
{
  queue<State*> bfsQ;
  set<State*> visited;
  
  if ( sig != NULL && 
       m_name.find(sig) == string::npos )
    return;

  //const char *c = "O";
  //if ( type == MFunction ) c = "F";
  fprintf(file, "digraph %s {\n", toString(true).c_str() );
  
  // Output global settings
  fprintf(file, "\tnode[nodesep=2.0];\n");
  fprintf(file, "\tgraph[overlap=false];\n");

  // Go over the state machine
  State* init_state = this->start;
  visited.clear();
  visited.insert(init_state);
  bfsQ.push(init_state);
  
  while ( !bfsQ.empty() ) {
    State* cur_s = bfsQ.front();
    bfsQ.pop();
    
    // We first generate the node description
    int id = cur_s->id;
    fprintf(file, 
	    "\t%d [%s, label=\"%s\"];\n", 
	    id, 
	    cur_s->graphviz_style(),
	    cur_s->toString().c_str());
    
    // We draw the transition edges
    State::TransIterator it, end;
    State::TransMap &t_edges = cur_s->out_edges;
    
    for ( it = t_edges.begin(), end = t_edges.end();
	  it != end; ++it ) {
      Transition* trans = it->second;
      State* next_s = trans->target;
      if ( visited.find(next_s) == visited.end() ) {
	bfsQ.push(next_s);
	visited.insert(next_s);
      }

      // Generate the transition descriptive string
      string final;
      trans->merge_reasons(final);
      fprintf(file, 
	      "\t%d -> %d [%s, label=\"%s\"];\n",
	      id, 
	      next_s->id,
	      trans->graphviz_style(),
	      final.c_str());
    }
  }
  
  fprintf(file, "}\n\n");
}


void ObjectMachine::_init(StateMachine::Mtype _type)
{
  start = new ObjectState(0);
  start->set_machine(this);
  start->set_map(start_map);
  add_state(start);

  hole = new ObjectState(1<<30);
  hole->set_machine(this);
  hole->set_map(hole_map);
  add_state(hole);

  type = _type;
  is_boilerplate = (_type == StateMachine::MBoilerplate);
  cause_deopt = false;
}


ObjectMachine::ObjectMachine()
{
  _init(StateMachine::MObject);
}


ObjectMachine::ObjectMachine(StateMachine::Mtype _type)
{
  _init(_type);
}

State* 
ObjectMachine::search_state(Map* exp_map, bool create)
{
  temp_o.map_d = exp_map;
  temp_o.machine = this;
  return StateMachine::search_state(&temp_o, create);
}


State* 
ObjectMachine::exit_state()
{
  State* exit_s = NULL;

  for ( map<int, State*>::iterator it = inst_at.begin();
	it != inst_at.end(); ++it ) {
    State* cur_s = it->second;
    if ( exit_s == NULL )
      exit_s = cur_s;
    else if ( exit_s != cur_s )
      return NULL;
  }
  
  return exit_s;
}


ObjectState*
ObjectMachine::jump_to_state_with_map(InstanceDescriptor* i_desc, int exp_map_id, bool new_instance)
{
  int ins_id = i_desc->id;
  ObjectState *cur_s = (ObjectState*)get_instance_pos(ins_id, new_instance);
  if ( exp_map_id == -1 ) return cur_s;
  
  Map* exp_map = find_map(exp_map_id);
  
  if ( cur_s->get_map() != exp_map ) {
    ObjectState* exp_s = (ObjectState*)search_state(exp_map, true);
    exp_s->is_missing = true;

    // And we make a link: cur_s -> exp_s
    Transition* trans = cur_s->transfer(exp_s, NULL);
    trans->insert_reason("?", 0);
    
    inst_at[ins_id] = exp_s;
    cur_s = exp_s;
  }
  
  return cur_s;
}


Transition* 
ObjectMachine::set_instance_map(InstanceDescriptor* i_desc, int map_id)
{
  int ins_id = i_desc->id;

  // Obtain current position of this instance
  State *cur_s = get_instance_pos(ins_id);
  
  // Build the target state
  // Using set_map will register this state to the map
  // We do not register for temporary variable
  temp_o.map_d = find_map(map_id);
  temp_o.machine = this;
  
  Transition* trans = cur_s->transfer( &temp_o, NULL );
  trans->insert_reason("?", 0);

  return trans;
}


Transition* 
ObjectMachine::evolve(InstanceDescriptor* i_desc, int old_map_id, int new_map_id, 
		      ObjectMachine* boilerplate, const char* trans_dec, int cost, bool new_instance)
{
  int ins_id = i_desc->id;

  // Now we check the validity of this state
  // In case we might miss something
  ObjectState* cur_s = jump_to_state_with_map(i_desc, old_map_id, new_instance);

  // Build the target state
  temp_o.map_d = (new_map_id == -1 ? cur_s->get_map() : find_map(new_map_id));
  temp_o.machine = this;

  // Transfer state
  Transition* trans = cur_s->transfer( &temp_o, boilerplate );
  trans->insert_reason(trans_dec, cost);

  // Renew the position of this instance
  migrate_instance(ins_id, trans);

  return trans;
}


FunctionMachine::FunctionMachine()
{
  // Fields
  type = StateMachine::MFunction;
  been_optimized = false;
  allow_opt = true;
  
  // Create the initil state
  start = new FunctionState(0);
  start->set_machine(this);
  ((FunctionState*)start)->set_code(start_code);
  add_state(start);

  //
  total_deopts = 0;
  deopt_counts.clear();
}


State* 
FunctionMachine::search_state(Map* exp_map, Code* exp_code, bool create)
{
  temp_f.map_d = exp_map;
  temp_f.code_d = exp_code;
  temp_f.machine = this;
  return StateMachine::search_state(&temp_f, create);
}

 
void 
FunctionMachine::set_opt_state( bool allow, const char* msg )
{
  allow_opt = allow;
  optMsg.assign(msg);
}


void
FunctionMachine::add_deopt( int bailout_id )
{
  deopt_counts[bailout_id]++;
  total_deopts++;
}


void
FunctionMachine::check_bailouts()
{
  map<int, int>::iterator it, end;

  if ( total_deopts < 2 ) return;

  for ( it = deopt_counts.begin(), end = deopt_counts.end();
	it != end; ++it ) {
    int bailout_id = it->first;
    int count = it->second;
    if ( count >= 0.4 * total_deopts ) {
      printf( "FactorOut: In %s, IC %d occupies %.1f%% of %d deopts.\n", 
	      m_name.c_str(), bailout_id, (double)count/total_deopts * 100, total_deopts ); 
    }
  }
}


Transition* 
FunctionMachine::evolve(InstanceDescriptor* i_desc, int map_id, int code_id, 
			const char* trans_dec, int cost, bool new_instance)
{ 
  int ins_id = i_desc->id;

  // Obtain current position of this instance
  FunctionState *cur_s = (FunctionState*)get_instance_pos(ins_id, new_instance);

  // Build the target state
  temp_f.map_d = (map_id == -1 ? cur_s->get_map() : find_map(map_id));
  temp_f.code_d = (code_id == -1 ? cur_s->get_code() : find_code(code_id));
  temp_f.machine = this;
  
  // Transfer state
  Transition* trans = cur_s->transfer( &temp_f, NULL );
  trans->insert_reason(trans_dec, cost);

  // Renew the position of this instance
  migrate_instance(ins_id, trans);

  return trans;
}


bool
InstanceDescriptor::has_history_state(State* his_s)
{
  State* cur_s = sm->get_instance_pos(id);
  if ( cur_s == NULL ) 
    return -1;

  State* q0 = sm->start;

  while ( cur_s != his_s && cur_s != q0 ) {
    cur_s = cur_s -> parent_link->source;
  }

  return cur_s == his_s;
}

bool
InstanceDescriptor::has_history_map(Map* his_map)
{
  State* his_s = his_map->to_state();
  return has_history_state(his_s);
}
