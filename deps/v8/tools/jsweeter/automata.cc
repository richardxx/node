// Implementation of the inline functions of state machine
// by richardxx, 2013

#include <queue>
#include <climits>
#include <cassert>
#include <cstring>
#include "options.h"
#include "events.h"
#include "automata.hh"

using namespace std;

// Global data
static ObjectState temp_o;
static FunctionState temp_f;

Map* StateMachine::start_map = new Map(INT_MAX);
Code* StateMachine::start_code = new Code(INT_MAX);
int StateMachine::id_counter = 0;

State*
InstanceDescriptor::location()
{ 
  return sm->find_instance(id); 
}

bool
InstanceDescriptor::has_history_state(State* his_s)
{
  State* cur_s = location();
  if ( cur_s == NULL ) return -1;

  State* q0 = sm->start;

  while ( cur_s != his_s && cur_s != q0 ) {
    cur_s = cur_s->parent_link->source;
  }

  return cur_s == his_s;
}


bool
InstanceDescriptor::has_history_map(Map* his_map)
{
  State* his_s = his_map->to_state();
  return has_history_state(his_s);
}


TransPacket::TransPacket()
{
  trans = NULL;
  reason.clear();
  contexts.clear();
  cost = 0;
  count = 0;
}

TransPacket::TransPacket(const char* desc, vector<StateMachine*>& ctxts_, int c)
{
  trans = NULL;
  reason = desc;
  contexts = ctxts_;
  cost = c;
  count = 0;
}

  
bool
TransPacket::describe(stringstream& ss, bool prt_lib) const
{
  int size = contexts.size();
  
  if ( !prt_lib ) {
    // We test if the immediate context is in the lib
    StateMachine* context = contexts[0];
    if ( context->is_in_lib() ) return false;
  }
  
  ss << "(";
  for ( int i = size - 1; i > -1; --i ) {
    StateMachine* context = contexts[i];
    if ( i < size-1 ) ss << "-->";
    ss << context->toString(false);
  }
  ss << ", " << reason << ")";

  return true;
}


TransPacket& 
TransPacket::operator=(const TransPacket& rhs)
{
  trans = rhs.trans;
  reason = rhs.reason;
  cost = rhs.cost;
  contexts = rhs.contexts;
  count = rhs.count;
  return *this;
}


bool 
TransPacket::operator<(const TransPacket& rhs) const
{
  // Compare their reasons first
  int d = reason.compare(rhs.reason);

  if ( d == 0 ) {
    // They are the same, compare the contexts
    int size1 = contexts.size();
    int size2 = rhs.contexts.size();
    if ( size1 != size2 ) return size1 < size2;
    
    for ( int i = 0; i < size1; ++i ) {
      StateMachine* ctxt1 = contexts[i];
      StateMachine* ctxt2 = rhs.contexts[i];
      if ( ctxt1 != ctxt2 ) return ctxt1->id < ctxt2->id;
    }
    
    // They are the same!
    return false;
  }
  
  return d < 0;
}


Transition::Transition()
{
  source = NULL;
  target = NULL;
}


Transition::Transition(State* s_, State* t_)
{
  source = s_;
  target = t_;
}


TransPacket*
Transition::reason_begin_with(const char* r)
{
  TpSet::iterator it = triggers.begin();
  TpSet::iterator end = triggers.end();

  while ( it != end ) {
    TransPacket *tp = *it;
    if ( tp->reason.find(r) != string::npos )
      return tp;
    ++it;
  }

  return NULL;
}


bool
Transition::reason_other_than(const char* r)
{
  TpSet::iterator it = triggers.begin();
  TpSet::iterator end = triggers.end();
  
  while ( it != end ) {
    TransPacket *tp = *it;
    if ( tp->reason.find(r) != string::npos ) return true;
    ++it;
  }

  return false;
}


TransPacket*
Transition::insert_reason(const char* r, vector<StateMachine*> &contexts, int cost)
{
  TransPacket finder(r, contexts, cost);
  return insert_reason(&finder);
}


// The caller takes care of the memory of tp
TransPacket*
Transition::insert_reason(TransPacket* tp)
{
  TpSet::iterator it = triggers.find(tp);
  
  if ( it != triggers.end() ) {
    TransPacket* old_tp = *it;
    old_tp->cost += tp->cost;
    tp = old_tp;
  }
  else {
    TransPacket *new_tp = new TransPacket(*tp);
    tp = new_tp;
    triggers.insert(tp);
  }
  
  tp->trans = this;
  tp->count++;
  return tp;
}


