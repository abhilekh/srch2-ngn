//$Id: Assert.h 3456 2013-06-14 02:11:13Z jiaying $

/*
 * The Software is made available solely for use according to the License Agreement. Any reproduction
 * or redistribution of the Software not in accordance with the License Agreement is expressly prohibited
 * by law, and may result in severe civil and criminal penalties. Violators will be prosecuted to the
 * maximum extent possible.
 *
 * THE SOFTWARE IS WARRANTED, IF AT ALL, ONLY ACCORDING TO THE TERMS OF THE LICENSE AGREEMENT. EXCEPT
 * AS WARRANTED IN THE LICENSE AGREEMENT, SRCH2 INC. HEREBY DISCLAIMS ALL WARRANTIES AND CONDITIONS WITH
 * REGARD TO THE SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES AND CONDITIONS OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT.  IN NO EVENT SHALL SRCH2 INC. BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF SOFTWARE.

 * Copyright © 2010 SRCH2 Inc. All rights reserved
 */

#ifndef ASSERT_H_
#define ASSERT_H_

#ifndef ANDROID
#include <execinfo.h>
#endif
#include <cstdio>
#include <cstdlib>
#include <cassert>

namespace srch2
{
namespace instantsearch
{

inline void print_trace (void)
{
#ifndef ANDROID
    void *array[10];
    size_t size;
    char **strings;
    size_t i;

    size = backtrace (array, 10);
    strings = backtrace_symbols (array, size);

    printf ("Obtained %d stack frames.\n", (int) size);

    for (i = 0; i < size; i++)
        printf ("%s\n", strings[i]);

    free (strings);
#endif
}

#ifndef ASSERT_LEVEL
#define ASSERT_LEVEL 1
#endif

#if ASSERT_LEVEL > 0
#define ASSERT(cond) do {\
        if (cond) {\
        } else {\
            printf("Assert failed in file=%s and line %d\n", __FILE__, __LINE__); \
            print_trace();\
            abort();\
        }\
} while(0)
#else

#define ASSERT(cond) (void)0

#endif
}}

#endif /* ASSERT_H_ */
