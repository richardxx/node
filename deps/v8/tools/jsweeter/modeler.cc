// Handling events, modeling the events as state machine
// By richardxx, 2013.10

#include <cstring>
#include <cassert>
#include <cstdlib>
#include <climits>
#include "options.h"
#include "events.h"
#include "modeler.hh"
#include "infer-deopt.hh"

using namespace std;

// Define the events handler prototype
typedef void (*EventHandler)(FILE*);

// Handler declarations
#define DeclEventHandler(name, handler, desc)	static void handler(FILE*);

OBJECT_EVENTS_LIST(DeclEventHandler)
FUNCTION_EVENTS_LIST(DeclEventHandler)
MAP_EVENTS_LIST(DeclEventHandler)
SIGNAL_EVENTS_LIST(DeclEventHandler)
SYS_EVENTS_LIST(DeclEventHandler)
  
static void
null_handler(FILE* file) { }

#undef DeclEventHandler

// Fill in the hanlder list
#define GetEventHandler(name, handler, desc) handler,

EventHandler handlers[] = {
  OBJECT_EVENTS_LIST(GetEventHandler)
  FUNCTION_EVENTS_LIST(GetEventHandler)
  MAP_EVENTS_LIST(GetEventHandler)
  SIGNAL_EVENTS_LIST(GetEventHandler)
  SYS_EVENTS_LIST(GetEventHandler)
  null_handler
};

#undef GetEventHandler


// Text descriptions of the events
#define EventText(name, handler, desc) desc,

const char* evt_text[] = {
  OBJECT_EVENTS_LIST(EventText)
  FUNCTION_EVENTS_LIST(EventText)
  MAP_EVENTS_LIST(EventText)
  SIGNAL_EVENTS_LIST(EventText)
  SYS_EVENTS_LIST(EventText)
  "\0"
};

#undef EventText

StateMachine* native_context = NULL;
StateMachine* miss_context = NULL;

// From allocation signature to state machine descriptor
static map<int, StateMachine*> machines[StateMachine::MCount];

// From function/object/map instance to internal descriptor
static map<int, InstanceDescriptor*> instances[StateMachine::MCount];

// Keep a gc object movement record
static map<int, int> gc_record;

// For deferred inference
static map<int, DeoptPack*> deferred_objs;

// Recording the maps at the map check site
static map<int, MapList*> map_lists;

// Internal id counters
static int id_counter[StateMachine::MCount];
static int sig_for_hidden[StateMachine::MCount];

// Tracked map for collecting the functions depend on it
static Map* tracked_map = NULL;


static InstanceDescriptor*
lookup_object(int o_addr)
{
  InstanceDescriptor* i_desc = find_instance(o_addr);
  if ( i_desc == NULL ) {
    // Perhaps it is a boilerplate
    i_desc = find_instance(o_addr, StateMachine::MBoilerplate);
  }
  return i_desc;
}

static StateMachine*
find_function(int context)
{
  InstanceDescriptor* i_desc = find_instance(context, StateMachine::MFunction);
  return i_desc == NULL ? miss_context : i_desc->sm;
}

static void 
register_map_notifier(Map* r)
{
  if ( do_analyze == false ) return;

  if ( tracked_map != NULL ) {
    // Output the list of deoptimized function for last tracked map
    tracked_map->deopt_deps(NULL);
  }
  
  tracked_map = r;
}


// ---------------Events Handlers------------------
static void
create_boilerplate_common(FILE* file, const char* msg)
{
  int n_ctxts, ctxt_id;
  vector<StateMachine*> contexts;
  StateMachine *context;
  int o_addr;
  int map_id;
  int index;

  fscanf( file, "%x %d", &o_addr, &n_ctxts );

  if ( n_ctxts == 0 ) {
    contexts.push_back(native_context);
  }
  else {
    for ( int i = 0; i < n_ctxts; ++i ) {
      fscanf( file, "%x", &ctxt_id );
      context = find_function(ctxt_id);
      contexts.push_back(context);
    }
  }

  // Since boilerplate is context independent, we only care the last context
  while ( contexts.size() > 1 ) contexts.pop_back();
  context = contexts[0];

  fscanf( file, "%x %d", &map_id, &index );
  
  InstanceDescriptor* i_desc = find_instance(o_addr, StateMachine::MBoilerplate, true);
  // Only one instance for each boilerplace automaton
  // Therefore, we use o_addr as the signature
  StateMachine* sm = find_signature(o_addr, StateMachine::MBoilerplate, true);
  
  if ( i_desc->sm != sm ) {
    i_desc->sm = sm;
  }

  // The automaton for this boilerplace just created
  if ( !sm->has_name() ) {
    char buf[ON_STACK_NAME_SIZE];
    sprintf( buf, "/%s#%d/", context->m_name.c_str(), index ); 
    sm->set_name(buf);
  }
  
  // Update transitions
  ObjectMachine* osm = (ObjectMachine*)sm;
  osm->evolve(i_desc, contexts, -1, map_id, NULL, msg, 0, true);
}


