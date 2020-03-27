#include <assert.h>
#include "common.h"
#include "point.h"
#include <math.h>

void
point_translate(struct point *p, double x, double y)
{
    p->x = p->x + x;
    p->y = p->y + y;
}

double
point_distance(const struct point *p1, const struct point *p2)
{
    return sqrt( (double) (p1->x - p2->x)*(p1->x - p2->x)
                          + (p1->y - p2->y)*(p1->y - p2->y));
}

int
point_compare(const struct point *p1, const struct point *p2)
{
    struct point *zero_vec = malloc(sizeof(struct point));
    zero_vec->x = 0.0;
    zero_vec->y = 0.0;
    //point_set(zero_vec, 0.0, 0.0);
    double distance_p1 = point_distance(p1, zero_vec);
    double distance_p2 = point_distance(p2, zero_vec);
    
    //delete zero_vec;
    
    if(distance_p1 > distance_p2) return 1;
    else if(distance_p1 == distance_p2) return 0;
    else return -1;
}
