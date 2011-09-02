/*
** $Id: semaphore.h,v 1.8 2006/06/07 09:21:28 krishnap Exp $
**
** Matthew Allen
** description: 
*/

#ifndef _CHIMERA_SEMAPHORE_H_
#define _CHIMERA_SEMAPHORE_H_

typedef struct
{
    int val;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} Sema;

void *sema_create (int val);
void sema_destroy (void *v);
int sema_p (void *v, double timeout);
void sema_v (void *v);

#endif /* _CHIMERA_SEMAPHORE_H_ */