static void 
create_obj_boilerplate(FILE* file)
{
  create_boilerplate_common(file, evt_text[CreateObjBoilerplate]);
}


static void 
create_array_boilerplate(FILE* file)
{
  create_boilerplate_common(file, evt_text[CreateArrayBoilerplate]);
}


static void
create_obj_common(FILE* file, InternalEvent event)
{
  int n_ctxts, ctxt_id;
  vector<StateMachine*> contexts;
  int o_addr;
  int alloc_sig;
  int map_id;
  int literal_index;
  char name_buf[ON_STACK_NAME_SIZE];

  fscanf( file, "%x %d", &o_addr, &n_ctxts );
  
  if ( n_ctxts == 0 ) {
    contexts.push_back(native_context);
  }
  else {
    for ( int i = 0; i < n_ctxts; ++i ) {
      fscanf( file, "%x", &ctxt_id );
      StateMachine *context = find_function(ctxt_id);
      contexts.push_back(context);
    }
  }
  
  fscanf( file, "%x %x", &map_id, &alloc_sig );
  
  // We first obtain the constructor name
  StateMachine* ctor = NULL;
  const char* ctor_name = NULL;

  if ( event <= CreateArrayLiteral ) {
    fscanf( file, " %d", &literal_index );
    ctor = find_signature(alloc_sig, StateMachine::MBoilerplate);
    ctor_name = ctor->toString().c_str();
    sprintf( name_buf, "%s", ctor_name );
  }
  else {
    ctor = find_function(alloc_sig);
    ctor_name = ctor->toString().c_str();
    sprintf( name_buf, "New %s", ctor_name);
  }

  // We lookup the instance first, because some operations may already use this object
  InstanceDescriptor* i_desc = find_instance(o_addr, StateMachine::MObject, true);
  // We lookup the automaton for this object with the signature of its constructor
  // Therefore, an object automaton is for all the objects with the same constructor
  StateMachine* sm = find_signature(alloc_sig, StateMachine::MObject, true);

  if ( i_desc->sm != sm ) {
    // In case they are different, it could be this instance was reclaimed by GC
    // We directly reset the machine to new machine
    i_desc->sm = sm;
  }

  // If this is a new machine
  if ( !sm->has_name() ) sm->set_name(name_buf);
  
  // We check if this object is created by cloning a boilerplate
  ObjectMachine* boilerplate = NULL;
  if ( event <= CreateArrayLiteral ) {
    boilerplate = (ObjectMachine*)find_signature(alloc_sig, StateMachine::MBoilerplate);
  }

  // Update transitions
  ObjectMachine* osm = (ObjectMachine*)sm;
  TransPacket* tp = osm->evolve(i_desc, contexts, -1, map_id, boilerplate, evt_text[event], 0, true);
  
  // Remember birth info
  i_desc->birth_place = tp;

  // Check if this operation forces deoptimizations
  if ( i_desc->force_deopt ) {
    if ( tracked_map != NULL ) tracked_map->deopt_deps(tp);
    i_desc->force_deopt = false;
  }

  // Processing the deferred deoptimizations
  map<int, DeoptPack*>::iterator it = deferred_objs.find(o_addr);
  if ( it != deferred_objs.end() ) {
    DeoptPack* deopt_pack = it->second;
    ObjectMachine* osm = check_deopt( *deopt_pack );
    if ( osm != NULL ) {
      osm->cause_deopt = true;
      deferred_objs.erase(it);
    }
  }
}


static void 
create_object_literal(FILE* file)
{
  create_obj_common(file, CreateObjectLiteral);
}


static void 
create_array_literal(FILE* file)
{
  create_obj_common(file, CreateArrayLiteral);
}


static void 
create_new_object(FILE* file)
{
  create_obj_common(file, CreateNewObject);
}


static void 
create_new_array(FILE* file)
{
  create_obj_common(file, CreateNewArray);
}


