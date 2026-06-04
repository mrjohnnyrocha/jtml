#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
  #if defined(JTML_C_API_BUILD)
    #define JTML_API __declspec(dllexport)
  #else
    #define JTML_API __declspec(dllimport)
  #endif
#else
  #define JTML_API __attribute__((visibility("default")))
#endif

typedef struct jtml_context jtml_context;

JTML_API const char* jtml_abi_version(void);
JTML_API void jtml_free(char* value);

JTML_API jtml_context* jtml_create(void);
JTML_API void jtml_destroy(jtml_context* ctx);
JTML_API const char* jtml_last_error(jtml_context* ctx);

JTML_API int jtml_render(const char* source, char** html_out, char** error_out);
JTML_API int jtml_load(jtml_context* ctx, const char* source, char** html_out, char** error_out);

JTML_API char* jtml_bindings(jtml_context* ctx);
JTML_API char* jtml_state(jtml_context* ctx);
JTML_API char* jtml_components(jtml_context* ctx);
JTML_API char* jtml_component_definitions(jtml_context* ctx);

JTML_API int jtml_dispatch(jtml_context* ctx,
                           const char* element_id,
                           const char* event_type,
                           const char* args_json,
                           char** bindings_out,
                           char** error_out);

JTML_API int jtml_component_action(jtml_context* ctx,
                                   const char* component_id,
                                   const char* action_name,
                                   const char* args_json,
                                   char** bindings_out,
                                   char** error_out);

#ifdef __cplusplus
}
#endif
