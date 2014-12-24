#include "pub.h"
#include "kref.h"

void kref_set(struct kref *ref, int num)
{
    atomic_set(&ref->refcount, num);
}

void kref_init(struct kref *ref)
{
    kref_set(ref, 1);
}

void kref_get(struct kref *ref)
{
    atomic_inc(&ref->refcount);
}

/**
    kref_put - decrement refcount for object.
    @kref: object.
    @release: pointer to the function that will clean up the object when the
 	     last reference to the object is released.
 	     This pointer is required, and it is not acceptable to pass kfree
 	     in as this function.

    Decrement the refcount, and if 0, call release().
    Return 1 if the object was removed, otherwise return 0.  Beware, if this
    function returns 0, you still can not count on the kref from remaining in
    memory.  Only use the return value if you want to see if the kref is now
    gone, not present.
*/
int kref_put(struct kref *ref, void (*release)(struct kref *ref))
{
    if (atomic_dec_and_test(&ref->refcount))
    {
        release(ref);
        return 1;
    }
    return 0;
}

