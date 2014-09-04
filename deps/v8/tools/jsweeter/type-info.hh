// Describe the type and code
// richardxx, 2014.3


#ifndef TYPE_INFO_H
#define TYPE_INFO_H

#include <vector>
#include "jsweeter.hh"

using std::vector;

class CoreInfo
{
 public:
  typedef vector<State*> RefSet;
  
 public:
  // Map: Map/Code -> States
  RefSet used_by;
  
 public:
 
  // Add a mapping to the used_by container
  virtual void add_usage(State* user_s) {
    used_by.push_back(user_s);
  }
  
  virtual void remove_usage(State* user_s) {
    int size = used_by.size();
    int i = 0;
    
    while ( i < size && used_by[i] != user_s ) ++i;
    if ( i < size ) used_by.erase(used_by.begin() + i);
  }
};


// Map is name for type descriptor in V8
class Map : public CoreInfo
{
public:
  int map_id;

public:
  Map(int new_map) {
    map_id = new_map;
  }

  int id() const { 
    return map_id; 
  }

  int& operator*() { 
    return map_id; 
  }

  bool has_bound() { return used_by.size() > 0; }

  State* to_state() 
  {
    // A map is uniquely used by a state
    return used_by[0];
  }

  void add_usage(State* user_s) 
  {
    if ( has_bound() ) {
      used_by[0] = user_s;
    }
    else {
      CoreInfo::add_usage(user_s);
    }
  }
  
  void remove_usage(State* user_s)
  {
    if ( has_bound() &&
	 used_by[0] == user_s ) {
      used_by.pop_back();
    }
    else {
      CoreInfo::remove_usage(user_s);
    }
  }
  
public:
  void update_map(int);
  
  //
  void add_dep(FunctionMachine*);

  // Deoptimize the functions depending on this map immediately
  void deopt_deps(TransPacket* tp);
  
private:
  // The set of functions that deopted on this map
  vector<FunctionMachine*> dep_funcs;
};


class Code : public CoreInfo
{
 public:
  int code_id;

 public:
  Code(int);
  int id() const { return code_id; }
  int& operator*() { return code_id; }

  void update_code(int);
};


class MapList
{
public:
  MapList() { }
  MapList(int size) { list.resize(size); }
  
  void push_back(Map* map) { list.push_back(map); }
  Map*& operator[](int index) { return list[index]; }
  Map* at(int index) { return list[index]; }
  int size() { return list.size(); }

private:
  vector<Map*> list;
};

extern Map* null_map;
extern Code* null_code;


// Find or create a map structure from given map_id
Map* 
find_map(int new_map, bool create=true);

bool
update_map( int old_id, int new_id );

// Find or create a code structure
Code* 
find_code(int new_code, bool create=true);

bool
update_code( int old_id, int new_id );

#endif
