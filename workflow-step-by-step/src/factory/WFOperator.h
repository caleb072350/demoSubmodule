#ifndef _WFOPERATOR_H_
#define _WFOPERATOR_H_

#include "Workflow.h"

/**
 * @file   WFOperator.h
 * @brief  Workflow Series/Parallel/Task operator
 */

/**
 * @brief      S=S>P
 * @note       Equivalent to x.push_back(&y)
*/
static inline SeriesWork& operator>(SeriesWork& x, ParallelWork& y);

/**
 * @brief      S=P>S
 * @note       Equivalent to y.push_front(&x)
*/
static inline SeriesWork& operator>(ParallelWork& x, SeriesWork& y);

/**
 * @brief      S=S>t
 * @note       Equivalent to x.push_back(&y)
*/
static inline SeriesWork& operator>(SeriesWork& x, SubTask& y);

/**
 * @brief      S=S>t
 * @note       Equivalent to x.push_back(y)
*/
static inline SeriesWork& operator>(SeriesWork& x, SubTask *y);

/**
 * @brief      S=t>S
 * @note       Equivalent to y.push_front(&x)
*/
static inline SeriesWork& operator>(SubTask& x, SeriesWork& y);

/**
 * @brief      S=t>S
 * @note       Equivalent to y.push_front(x)
*/
static inline SeriesWork& operator>(SubTask *x, SeriesWork& y);

#endif