void 
Transition::merge_reasons(string& final, bool extra_newline)
{
  int i = 0;
  int cost = 0;
  bool prt_plus = false;
  stringstream ss;
  
  TpSet::iterator it = triggers.begin();
  TpSet::iterator end = triggers.end();

  for ( ; it != end; ++it ) {
    if ( i >= 30 ) break;
    if ( prt_plus ) {
      ss << " + ";
      if ( extra_newline ) ss << "\\n";
    }
    TransPacket* tp = *it;
    if ( tp->describe(ss, false) ) {
      cost += tp->cost;
      prt_plus = true;
      ++i;
    }
    else {
      prt_plus = false;
    }
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


// const char* 
// SummaryTransition::graphviz_style()
// {
//   return "style=dotted";
// }


State::State() 
{
  id = -1;
  depth = 0x6fffffff;
  parent_link = NULL;
  machine = NULL;
  out_edges.clear();
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
State::transfer(State* maybe_next_s, ObjectMachine* boilerplate, bool is_missing)
{
  Transition* trans;
  State* next_s;

  // Search for the same state
  trans = find_transition(maybe_next_s, boilerplate != NULL);
  
  if ( trans == NULL ) {
    // Not exist, We add it
    next_s = machine->search_state(maybe_next_s);

    if ( boilerplate != NULL ) {
      trans = new SummaryTransition(this, next_s, boilerplate);
    }
    else {
      trans = new Transition(this, next_s);
    }
    
    if (!is_missing) {
      // Update the parent link only for regular evolutions
      if ( (depth+1) < next_s->depth ) {
	// We update the parent link to maintain the SPT
	next_s->parent_link = trans;
	next_s->depth = depth + 1;
      }
    }
    else if (next_s->parent_link == NULL) {
      // Just keep the node connected
      next_s->parent_link = trans;
    }
    
    out_edges[next_s] = trans;
  }

  return trans;
}


ObjectState::ObjectState()
  : State()
{ 
  attach_map(null_map); 
}


ObjectState::ObjectState( int my_id )
  : State()
{
  id = my_id;
  attach_map(null_map);
}


void 
ObjectState::set_map(Map* new_map)
{
  if ( map_d != null_map )
    map_d->remove_usage(this);
  
  map_d = new_map;
  map_d->add_usage(this);
}

bool 
ObjectState::less_than(const State* other) const
{
  return map_d->id() < other->get_map()->id();
}


State* 
ObjectState::clone() const
{
  ObjectState* new_s = new ObjectState;
  new_s->set_map(map_d);
  new_s->machine = machine;
  new_s->id = machine->get_next_id();
  machine->add_state(new_s);
  return new_s;
}


string 
ObjectState::toString() const 
{
  stringstream ss;

  if ( id == 0 )
    ss << machine->m_name;
  else
    ss << hex << map_d->map_id;
  
  return ss.str();
}

const char* 
ObjectState::graphviz_style() const
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
  : ObjectState()
{ 
  attach_code(null_code);
}


FunctionState::FunctionState( int my_id )
  : ObjectState(my_id)
{
  attach_code(null_code);
}
  

void 
FunctionState::set_code(Code* new_code)
{
  if ( code_d != null_code )
    code_d->remove_usage(this);
  
  code_d = new_code;
  code_d->add_usage(this);
}


// Implement virtual functions
bool 
FunctionState::less_than(const State* other) const
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
  new_s->id = machine->get_next_id();
  machine->add_state(new_s);
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
  return sm;
}


StateMachine::StateMachine()
{
  states.clear();
  inst_at.clear();
  m_name="";
}

bool 
StateMachine::is_in_lib()
{
  if ( has_name() ) {
    if ( m_name.find("v8natives.js") != string::npos ||
	 m_name.find("runtime.js") != string::npos ||
	 m_name.find("array.js") != string::npos ||
	 m_name.find("messages.js") != string::npos ||
	 m_name.find("string.js") != string::npos ||
	 m_name.find("regexp.js") != string::npos ||
	 m_name.find("date.js") != string::npos ||
	 m_name.find("json.js") != string::npos ||
	 m_name.find("math.js") != string::npos ||
	 m_name.find("uri.js") != string::npos ||
	 m_name.find("arraybuffer.js") != string::npos ||
	 m_name.find("typedarray.js") != string::npos )
      return true;
  }

  return false;
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

  return states.size(); 
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
  State *res = NULL;

  if ( it == states.end() ) {
    if ( create ) {
      res = s->clone();
    }
  }
  else
    res = *it;
  
  return res;
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
StateMachine::find_instance(int d, bool new_instance)
{
  State* s = NULL;
  
  map<int, State*>::iterator it = inst_at.find(d);

  if ( it != inst_at.end() &&
       new_instance == false ) {
    // We obtain the old position
    s = it->second;
  }
  else {
    // This is a new instance or reallocated to another object
    s = start;
    inst_at[d] = s;
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
StateMachine::migrate_instance(int ins_d, TransPacket* tp)
{
  Transition *trans = tp->trans;
  State* src = trans->source;
  State* tgt = trans->target;

  // Update instance <-> state maps
  inst_at[ins_d] = tgt;
  
  // Call monitoring action
  if ( do_analyze ) {
    Map* src_map = ((ObjectState*)src)->get_map();
    src_map->deopt_deps(tp);
  }
}


// Actually we search backwardly and push the deque in front
int
StateMachine::forward_search_path(State* cur_s, State* end_s, deque<Transition*> *path)
{
  int dist = 0;
  
  // Swap the ends
  State* temp = cur_s;
  cur_s = end_s;
  end_s = temp;
  
  while ( cur_s != end_s ) {
    // The search only follows parent link, which is incomplete
    Transition* trans = cur_s->parent_link;
    if ( trans == NULL ) break;
    dist++;
    cur_s = trans->source;
    if ( path != NULL ) path->push_front(trans);
  }

  if ( cur_s == start && end_s != start ) {
    // the path does not exist
    if ( path != NULL ) path->clear();
    return -1;
  }

  return dist;
}


int 
StateMachine::backward_search_path(State* cur_s, State* end_s, deque<Transition*> *path)
{
  int dist = 0;
  
  while ( cur_s != end_s ) {
    Transition* trans = cur_s -> parent_link;
    if ( trans == NULL ) break;
    dist++;
    cur_s = trans->source;
    if ( path != NULL ) path->push_back(trans);
  }

  if ( cur_s == start && end_s != start ) {
    // the path does not exist
    if ( path != NULL ) path->clear();
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


void 
ObjectMachine::_init(StateMachine::Mtype _type)
{
  start = new ObjectState(0);
  start->set_machine(this);
  start->attach_map(start_map);
  start->depth = 0;
  add_state(start);

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
  ObjectState *cur_s = (ObjectState*)find_instance(ins_id, new_instance);
  if ( exp_map_id == -1 ) return cur_s;
  
  Map* exp_map = find_map(exp_map_id);
  if ( cur_s->get_map() != exp_map ) {
    // And we make a missing link: cur_s -> exp_s
    ObjectState* exp_s;

    if ( !exp_map->has_bound() ) {
      temp_o.map_d = exp_map;
      temp_o.machine = this;
      exp_s = &temp_o;
    }
    else {
      exp_s = (ObjectState*)exp_map->to_state();
    }

    Transition* trans = cur_s->transfer(exp_s, NULL, true);
    vector<StateMachine*> contexts;
    contexts.push_back(miss_context);
    trans->insert_reason("?", contexts);
    
    inst_at[ins_id] = exp_s;
    cur_s = exp_s;
  }
  
  return cur_s;
}

/*
TransPacket* 
ObjectMachine::set_instance_map(InstanceDescriptor* i_desc, int map_id)
{
  int ins_id = i_desc->id;

  // Obtain current position of this instance
  State *cur_s = find_instance(ins_id);
  
  // Build the target state
  // Using set_map will register this state to the map
  // We do not register for temporary variable
  temp_o.map_d = find_map(map_id);
  temp_o.machine = this;
  
  Transition* trans = cur_s->transfer(&temp_o, NULL, true);
  TransPacket* tp = trans->insert_reason("?");

  return tp;
}
*/

TransPacket* 
ObjectMachine::evolve(InstanceDescriptor* i_desc, vector<StateMachine*> &contexts, int old_map_id, int new_map_id, 
		      ObjectMachine* boilerplate, const char* trans_dec, int cost, bool new_instance)
{
  int ins_id = i_desc->id;

  // Now we check the validity of this state
  // In case we might miss something
  ObjectState* cur_s = jump_to_state_with_map(i_desc, old_map_id, new_instance);

  // Build the target state
  Map* map_d = (new_map_id == -1 ? cur_s->get_map() : find_map(new_map_id));
  temp_o.map_d = map_d;
  temp_o.machine = this;
  
  // Transfer state
  Transition* trans = cur_s->transfer(&temp_o, boilerplate);
  TransPacket* tp = trans->insert_reason(trans_dec, contexts);
  assert( map_d->has_bound() );

  // Renew the position of this instance
  migrate_instance(ins_id, tp);

  return tp;
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
  start->depth = 0;
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
    if ( count >= 4 && count >= 0.4 * total_deopts ) {
      printf( "factorOut: In %s, IC %d occupies %.1f%% of %d deopts.\n", 
	      m_name.c_str(), bailout_id, (double)count/total_deopts * 100, total_deopts ); 
    }
  }
}


TransPacket* 
FunctionMachine::evolve(InstanceDescriptor* i_desc, int map_id, 
			int code_id, const char* trans_dec, int cost, bool new_instance)
{ 
  int ins_id = i_desc->id;
  
  // Obtain current position of this instance
  FunctionState *cur_s = (FunctionState*)find_instance(ins_id, new_instance);

  // Build the target state
  temp_f.map_d = (map_id == -1 ? cur_s->get_map() : find_map(map_id));
  temp_f.code_d = (code_id == -1 ? cur_s->get_code() : find_code(code_id));
  temp_f.machine = this;
  
  // Transfer state
  Transition* trans = cur_s->transfer( &temp_f, NULL );
  vector<StateMachine*> contexts;
  contexts.push_back(native_context);
  TransPacket* tp = trans->insert_reason(trans_dec, contexts, cost);

  // Renew the position of this instance
  migrate_instance(ins_id, tp);

  return tp;
}


void 
print_transition(Transition* trans, bool prt_src, bool prt_trans, bool prt_tgt, const char* line_header, const char dir)
{
  State* src = trans->source;
  State* tgt = trans->target;
  
  if ( prt_src == true ) {
    // The symbol for the leading transition
    const char* pstr = NULL;
    if ( trans->type() == Transition::TNormal )
      pstr = src->toString().c_str();
    else {
      SummaryTransition* strans = (SummaryTransition*)trans;
      pstr= strans->boilerplate->toString().c_str();
    }
   
    printf( "%s<%s>", line_header, pstr);
    if ( dir == '|' ) printf( "\n" );
  }
  
  if ( prt_trans ) {
    string reason;
    trans->merge_reasons(reason, false);
    if ( dir == '|' ) {
      if ( prt_src || prt_tgt ) printf( "%s|\n", line_header );
      printf( "%s%s\n", line_header, reason.c_str());
      if ( prt_src || prt_tgt ) printf( "%s|\n", line_header );
    }
    else {
      if ( prt_src || prt_tgt ) printf( "-" );
      printf( "%s", reason.c_str() );
      if ( prt_src || prt_tgt ) printf( "-" );
    }
  }
  
  if ( prt_tgt ) {
    if ( dir == '|' ) printf( "%s", line_header );
    printf( "<%s>" , tgt->toString().c_str() );
    if ( dir == '|' ) printf( "\n" );
  }
}

static int
print_first_with_tabbing(Transition* trans, char* tabs, int n_tabs, int title_len)
{
  print_transition( trans, true, false, false, tabs );
  
  // Padding each line with some tabs
  while ( n_tabs*8 - title_len < 2 ) { 
    tabs[n_tabs++] = '\t';
  }
  tabs[n_tabs] = 0;
  
  // Print rest of the first transition
  print_transition( trans, false, true, true, tabs );
  return n_tabs;
}


void 
print_path(deque<Transition*>& path, const char* title, int skip_n)
{
  bool f_first = true;
  string reason;
  
  // Calculate the line header for each transition print
  int title_len = strlen(title);
  int n_tabs = title_len / 8;
  char tabs[32];
  for ( int i = 0; i < n_tabs; ++i ) tabs[i] = '\t';
  tabs[n_tabs] = 0;

  printf( "%s", title );

  // Skip the first several elements
  if ( skip_n > 0 ) {
    // We print the first element for reference
    Transition* trans = path[0];
    n_tabs = print_first_with_tabbing( trans, tabs, n_tabs, title_len );
    printf( "%s|\n", tabs );
    printf( "%s...(Omit %d transitions)\n", tabs, skip_n-1 );
    f_first = false;
  }

  for ( int i = skip_n; i < path.size(); ++i ) {
    Transition* trans = path[i];
    
    if ( f_first == true ) {
      n_tabs = print_first_with_tabbing( trans, tabs, n_tabs, title_len );
      f_first = false;
    }
    else {
      print_transition( trans, false, true, true, tabs );
    }
  }
}
