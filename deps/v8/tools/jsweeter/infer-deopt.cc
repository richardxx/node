#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <utility>
#include <cassert>
#include "events.h"
#include "modeler.hh"
#include "type-info.hh"
#include "infer-deopt.hh"

using namespace std;

struct PathPack
{
  int dist;
  deque<Transition*> path1;
  deque<Transition*> path2;
};


static void
compute_distance(ObjectMachine* osm, State* inst_s, State* exp_s, PathPack* pp)
{
  // inst_s -> exp_s
  int d = osm->forward_search_path(inst_s, exp_s, &(pp->path1));
  if ( d > 0 ) {
    pp->dist = d;
    return;
  }
  
  // exp_s -> inst_s
  d = osm->forward_search_path(exp_s, inst_s, &(pp->path1));
  if ( d > 0 ) {
    pp->dist = -d;
    return;
  }
  
  // lca_s -> exp_s, lca_s -> inst_s 
  deque<Transition*> &exp_path = pp->path1;
  deque<Transition*> &inst_path = pp->path2;
  
  // Obtain the path root -> exp_s
  osm->forward_search_path(osm->start, exp_s, &exp_path);
  int i = exp_path.size() - 1;
  
  for ( ; i > -1; --i ) {
    Transition* trans = exp_path[i];
    State* lca_s = trans->source;
    if ( osm->forward_search_path(lca_s, inst_s, &inst_path) > 0 ) {
      // Found
      break;
    }
    inst_path.clear();
  }
  
  while ( i > 0 ) {
    exp_path.pop_front();
    --i;
  }

  pp->dist = 0;
}

static void
process_hetero_type(int id, Map* exp_map, Map* inst_map)
{
  printf( "%d. Heterogeneous case: exp_map = %x, inst_map = %x.\n", 
	  id, exp_map->id(), inst_map->id() );

  State* exp_s = NULL;
  StateMachine* sm_exp = NULL;
  
  if ( exp_map->has_bound() ) {
    exp_s = exp_map->to_state();
    sm_exp = exp_s->get_machine();
  }

  State* inst_s = inst_map->to_state();
  StateMachine* sm_inst = inst_s->get_machine();

  const char *title = "";

  if ( sm_exp == sm_inst ) {
    // A case that using different closure instances as the constructors
    printf( "uniCtors" );
  }

  deque<Transition*> path;
  int skip_n = 0;

  sm_inst->forward_search_path( sm_inst->start, inst_s, &path );
  if ( path.size() > 5 ) skip_n = path.size() - 5; 
  print_path( path, "inst_map:", skip_n );
  printf("\n");

  if ( exp_map->has_bound() ) {
    path.clear();
    sm_exp->forward_search_path( sm_exp->start, exp_s, &path );
    if ( path.size() > 5 ) skip_n = path.size() - 5; 
    print_path( path, "exp_map:", skip_n );
    printf( "\n" );
  } 
}

static
void handle_future_type( int d, deque<Transition*>& path )
{
  int i = 0;
  
  for ( ; i < d; ++i ) {
    Transition *trans = path[i];
    // Check if all events are NewField
    if (trans->reason_other_than( evt_text[NewField] ) ) break;
  }
  
  //if ( i == d ) {
  print_path(path, "advFlds:");
  printf( "\n" );
    //}
}

static
void handle_past_type( InstanceDescriptor* i_obj, int d, deque<Transition*>& path )
{
  bool is_dict_mode = false;
  bool oth_evt = false;

  for ( int i = 0; i < d; ++i ) {
    Transition *trans = path[i];
    
    if ( i_obj->is_watched == true ) {
      if ( trans->reason_begin_with(evt_text[ElemToSlowMode]) != NULL ||
	   trans->reason_begin_with(evt_text[PropertyToSlowMode]) != NULL ) {
	is_dict_mode = true;
      }
      else {
	if ( trans->reason_begin_with(evt_text[ElemToFastMode]) != NULL ||
	     trans->reason_begin_with(evt_text[PropertyToFastMode]) != NULL ) {
	  is_dict_mode = false;
	}
      }
    }

    if ( trans->reason_other_than(evt_text[NewField]) ) {
      if ( trans->reason_begin_with(evt_text[ChangePrototype]) ) {
	print_path(path, "useMixin:", i);
      }
      oth_evt = true;
    }
  }

  if ( i_obj->is_watched == true && is_dict_mode == true ) {
    printf( "\tmovMap: %x\n", i_obj->id);
    i_obj->is_watched = false;
  }

  //if ( d <= 3 && oth_evt == false ) {
  print_path(path, "advFlds:");
  printf( "\n" );
    //}
}

