#ifndef __KREF_H__
#define __KREF_H__

struct kref {
	int refcount;
};

void kref_set(struct kref *ref, int num);
void kref_init(struct kref *ref);
void kref_get(struct kref *ref);
int kref_put(struct kref *ref, void (*release) (struct kref *ref));

#endif

