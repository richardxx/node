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


ObjectMachine*
check_deoptimization(int, int, FunctionMachine*, int);


#endif
