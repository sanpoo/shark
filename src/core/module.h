#ifndef __MODULE_H__
#define __MODULE_H__

struct module
{
    int (*master_init)();
    int (*worker_init)();
};

//#define DECLARE_MODULE(name)   __attribute__((constructor)) static void #name()


#endif