static void
create_context(FILE* file)
{
  int n_ctxts, ctxt_id;
  vector<StateMachine*> contexts;
  int o_addr;
  int alloc_sig;
  int map_id;
  
  fscanf( file, "%x %d", &o_addr, &n_ctxts );
  
  if ( n_ctxts == 0 ) {
    contexts.push_back(native_context);
  }
  else {
    for ( int i = 0; i < n_ctxts; ++i ) {
      fscanf( file, "%x", &ctxt_id );
      StateMachine *context = find_function(ctxt_id);
      contexts.push_back(context);
    }
  }
  
  fscanf( file, "%x %x", &alloc_sig, &map_id);
  
  // We lookup the instance first, because some operations may already use this object
  InstanceDescriptor* i_desc = find_instance(o_addr, StateMachine::MObject, true);
  
  // We lookup the automaton for this object with the signature of its constructor
  // Therefore, an object automaton is for all the objects with the same constructor
  StateMachine* sm = find_signature(alloc_sig, StateMachine::MObject, true);

  if ( i_desc->sm != sm ) {
    // In case they are different, it could be this instance was reclaimed by GC
    // We directly reset the machine to new machine
    i_desc->sm = sm;
  }

  // If this is a new machine
  if ( !sm->has_name() ) {
    sm->set_name( "FunctionContext" );
  }
  
  // Update transitions
  ObjectMachine* osm = (ObjectMachine*)sm;
  TransPacket* tp = osm->evolve(i_desc, contexts, -1, map_id, NULL, evt_text[CreateContext], 0, true);

  // Check if this operation forces deoptimizations
  if ( i_desc->force_deopt ) {
    if ( tracked_map != NULL ) tracked_map->deopt_deps(tp);
    i_desc->force_deopt = false;
  }
  
  // Processing the deferred deoptimizations
  map<int, DeoptPack*>::iterator it = deferred_objs.find(o_addr);
  if ( it != deferred_objs.end() ) {
    DeoptPack* deopt_pack = it->second;
    ObjectMachine* osm = check_deopt( *deopt_pack );
    if ( osm != NULL ) {
      osm->cause_deopt = true;
      deferred_objs.erase(it);
    }
  }
}


static void
copy_object(FILE* file)
{
  int n_ctxts, ctxt_id;
  vector<StateMachine*> contexts;
  int dst, src;
  
  fscanf( file, "%x %d", &dst, &n_ctxts );
  
  if ( n_ctxts == 0 ) {
    contexts.push_back(native_context);
  }
  else {
    for ( int i = 0; i < n_ctxts; ++i ) {
      fscanf( file, "%x", &ctxt_id );
      StateMachine *context = find_function(ctxt_id);
      contexts.push_back(context);
    }
  }

  fscanf( file, "%x", &src);

  InstanceDescriptor* src_desc = find_instance(src);
  if ( src_desc == NULL ) return;
  
  // Map the object to a state
  StateMachine* sm = src_desc->sm;  
  State* s = sm->find_instance(src);
  sm->add_instance(dst, s);
  
  InstanceDescriptor* dst_desc = find_instance(dst, StateMachine::MObject, true);
  dst_desc->birth_place = src_desc->birth_place;
  dst_desc->sm = sm;
}


// Create a transition for concrete operation
static void
OpTransitionCommon( vector<StateMachine*> &contexts, int o_addr, int old_map_id, int map_id, const char* msg, int cost = 0 )
{
  InstanceDescriptor* i_desc = lookup_object(o_addr);
  if ( i_desc == NULL ) return;

  // Create transition
  StateMachine* sm = i_desc->sm;
  ObjectMachine* osm = (ObjectMachine*)sm;
  TransPacket* tp = osm->evolve(i_desc, contexts, old_map_id, map_id, NULL, msg, cost);

  // Check if this operation incurs storage change
  if ( i_desc->prop_dict ) {
    State *cur_s = i_desc->location();
    if ( cur_s->depth >= 15 ) {
      // Add too many fields
      int n_flds = 0;
      
      deque<Transition*> path;
      State *start = sm->start;
      sm->forward_search_path(start, cur_s, &path);

      int size = path.size();
      for ( int i = 0; i < size; ++i ) {
	Transition *back = path[i];
	if ( back->reason_begin_with(evt_text[NewField]) != NULL )
	  ++n_flds;
	if ( back->reason_begin_with(evt_text[DelField]) != NULL ) {
	  n_flds = 100;
	  break;
	}
      }
      
      if (n_flds >= 15 ) {
	printf( "properties -> dictionary\n" );
	print_path(path, "Last 15:", size > 15 ? size - 15 : 0);
	printf( "\n" );
	i_desc->prop_dict = false;
      }
      else {
	i_desc->is_watched = true;
      }
    }
  }
  else if ( i_desc->elem_dict ) {
    deque<Transition*> path;
    State *start = sm->start;
    State *cur_s = i_desc->location();
    sm->forward_search_path(start, cur_s, &path);
    
    int size = path.size();
    printf( "elements -> dictionary\n" );
    print_path(path, "Last 15:", size > 15 ? size - 15 : 0);
    printf( "\n" );
    i_desc->elem_dict = false;
  }

  // Check if this operation forces deoptimizations
  if ( i_desc->force_deopt ) {
    if ( tracked_map != NULL ) tracked_map->deopt_deps(tp);
    i_desc->force_deopt = false;
  }
}


