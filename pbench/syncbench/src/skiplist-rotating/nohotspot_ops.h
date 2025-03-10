/*
 * Interface for the No Hot Spot Non-Blocking Skip List
 * operations.
 *
 * Author: Ian Dick, 2013.
 */

#ifndef NOHOTSPOT_OPS_H_
#define NOHOTSPOT_OPS_H_

#include "common.h"
#include "skiplist.h"
#include "background.h"
//#include "garbagecoll.h"
//#include "ptst.h"

//typedef enum sl_optype sl_optype_t;
enum sl_optype {
        CONTAINS,
        DELETE,
        INSERT
};
typedef enum sl_optype sl_optype_t;
int sl_do_operation(set_t *set, sl_optype_t optype,
                    unsigned int key, void *val);

/* these are macros instead of functions to improve performance */
#define sl_contains(a, b) sl_do_operation((a), CONTAINS, (b), NULL);
#define sl_delete(a, b) sl_do_operation((a), DELETE, (b), NULL);
#define sl_insert(a, b, c) sl_do_operation((a), INSERT, (b), (c));

#endif /* NOHOTSPOT_OPS_H_ */
