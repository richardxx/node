// Mining facility for performance bug hunting and reasoning
// By richardxx, 2013.8

#ifndef MINER_H
#define MINER_H

#include <deque>
#include "type-info.hh"

struct DeoptPack
{
  int failed_obj;
  MapList* map_list;
  FunctionMachine* deopt_f;
  int bailout_id;
  
  DeoptPack(int fo, MapList* list, FunctionMachine* fm, int id)
  {
    failed_obj = fo;
    map_list = list;
    deopt_f = fm;
    bailout_id = id;
  }

  DeoptPack( const DeoptPack& other )
  {
    failed_obj = other.failed_obj;
    map_list = other.map_list;
    deopt_f = other.deopt_f;
    bailout_id = other.bailout_id;
  }
};

ObjectMachine* 
check_deopt(struct DeoptPack&);

void
summarize_deopt();

#endif