static void
change_prototype(FILE* file)
{
  int n_ctxts, ctxt_id;
  vector<StateMachine*> contexts;
  int o_addr;
  int map_id, proto;
  char msg[ON_STACK_NAME_SIZE];
  
  fscanf( file, "%x %d", &o_addr, &n_ctxts );
  
  if ( n_ctxts == 0 ) {
    contexts.push_back(native_context);
  }
  else {
    for ( int i = 0; i < n_ctxts; ++i ) {
      fscanf( file, "%x", &ctxt_id );
      StateMachine *context = find_function(ctxt_id);
      contexts.push_back(context);
    }
  }

  fscanf( file, "%x %x", &map_id, &proto );
  
  sprintf(msg, "%s: %x", evt_text[ChangePrototype], proto);
  OpTransitionCommon( contexts, o_addr, -1, map_id, msg );
}

static void
field_update_common(FILE* file, InternalEvent type)
{
  int n_ctxts, ctxt_id;
  vector<StateMachine*> contexts;
  int o_addr;
  int old_map_id, map_id;
  int value;
  char f_name[ON_STACK_NAME_SIZE], msg[ON_STACK_NAME_SIZE];
  
  fscanf( file, "%x %d", &o_addr, &n_ctxts );
  
  if ( n_ctxts == 0 ) {
    contexts.push_back(native_context);
  }
  else {
    for ( int i = 0; i < n_ctxts; ++i ) {
      fscanf( file, "%x", &ctxt_id );
      StateMachine *context = find_function(ctxt_id);
      contexts.push_back(context);
    }
  }

  fscanf( file, "%x %x %x %[^\t\n]", &old_map_id, &map_id, &value, f_name );
  
  // Construct the encoded message
  sprintf( msg, "%s: %s=%d", evt_text[type], f_name, value );
  OpTransitionCommon( contexts, o_addr, old_map_id, map_id, msg ); 
}


static void
new_field(FILE* file)
{
  field_update_common(file, NewField);
}


static void
upt_field(FILE* file)
{
  field_update_common(file, UptField);
}


static void
del_field(FILE* file)
{
  field_update_common(file, DelField);
}

static void
elem_update_common(FILE* file, InternalEvent event)
{
  int n_ctxts, ctxt_id;
  vector<StateMachine*> contexts;
  int o_addr;
  int old_map_id, map_id;
  int index;
  char msg[ON_STACK_NAME_SIZE];
  
  fscanf( file, "%x %d", &o_addr, &n_ctxts );
  
  if ( n_ctxts == 0 ) {
    contexts.push_back(native_context);
  }
  else {
    for ( int i = 0; i < n_ctxts; ++i ) {
      fscanf( file, "%x", &ctxt_id );
      StateMachine *context = find_function(ctxt_id);
      contexts.push_back(context);
    }
  }

  fscanf( file, "%x %x %d", &old_map_id, &map_id, &index );
  
  sprintf( msg, "%s: %d", evt_text[event], index );
  OpTransitionCommon( contexts, o_addr, old_map_id, map_id, msg );
}

static void
set_elem(FILE* file)
{
  elem_update_common(file, SetElem);
}

static void
del_elem(FILE* file)
{
  elem_update_common(file, DelElem);
}

static void
self_copy_common(FILE* file, InternalEvent event)
{
  int n_ctxts, ctxt_id;
  vector<StateMachine*> contexts;
  int o_addr;
  int bytes;
    
  fscanf( file, "%x %d", &o_addr, &n_ctxts );
  
  if ( n_ctxts == 0 ) {
    contexts.push_back(native_context);
  }
  else {
    for ( int i = 0; i < n_ctxts; ++i ) {
      fscanf( file, "%x", &ctxt_id );
      StateMachine *context = find_function(ctxt_id);
      contexts.push_back(context);
    }
  }
  
  fscanf( file, "%d", &bytes );  
  OpTransitionCommon( contexts, o_addr, -1, -1, evt_text[event], bytes );
}


static void
cow_copy(FILE* file)
{
  self_copy_common(file, CowCopy);
}

static void
expand_array(FILE* file)
{
  self_copy_common(file, ExpandArray);
}


