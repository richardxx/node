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
}


void
Map::add_dep(FunctionMachine* fsm)
{
  dep_funcs.push_back(fsm);
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


bool
update_map( int old_id, int new_id )
{
  map<int, Map*>::iterator it = all_maps.find(old_id);
  if ( it != all_maps.end() ) {
    all_maps[new_id] = it->second;
    all_maps.erase(it);
    return true;
  }
  return false;
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

bool
update_code( int old_id, int new_id )
{
  map<int, Code*>::iterator it = all_codes.find(old_id);
  if ( it != all_codes.end() ) {
    all_codes[new_id] = it->second;
    all_codes.erase(it);
    return true;
  }
  return false;
}
