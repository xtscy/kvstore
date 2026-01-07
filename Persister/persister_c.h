#ifndef _PERSISTER_C_H_
#define _PERSISTER_C_H_

#ifdef __cplusplus
extern "C" {
#endif


typedef void* persister_handle;
persister_handle persister_create_c(const char *path);
bool persister_insert(persister_handle, uint16_t, const char*, int);
bool persister_get(persister_handle, uint16_t, const char*);
bool persister_quit(persister_handle handle);
#ifdef __cplusplus
}
#endif

#endif