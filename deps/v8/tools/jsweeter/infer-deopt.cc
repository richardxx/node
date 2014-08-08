#include <cstdio>
#include "modeler.hh"
#include "type-info.hh"
#include "infer-deopt.hh"

using namespace std;


static 
void print_path(deque<Transition*>& path, int skip_n = 0, bool upd_fld_only = false)
{
  bool f_first = true;
  string reason;
  
  // Skip the first several elements
  for ( int i = skip_n; i < path.size(); ++i ) {
    Transition* trans = path[i];
    State* src = trans->source;
    State* tgt = trans->target;
    
    reason.clear();
    trans->merge_reasons(reason, false);
    
    if ( !upd_fld_only ||
	 reason.find( "!Fld" ) != string::npos ) {
      
      if ( f_first == true ) {
	// The symbol for the leading transition
	const char* pstr = NULL;
	if ( trans->type() == Transition::TNormal )
	  pstr = src->toString().c_str();
	else {
	  SummaryTransition* strans = (SummaryTransition*)trans;
	  pstr= strans->exit->machine->toString().c_str();
      }
	
	printf( "[%s]", pstr);
	f_first = false;
      }
      
      printf( "-%s-[%s]", reason.c_str(), tgt->toString().c_str() );
    }
  }
  
  printf("\n");
}


static int
compute_distance(ObjectMachine* osm, State* s1, State* s2, deque<Transition*>& path)
{
  // s1 -> s2
  int d = osm->forward_search_path(s1, s2, path);
  if ( d > 0 ) return d;

  // s2 -> s1
  d = osm->forward_search_path(s2, s1, path);
  if ( d > 0 ) return -d;

  // unreachable
  return 0;
}

struct PathPack
{
  int dist;
  deque<Transition*> path;
};

// Determine the fragile types 
ObjectMachine*
check_deopt(DeoptPack& deopt_pack)
{
  // Identify the instance
  InstanceDescriptor* i_obj = find_instance(deopt_pack.failed_obj, StateMachine::MObject);
  if ( i_obj == NULL ) {
    //printf( "\tOops, never saw this instance: %x\n\n", failed_obj );
    // Defer the inference based on this object
    return NULL;
  }

  ObjectMachine* osm = (ObjectMachine*)i_obj->sm;
  State* inst_state = osm->get_instance_pos(i_obj->id, false); 

  // Process maps
  vector<PathPack*> paths;
  MapList* map_list = deopt_pack.map_list;
  int size = map_list->size();
  int n_homo = 0;

  for ( int i = 0; i < size; ++i ) {
    Map* exp_map = map_list->at(i);
    State* exp_state = osm->search_state(exp_map, false);

    if ( exp_state == NULL ) {
    
      // This optimized code is trained by heterogeneous objects
      // printf( "\tThe optimized code is NOT trained by %s:\n", osm->toString().c_str() );
      /*
      CoreInfo::RefSet &ref_states = exp_map->used_by;
      if ( ref_states.size() > 0 ) {
	printf( "\tIt is trained by:\n" );
      CoreInfo::RefSet::iterator it, end;
      int i = 0;
      for ( it = ref_states.begin(), end = ref_states.end();
	    it != end; ++it ) {
	StateMachine* sm = (*it)->machine;
	((ObjectMachine*)sm)->cause_deopt = true;
	if ( sm->has_name() || i < 3 ) {
          printf( "\t\t[%s]\n", sm->toString().c_str() );
	  if ( !sm->has_name() ) ++i;
	}
      }
      }
      else {
	printf( "\tCaused by a heterogeneous type: %x\n", exp_map->id() );
      }
      
      printf( "\n" );
      */
      continue;
    }

    printf( "[%s] deoptimized at IC %d, *fix*:\n", 
	    deopt_pack.deopt_f->toString().c_str(), deopt_pack.bailout_id );

    // Case split
    printf( "\tCaused by object: %s<%x>\n",
	    osm->toString().c_str(), deopt_pack.failed_obj );
    
    PathPack *pp = new PathPack;
    pp->dist = compute_distance(osm, inst_state, exp_state, pp->path);
    paths.push_back(pp);
    ++n_homo;
  }

  for ( int i = 0; i < paths.size(); ++i ) {
    if ( d > 0 ) {
      /*
       * Case 1:
       * R(inst_state, exp_state): exp_map might be a map for failed_obj in future
       *
       */
      printf( "\tR(inst, exp) = %d: ", d );
      print_path(path, d >= 3 ? d - 3 : 0);
    }
    else if ( d < 0 ) {
      /*
       * Case 2:
       * R(exp_state, inst_state): failed_obj or its group owned exp_map in the past.
       *
       */
      printf( "\tR(inst, exp) = %d: ", d );
      
      // We output the last three transitions
      print_path(path, d <= -3 ? -(d+3) : 0);
    }
    else {
      /*
       * Case 3:
       * \exists s3, R(s3, inst_state) && R(s3, exp_state)
       * s3 is a split point.
       */
      deque<Transition*> tmp_path;
      
      // First obtain the path q0 -> exp_state
      osm->forward_search_path(osm->start, exp_state, tmp_path);
      int total_len = tmp_path.size();
      
      for ( int i = total_len - 1; i > -1; --i ) {
	Transition* trans = tmp_path[i];
	State* t3 = trans->source;
	if ( i_obj->has_history_state(t3) ) {
	  // Found
	  int d = total_len - i;
	  printf( "\tR(t3, exp) = %d: ", d );
	  print_path(tmp_path, d, true); // d > 3 ? total_len - 3 : i, true);
	  
	  osm->forward_search_path(t3, inst_state, path);
	  d = path.size();
	  printf( "\tR(t3, inst) = %d: ", d); 
	  print_path(path, 0, true); //d > 3 ? d - 3: 0, true); 
	  
	  break;
	}
      }
    }
  }

  printf( "\n" );
  return osm;
}
