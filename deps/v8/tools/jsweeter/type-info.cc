/*
 * Processing type and code related matters.
 * by richardxx, 2014.3
 */

#include "type-info.hh"


Map* null_map = new Map(-1);
Code* null_code = new Code(-1);


static map<int, Map*> all_maps;
static map<int, Code*> all_codes;


void 
Map::update_map(int new_id)
{
  // Update global map structure                                                                                                                                                        
  all_maps.erase(map_id);
  all_maps[new_id] = this;

  // Now we change the map id to new id                                                                                                                                                
  map_id = new_id;

  /*
  // Update states record for machines use this map
  RefSet::iterator it = used_by.begin();
  RefSet::iterator end = used_by.end();

  for ( ; it != end; ++it ) {
    State* s = *it;
    StateMachine *sm = s->machine;
    sm->delete_state(s);
  }

  // Now we change the map id to new id
  map_id = new_id;
  
  // We add the states back their machines
  it = used_by.begin();
  for ( ; it != end; ++it ) {
    State* s = *it;
    StateMachine *sm = s->machine;
    sm->add_state(s);
  }
  */
}


void
Map::add_dep(FunctionMachine* fsm)
{
  dep_funcs.push_back(fsm);
}


void 
Map::deopt_deps(Transition* trans)
{
  vector<FunctionMachine*>::iterator it_funcs;

  if ( dep_funcs.size() == 0 ) return;
  
  printf( "Forced to deoptimize:\n" );

  if ( trans != NULL ) {
    TransPacket* tp = trans->last_;
    StateMachine* trigger_obj = trans->source->machine;
    
    string def_func_name;
    if ( tp->context != NULL )
      def_func_name = tp->context->toString();
    else
      def_func_name = "global";
    
    string &action = tp->reason;
    string obj_name = trigger_obj->toString();
    
    printf( "\tIn <%s>: %s [%s]\n",
	    def_func_name.c_str(),
	    action.c_str(),
	    obj_name.c_str() );
  }
  else {
    printf( "\t(?)\n" );
  }

  printf( "\t===>\n" );
  
  // Iterate the deoptimized functions
  for ( it_funcs = dep_funcs.begin();
	it_funcs != dep_funcs.end(); ++it_funcs ) {
    FunctionMachine* fm = *it_funcs;
    string fm_name = fm->toString();
    
    printf( "\t [%s]\n", fm_name.c_str() );
  }
  
  printf( "\n" );
  
  dep_funcs.clear();
}

Code::Code(int new_code)
{
  code_id = new_code;
}


void Code::update_code(int new_code_id)
{
  all_codes.erase(code_id);
  code_id = new_code_id;
  all_codes[new_code_id] = this;
}


Map* 
find_map(int new_map, bool create)
{
  map<int, Map*>::iterator it = all_maps.find(new_map);
  Map* res = null_map;

  if ( it == all_maps.end() ) {
    if ( create == true ) {
      res = new Map(new_map);
      all_maps[new_map] = res;
    }
  }
  else
    res = it->second;
  
  return res;
}


Code* 
find_code(int new_code, bool create)
{
  map<int, Code*>::iterator it = all_codes.find(new_code);
  Code* res = null_code;
  
  if ( it == all_codes.end() ) {
    if ( create == true ) {
      res = new Code(new_code);
      all_codes[new_code] = res;
    }
  }
  else
    res = it->second;
  
  return res;
}