static void
parse_fld_op_msg( string& reason, string& f_name, int& value )
{
  int name_beg = reason.find( ":" ) + 2;
  int equal_sgn = reason.find( "=", name_beg );
  f_name = reason.substr(name_beg, equal_sgn - name_beg);
  value = atoi( reason.substr(equal_sgn+1).c_str() );
}


static void
print_pair_transitions( pair<Transition*, Transition*> &pr )
{
  Transition *prev = pr.first;
  Transition *curr = pr.second;
  
  if ( prev != NULL ) {
    printf( "A: " );
    print_transition(prev, false, true, false, "", '-');
    printf( ",  " );
  }
	  
  if ( curr != NULL ) {
    printf( "B: " );
    print_transition(curr, false, true, false, "", '-');
    printf( "\n" );
  }
}

static
void handle_split_type( deque<Transition*>& path1, deque<Transition*>& path2 )
{
  map<string, int> cls_val, fld_pos;
  map<string, int>::iterator it;
  vector<pair<Transition*, Transition*> > advF, ordF;
  string f_name;
  int value;

  int d = path1.size();
  for ( int i = 0; i < d; ++i ) {
    Transition *trans = path1[i];
    TransPacket *tp = NULL;

    if ( (tp=trans->reason_begin_with(evt_text[NewField])) != NULL ||
	 (tp=trans->reason_begin_with(evt_text[UptField])) != NULL ) {
      // Parse the reason to get the field and value message
      string &reason = tp->reason;
      parse_fld_op_msg( reason, f_name, value );
      
      if ( value != 0 ) {
	// It's a closure
	cls_val[f_name] = value;
      }
      else if ( reason.find(evt_text[NewField]) != string::npos ) {
	fld_pos[f_name] = i;
      }
    }
  }
  

  d = path2.size();
  for ( int i = 0; i < d; ++i ) {
    Transition *trans = path2[i];
    TransPacket *tp = NULL;

    if ( (tp=trans->reason_begin_with(evt_text[NewField])) != NULL ||
	 (tp=trans->reason_begin_with(evt_text[UptField])) != NULL ) {
      // Parse the reason to get the field and value message
      string &reason = tp->reason;
      parse_fld_op_msg( reason, f_name, value );
      bool assigned_cls = false;

      if ( value != 0 ) {
	// It's a closure
	it = cls_val.find(f_name);
	if ( it != cls_val.end() && it->second != value ) {
	  // And, both evolution paths assign closures to the same field
	  Transition *prev = path1[it->second];
	  cls_val.erase(it);
	  advF.push_back( make_pair(prev, trans) );
	  assigned_cls = true;
	}
      }
      
      if ( !assigned_cls ) {
	it = fld_pos.find(f_name);
	if ( it != fld_pos.end() && it->second != i ) {
	  Transition *prev = path1[it->second];
	  fld_pos.erase(it);
	  ordF.push_back( make_pair(prev, trans) );
	}
      }
    }
  }
  
  int size1 = advF.size();
  if ( size1 != 0 ) {
    printf( "%s:\n", size1 > 8 ? "useMixin" : "advFlds" );
    for ( int i = 0; i < size1; ++i ) {
      pair<Transition*, Transition*> pr = advF[i];
      print_pair_transitions(pr);
    }
    printf( "\n" );
  }
  
  int size2 = ordF.size();
  if ( size2 != 0 ) {
    printf( "ordFlds:\n" );
    for ( int i = 0; i < size2; ++i ) {
      pair<Transition*, Transition*> pr = ordF[i];
      print_pair_transitions(pr);
    }
    printf( "\n" );
  }

  if ( size1 == 0 && size2 == 0 ) {
    
    Map* map_lca = path1.front()->source->get_map();
    Map* map_exp = path1.back()->target->get_map();
    Map* map_inst = path2.back()->target->get_map();
    
    printf( "lca = %x, exp = %x, inst = %x\n",
	    map_lca->id(), map_exp->id(), map_inst->id() );

    if ( path1.size() < 5 ) {
      print_path( path1, "lca -> exp" );
      printf( "\n" );
    }
    
    if ( path2.size() < 5 ) {
      print_path( path2, "lca -> inst" );
      printf( "\n" );
    }
  }
}

