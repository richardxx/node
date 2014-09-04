// List of events and their handlers

#ifndef EVENTS_H
#define EVENTS_H


#define OBJECT_EVENTS_LIST(V)						\
  V(CreateObjBoilerplate,   create_obj_boilerplate, "+ObjTemp")		\
  V(CreateArrayBoilerplate, create_array_boilerplate, "+AryTemp")	\
  V(CreateObjectLiteral,    create_object_literal,  "+ObjLit")		\
  V(CreateArrayLiteral,     create_array_literal, "+AryLit")		\
  V(CreateNewObject,        create_new_object, "+Obj")			\
  V(CreateNewArray,         create_new_array,  "+Ary")			\
  V(CreateContext,        create_context, "+FCxt")			\
  V(CopyObject,           copy_object,  "#Obj")				\
  V(ChangePrototype,      change_prototype,  "!Proto")			\
  V(NewField,             new_field, "+Fld")				\
  V(UptField, 	          upt_field, "!Fld")				\
  V(DelField,             del_field, "-Fld")				\
  V(SetElem,              set_elem,  "!Elm")				\
  V(DelElem,              del_elem,  "-Elm")				\
  V(CowCopy,              cow_copy,  "#Cow")				\
  V(ExpandArray,          expand_array, "^Ary")

#define FUNCTION_EVENTS_LIST(V)						\
  V(CreateFunction,       create_function,  "+func()")			\
  V(GenFullCode,          gen_full_code, "+FulCode")			\
  V(GenOptCode,           gen_opt_code, "+OptCode")			\
  V(GenOsrCode,           gen_osr_code, "+OsrCode")			\
  V(DisableOpt,           disable_opt, "||Code")			\
  V(ReenableOpt,          reenable_opt, ">Code")			\
  V(OptFailed,            gen_opt_failed, "OptFail")			\
  V(RegularDeopt,         regular_deopt, "Deopt")			\
  V(DeoptAsInline,        deopt_as_inline, "DeoptInl")			\
  V(ForceDeopt,           force_deopt, "FrcDeopt")

#define MAP_EVENTS_LIST(V)						\
  V(BeginDeoptOnMap,      begin_deopt_on_map,  "BegDeoptOnMap")		\
  V(GenDeoptMaps,         gen_deopt_maps,  "GenDeoptMaps" )

#define SIGNAL_EVENTS_LIST(V)						\
  V(ElemToSlowMode,       elem_to_slow, "Elm->Slow")			\
  V(PropertyToSlowMode,   prop_to_slow, "Prop->Slow")			\
  V(ElemToFastMode,       elem_to_fast, "Elm->Fast")			\
  V(PropertyToFastMode,   prop_to_fast, "Prop->Fast")			\
  V(ElemTransition,       elem_transition, "^Elm")			

#define SYS_EVENTS_LIST(V)						\
  V(GCMoveObject,         gc_move_object,  "MovObj")			\
  V(GCMoveCode,           gc_move_code,  "MovCode")			\
  V(GCMoveShared,         gc_move_shared,  "MovShared")			\
  V(GCMoveMap,            gc_move_map,   "MovMap")


// Events types
#define GetEventName(name, handler, desc) name,

enum InternalEvent {  
  OBJECT_EVENTS_LIST(GetEventName)
  FUNCTION_EVENTS_LIST(GetEventName)
  MAP_EVENTS_LIST(GetEventName)
  SIGNAL_EVENTS_LIST(GetEventName)
  SYS_EVENTS_LIST(GetEventName)
  events_count
};

#undef GetEventname


// The text description of all events
extern const char* evt_text[];

#endif
