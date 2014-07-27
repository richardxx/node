// Mining facility for performance bug hunting and reasoning
// By richardxx, 2013.8

#ifndef MINER_H
#define MINER_H

#include <deque>
#include "type-info.hh"

enum FixType {
  addF,
  fOrder,
  fPromote,
  ckStore,
  ckIntOF,
  factorOut,

  FixCount
};

struct DeoptPack
{
  int failed_obj;
  int exp_mapID;
  FunctionMachine* deopt_f;
  int bailout_id;
  
  DeoptPack(int fo, int em, FunctionMachine* fm, int id)
  {
    failed_obj = fo;
    exp_mapID = em;
    deopt_f = fm;
    bailout_id = id;
  }

  DeoptPack( const DeoptPack& other )
  {
    failed_obj = other.failed_obj;
    exp_mapID = other.exp_mapID;
    deopt_f = other.deopt_f;
    bailout_id = other.bailout_id;
  }
};


ObjectMachine*
check_deoptimization(struct DeoptPack&);


#endif
