// List of events and their handlers

#ifndef EVENTS_H
#define EVENTS_H


#define OBJECT_EVENTS_LIST(V)					\
  V(CreateObjBoilerplate,   create_obj_boilerplate)		\
  V(CreateArrayBoilerplate, create_array_boilerplate)		\
  V(CreateObjectLiteral,    create_object_literal)		\
  V(CreateArrayLiteral,     create_array_literal)		\
  V(CreateNewObject,        create_new_object)			\
  V(CreateNewArray,         create_new_array)			\
  V(CreateFunction,       create_function)			\
  V(CopyObject,           copy_object)				\
  V(ChangePrototype,      change_prototype)			\
  V(SetMap,               set_map)				\
  V(NewField,             new_field)				\
  V(DelField,             del_field)				\
  V(WriteFieldTransition, write_field_transition)		\
  V(ElemTransition,       elem_transition)			\
  V(CowCopy,              cow_copy)				\
  V(ElemToSlowMode,       elem_to_slow)				\
  V(PropertyToSlowMode,   prop_to_slow)				\
  V(ElemToFastMode,       elem_to_fast)				\
  V(PropertyToFastMode,   prop_to_fast)				


#define FUNCTION_EVENTS_LIST(V)				\
  V(GenFullCode,          gen_full_code)		\
  V(GenOptCode,           gen_opt_code)                 \
  V(GenOsrCode,           gen_osr_code)			\
  V(SetCode,              set_code)			\
  V(DisableOpt,           disable_opt)			\
  V(ReenableOpt,          reenable_opt)			\
  V(OptFailed,            gen_opt_failed)		\
  V(RegularDeopt,         regular_deopt)                \
  V(DeoptAsInline,        deopt_as_inline)		\
  V(ForceDeopt,           force_deopt)


#define MAP_EVENTS_LIST(V)				\
  V(BeginDeoptOnMap,      begin_deopt_on_map)		


#define SYS_EVENTS_LIST(V)				\
  V(GCMoveObject,         gc_move_object)		\
  V(GCMoveCode,           gc_move_code)			\
  V(GCMoveShared,         gc_move_shared)		\
  V(GCMoveMap,            gc_move_map)			\
  V(NotifyStackDeoptAll, notify_stack_deopt_all)	

#endif