static
void report_suggests(int id, InstanceDescriptor* i_obj, vector<PathPack*>& paths)
{
  for ( int i = 0; i < paths.size(); ++i ) {
    printf( "\n%d. ", id++ );
    
    PathPack *pp = paths[i];
    int d = pp->dist;
    
    if ( d > 0 ) {
      /*
       * Case 1:
       * R(inst_s, exp_s): exp_map might be a map for failed_obj in future
       *
       */
      printf( "R(inst, exp) = %d\n", d ); 
      //if ( d <= 3 ) {
	handle_future_type( d, pp->path1 );
	//}
    }
    else if ( d < 0 ) {
      /*
       * Case 2:
       * R(exp_s, inst_s): failed_obj or its group owned exp_map in the past.
       *
       */
      printf( "R(exp, inst) = %d\n", -d );
      handle_past_type( i_obj, -d, pp->path1 );
    }
    else {
      /*
       * Case 3:
       * \exists s3, R(s3, inst_s) && R(s3, exp_s)
       * s3 is a split point.
       */
      printf( "R(lca, exp) = %d, R(lca, inst) = %d\n",
	      pp->path1.size(), pp->path2.size() );
      handle_split_type( pp->path1, pp->path2 );
    }
  }
}


ObjectMachine*
check_deopt(DeoptPack& deopt_pack)
{
  // Identify the instance
  InstanceDescriptor* i_obj = find_instance(deopt_pack.failed_obj);
  if ( i_obj == NULL ) return NULL;
  
  assert( i_obj->sm->type == StateMachine::MObject );
  ObjectMachine* osm = (ObjectMachine*)i_obj->sm;

  // Process maps
  vector<PathPack*> paths;
  MapList* map_list = deopt_pack.map_list;
  int size = map_list->size();
  int n_homo = 0;

  // Report title
  stringstream ss;
  i_obj->birth_place->describe(ss);
  printf( "Deopt: func=%s, bailout=%d, obj=<%s, %s>:\n", 
	  deopt_pack.deopt_f->toString().c_str(), deopt_pack.bailout_id,
	  ss.str().c_str(),
	  osm->toString().c_str() );
  
  State* inst_s = i_obj->location(); 

  for ( int i = 0; i < size; ++i ) {
    Map* exp_map = map_list->at(i);
    State* exp_s = osm->search_state(exp_map, false);

    if ( exp_s == NULL ) {
      // exp_map is heterogeneous to the inst_map
      process_hetero_type(i+1, exp_map, inst_s->get_map());
      continue;
    }
 
    PathPack *pp = new PathPack;
    compute_distance(osm, inst_s, exp_s, pp);
    paths.push_back(pp);
    ++n_homo;
  }

  // Decide if this type is redundant
  //if ( n_homo >= 0.3*size ) {
  report_suggests(size - n_homo + 1, i_obj, paths);
    //}

  // Avoid memory leak
  for ( int i = 0; i < paths.size(); ++i ) {
    PathPack *pp = paths[i];
    delete pp;
  }
  
  return osm;
}