static void
create_function(FILE* file)
{
  int f_addr;
  int alloc_sig;
  int map_id;
  int code;
  char name_buf[ON_STACK_NAME_SIZE];
  
  fscanf( file, "%x %x %x %x %[^\t\n]",
	  &f_addr, &alloc_sig, &map_id, &code, name_buf);
  
  // We lookup the instance first, because some operations may already use this object
  InstanceDescriptor* i_desc = find_instance(f_addr, StateMachine::MFunction, true);
  // We lookup the automaton for this object with the signature of its constructor
  // Therefore, an object automaton is for all the objects with the same constructor
  StateMachine* sm = find_signature(alloc_sig, StateMachine::MFunction, true);
  
  if ( i_desc->sm != sm ) {
    // In case they are different, it could be this instance was reclaimed by GC
    // We directly reset the machine to new machine
    i_desc->sm = sm;
  }
  
  // If this is a new machine
  if ( !sm->has_name() ) {
    sm->set_name(name_buf);
  }

  // Update transitions
  FunctionMachine* fsm = (FunctionMachine*)sm;
  TransPacket* tp = fsm->evolve(i_desc, map_id, code, evt_text[CreateFunction], 0, true);
  
  // Remember the birth info
  i_desc->birth_place = tp;
}


static Transition*
SimpleFunctionTransition( int f_addr, int code, const char* msg, int cost = 0 )
{
  InstanceDescriptor* i_desc = find_instance(f_addr, StateMachine::MFunction);
  if ( i_desc == NULL ) return NULL;

  FunctionMachine* fsm = (FunctionMachine*)i_desc->sm;
  TransPacket *tp = fsm->evolve(i_desc, -1, code, msg, cost);
  return tp->trans;
}


static void
gen_full_code(FILE* file)
{
  int f_addr, code;
  fscanf( file, "%x %x", &f_addr, &code );
  SimpleFunctionTransition( f_addr, code, evt_text[GenFullCode] );
}


static void
gen_opt_code(FILE* file)
{
  int f_addr, code;
  char opt_buf[ON_STACK_NAME_SIZE];
  
  sprintf( opt_buf, "%s: ", evt_text[GenOptCode] );
  fscanf( file, "%x %x %[^\t\n]", 
	  &f_addr, &code, 
	  opt_buf + strlen(opt_buf) );
  
  Transition* trans = SimpleFunctionTransition( f_addr, code, opt_buf );
  if ( trans != NULL ) {
    State* s = trans->source;
    FunctionMachine* fm = (FunctionMachine*)s->machine;
    fm->been_optimized = true;
  }
}


static void
gen_osr_code(FILE* file)
{
  int f_addr, code;
  char opt_buf[ON_STACK_NAME_SIZE];

  sprintf( opt_buf, "%s: ", evt_text[GenOsrCode] );
  fscanf( file, "%x %x %[^\t\n]",
          &f_addr, &code, 
	  opt_buf + strlen(opt_buf) );

  Transition* trans = SimpleFunctionTransition( f_addr, code, opt_buf );
  State* s = trans->source;
  FunctionMachine* fm = (FunctionMachine*)s->machine;
  fm->been_optimized = true;
}

/*
static void
set_code(FILE* file)
{
  int f_addr, code;
  
  fscanf( file, "%x %x", &f_addr, &code );
}
*/

static void
disable_opt(FILE* file)
{
  int shared, f_addr;
  char opt_buf[ON_STACK_NAME_SIZE];

  fscanf( file, "%x %x %[^\t\n]",
          &f_addr, &shared, opt_buf );

  StateMachine* sm = find_signature(shared, StateMachine::MFunction);
  if ( sm == NULL ) return;
  FunctionMachine* fsm = (FunctionMachine*)sm;
  fsm->set_opt_state( false, opt_buf );
}


static void
reenable_opt(FILE* file)
{
  int shared, f_addr;
  char opt_buf[ON_STACK_NAME_SIZE];

  fscanf( file, "%x %x %[^\t\n]",
          &f_addr, &shared, opt_buf );

  StateMachine* sm = find_signature(shared, StateMachine::MFunction);
  if ( sm == NULL ) return;
  FunctionMachine* fsm = (FunctionMachine*)sm;
  fsm->set_opt_state( true, opt_buf );
}


static void
gen_opt_failed(FILE* file)
{
  int f_addr, new_code;
  char opt_buf[ON_STACK_NAME_SIZE];

  sprintf( opt_buf, "%s: ", evt_text[OptFailed] );
  int last_pos = strlen(opt_buf);
  
  fscanf( file, "%x %x %[^\t\n]",
          &f_addr, 
	  &new_code, opt_buf + last_pos );

  InstanceDescriptor* i_desc = find_instance(f_addr, StateMachine::MFunction);
  if ( i_desc == NULL ) return;
  FunctionMachine* fsm = (FunctionMachine*)i_desc->sm;
    
  if ( opt_buf[last_pos] == '-' &&
       opt_buf[last_pos+1] == '\0' ) {
    // Reuse the disable message
    sprintf( opt_buf+last_pos, "%s", fsm->optMsg.c_str() );
  }
  
  fsm->evolve( i_desc, -1, new_code, opt_buf );
}


