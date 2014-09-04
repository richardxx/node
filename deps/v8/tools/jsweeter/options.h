// Options

#ifndef OPTIONS_H
#define OPTIONS_H

#define ON_STACK_NAME_SIZE 512

extern const char* input_file;
extern const char* visual_file;
extern const char* slice_sig;

extern bool debug_mode;
extern int draw_mode;
extern int states_count_limit;
extern bool do_analyze;

enum DrawMode {
  DRAW_OBJECTS_ONLY = 0,
  DRAW_FUNCTIONS_ONLY = 1,
  DRAW_BOTH = 2
};

#endif