static Transition*
do_deopt_common(int f_addr, int old_code, int new_code, const char* msg)
{
  InstanceDescriptor* i_func = find_instance(f_addr, StateMachine::MFunction);
  if ( i_func == NULL ) return NULL;
  
  // We first decide if the old_code is used currently
  FunctionMachine* fsm = (FunctionMachine*)i_func->sm;
  FunctionState* cur_s = (FunctionState*)fsm->find_instance(i_func->id);
  
  if ( cur_s->code_d->id() != old_code ) {
    //This might be missing sites in V8 engine to capture all VM actions.
    //A quick fix, we directly make a transition.    
    fsm->evolve(i_func, -1, old_code, "Opt: ?");
  }
  
  // Then, we transfer to the new_code
  char full_buf[ON_STACK_NAME_SIZE];
  sprintf( full_buf, "Deopt: %s", msg );
  
  TransPacket *tp = fsm->evolve(i_func, -1, new_code, full_buf);
  return tp->trans;
}


static void
regular_deopt(FILE* file)
{
  int f_addr, old_code, new_code;
  int failed_obj, ckmap_site, bailout_id;
  char msg[ON_STACK_NAME_SIZE];

  fscanf( file, "%x %x %x %x %d %[^\t\n]",
	  &f_addr,
	  &old_code, &new_code,
	  &failed_obj, &ckmap_site,
	  msg );
  
  // We first model this transition
  Transition* trans = do_deopt_common(f_addr, old_code, new_code, msg);
  if ( trans == NULL ||
       !do_analyze ) return;

  // Obtain the bailout ID
  int i = 0;
  while ( msg[i] && msg[i] != '@' ) ++i;

  // Record this deopt
  FunctionMachine* funcM = (FunctionMachine*)trans->source->machine;
  if ( msg[i] == '@' ) {
    bailout_id = atoi( msg + i + 1 );
    funcM->add_deopt( bailout_id );
  }

  // We don't process the soft deopts
  if ( strncmp( msg, "soft", 4 ) == 0 ) {
    return;
  }

  // Check
  MapList* list = map_lists[ckmap_site];
  DeoptPack deopt_pack(failed_obj, list, funcM, bailout_id);
  ObjectMachine* osm = check_deopt(deopt_pack);
  
  if ( osm == NULL ) {
    // Cache this object and resolve the deoptimization later
    DeoptPack *dp = new DeoptPack( deopt_pack );
    deferred_objs[failed_obj] = dp; 
  }
  else {
    // Can only deoptimize once for each site
    //map_lists.erase(ckmap_site);
    osm->cause_deopt = true;
  }
}


static void
deopt_as_inline(FILE* file)
{
  int f_addr;
  int old_code, new_code;
  int real_deopt_func;

  fscanf( file, "%x %x %x %x",
	  &f_addr, &old_code, &new_code,
	  &real_deopt_func );

  Transition* trans = do_deopt_common(f_addr, old_code, new_code, evt_text[DeoptAsInline]);
}


static void
force_deopt(FILE* file)
{
  int f_addr;
  int old_code, new_code;

  fscanf( file, "%x %x %x",
	  &f_addr, &old_code, &new_code );

  Transition* trans = do_deopt_common(f_addr, old_code, new_code, "Forced");
  if ( trans == NULL ) return;
  
  // Add the deoptimization to the list
  FunctionMachine* fsm = (FunctionMachine*)trans->source->machine;  
  if ( tracked_map != NULL && fsm->has_name() )
    tracked_map->add_dep(fsm);
}

// BeingDeoptOnMap is used in pair with ForceDeopt
static void
begin_deopt_on_map(FILE* file)
{
  int o_addr, map_id;
  fscanf( file, "%x %x", &o_addr, &map_id );

  InstanceDescriptor* i_desc = find_instance(o_addr, StateMachine::MObject);
  if ( i_desc != NULL ) {
    i_desc->force_deopt = true;
  }
  
  // Map change can result in code deoptimization
  Map* map_d = find_map(map_id);
  register_map_notifier(map_d);
}

static void
gen_deopt_maps(FILE* file)
{
  int ckmap_site, map_count;
  int map_id;

  fscanf( file, "%d %d", &ckmap_site, &map_count );
  MapList* list = new MapList;
  
  for ( int i = 0; i < map_count; ++i ) {
    fscanf( file, "%x", &map_id );
    Map* map_d = find_map(map_id);
    list->push_back(map_d);
  }
  
  map_lists[ckmap_site] = list;
}


static void
elem_to_slow(FILE* file)
{
  int o_addr;
  
  fscanf( file, "%x", &o_addr );
  
  InstanceDescriptor* i_desc = lookup_object(o_addr);
  if ( i_desc == NULL ) return;
  i_desc->elem_dict = true;
}


static void
prop_to_slow(FILE* file)
{
  int o_addr;
    
  fscanf( file, "%x", &o_addr );

  InstanceDescriptor* i_desc = lookup_object(o_addr);
  if ( i_desc != NULL )
    i_desc->prop_dict = true;
}


static void
elem_to_fast(FILE* file)
{
  int o_addr;
  
  fscanf( file, "%x", &o_addr );

  InstanceDescriptor* i_desc = lookup_object(o_addr);
  if ( i_desc != NULL )
    i_desc->elem_dict = false;
}


static void
prop_to_fast(FILE* file)
{
  int o_addr;
  
  fscanf( file, "%x", &o_addr );
  
  InstanceDescriptor* i_desc = lookup_object(o_addr);
  if ( i_desc != NULL )
    i_desc->prop_dict = false;
}

static void
elem_transition(FILE* file)
{
  int o_addr;
  fscanf( file, "%x", &o_addr );
}


static void
gc_move_object(FILE* file)
{
  int from, to;
  bool found = false;
  fscanf( file, "%x %x", &from, &to );

  // Update the specific instance
  for ( int i = StateMachine::MBoilerplate; i <= StateMachine::MFunction; ++i ) {
    StateMachine::Mtype type = (StateMachine::Mtype)i;
    InstanceDescriptor* i_desc = find_instance(from, type);
    if ( i_desc != NULL ) {
      // First update the mapping for global descriptors
      instances[type].erase(from);
      instances[type][to] = i_desc;
      
      // Second update the mapping for local record
      StateMachine* sm = i_desc->sm;
      sm->rename_instance(from, to);

      i_desc->raw_addr = to;
      found = true;
      break;
    }
  }

  // Some objects are used as signatures for other objects
  for ( int i = StateMachine::MBoilerplate; i <= StateMachine::MObject; ++i ) {
    StateMachine::Mtype type = (StateMachine::Mtype)i;
    StateMachine* sm = find_signature(from, type);
    if ( sm != NULL ) {
      machines[type].erase(from);
      machines[type][to] = sm;
      found = true;
    }
  }
  
  if ( !found ) {
    // Is this a map?
    if ( !update_map(from, to) )
      update_code(from, to);
  }
  
  gc_record[from] = to;
}


static void
gc_move_map(FILE* file)
{
  int old_id, new_id;
  fscanf( file, "%x %x", &old_id, &new_id );

  Map* map_d = find_map(old_id);
  if ( map_d == null_map ) return;
  map_d->update_map(new_id);
}


static void
gc_move_shared(FILE* file)
{
  int from, to;
  fscanf( file, "%x %x", &from, &to );

  // Update functioin machine
  StateMachine::Mtype type = StateMachine::MFunction;
  StateMachine* sm = find_signature(from, type);
  if ( sm != NULL ) {
    machines[type].erase(from);
    machines[type][to] = sm;
  }
  
  // Update the object machine that uses sharedinfo as signature
  type = StateMachine::MObject;
  sm = find_signature(from, type);
  if ( sm != NULL ) {
    machines[type].erase(from);
    machines[type][to] = sm;
  }
}


static void
gc_move_code(FILE* file)
{
  int old_code, new_code;
  fscanf( file, "%x %x", 
	  &old_code, &new_code );

  Code* code_d = find_code(old_code);
  if ( code_d == null_code ) return;

  code_d->update_code(new_code);
  // We make a transition to all the instances owning this code
  // set<State*, State::state_ptr_cmp> it, end;
  // for ( it = code_d->used_by.begin(),
  // 	  end = code_d->used_by.end(); it != end; ++it ) {
    
  //   State* fs = *it;
  //   StateMachine* fsm = fs->machine;
  // }
}


// ---------------Public interfaces-------------------

StateMachine* 
find_signature(int m_sig, StateMachine::Mtype type, bool create)
{
  StateMachine* sm = miss_context;
  map<int, StateMachine*> &t_mac = machines[type];
  map<int, StateMachine*>::iterator i_sm = t_mac.find(m_sig);
  
  if ( i_sm != t_mac.end() ) {
    // found
    sm = i_sm->second;
  }
  else {
    // Perhaps a GC run just preempted
    map<int, int>::iterator gc_it = gc_record.find(m_sig);
    if ( gc_it != gc_record.end() ) {
      m_sig = gc_it->second;
      i_sm = t_mac.find(m_sig);
    }
    
    if ( i_sm != t_mac.end() ) {
      sm = i_sm->second;
    }
    else if ( create == true ) {
      sm = StateMachine::NewMachine(type);
      t_mac[m_sig] = sm;
    }
  }
  
  return sm;
}


InstanceDescriptor*
find_instance( int ins_addr, StateMachine::Mtype type, bool create_descriptor)
{
  map<int, InstanceDescriptor*> &t_ins_map = instances[type];
  map<int, InstanceDescriptor*>::iterator it;
  
  it = t_ins_map.find(ins_addr);

  if ( it == t_ins_map.end() ) {
    // Perhaps a GC run just preempted
    map<int, int>::iterator gc_it = gc_record.find(ins_addr);
    if ( gc_it != gc_record.end() ) {
      ins_addr = gc_it->second;
      it = t_ins_map.find(ins_addr);
    }
  }

  if ( it == t_ins_map.end() ) {  
    if ( !create_descriptor ) return NULL;
    
    // Create a new instance descriptor
    InstanceDescriptor* i_desc = new InstanceDescriptor;
    i_desc->id = id_counter[type]++;
    i_desc->raw_addr = ins_addr;

    StateMachine* sm = find_signature( sig_for_hidden[type], type, true );
    sm->set_name("$Hidden$");
    i_desc->sm = sm;
    sig_for_hidden[type]--;

    t_ins_map[ins_addr] = i_desc;
    return i_desc;
  }
  
  return it->second;
}


void 
prepare_machines()
{
  for ( int i = 0; i < StateMachine::MCount; ++i ) {
    id_counter[i] = 0;
    sig_for_hidden[i] = -1;
  }
  
  // Build a native context
  InstanceDescriptor* i_native = find_instance( 0, StateMachine::MFunction, true );
  native_context = i_native->sm;
  native_context->set_name("global");

  // Build a missing context
  InstanceDescriptor* i_miss = find_instance( INT_MAX, StateMachine::MFunction, true );
  miss_context = i_miss->sm;
  miss_context->set_name("*MISS*");
}


void 
clean_machines()
{
  // First output the cached events
  register_map_notifier(NULL);

  // Output factorOut suggestions
  map<int, StateMachine*> &t_mac = machines[StateMachine::MFunction];
  
  for ( map<int, StateMachine*>::iterator it = t_mac.begin();
	it != t_mac.end(); ++it ) {
    FunctionMachine* fsm = (FunctionMachine*)it->second;
    fsm->check_bailouts();
  }
}


void
visualize_machines(const char* file_name)
{
  FILE* file;
  extern int states_count_limit;

  file = fopen(file_name, "w");
  if ( file == NULL ) {
    fprintf(stderr, "Cannot create visualization file %s\n", file_name);
    return;
  }
  
  // Do BFS to draw graphs
  for ( int i = StateMachine::MBoilerplate; i < StateMachine::MCount; ++i ) {
    
    if ( i == StateMachine::MObject &&
	 draw_mode == DRAW_FUNCTIONS_ONLY )
      continue;

    if ( draw_mode == DRAW_OBJECTS_ONLY &&
	 (i == StateMachine::MFunction || i == StateMachine::MBoilerplate))
      continue;    

    map<int, StateMachine*> &t_mac = machines[i];
    for ( map<int, StateMachine*>::iterator it = t_mac.begin();
	  it != t_mac.end();
	  ++it ) {
      StateMachine* sm = it->second;

      if ( i == StateMachine::MObject || i == StateMachine::MBoilerplate ) {
	// Discard uninteresting objects
	if ( ((ObjectMachine*)sm)->cause_deopt == false ) {
	  if ( sm->count_instances() == 1 ) continue;
	  if ( sm->size() < states_count_limit ) continue;
	  if ( !sm->has_name() ) continue;
	}
      }
      else if ( i == StateMachine::MFunction ) {
	if ( ((FunctionMachine*)sm)->been_optimized == false ) continue;
      }
    
      //printf( "done\n" );
      sm->draw_graphviz(file, slice_sig);
    }
  }
    
  fclose(file);
}


static void
sanity_check()
{
#define ASSERT_HANDLER(name, handler, desc) assert(handlers[name] == handler);

  OBJECT_EVENTS_LIST(ASSERT_HANDLER)
  FUNCTION_EVENTS_LIST(ASSERT_HANDLER)
  MAP_EVENTS_LIST(ASSERT_HANDLER)
  SIGNAL_EVENTS_LIST(ASSERT_HANDLER)
  SYS_EVENTS_LIST(ASSERT_HANDLER)
}


bool 
build_automata(const char* log_file)
{
  FILE* file;
  int event_type;
  
  file = fopen( log_file, "r" );
  if ( file == NULL ) return false;
  
  prepare_machines();
  
  int i = 0;
  while ( fscanf(file, "%d", &event_type) == 1 ) {
    if ( debug_mode ) {
      printf("before %d: Event ID = %d, ", i, event_type);
      fflush(stdout);
    }

    handlers[event_type](file);

    if ( debug_mode ) {
      sanity_check();
      printf("after %d: Event ID = %d\n", i, event_type);
      fflush(stdout);
      ++i;
    }
  }

  if ( debug_mode ) {
    printf( "Total events = %d\n", i );
  }

  fclose(file);
  return true;
}

